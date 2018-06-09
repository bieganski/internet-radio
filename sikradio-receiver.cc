#include <stdio.h>
#include <iostream>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <byteswap.h>
#include <poll.h>
#include "utils.h"
#include <thread>
#include <atomic>
#include <cassert>
#include <mutex>
#include <condition_variable>
#include <csignal>
#include <fcntl.h>

#include "telnet_consts.hpp"
#include "recv_consts.hpp"
#include "telnet_consts.hpp"

#include "GroupSock.h"
#include "MessageParser.h"
#include "AudioFIFO.h"
#include "Menu.h"
#include "Transmitter.h"
#include "StationChanger.h"
#include "utils.h"

using namespace std;
using namespace Recv_Consts;
using namespace TelnetConstants;

std::atomic<bool> PROGRAM_RUNNING(true);

std::string SHRD_ACT_STATION;

std::atomic<uint64_t> ACT_SESS(0); // actual session id

AudioFIFO SHRD_FIFO{0, Recv_Consts::BSIZE}; // buffer fifo, it needs reinit
// (varing packet length)

std::mutex fifo_mut;

std::map<std::string, Transmitter> SHRD_TRANSMITTERS{}; // name identifies transmitter

std::string NAME; // name of station to be played

Menu SHRD_MENU = Menu(SHRD_ACT_STATION, SHRD_TRANSMITTERS);

StationChanger ST_CHNGR{};

std::string DISCOVER_ADDR{"255.255.255.255"};

const char *LOOKUP = "ZERO_SEVEN_COME_IN\n";

const char *ST_CHNGD = "STATION_CHANGED";


void parse_args(int argc, char *argv[]) {
    int c;
    unsigned tmp;
    stringstream ss;
    string tmp_str;
    while ((c = getopt(argc, argv, "d:C:U:b:R:n:")) != -1) {
        switch (c) {
            case 'd': // discover address
                if (!proper_ip(optarg))
                    err("Wrong argument.");
                DISCOVER_ADDR = string{optarg};
                break;
            case 'C': // control datagrams port
                tmp = get_pos_nr_or_err(optarg);
                if (!tmp || (tmp >> 16))
                    err("Port number must be 16-bit unsigned!");
                CTRL_PORT = (uint16_t) tmp;
                break;
            case 'U': // user interface port
                tmp = get_pos_nr_or_err(optarg);
                if (!tmp || (tmp >> 16))
                    err("Port number must be 16-bit unsigned!");
                UI_PORT = (uint16_t) tmp;
                break;
            case 'b': // buffer size
                BSIZE = get_pos_nr_or_err(optarg);
                break;
            case 'R': // retransmission time
                RTIME = get_pos_nr_or_err(optarg);
                break;
            case 'n': // transmittor name
                ss = stringstream{};
                for (int i = optind - 1; i < argc && *argv[i] != '-'; i++) {
                    ss << argv[i];
                    if (argc - 1 != i)
                        ss << " ";
                }
                NAME = ss.str();
                if (NAME.size() >= NAME_LEN)
                    err("Transmitter name too long.");
                break;
            default:
                // getopt handles wrong arguments
                exit(1);
        }
    }
}

const size_t MAX_TELNET_CONN = 10; // maximum telnet clients

const size_t TIMEOUT = 20; // seconds

const size_t LOOKUP_TIME = 5; // seconds


const size_t SOCKS_IN_USE = 4; // reply, data, UI-accept socks
const size_t LOOKUP_SOCK = 0;
const size_t DATA_SOCK = 1;
const size_t UI_ACCEPT_SOCK = 2;
const size_t ST_CHNG_SOCK = 3;

std::atomic<size_t> UI_CONNECTED{0};

struct pollfd FDS[SOCKS_IN_USE + MAX_TELNET_CONN];

char BUFF[BUFF_LEN];

std::atomic<bool> PLAY{false};


void refreshUI() {
    string displ = SHRD_MENU.str();
    for (size_t i = 0; i < UI_CONNECTED; i++) {
        write(FDS[SOCKS_IN_USE + i].fd, CLEAR, strlen(CLEAR));
        write(FDS[SOCKS_IN_USE + i].fd, displ.c_str(), displ.size());
    }
}

/**
 * Prepares and returns sock used by TCP UI.
 */
int set_TCP_accept_sock() {
    struct sockaddr_in srvr;
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        err("socket create error");

    srvr.sin_family = AF_INET;
    srvr.sin_addr.s_addr = htonl(INADDR_ANY);
    srvr.sin_port = htons(UI_PORT);

    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        err("socket option error");

    if (bind(sock, (struct sockaddr *) &srvr, sizeof(srvr)) < 0)
        err("socket bind error");

    if (listen(sock, QUEUE_LEN) < 0)
        err("socket listen error");

    return sock;
}


/**
 * Handles incoming UI connect requests.
 */
void accept_incoming(int sock) {
    struct sockaddr_in cli_addr;
    socklen_t addr_len;

    if (UI_CONNECTED == MAX_TELNET_CONN)
        return;

    int msg_sock = accept(sock, (struct sockaddr *) &cli_addr, &addr_len);
    if (msg_sock < 0)
        return;

    size_t act_UI_idx = SOCKS_IN_USE + UI_CONNECTED;
    FDS[act_UI_idx].fd = msg_sock;
    FDS[act_UI_idx].events = POLLIN;
    FDS[act_UI_idx].revents = 0;

    int optval = 1;
    if (setsockopt(msg_sock, SOL_SOCKET, SO_REUSEADDR, (void *) &optval,
                   sizeof(int)) < 0)
        err("setsockopt(SO_REUSEADDR) failed");

    // configuring telnet
    if (write(msg_sock, NEGOTIATE, strlen(NEGOTIATE)) != strlen(NEGOTIATE)) {
        err("negotiate error");
    }

    ++UI_CONNECTED;
    refreshUI();
}


/**
 * Reacts on users' actions (UP/DOWN), changing station.
 */
void UI_act(int sock) {
    ssize_t recv_len = read(sock, BUFF, BUFF_LEN);
    BUFF[recv_len] = '\0';

    if (SHRD_TRANSMITTERS.empty()) {
        return;
    }

    auto it = SHRD_TRANSMITTERS.find(SHRD_ACT_STATION);
    if (strncmp(BUFF, UP, 3) == 0) {
        if (it == SHRD_TRANSMITTERS.begin())
            return;
        SHRD_ACT_STATION = get<0>(*(++it));
    }
    else if (strncmp(BUFF, DOWN, 3) == 0) {
        if (++it == SHRD_TRANSMITTERS.end())
            return;
        SHRD_ACT_STATION = get<0>(*(it));
    }
    else { ; // ignore bad request
        return;
    }
    refreshUI();
    ST_CHNGR.change_station();
}


/**
 * Simply parser REPLY message, updating list of visible transmitters.
 */
void parse_reply(int sock) {
    MessageParser mp;
    ssize_t recv_len = read(sock, BUFF, BUFF_LEN);
    BUFF[recv_len] = '\0';
    if (mp.parse(BUFF) != Mess::REPLY) {
        return;
    }

    stringstream ss(BUFF);
    string bor, addr, port, name, tmp_name;
    ss >> bor >> addr >> port;
    ss >> name;
    while (ss >> tmp_name) {
        name.append(" ");
        name.append(tmp_name);
    }

    auto it = SHRD_TRANSMITTERS.find(name);
    if (it == SHRD_TRANSMITTERS.end()) {
        // new station detected
        Transmitter tr = Transmitter(addr, (in_port_t) stoi(port));
        SHRD_TRANSMITTERS.insert(std::make_pair(name, tr));
        if ((SHRD_TRANSMITTERS.size() == 1 && NAME.empty()) || name == NAME) {
            // start playing new station
            SHRD_ACT_STATION = name;
            ST_CHNGR.change_station();
        }
        refreshUI();
    }
    else {
        // update old one
        std::get<1>(*it).last_reply_time = (uint64_t) time(NULL);
    }
}


void send_lookup(int sock) {
    sockaddr_in broad;
    socklen_t len = sizeof(broad);
    memset(&broad, '\0', sizeof(sockaddr_in));
    inet_aton(DISCOVER_ADDR.c_str(), &broad.sin_addr);
    broad.sin_family = AF_INET;
    broad.sin_port = htons(CTRL_PORT);

    sendto(sock, LOOKUP, strlen(LOOKUP), 0, (sockaddr *) &broad, len);
}

/**
 * Iterate through visible stations to remove old ones
 * (seen more than TIMEOUT earlier).
 */
void check_timeout() {
    for (auto &sndr : SHRD_TRANSMITTERS) {
        if (time(NULL) - std::get<1>(sndr).last_reply_time >= TIMEOUT) {
            // old one, need to delete it
            string del_name = get<0>(sndr);
            auto del_it = SHRD_TRANSMITTERS.find(get<0>(sndr));
            assert(del_it != SHRD_TRANSMITTERS.end());
            SHRD_TRANSMITTERS.erase(del_it);

            if (del_name == SHRD_ACT_STATION) { // we delete actually played one
                if (SHRD_TRANSMITTERS.empty()) // last one
                    SHRD_ACT_STATION = string{};
                else
                    SHRD_ACT_STATION = string{
                        get<0>(*SHRD_TRANSMITTERS.begin())};
                ST_CHNGR.change_station();
            }
            refreshUI();
        }
    }
}

/**
 * It does all the stuff needed to change station.
 */
void change_station(int sock, GroupSock &data_sock) {
    ssize_t rcv_len = read(sock, BUFF, BUFF_LEN);
    BUFF[rcv_len] = '\0';
    if (strcmp(BUFF, ST_CHNGD) != 0)
        assert(false);

    PLAY = false;
    fifo_mut.lock();
    SHRD_FIFO.clear();
    fifo_mut.unlock();

    if (SHRD_ACT_STATION.empty()) {
        // no station available at this moment
        FDS[DATA_SOCK].fd = -1;
    }
    else {
        data_sock = GroupSock{Type::MULTICAST};
        Transmitter &tr = get<1>(*SHRD_TRANSMITTERS.find(SHRD_ACT_STATION));
        data_sock.add_member(tr.mcast.c_str());
        data_sock.bind(tr.mcast.c_str(), tr.port);
        FDS[DATA_SOCK].fd = data_sock.get_sock();
    }
    refreshUI();
}

/**
 * Play action, done by separate thread, so it needs mutexes.
 */
void play() {
    if (!PLAY)
        return;
    fifo_mut.lock();
    assert(SHRD_FIFO.playing_possible());
    std::pair<uint64_t, std::string> _res = SHRD_FIFO.str();
    uint64_t byte_to_read = std::get<0>(_res);
    fifo_mut.unlock();

    cout << std::get<1>(_res);

    while (PLAY) {
        usleep(60);
        fifo_mut.lock();
        std::pair<uint64_t, std::string> res = SHRD_FIFO.str(byte_to_read);
        fifo_mut.unlock();
        cout << std::get<1>(res);
        byte_to_read = std::get<0>(res);
    }
}

void rexmit(std::set<uint64_t> to_rexmit) {
    stringstream ss;
    int cntr = 5;
    assert(!to_rexmit.empty());
    ss << "LOUDER_PLEASE ";
    bool first = true;
    for (uint64_t i : to_rexmit) {
        if (!first)
            ss << ",";
        else
            first = false;
        ss << i;
    }
    while (cntr--) {
        usleep(Recv_Consts::RTIME);
        // send rexmit to actual server
    }
}

void parse_data_pack(ssize_t rcv_len, std::thread &play_thrd) {
    std::set<uint64_t> to_rexmit;
    if (rcv_len <= 0)
        err("receive error");
    uint64_t _SESS_ID, _FIRST_BYTE;
    size_t _PSIZE;
    memcpy(&_SESS_ID, BUFF, SESS_ID_SIZE);
    memcpy(&_FIRST_BYTE, BUFF + SESS_ID_SIZE, BYTE_NUM_SIZE);

    _PSIZE = rcv_len - INFO_LEN;
    _SESS_ID = bswap_64(_SESS_ID);
    _FIRST_BYTE = bswap_64(_FIRST_BYTE);


    // now, when we got pack received we wish to put it in FIFO,
    // previously checking if pack is first one, if session id is same
    // and if we are able to start playing music

    if (ACT_SESS > _SESS_ID)
        return;

    if (ACT_SESS < _SESS_ID) {
        ACT_SESS = _SESS_ID;
        PLAY = false;
        fifo_mut.lock();
        SHRD_FIFO.clear();
        fifo_mut.unlock();
        return; // only set new sess id, abandon first package
    }

    assert(_PSIZE != 0);

    fifo_mut.lock();
    if (SHRD_FIFO.empty()) {
        // first package here
        SHRD_FIFO.reinit(_PSIZE);
    }

    // insert_pack method handles normal and rexmit packs
    to_rexmit = SHRD_FIFO.insert_pack(_FIRST_BYTE, BUFF + INFO_LEN, _PSIZE);

    if (!to_rexmit.empty()) {
        //thread rexmit_th = thread{rexmit, to_rexmit};
        //rexmit_th.detach(); // its not infinitely looped
        // TODO sending rexmit
    }

    if (PLAY == false && SHRD_FIFO.playing_possible()) {
        if (!SHRD_FIFO.complete()) {
            fifo_mut.unlock();
            ST_CHNGR.change_station();
            return;
        }
        // here playing is available
        fifo_mut.unlock();
        if (play_thrd.joinable())
            play_thrd.join();
        play_thrd = thread{play};
        PLAY = true;
    }
    fifo_mut.unlock();
}

/**
 * Receives and passes to further processing data packs.
 */
void recv_data_pack(int sock, std::thread &play_thrd) {
    Transmitter &tr = get<1>(*SHRD_TRANSMITTERS.find(SHRD_ACT_STATION));
    struct sockaddr_in addr = GroupSock::make_addr(tr.mcast.c_str(), tr.port);
    socklen_t addrlen = sizeof(addr);
    ssize_t rcv_len = recvfrom(sock, BUFF, BUFF_LEN, 0, (sockaddr *) &addr,
                               &addrlen);
    parse_data_pack(rcv_len, play_thrd);
}



void main_server() {
    thread PLAY_THREAD{play};

    GroupSock lookup_sock{};

    GroupSock data_sock{Type::MULTICAST, O_NONBLOCK};

    int UI_accept_sock = set_TCP_accept_sock();

    FDS[LOOKUP_SOCK].fd = lookup_sock.get_sock();
    FDS[DATA_SOCK].fd = data_sock.get_sock();
    FDS[UI_ACCEPT_SOCK].fd = UI_accept_sock;
    FDS[ST_CHNG_SOCK].fd = ST_CHNGR.read_sock();

    for (size_t i = 0; i < SOCKS_IN_USE; i++) {
        FDS[i].events = POLLIN;
        FDS[i].revents = 0;
    }

    send_lookup(lookup_sock.get_sock());

    while (PROGRAM_RUNNING) {
        size_t act_conn = UI_CONNECTED.load();
        int ret = poll(FDS, SOCKS_IN_USE + act_conn, LOOKUP_TIME * 1000);

        if (ret < 0)
            err("poll error");

        if (ret == 0) {
            // send lookup and check for stations to be deleted (timeout)
            send_lookup(lookup_sock.get_sock());
            check_timeout();
            continue;
        }

        for (unsigned i = 0; i < SOCKS_IN_USE + act_conn; i++) {
            if (FDS[i].revents & POLLIN) {
                FDS[i].revents &= ~POLLIN;
                switch (i) {
                    case (LOOKUP_SOCK):
                        parse_reply(lookup_sock.get_sock());
                        break;
                    case (DATA_SOCK):
                        recv_data_pack(data_sock.get_sock(), PLAY_THREAD);
                        break;
                    case (ST_CHNG_SOCK):
                        change_station(ST_CHNG_SOCK, data_sock);
                        if (PLAY_THREAD.joinable())
                            PLAY_THREAD.join();
                        break;
                    case (UI_ACCEPT_SOCK):
                        accept_incoming(UI_accept_sock);
                        break;
                    default:
                        // one of connected to UI acted
                        assert(i >= SOCKS_IN_USE);
                        UI_act(FDS[i].fd);
                        break;
                }
            }
        } // for
    }

    close(lookup_sock.get_sock());
    close(data_sock.get_sock());
    close(UI_accept_sock);
}

int main(int argc, char **argv) {
    parse_args(argc, argv);

    main_server();
}
