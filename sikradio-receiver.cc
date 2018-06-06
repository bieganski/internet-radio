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

#include "telnet_consts.hpp"
#include "utils.h"
#include "recv_consts.hpp"

#include "GroupSock.h"
#include "MessageParser.h"
#include "AudioFIFO.h"
#include "Menu.h"
#include "Transmitter.h"


using namespace std;
using namespace Recv_Consts;
using namespace TelnetConstants;

std::atomic<bool> PROGRAM_RUNNING(true);

// std::atomic<bool> REFRESH(true);


std::atomic<bool> STATION_CHANGED(false);

std::string SHRD_ACT_STATION;

std::mutex act_stat_mut;


std::mutex ref_mut;

std::condition_variable ref_cv;

std::atomic<uint64_t> ACT_SESS(0); // actual session id

AudioFIFO SHRD_FIFO{0, BSIZE}; // buffer fifo, it needs reinit
// (varing packet length)

std::mutex fifo_mut;

std::map<std::string, Transmitter> SHRD_TRANSMITTERS{}; // name identifies transmitter

std::mutex trans_mut;

std::vector<int> SHRD_SOCKS; // connected to UI sockets

std::mutex socks_mut;

std::string NAME; // name of station to be played

Menu SHRD_MENU = Menu(SHRD_SOCKS, socks_mut, STATION_CHANGED, SHRD_ACT_STATION,
                      act_stat_mut, NAME);

std::mutex menu_mut;

const char *LOOKUP = "ZERO_SEVEN_COME_IN\n";

const size_t QUEUE_LEN = 10;

const size_t BUFF_LEN = 5000;

const size_t MAX_TELNET_CONN = 10;

std::atomic<size_t> connected(0);

// TODO !!!!!!!!!!!!! NONBLOCKING O_NONBLOCK


// TODO przy lookupie sprawdzic dlugosc nazwy

// TODO sprawdzac session id i inne (?) rzeczy jak sie dostanie pakiet


// TODO NAZWA - ZEBY BYLO WIECEJ JAK JEDNO SLOWO


// TODO TODO TODO TODO UZYJ DISCOVER PORT


void signalHandler(int signum) {
    cout << "\nI received signal (" << signum << "). Close all connections.\n";
    PROGRAM_RUNNING = false;
    exit(signum);
}

void parse_args(int argc, char *argv[]) {
    int c;
    unsigned tmp;
    while ((c = getopt(argc, argv, "d:C:U:b:R:n:")) != -1) {
        switch (c) {
            case 'd': // discover address
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
                if (strlen(optarg) >= NAME_LEN)
                    err("-n parameter too long!");
                NAME = string(optarg);
                break;
            default:
                // getopt handles wrong arguments
                exit(1);
        }
    }
}


void refreshUI() {
    menu_mut.lock();
    SHRD_MENU.display();
    menu_mut.unlock();
}


// TODO reusse addr w wielu miejscach

// TODO usuwanie odlaczonych klientow z vectora

void accept_incoming(int sock) {
    struct sockaddr_in cli_addr;
    socklen_t addr_len;
    if (listen(sock, QUEUE_LEN) < 0) {
        perror("socket listen error");
        exit(1);
    }
    while (PROGRAM_RUNNING) {
        printf("waiting for client\n");
        int msg_sock = accept(sock, (struct sockaddr *) &cli_addr, &addr_len);
        if (msg_sock < 0) {
            perror("accept error");
            exit(1);
        }

        ++connected;
        if (connected == MAX_TELNET_CONN)
            return;

        int optval = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *) &optval,
                       sizeof(int)) < 0)
            err("setsockopt(SO_REUSEADDR) failed");
        // configuring telnet
        if (write(msg_sock, NEGOTIATE, strlen(NEGOTIATE)) !=
            strlen(NEGOTIATE)) {
            err("negotiate error");
        }
        socks_mut.lock();
        cout << "ACC: wrzucam " << msg_sock << "do listy podlaczonych\n";
        SHRD_SOCKS.emplace_back(msg_sock);
        socks_mut.unlock();
        printf("client connected\n");
        refreshUI();
    }
}

void telnetUI() {
    char ui_buff[100];
    struct sockaddr_in server_address;
    int sock = socket(PF_INET, SOCK_STREAM, 0); // creating IPv4 TCP socket
    if (sock < 0) {
        perror("socket create error");
        exit(1);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(UI_PORT);

    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("socket option error");
        exit(1);
    }

    if (bind(sock, (struct sockaddr *) &server_address,
             sizeof(server_address)) < 0) {
        perror("socket bind error");
        exit(1);
    }


    std::thread ACCEPT_THREAD(accept_incoming, sock);

    struct pollfd fds[MAX_TELNET_CONN];


    cout << "UI: udostepniam telneta\n";
    while (PROGRAM_RUNNING) {
        size_t act_conn = connected.load();
        for (unsigned i = 0; i < act_conn; i++) {
            fds[i].revents = 0;
        }
        int ret = poll(fds, act_conn, 100); // 5 seconds wait TODO
        if (ret > 0) { // POLLIN occured
            cout << "***************UI: szukam pollinow\n";
            for (unsigned i = 0; i < act_conn; i++) {
                if (fds[i].revents & POLLIN) {
                    cout << "----------------UI: zlapalem pollina\n";
                    ssize_t read_len = read(fds[i].fd, ui_buff, 100);
                    ui_buff[read_len] = '\0';
                    menu_mut.lock();
                    SHRD_MENU.act(ui_buff);
                    menu_mut.unlock();
                }
            }
        }
    }

    if (close(sock) < 0) {
        err("close error");
    }
    ACCEPT_THREAD.join();
    close(sock);
}

void read_and_output() {
    char buffer[BUFF_LEN];
    struct ip_mreq ip;
    struct sockaddr_in trans_addr;
    socklen_t addr_len = sizeof(trans_addr);
    trans_addr.sin_family = AF_INET;
    uint64_t _SESS_ID, _FIRST_BYTE;
    size_t _PSIZE;

    GroupSock data_multi(Type::MULTICAST);
    while (PROGRAM_RUNNING) {
        if (STATION_CHANGED) {
            STATION_CHANGED = false;
            fifo_mut.lock();
            SHRD_FIFO.clean();
            fifo_mut.unlock();
            data_multi.drop_member(ip);
            act_stat_mut.lock();
            string name = SHRD_ACT_STATION;
            act_stat_mut.unlock();
            trans_mut.lock();
            auto it = SHRD_TRANSMITTERS.find(name);
            assert(it != SHRD_TRANSMITTERS.end());
            Transmitter &tr = std::get<1>(*it);
            trans_mut.unlock();
            ip = data_multi.add_member(tr.mcast.c_str());
            trans_addr = GroupSock::make_addr(tr.mcast.c_str(), tr.port);
        }
        ssize_t rcv_len = recvfrom(data_multi.get_sock(), buffer,
                                   sizeof(buffer), 0,
                                   (sockaddr *) &trans_addr, &addr_len);
        // data pack received, need to fetch session id and first byte
        if (rcv_len < 0)
            err("recvfrom");
        memcpy(&_SESS_ID, buffer, SESS_ID_SIZE);
        memcpy(&_FIRST_BYTE, buffer + SESS_ID_SIZE, BYTE_NUM_SIZE);

        _PSIZE = rcv_len - INFO_LEN;
        _SESS_ID = bswap_64(_SESS_ID);
        _FIRST_BYTE = bswap_64(_FIRST_BYTE);

        printf("DATA_READ: sess:%llu, fb:%llu, psize:%u\n", _SESS_ID,
               _FIRST_BYTE, _PSIZE);
        fifo_mut.lock();
        SHRD_FIFO.insert_pack(_FIRST_BYTE, buffer + INFO_LEN, _PSIZE);
        fifo_mut.unlock();
    }
}


void update_sndr_list(char *reply_msg) {
    stringstream ss(reply_msg);
    string bor, name, addr, port;

    ss >> bor >> addr >> port;
    while(ss >> name);
    cout << "bor: " << bor << "addr:" << addr << "port: " << port << "name:" << name << "\n";
    trans_mut.lock();
    auto it = SHRD_TRANSMITTERS.find(name);
    auto end = SHRD_TRANSMITTERS.end();
    trans_mut.unlock();
    if (it == end) {
        // new station
        Transmitter tr = Transmitter(addr, (in_port_t) stoi(port));
        trans_mut.lock();
        SHRD_TRANSMITTERS.insert(std::make_pair(name, tr));
        trans_mut.unlock();
        menu_mut.lock();
        SHRD_MENU.add_station(name);
        menu_mut.unlock();
        //refreshUI();
    }
    else {
        std::get<1>(*it).last_reply_time = (uint64_t) time(NULL);
    }
}


void lookup() {
    GroupSock discv_sock{};
    // TODO te 2 linijki powiunny byc.... zamiast sendto
//    discv_sock.bind(DISCOVER_ADDR, CTRL_PORT);
//    write(discv_sock.get_sock(), LOOKUP, strlen(LOOKUP) + 1);
    struct sockaddr_in my_addr;
    socklen_t len = sizeof(my_addr);
    if (getsockname(discv_sock.get_sock(), (struct sockaddr *) &my_addr,
                    &len) ==
        -1)
        err("getsockname");


    struct sockaddr_in BROAD;
    socklen_t BROAD_SIZE = sizeof(BROAD);
    memset(&BROAD, '\0', sizeof(sockaddr_in));
    BROAD.sin_addr.s_addr = htonl(INADDR_ANY);
    BROAD.sin_family = AF_INET;
    BROAD.sin_port = htons(CTRL_PORT);

    // TODO kurwa to ma byc discover

//
//    sendto(discv_sock.get_sock(), LOOKUP, strlen(LOOKUP) + 1, 0, (sockaddr*) &BROAD, BROAD_SIZE);
//
//    char huj[1111];
//    ssize_t read_len = read(discv_sock.get_sock(), huj, 10000);
//    cout << "KURWA: WCZYTALEM " << huj;
//

    bool changed_sth;
    struct pollfd fds;
    fds.fd = discv_sock.get_sock();
    fds.events = POLLIN;
    fds.revents = 0;
    char buff[5000];

    while (PROGRAM_RUNNING) {
        changed_sth = false;
        fds.revents = 0;
        int ret = poll(&fds, 1, 100); // 5 seconds wait TODO

        if (ret == 0) {
            // time down, send lookup and maybe delete old stations
            // TODO za dlugi lookup
            sendto(discv_sock.get_sock(), LOOKUP, strlen(LOOKUP) + 1, 0,
                   (sockaddr *) &BROAD, BROAD_SIZE);
            trans_mut.lock();
            for (auto &sndr : SHRD_TRANSMITTERS) {
                if (time(NULL) - std::get<1>(sndr).last_reply_time >= 20) {
                    changed_sth = true;
                    // TODO tu moze sie blokowac
                    SHRD_TRANSMITTERS.erase(std::get<0>(sndr));
                    menu_mut.lock();
                    SHRD_MENU.rmv_station(std::get<0>(sndr));
                    menu_mut.unlock();
                }
            }
            trans_mut.unlock();
        }
        else if (ret > 0) { // POLLIN occured
            ssize_t read_len = recvfrom(fds.fd, buff, 5000, 0,
                                        (sockaddr *) &my_addr, &len);
            buff[read_len] = '\0';
            assert(fds.events & POLLIN);
            MessageParser mp;
            switch (mp.parse(buff)) {
                case (Mess::LOOKUP):
                    cout << "CTRL: LOOKUP: " << buff << "\n";
                    break;
                case (Mess::REXMIT):
                    cout << "CTRL: REXMIT: " << buff << "\n";
                    break;
                case (Mess::REPLY):
                    // only message I react to
                    update_sndr_list(buff);
                    break;
                case (Mess::UNKNOWN):
                    cout << "CTRL: NIEZNANE: " << buff << "\n";
                    // just skip improper message
                    break;
                default:
                    cout << "CTRL: NIEZNANE: " << buff << "68\n";
                    break;
            }
        }
        if (changed_sth) {
            // TODO
        }
    } // while
}

#ifdef lol
void lookup_play() {
    GroupSock discv_sock{};
    discv_sock.bind(DISCOVER_ADDR, CTRL_PORT);
    write(discv_sock.get_sock(), LOOKUP, strlen(LOOKUP) + 1);
    char huj[1111];
    ssize_t len = read(discv_sock.get_sock(), huj, 10000);
    cout << "KURWA: WCZYTALEM " << huj;
}
#endif

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

int main(int argc, char *argv[]) {
    //signal(SIGINT, signalHandler);
    cout << "UI: " << UI_PORT << "\n";
    parse_args(argc, argv);

    std::thread LOOKUP_THREAD(lookup);
    std::thread UI_THREAD(telnetUI);
    std::thread OUTPUT_THREAD(read_and_output);

    LOOKUP_THREAD.join();
    UI_THREAD.join();
    OUTPUT_THREAD.join();

    return 0;
}