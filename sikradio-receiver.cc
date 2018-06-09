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

AudioFIFO SHRD_FIFO{0, BSIZE}; // buffer fifo, it needs reinit
// (varing packet length)

std::mutex fifo_mut;

std::map<std::string, Transmitter> SHRD_TRANSMITTERS{}; // name identifies transmitter

std::string NAME; // name of station to be played

Menu SHRD_MENU = Menu(SHRD_ACT_STATION, SHRD_TRANSMITTERS);

StationChanger ST_CHNGR{};

std::string DISCOVER_ADDR{"255.255.255.255"};

const char *LOOKUP = "ZERO_SEVEN_COME_IN\n";

const char *ST_CHNGD = "STATION_CHANGED";

// TODO !!!!!!!!!!!!! NONBLOCKING O_NONBLOCK


// TODO przy lookupie sprawdzic dlugosc nazwy

// TODO sprawdzac session id i inne (?) rzeczy jak sie dostanie pakiet



// TODO TODO TODO TODO UZYJ DISCOVER PORT


// TODO WAZNE:        ZROBIC DOBRZE NAZWE " " "" " "


void signalHandler(int signum) {
    cout << "\nI received signal (" << signum << "). Close all connections.\n";
    PROGRAM_RUNNING = false;
    // todo moze join play?
}


void parse_args(int argc, char *argv[]) {
    int c;
    unsigned tmp;
    stringstream ss;
    string tmp_str;
    while ((c = getopt(argc, argv, "d:C:U:b:R:n:")) != -1) {
        switch (c) {
            case 'd': // discover address
                tmp_str = string{optarg};
                DISCOVER_ADDR = tmp_str;
                // TODO
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





// TODO reusse addr w wielu miejscach

// TODO usuwanie odlaczonych klientow z vectora


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

std::mutex STATION_TIED_MUT; // mutex associated with station changing

std::atomic<bool> PLAY{false};


void refreshUI() {
    string displ = SHRD_MENU.str();
    for (int i = 0; i < UI_CONNECTED; i++) {
        write(FDS[SOCKS_IN_USE + i].fd, CLEAR, strlen(CLEAR));
        write(FDS[SOCKS_IN_USE + i].fd, displ.c_str(), displ.size());
    }
}


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

void accept_incoming(int sock) {
    struct sockaddr_in cli_addr;
    socklen_t addr_len;

    if (UI_CONNECTED == MAX_TELNET_CONN)
        return;

    int msg_sock = accept(sock, (struct sockaddr *) &cli_addr, &addr_len);
    if (msg_sock < 0)
        err("accept error"); // TODO cos lżejszego :>

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


void UI_act(int sock) {
    ssize_t recv_len = read(sock, BUFF, BUFF_LEN);
    BUFF[recv_len] = '\0';

    if (SHRD_TRANSMITTERS.empty()) {
        return;
    }

    auto it = SHRD_TRANSMITTERS.find(SHRD_ACT_STATION);
    if (strncmp(BUFF, UP, 3) == 0) {
        //cout << "up\n";
        if (it == SHRD_TRANSMITTERS.begin())
            return;
        SHRD_ACT_STATION = get<0>(*(++it));
    }
    else if (strncmp(BUFF, DOWN, 3) == 0) {
        //cout << "down\n";
        if (++it == SHRD_TRANSMITTERS.end())
            return;
        SHRD_ACT_STATION = get<0>(*(it));
    }
    else { ; // ignore bad request
        return;
    }
    ST_CHNGR.change_station(); // TODO on chyba zrobi refresh
}


void parse_reply(int sock) {
    MessageParser mp;
    //cout << "------------parse reply";
    ssize_t recv_len = read(sock, BUFF, BUFF_LEN);
    BUFF[recv_len] = '\0';
    if (mp.parse(BUFF) != Mess::REPLY) {
        cout << "DOSTALEM NIEREPLAY:" << BUFF;
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

    STATION_TIED_MUT.lock();
    auto it = SHRD_TRANSMITTERS.find(name);
    if (it == SHRD_TRANSMITTERS.end()) {
        // new station detected
        //cout << "SNDR_UPDT: dodaję nową stację" << name << "\n";
        Transmitter tr = Transmitter(addr, (in_port_t) stoi(port));
        SHRD_TRANSMITTERS.insert(std::make_pair(name, tr));
        if (SHRD_TRANSMITTERS.size() == 1 || name == NAME) {
            // start playing new station
            //cout << "daje stacje domyslna\n";
            SHRD_ACT_STATION = name;
            ST_CHNGR.change_station();
        }
        refreshUI();
    }
    else {
        // update old one
        //cout << "SNDR_UPDT: ZMIENIAM STACJE\n";
        std::get<1>(*it).last_reply_time = (uint64_t) time(NULL);
    }
    STATION_TIED_MUT.unlock();
}


char *wypisz(sockaddr_in *lol) {
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(lol->sin_addr), str, INET_ADDRSTRLEN);
    cout << "LOL: port:" << ntohs(lol->sin_port) << ", addr:" << str << "\n";
    return str;
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


void change_station(int sock, GroupSock &data_sock) {
    ssize_t rcv_len = read(sock, BUFF, BUFF_LEN);
    BUFF[rcv_len] = '\0';
    if (strcmp(BUFF, ST_CHNGD) != 0)
        assert(false);

    PLAY = false;

    if (SHRD_ACT_STATION.empty()) {
        // no station available at this moment
        FDS[DATA_SOCK].fd = -1;
    }
    else {
        //cout << "MUT: 1 -przed\n";
        fifo_mut.lock();
        //cout << "CZYSZCZE KOLEJKE\n";
        SHRD_FIFO.clear();
        fifo_mut.unlock();
        //cout << "MUT: 1 -po\n";
        data_sock = GroupSock{Type::MULTICAST};
        Transmitter &tr = get<1>(*SHRD_TRANSMITTERS.find(SHRD_ACT_STATION));
        data_sock.add_member(tr.mcast.c_str());
        data_sock.bind(tr.mcast.c_str(), tr.port);
        FDS[DATA_SOCK].fd = data_sock.get_sock();
    }

    refreshUI();
}

void play() {
    if (!PLAY)
        return;
    //cout << "MUT: 2-przed\n";
    cout << "PLAY: ZACZYNAM!!!!!\n";
    fifo_mut.lock();
    assert(SHRD_FIFO.playing_possible());
    std::pair<uint64_t, std::string> _res = SHRD_FIFO.str();
    uint64_t byte_to_read = std::get<0>(_res);
    fifo_mut.unlock();
    if (std::get<1>(_res).empty()) {
        // end
        return;
    }
    //cout << "MUT: 2 -po\n";
    cout << std::get<1>(_res);
    //cout << "PLAY: mogę 1  wypisac " << get<1>(_res).size() << " bajtow.\n";
    // read till next packet in queue
    while (PLAY) {
        usleep(100); // TODO pobawic sie tym
        //cout << "MUT: 3 -przed\n";
        fifo_mut.lock();
        std::pair<uint64_t, std::string> res = SHRD_FIFO.str(byte_to_read);
        fifo_mut.unlock();

        cout << std::get<1>(res);
        //cout << "MUT: 3 -po\n";
        //cout << "PLAY: mogę 2 wypisac " << get<1>(res).size() << " bajtow.\n";
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
        //cout << "wysylam rexmit";
        usleep(RTIME);
        // tODO wyslij to do serwera
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

    if (ACT_SESS < _SESS_ID) { // TODO to jest zle raczej
        ACT_SESS = _SESS_ID;
        ST_CHNGR.change_station();
        return; // only set new sess id, abandon first package
    }
    //cout << "MUT: 4 -przed\n";
    fifo_mut.lock();
    if (SHRD_FIFO.empty()) {
        // first package here
        SHRD_FIFO.reinit(_PSIZE);
    }

    // insert pack method handles normal and rexmit packs
    to_rexmit = SHRD_FIFO.insert_pack(_FIRST_BYTE, BUFF + INFO_LEN, _PSIZE);

    if (!to_rexmit.empty()) {
        thread rexmit_th = thread{rexmit, to_rexmit};
        rexmit_th.detach();
    }

    if (PLAY == false && SHRD_FIFO.playing_possible()) {
        if (!SHRD_FIFO.complete()) {
            fifo_mut.unlock();
            //cout << "MUT: 4 -po\n";
            ST_CHNGR.change_station();
            return;
        }
        fifo_mut.unlock();
        // here playing is available
        if (play_thrd.joinable())
            play_thrd.join();
        play_thrd = thread{play};
        PLAY = true;
    }
    fifo_mut.unlock();
    //cout << "MUT: 4 -po\n";
}

void recv_data_pack(int sock, std::thread &play_thrd) {
    Transmitter &tr = get<1>(*SHRD_TRANSMITTERS.find(SHRD_ACT_STATION));
    struct sockaddr_in addr = GroupSock::make_addr(tr.mcast.c_str(), tr.port);
    socklen_t addrlen = sizeof(addr);
    ssize_t rcv_len = recvfrom(sock, BUFF, BUFF_LEN, 0, (sockaddr *) &addr,
                               &addrlen);
    //cout << "RCV: dostalem pakiet " << rcv_len << "bajtow\n";
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

    for (int i = 0; i < SOCKS_IN_USE; i++) {
        FDS[i].events = POLLIN;
        FDS[i].revents = 0;
    }

    send_lookup(lookup_sock.get_sock());

    while (PROGRAM_RUNNING) {
        size_t act_conn = UI_CONNECTED.load();
        int ret = poll(FDS, SOCKS_IN_USE + act_conn, 222);
        // TODO czas LOOKUP_TIME

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
                //cout << "wpadl pollin nr. " << i << "\n";
                switch (i) {
                    case (LOOKUP_SOCK):
                        parse_reply(lookup_sock.get_sock());
                        break;
                    case (DATA_SOCK):
                        recv_data_pack(data_sock.get_sock(), PLAY_THREAD);
                        break;
                    case (ST_CHNG_SOCK): // TODO niech UI itp zmienia sock
                        //cout << "-----OSTRO ZLAPALEM CHANGA\n";
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
            FDS[i].revents &= ~POLLIN;
        } // for

    }
}

int main(int argc, char **argv) {
    //cout << "UI: " << UI_PORT << "\n";
    parse_args(argc, argv);

    main_server();
}



#ifdef lol
int main(int argc, char *argv[]) {
    /* argumenty wywołania programu */
    char *multicast_dotted_address;
    in_port_t local_port;

    if (argc != 3)
        err("Usage: %s multicast_dotted_address local_port");
    multicast_dotted_address = argv[1];
    local_port = (in_port_t) atoi(argv[2]);


    // TEN CZYTA CO MU WYSLANO
    //std::thread READ_THREAD(read_data, multicast_dotted_address, local_port);




    // TEN WYSYLA TEST NA BROAD
    GroupSock bcast_sock{};
    //bcast_sock.connect(INADDR_BROADCAST, CTRL_PORT);
    struct sockaddr_in BROAD;
    socklen_t BROAD_SIZE = sizeof(BROAD);
    memset(&BROAD, '\0', sizeof(sockaddr_in));
    BROAD.sin_addr.s_addr = htonl(INADDR_ANY);
    BROAD.sin_family = AF_INET;
    BROAD.sin_port = htons(CTRL_PORT);

    struct sockaddr_in wynik;
    socklen_t len = sizeof(wynik);
    if (getsockname(bcast_sock.get_sock(), (struct sockaddr *) &wynik, &len) ==
        -1)
        err("getsockname");

    // ten nowy czyta odp
    cout << "czytam na porcie " << ntohs(wynik.sin_port) << "\n";
    thread lol = thread(czytaj_odp, ntohs(wynik.sin_port),
                        bcast_sock.get_sock());

    sleep(2);
    // TODO dodatkowe \0 na koncu nie powinny byc chyba
    const char *rex = "LOUDER_PLEASE 0,512,1024,1536,5632,3584,-3,ab,2,21\n";
    //write(bcast_sock.get_sock(), rex, strlen(rex) + 1);
    sendto(bcast_sock.get_sock(), rex, strlen(rex), 0, (sockaddr *) &BROAD,
           BROAD_SIZE);
    const char *look = "ZERO_SEVEN_COME_IN\n";
    // write(bcast_sock.get_sock(), look, strlen(look) + 1);
    sendto(bcast_sock.get_sock(), look, strlen(look), 0,
           (sockaddr *) &BROAD, BROAD_SIZE);
    // TODO teraz nie wysylam \0

    PROGRAM_RUNNING = false;
    //READ_THREAD.join();
    lol.join();

    return 0;
}
#endif


#ifdef lol
//void update_sndr_list(char *reply_msg) {
//    stringstream ss(reply_msg);
//    string bor, addr, port, name, tmp_name;
//    ss >> bor >> addr >> port;
//    ss >> name;
//    while (ss >> tmp_name) {
//        name.append(" ");
//        name.append(tmp_name);
//    }
//    //cout << "bor: " << bor << "addr:" << addr << "port: " << port << "name:"
//    //     << name << "\n";
//    trans_mut.lock();
//    auto it = SHRD_TRANSMITTERS.find(name);
//    auto end = SHRD_TRANSMITTERS.end();
//    trans_mut.unlock();
//    if (it == end) {
//        // new station
//        cout << "SNDR_UPDT: dodaję nową stację" << name << "\n";
//        Transmitter tr = Transmitter(addr, (in_port_t) stoi(port));
//        trans_mut.lock();
//        bool empty = SHRD_TRANSMITTERS.empty();
//        SHRD_TRANSMITTERS.insert(std::make_pair(name, tr));
//        trans_mut.unlock();
//        // maybe it needs to start playing? It depends on -n option
//        if (empty || NAME == name) {
//            act_stat_mut.lock();
//            SHRD_ACT_STATION = name;
//            act_stat_mut.unlock();
//            cout << "SNDR_UPDT: ZMIENIAM STACJE\n";
//            STATION_CHANGED = true;
//        }
//        menu_mut.lock();
//        SHRD_MENU.display();
//        menu_mut.unlock();
//        cout << "SNDR_UPDT: skonczylem dodawac\n";
//    }
//    else {
//        // second time we need actual state, need to lock once more :(
//        trans_mut.lock();
//        auto it_sec = SHRD_TRANSMITTERS.find(name);
//        auto end_sec = SHRD_TRANSMITTERS.end();
//        if (it_sec != end_sec)
//            std::get<1>(*it).last_reply_time = (uint64_t) time(NULL);
//        trans_mut.unlock();
//    }
//}


//void read_and_output() {
//    char buffer[BUFF_LEN];
//    struct ip_mreq ip;
//    struct sockaddr_in trans_addr;
//    socklen_t addr_len = sizeof(trans_addr);
//    trans_addr.sin_family = AF_INET;
//    uint64_t _SESS_ID, _FIRST_BYTE;
//    size_t _PSIZE;
//    std::atomic<bool> PLAY(false);
//    GroupSock data_multi(Type::MULTICAST);
//    while (PROGRAM_RUNNING) {
//        if (STATION_CHANGED) {
//            cout << "READ: zlapalem STATION CHANGED\n";
//            STATION_CHANGED = false;
//            fifo_mut.lock();
//            SHRD_FIFO.clean();
//            fifo_mut.unlock();
//            //data_multi.drop_member(ip);
//            //data_multi = GroupSock(Type::MULTICAST);
//            act_stat_mut.lock();
//            //cout << "READ: ---------------------- 1\n";
//            string name = SHRD_ACT_STATION;
//            act_stat_mut.unlock();
//            trans_mut.lock();
//            //cout << "READ: ---------------------- 2\n";
//            auto it = SHRD_TRANSMITTERS.find(name);
//            auto end = SHRD_TRANSMITTERS.end();
//            auto new_it = SHRD_TRANSMITTERS.begin();
//            trans_mut.unlock();
//            if (it == end) {
//                // station to be played disappeared
//                if (new_it == end) {
//                    // empty
//                    act_stat_mut.lock();
//                    SHRD_ACT_STATION = string{};
//                    act_stat_mut.unlock();
//                }
//                else {
//                    act_stat_mut.lock();
//                    SHRD_ACT_STATION = get<0>(*it);
//                    act_stat_mut.unlock();
//                }
//                continue;
//            }
//            Transmitter &tr = std::get<1>(*it);
//            //cout << "READ: ---------------------- 3\n";
//            cout << "UWAGA: dodaje membera " << tr.mcast.c_str() << tr.port
//                 << "\n";
//            //data_multi.bind(tr.mcast.c_str(), tr.port);
//            data_multi.bind("231.10.11.12", 2137);
//            ip = data_multi.add_member(tr.mcast.c_str());
//            trans_addr = GroupSock::make_addr(tr.mcast.c_str(), tr.port);
//        }
//        act_stat_mut.lock();
//        if (SHRD_ACT_STATION.empty()) {
//            act_stat_mut.unlock();
//            continue;
//        }
//        act_stat_mut.unlock();
//        ssize_t rcv_len = recvfrom(data_multi.get_sock(), buffer,
//                                   sizeof(buffer), MSG_DONTWAIT,
//                                   (sockaddr *) &trans_addr, &addr_len);
//        if (rcv_len <= 0)
//            continue;
//        // data pack received, need to fetch session id and first byte
//        cout << "READ: ---------------------- 5\n";
//        if (rcv_len < 0)
//            err("recvfrom");
//        memcpy(&_SESS_ID, buffer, SESS_ID_SIZE);
//        memcpy(&_FIRST_BYTE, buffer + SESS_ID_SIZE, BYTE_NUM_SIZE);
//
//        _PSIZE = rcv_len - INFO_LEN;
//        _SESS_ID = bswap_64(_SESS_ID);
//        _FIRST_BYTE = bswap_64(_FIRST_BYTE);
//
//        printf("DATA_READ: sess:%llu, fb:%llu, psize:%u\n", _SESS_ID,
//               _FIRST_BYTE, _PSIZE);
//        fifo_mut.lock();
//        SHRD_FIFO.insert_pack(_FIRST_BYTE, buffer + INFO_LEN, _PSIZE);
//        // TODO tutaj dane idą na wyjscie
//        if (SHRD_FIFO.playing_possible()) {
//            if (!SHRD_FIFO.complete()) {
//                // odtwarzanie od nowa
//                // unlock
//            }
//            // here playing is available
//        }
//        fifo_mut.unlock();
//    }
//}


//void dummy() {
//    char buffer[2000];
//    struct ip_mreq ip1, ip2;
//    GroupSock data_multi(Type::MULTICAST);
//    data_multi.bind("231.10.11.12", 2137);
//    struct sockaddr_in addr1 = GroupSock::make_addr("231.10.11.12", 2137);
//    //struct sockaddr_in addr2 = GroupSock::make_addr("240.10.11.12", 2140);
//    socklen_t lol = sizeof(sockaddr_in);
//    ip1 = data_multi.add_member("231.10.11.12");
//    //data_multi.drop_member()
//    //ip2 = data_multi.add_member("240.10.11.12");
//
//    while (true) {
//        ssize_t rcv_len = recvfrom(data_multi.get_sock(), buffer,
//                                   sizeof(buffer), MSG_DONTWAIT,
//                                   (sockaddr *) &addr1, &lol);
//        if (rcv_len <= 0)
//            continue;
//        cout << "WCZYTALEM Z " << "231.10.11.12 dokladnie " << rcv_len
//             << "bajtow\n";
//    }
//
//}


//
//void update_sndr_list(char *reply_msg) {
//    stringstream ss(reply_msg);
//    string bor, addr, port, name, tmp_name;
//    ss >> bor >> addr >> port;
//    ss >> name;
//    while (ss >> tmp_name) {
//        name.append(" ");
//        name.append(tmp_name);
//    }
//    //cout << "bor: " << bor << "addr:" << addr << "port: " << port << "name:"
//    //     << name << "\n";
//    trans_mut.lock();
//    auto it = SHRD_TRANSMITTERS.find(name);
//    auto end = SHRD_TRANSMITTERS.end();
//    trans_mut.unlock();
//    if (it == end) {
//        // new station
//        cout << "SNDR_UPDT: dodaję nową stację" << name << "\n";
//        Transmitter tr = Transmitter(addr, (in_port_t) stoi(port));
//        trans_mut.lock();
//        bool empty = SHRD_TRANSMITTERS.empty();
//        SHRD_TRANSMITTERS.insert(std::make_pair(name, tr));
//        trans_mut.unlock();
//        // maybe it needs to start playing? It depends on -n option
//        if (empty || NAME == name) {
//            act_stat_mut.lock();
//            SHRD_ACT_STATION = name;
//            act_stat_mut.unlock();
//            cout << "SNDR_UPDT: ZMIENIAM STACJE\n";
//            STATION_CHANGED = true;
//        }
//        refreshUI();
//        cout << "SNDR_UPDT: skonczylem dodawac\n";
//    }
//    else {
//        // second time we need actual state, need to lock once more :(
//        trans_mut.lock();
//        auto it_sec = SHRD_TRANSMITTERS.find(name);
//        auto end_sec = SHRD_TRANSMITTERS.end();
//        if (it_sec != end_sec)
//            std::get<1>(*it).last_reply_time = (uint64_t) time(NULL);
//        trans_mut.unlock();
//    }
//}

//void lookup() {
//    GroupSock discv_sock{};
//    // TODO te 2 linijki powiunny byc.... zamiast sendto
////    discv_sock.bind(DISCOVER_ADDR, CTRL_PORT);
////    write(discv_sock.get_sock(), LOOKUP, strlen(LOOKUP) + 1);
//    struct sockaddr_in my_addr;
//    socklen_t len = sizeof(my_addr);
//    if (getsockname(discv_sock.get_sock(), (struct sockaddr *) &my_addr,
//                    &len) == -1)
//        err("getsockname");
//
//
//    struct sockaddr_in BROAD;
//    socklen_t BROAD_SIZE = sizeof(BROAD);
//    memset(&BROAD, '\0', sizeof(sockaddr_in));
//    BROAD.sin_addr.s_addr = htonl(INADDR_ANY);
//    BROAD.sin_family = AF_INET;
//    BROAD.sin_port = htons(CTRL_PORT);
//
//    // TODO kurwa to ma byc discover
//
////
////    sendto(discv_sock.get_sock(), LOOKUP, strlen(LOOKUP) + 1, 0, (sockaddr*) &BROAD, BROAD_SIZE);
////
////    char huj[1111];
////    ssize_t read_len = read(discv_sock.get_sock(), huj, 10000);
////    cout << "KURWA: WCZYTALEM " << huj;
////
//
//    struct pollfd fds;
//    fds.fd = discv_sock.get_sock();
//    fds.events = POLLIN;
//    fds.revents = 0;
//    char buff[5000];
//
//    while (PROGRAM_RUNNING) {
//        fds.revents = 0;
//        int ret = poll(&fds, 1, 100); // 5 seconds wait TODO
//
//        if (ret == 0) {
//            // time down, send lookup and maybe delete old stations
//            // TODO za dlugi lookup
//            sendto(discv_sock.get_sock(), LOOKUP, strlen(LOOKUP) + 1, 0,
//                   (sockaddr *) &BROAD, BROAD_SIZE);
//            act_stat_mut.lock();
//            string act_station = SHRD_ACT_STATION;
//            act_stat_mut.unlock();
//            for (auto &sndr : SHRD_TRANSMITTERS) {
//                if (time(NULL) - std::get<1>(sndr).last_reply_time >= TIMEOUT) {
//                    // old one, need to delete it
//                    string to_del = get<0>(sndr);
//                    if (act_station == to_del) {
//                        trans_mut.lock();
//                        bool none_left = SHRD_TRANSMITTERS.empty();
//                        string new_act_station = none_left ? string()
//                                                           : string(
//                                get<0>(*SHRD_TRANSMITTERS.begin()));
//                        trans_mut.unlock();
//                        act_stat_mut.lock();
//                        SHRD_ACT_STATION = new_act_station;
//                        act_stat_mut.unlock();
//                        STATION_CHANGED = true;
//                    }
//                    trans_mut.lock();
//                    SHRD_TRANSMITTERS.erase(to_del);
//                    trans_mut.unlock();
//                    SHRD_MENU.display();
//                }
//            }
//
//        }
//        else if (ret > 0) { // POLLIN occured
//            ssize_t read_len = recvfrom(fds.fd, buff, 5000, 0,
//                                        (sockaddr *) &my_addr, &len);
//            buff[read_len] = '\0';
//            assert(fds.events & POLLIN);
//            MessageParser mp;
//            switch (mp.parse(buff)) {
//                case (Mess::LOOKUP):
//                    //cout << "CTRL: LOOKUP: " << buff << "\n";
//                    break;
//                case (Mess::REXMIT):
//                    //cout << "CTRL: REXMIT: " << buff << "\n";
//                    break;
//                case (Mess::REPLY):
//                    // only message I react to
//                    //cout << "CTRL: REPLY: " << buff << "\n";
//                    update_sndr_list(buff);
//                    break;
//                case (Mess::UNKNOWN):
//                    //cout << "CTRL: NIEZNANE: " << buff << "\n";
//                    // just skip improper message
//                    break;
//                default:
//                    //cout << "CTRL: NIEZNANE: " << buff << "68\n";
//                    break;
//            }
//        }
//    } // while
//}

//void telnetUI() {
//    char ui_buff[100];
//    struct sockaddr_in server_address;
//    int sock = socket(PF_INET, SOCK_STREAM, 0); // creating IPv4 TCP socket
//    if (sock < 0) {
//        perror("socket create error");
//        exit(1);
//    }
//
//    server_address.sin_family = AF_INET;
//    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
//    server_address.sin_port = htons(UI_PORT);
//
//    int enable = 1;
//    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
//        perror("socket option error");
//        exit(1);
//    }
//
//    if (bind(sock, (struct sockaddr *) &server_address,
//             sizeof(server_address)) < 0) {
//        perror("socket bind error");
//        exit(1);
//    }
//
//
//    std::thread ACCEPT_THREAD(accept_incoming, sock);
//
//    struct pollfd fds[MAX_TELNET_CONN];
//
//
//    //cout << "UI: udostepniam telneta\n";
//    while (PROGRAM_RUNNING) {
//        size_t act_conn = connected.load();
//        socks_mut.lock();
//        for (unsigned i = 0; i < act_conn; i++) {
//            fds[i].events = POLLIN;
//            fds[i].revents = 0;
//            fds[i].fd = SHRD_SOCKS[i];
//        }
//        socks_mut.unlock();
//        int ret = poll(fds, act_conn, 100); // 5 seconds wait TODO
//        if (ret > 0) { // POLLIN occured
//            //cout << "***************UI: szukam pollinow\n";
//            for (unsigned i = 0; i < act_conn; i++) {
//                if (fds[i].revents & POLLIN) {
//                    //cout << "----------------UI: zlapalem pollina\n";
//                    ssize_t read_len = read(fds[i].fd, ui_buff, 100);
//                    ui_buff[read_len] = '\0';
//                    menu_mut.lock();
//                    SHRD_MENU.act(ui_buff);
//                    menu_mut.unlock();
//                }
//            }
//        }
//    }
//
//    if (close(sock) < 0)
//        err("close error");
//    ACCEPT_THREAD.join();
//    for (auto i : SHRD_SOCKS)
//        if (close(i) < 0)
//            err("close error");
//}

//int main(int argc, char *argv[]) {
//    //signal(SIGINT, signalHandler);
//    cout << "UI: " << UI_PORT << "\n";
//    parse_args(argc, argv);
//
//    std::thread LOOKUP_THREAD(lookup);
//    std::thread UI_THREAD(telnetUI);
//    std::thread OUTPUT_THREAD(read_and_output);
//
//    LOOKUP_THREAD.join();
//    UI_THREAD.join();
//    OUTPUT_THREAD.join();
//
//    //dummy();
//    return 0;
//}
#endif