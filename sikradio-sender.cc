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

#include <thread>
#include <sys/select.h>

#include "menu.h"
#include "telnet_consts.hpp"
#include "utils.h"
#include "consts.hpp"

#include "GroupSock.h"
#include "AudioFIFO.h"
#include "MessageParser.h"

using namespace std;
using namespace Constants;
using namespace TelnetConstants;


// TODO ***** WAZNE       poczytac o REUSE ADDR

// TODO ***** ioctl() API sets the socket to be nonblocking.

// TODO *****

const size_t BUF_DEF_SIZE = 2000;

char buff[BUF_DEF_SIZE];
char ctrl_buff[BUF_DEF_SIZE];

// these not in consts file because of varying size
char *MCAST_ADDR = nullptr;
char *NAME;
const size_t NAME_LEN = 64;
uint64_t SESS_ID; // session id

volatile bool PROGRAM_RUNNING = true;

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
                CTRL_PORT = get_pos_nr_or_err(optarg);
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

void send_data_udp(int sock, uint64_t first_byte_num) {
    // we use network byte order
    uint64_t sess_net = bswap_64(SESS_ID);
    uint64_t first_byte_net = bswap_64(first_byte_num);
    memcpy(buff, &sess_net, sizeof(uint64_t));
    memcpy(buff + SESS_ID_SIZE, &first_byte_net, sizeof(uint64_t));
    write(sock, buff, PSIZE + INFO_LEN); // UDP send
}

void read_and_send() {
    GroupSock data_sock{};
    data_sock.connect(MCAST_ADDR, DATA_PORT);

    uint64_t first_byte = 0;
    ssize_t len;
    int symulacja;
    symulacja = open("menu.cc", O_RDONLY);
    do {
        //len = read(STDIN_FILENO, buff, PSIZE);
        len = read(symulacja, buff + INFO_LEN, PSIZE);
        if (len != PSIZE) {
            finish_job();
        }
        else {
            send_data_udp(data_sock.get_sock(), first_byte);
            first_byte += len;
        }
    } while (len > 0);
}



// TODO to jest blokujące

void recv_ctrl_packs() {
    int ret;
    ssize_t len;
    fd_set read_set;
    struct timeval tv;

    GroupSock ctrl_sock{};
    ctrl_sock.bind(INADDR_ANY, CTRL_PORT);
    // TODO chyba powinien być blokujący
    int sock_fd = ctrl_sock.get_sock();
    tv.tv_sec = 0;
    tv.tv_usec = RTIME * 1000; // RTIME is in miliseconds

    FD_ZERO(&read_set);

    while (PROGRAM_RUNNING) {
        FD_SET(sock_fd, &read_set);
        ret = select(sock_fd + 1, &read_set, NULL, NULL, &tv);
        if (tv.tv_usec == 0) {
            tv.tv_usec = RTIME * 1000;
            tv.tv_sec = 0;
        }

        if (ret == -1) {
            err("select error!");
        }
        else if (ret == 0) { // timeout
            ; // nothing to do
        }
        else {
            len = read(sock_fd, ctrl_buff, BUF_DEF_SIZE);
            ctrl_buff[len] = '\0'; // just to be sure
            MessageParser mp;
            switch (mp.parse(ctrl_buff)) {
                case(LOOKUP):
                    cout << "CTRL: LOOKUP: " << ctrl_buff << "\n";
                    break;
                case(REXMIT):
                    cout << "CTRL: REXMIT: " << ctrl_buff << "\n";
                    break;
                case(UNKNOWN):
                    cout << "CTRL: NIEZNANE: " << ctrl_buff << "\n";
                    break;
                default:
                    assert(false); // must be UNKNOWN
                    break;
            }
        }
    } // while (PROGRAM_RUNNING)
}

int main(int argc, char *argv[]) {
    SESS_ID = (uint64_t) time(NULL);
    NAME = (char *) malloc(NAME_LEN);
    parse_args(argc, argv);


    // TEN ODBIERA KONTROLNE PAKIETY
    std::thread CTRL_THREAD(recv_ctrl_packs);


    // TEN CZYTA Z WEJSCIA I WYSYLA DANE
    //read_and_send();


    free(NAME);
    PROGRAM_RUNNING = false;
    CTRL_THREAD.join();

    return 0;
}