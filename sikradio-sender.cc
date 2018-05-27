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

#include "menu.h"
#include "telnet_consts.hpp"
#include "utils.h"
#include "consts.hpp"

using namespace std;
using namespace Constants;
using namespace TelnetConstants;

#define TTL_VALUE       10
#define BUF_DEF_SIZE    2000
char buff[BUF_DEF_SIZE];

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


/**
 * Connects to given address used to send broadcast packages.
 * @return broadcast sock
 */
int set_brcst_sock(const char *addr, const unsigned short port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        err("socket error!");
    int optval = 1;
    // broadcast option
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *) &optval,
                   sizeof optval) < 0)
        err("setsockopt broadcast set error!");
    // TTL set
    optval = TTL_VALUE;
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void *) &optval,
                   sizeof optval) < 0)
        err("setsockopt multicast TTL set error!");

    struct sockaddr_in addr_in;
    memset(&addr_in, '\0', sizeof(struct sockaddr_in));
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(port);
    if (inet_aton(addr, &addr_in.sin_addr) == 0)
        err("inet_aton error!");

    if (connect(sock, (struct sockaddr *) &addr_in, sizeof addr_in) < 0)
        err("connect error!");
    std::cout << "polaczylem sie z " << addr << " na porcie " << port;
    return sock;
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
            //AudioPack new_pack = AudioPack(SESS_ID, first_byte, buff, len);
            //new_pack.to_str();
            //new_pack.send_udp(sock, len);
            send_udp(sock, first_byte);
            first_byte += len;
        }
    } while (len > 0);
}

#ifdef lol
void read_and_send(int sock) {
    char napis[] = "kurde bele co za cwele";
    size_t rozmiar = strlen(napis);
    int len = 100;
    do {
        write(sock, napis, rozmiar);
    } while (len-- > 0);
}
#endif

int main(int argc, char *argv[]) {
    SESS_ID = (uint64_t) time(NULL);
    NAME = (char *) malloc(NAME_LEN);
    parse_args(argc, argv);
    int data_sock = set_brcst_sock(MCAST_ADDR, DATA_PORT);
    switch (fork()) {
        case 0:
            exit(1);
            break;
        default:
            break;
    }
    read_and_send(data_sock);
    return 0;
}