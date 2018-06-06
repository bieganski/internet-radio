#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <cstdlib>
#include <cassert>
#include <ctime>
#include <iostream>
#include <byteswap.h>
#include <fcntl.h>
#include <csignal>
#include <sys/wait.h>
#include <vector>
#include <set>
#include <sstream>
#include <thread>
#include <sys/select.h>
#include <mutex>
#include <atomic>

#include "Menu.h"
#include "telnet_consts.hpp"
#include "utils.h"
#include "send_consts.hpp"

#include "GroupSock.h"
#include "AudioFIFO.h"
#include "MessageParser.h"

using namespace std;
using namespace Send_Consts;
using namespace TelnetConstants;


// TODO ***** WAZNE       poczytac o REUSE ADDR

// TODO ***** ioctl() API sets the socket to be nonblocking.

// TODO *****

const size_t BUF_DEF_SIZE = 5000;

char data_buff[BUF_DEF_SIZE];
char help_buff[BUF_DEF_SIZE];
char ctrl_buff[BUF_DEF_SIZE];

// these not in consts file because of varying size
char *MCAST_ADDR = nullptr;
char *NAME = nullptr;
const size_t NAME_LEN = 64;
uint64_t SESS_ID; // session id

std::set<uint64_t> TO_REXMIT{};
std::set<uint64_t> TO_REXMIT_TMP{};
AudioFIFO FIFO{PSIZE, FSIZE};

std::mutex rexmit_mut;

std::atomic<bool> PROGRAM_RUNNING(true);

bool proper_ip(const char *str) {
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, str, &(sa.sin_addr));
    return result != 0;
}

void parse_args(int argc, char *argv[]) {
    int c;
    unsigned tmp;
    while ((c = getopt(argc, argv, "a:P:C:p:f:R:n:")) != -1) {
        switch (c) {
            case 'a': // broadcast adress
                if (!proper_ip(optarg))
                    err("Wrong argument.");
                MCAST_ADDR = optarg;
                break;
            case 'P': // data datagrams port
                tmp = get_pos_nr_or_err(optarg);
                if (!tmp || (tmp >> 16))
                    err("Port number must be 16-bit unsigned.");
                DATA_PORT = (uint16_t) tmp;
                break;
            case 'C': // control datagrams port
                tmp = get_pos_nr_or_err(optarg);
                if (!tmp || (tmp >> 16))
                    err("Port number must be 16-bit unsigned.");
                CTRL_PORT = (uint16_t) tmp;
                break;
            case 'p': // packet size
                PSIZE = get_pos_nr_or_err(optarg);
                break;
            case 'f': // FIFO size
                FSIZE = get_pos_nr_or_err(optarg);
                break;
            case 'R': // retransmission time
                RTIME = get_pos_nr_or_err(optarg);
                break;
            case 'n': // transmittor name
                if (strlen(optarg) >= NAME_LEN)
                    err("Transmitter name too long.");
                NAME = strncpy(NAME, optarg, NAME_LEN);
                NAME[strlen(optarg)] = '\0'; // just to be sure
                break;
            default:
                // getopt handles wrong arguments
                exit(1);
        }
    }

    if (MCAST_ADDR == nullptr)
        err("-a option is mandatory!");
}


void finish_job() {
    exit(1);
}


/*
 * Writes session id and first byte number to given buffer
 * in network order and sends it to given socket.
 */
void send_data_udp(int sock, uint64_t first_byte_num, const char *data) {
    // we use network byte order
    memcpy(help_buff + INFO_LEN, data, PSIZE);
    uint64_t sess_net = bswap_64(SESS_ID);
    uint64_t first_byte_net = bswap_64(first_byte_num);
    memcpy(help_buff, &sess_net, sizeof(uint64_t));
    memcpy(help_buff + SESS_ID_SIZE, &first_byte_net, sizeof(uint64_t));
    write(sock, help_buff, PSIZE + INFO_LEN); // UDP send
}

void read_and_send() {
    GroupSock data_sock{};
    data_sock.connect(MCAST_ADDR, DATA_PORT);

    uint64_t first_byte = 0;
    ssize_t len;
    int symulacja;
    symulacja = open("/dev/zero", O_RDONLY);
    do {
        //len = read(STDIN_FILENO, buff, PSIZE);
        len = read(symulacja, data_buff, PSIZE);
        if (len != PSIZE) {
            finish_job();
        }
        else {
            cout << "wysylam fb: " << first_byte << "\n";
            send_data_udp(data_sock.get_sock(), first_byte, data_buff);
            FIFO.push_back(first_byte, data_buff, PSIZE);
            first_byte += len;
        }
    } while (len > 0);
    close(symulacja);
}


void parse_rexmit(const char *str) {
    string mess = string(str);
    stringstream tmp(mess); // to get rid of trash content
    tmp >> mess;
    tmp >> mess;
    stringstream ss(mess);
    cout << "HUJ: " << mess << "\n";
    while (getline(ss, mess, ',')) {
        if (is_positive_number(mess.c_str())) {
            rexmit_mut.lock();
            TO_REXMIT_TMP.insert(
                (uint64_t) strtoull(mess.c_str(), nullptr, 10));
            cout << "dodalem do TMP liczbe "
                 << strtoull(mess.c_str(), nullptr, 10) << "\n";
            rexmit_mut.unlock();
        }
    }
    cout << "koniec dodawania, w TMP jest " << TO_REXMIT_TMP.size()
         << "elemetnow\n";
}

void send_rexmit(int sock) {
    rexmit_mut.lock();
    assert(TO_REXMIT.empty());
    TO_REXMIT = TO_REXMIT_TMP;
    TO_REXMIT_TMP.clear();
    rexmit_mut.unlock(); // let other thread add to tmp set
    cout << "REX: okej, szykuje sie do wysylania\n";
    for (uint64_t fb : TO_REXMIT) {
        ssize_t data_idx = FIFO.idx(fb);
        cout << "REX: wysylam IDX " << data_idx << "\n";
        if (data_idx >= 0) { // else none in queue
            cout << "REX: fb=" << fb << ", len=" << FIFO[data_idx].size()
                 << "\n";
            send_data_udp(sock, fb, FIFO[data_idx].c_str());
        }
    }
    TO_REXMIT.clear();
}

void send_lookup_response(int sock, struct sockaddr_in * addr) {
    stringstream ss;
    ss << "BOREWICZ_HERE " << MCAST_ADDR << " " << DATA_PORT <<
       " " << NAME << "\n";
    cout << "LOOK: wysylam swoje dane\n";
    sendto(sock, ss.str().c_str(), ss.str().size(), 0, (sockaddr*)addr, sizeof(*addr));
    // write(sock, ss.str().c_str(), ss.str().size());
}


void recv_ctrl_packs() {
    int ret;
    ssize_t len;
    fd_set read_set;
    struct timeval tv;
    GroupSock ctrl_sock{}; // receive TODO should be blocking?
    ctrl_sock.bind(INADDR_ANY, CTRL_PORT);
    bool to_join = false;
    thread rex;

    GroupSock bcast_sock{}; // send back

    int ctrl_sock_fd = ctrl_sock.get_sock();
    tv.tv_sec = 0;
    tv.tv_usec = RTIME * 1000; // RTIME is in miliseconds

    FD_ZERO(&read_set);

    while (PROGRAM_RUNNING) {
        FD_SET(ctrl_sock_fd, &read_set);
        ret = select(ctrl_sock_fd + 1, &read_set, nullptr, nullptr, &tv);

        if (ret == -1) {
            err("select error!");
        }
        else if (ret == 0) { // timeout
            tv.tv_usec = RTIME * 1000;
            tv.tv_sec = 0;
            // time down - need to send rexmit
            if (to_join)
                rex.join();
            cout << "koniec czasu, powinienem wyslac tyle: "
                 << TO_REXMIT_TMP.size() << "\n";
            rex = thread(send_rexmit, bcast_sock.get_sock());
            to_join = true;
        }
        else {
            struct sockaddr_in lol;
            socklen_t roz = sizeof(lol);
            len = recvfrom(ctrl_sock_fd, ctrl_buff, BUF_DEF_SIZE, 0,
                           (sockaddr *)&lol, &roz);
            //cout << "UWAGA: DOSTALEM PORT" << ntohs(lol.sin_port) << "A adres " << ntohl(lol.sin_addr.s_addr);
            ctrl_buff[len] = '\0'; // just to be sure
            MessageParser mp;
            switch (mp.parse(ctrl_buff)) {
                case (Mess::LOOKUP):
                    cout << "CTRL: LOOKUP: " << ctrl_buff << "\n";
                    // need to send back immediately
                    send_lookup_response(bcast_sock.get_sock(), &lol);
                    break;
                case (Mess::REXMIT):
                    cout << "CTRL: REXMIT: " << ctrl_buff << "\n";
                    parse_rexmit(ctrl_buff);
                    break;
                case (Mess::REPLY):
                    // do nothing, actually any transmitter sent it
                    break;
                case (Mess::UNKNOWN):
                    cout << "CTRL: NIEZNANE: " << ctrl_buff << "\n";
                    // just skip improper message
                    break;
                default:
                    break;
            }
        }
    } // while (PROGRAM_RUNNING)
}


// TODO Każdy komunikat to pojedyncza linia tekstu zakończona uniksowym znakiem
// końca linii. Poza znakiem końca linii dopuszcza się jedynie znaki o numerach
// od 32 do 127 według kodowania ASCII.

int main(int argc, char *argv[]) {
    SESS_ID = (uint64_t) time(nullptr);
    const char *DEFAULT_NAME = "Nienazwany Nadajnik";
    NAME = (char *) malloc(NAME_LEN);
    memcpy(NAME, DEFAULT_NAME, strlen(DEFAULT_NAME) + 1); // parsing may change
    assert(NAME[strlen(DEFAULT_NAME)] == '\0');
    parse_args(argc, argv);


    // TEN ODBIERA KONTROLNE PAKIETY
    std::thread CTRL_THREAD(recv_ctrl_packs);
    //CTRL_THREAD.detach();

    // TEN CZYTA Z WEJSCIA I WYSYLA DANE
    read_and_send();

    // PROGRAM_RUNNING = false;
    assert(PROGRAM_RUNNING == true);
    CTRL_THREAD.join();
    free(NAME);
    return 0;
}