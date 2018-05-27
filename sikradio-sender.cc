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
#include <signal.h>

#include "menu.h"
#include "telnet_consts.hpp"
#include "utils.h"
#include "consts.hpp"

using namespace std;
using namespace Constants;
using namespace TelnetConstants;


const size_t BUF_DEF_SIZE = 2000;

char buff[BUF_DEF_SIZE];
char ctrl_buff[BUF_DEF_SIZE];

// these not in consts file because of varying size
char *MCAST_ADDR = nullptr;
char *NAME;
const size_t NAME_LEN = 64;
uint64_t SESS_ID; // session id


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

void send_udp(int sock, uint64_t first_byte_num) {
    // we use network byte order
    uint64_t sess_net = bswap_64(SESS_ID);
    uint64_t first_byte_net = bswap_64(first_byte_num);
    memcpy(buff, &sess_net, sizeof(uint64_t));
    memcpy(buff + SESS_ID_SIZE, &first_byte_net, sizeof(uint64_t));
    write(sock, buff, PSIZE + INFO_LEN); // UDP send
}

void read_and_send(int sock) {
    uint64_t first_byte = 0;
    ssize_t len;
    int symulacja;
    symulacja = open("menu.cc", O_RDONLY);
    do {
        //len = read(STDIN_FILENO, buff, PSIZE);
        len = read(symulacja, buff + INFO_LEN, PSIZE);
        if (len != PSIZE) {
            cout << "KONIEC CZYTANIA: len jest " << len << std::endl;
            finish_job();
        }
        else {
            send_udp(sock, first_byte);
            first_byte += len;
        }
    } while (len > 0);
}


void recv_ctrl_packs() {
    int ctrl_sock = set_brcst_sock(INADDR_ANY, CTRL_PORT);
    ssize_t len;
    do {
        len = read(ctrl_sock, ctrl_buff, BUF_DEF_SIZE);
        cout << "CTRL: dostalem pakiet taki: \n" << ctrl_buff << "\n";
    } while (len > 0);
    // sleep(1);
    exit(1);
    // cout << INADDR_ANY;
}

int main(int argc, char *argv[]) {
    SESS_ID = (uint64_t) time(NULL);
    NAME = (char *) malloc(NAME_LEN);
    parse_args(argc, argv);
    int data_sock = set_brcst_sock(MCAST_ADDR, DATA_PORT);
    pid_t ctrl_ps; // process receiving control packages
    switch (ctrl_ps = fork()) {
        case 0:
            cout << "jesetm syunem";
            fflush(stdin);
            recv_ctrl_packs();
            break;
        default:
            read_and_send(data_sock);
            //kill(ctrl_ps, SIGKILL);
            break;
    }

    return 0;
}