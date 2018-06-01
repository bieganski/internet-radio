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
#include "utils.h"
#include <thread>
#include <atomic>
#include <cassert>

#include "menu.h"
#include "telnet_consts.hpp"
#include "utils.h"
#include "recv_consts.hpp"

#include "GroupSock.h"
#include "MessageParser.h"

using namespace std;
using namespace Recv_Consts;
using namespace TelnetConstants;

std::atomic<bool> PROGRAM_RUNNING(true);

// TODO przy lookupie sprawdzic dlugosc nazwy


// TODO sprawdzac session id i inne (?) rzeczy jak sie dostanie pakiet

size_t NAME_LEN = 60;
char * NAME = nullptr;

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
                    err("Port number must be 16-bit unsigned.");
                CTRL_PORT = (uint16_t) tmp;
                break;
            case 'U': // user interface port
                tmp = get_pos_nr_or_err(optarg);
                if (!tmp || (tmp >> 16))
                    err("Port number must be 16-bit unsigned.");
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
                    err("Transmitter name too long.");
                NAME = (char *) malloc(NAME_LEN);
                NAME = strncpy(NAME, optarg, NAME_LEN);
                NAME[strlen(optarg)] = '\0'; // just to be sure
                break;
            default:
                // getopt handles wrong arguments
                exit(1);
        }
    }
}



void read_data(const char * multi_addr, in_port_t port) {
    GroupSock data_multi(Type::MULTICAST);
    struct ip_mreq ip = data_multi.add_member(multi_addr);
    data_multi.bind(INADDR_ANY, port);

    ssize_t rcv_len;
    char buffer[5000]; // TODO const

    /* czytanie tego, co odebrano */
    while (PROGRAM_RUNNING) {
        rcv_len = read(data_multi.get_sock(), buffer, sizeof(buffer));
        assert(rcv_len >= 0);
        if (rcv_len < 0)
            err("read");
        else {
            printf("read %zd bytes: \n", rcv_len);
            uint64_t l1, l2;
            memcpy(&l1, buffer, 8);
            memcpy(&l2, buffer + 8, 8);
            uint64_t ln1 = bswap_64(l1);
            uint64_t ln2 = bswap_64(l2);
            printf("ln1, ln2: %llu, %llu,\n",ln1, ln2 );
        }
    }

    data_multi.drop_member(ip);
}


void czytaj_odp(in_port_t port, int socketix) {
//    GroupSock sock{};
//    sock.bind(INADDR_ANY, port);

    ssize_t len;
    char huj[10000];
    while (PROGRAM_RUNNING) {
        len = read(socketix, huj, 10000);
        assert(len >= 0);
        cout << "dostalem rexmita: ";
        uint64_t l1, l2;
        memcpy(&l1, huj, 8);
        memcpy(&l2, huj + 8, 8);
        uint64_t ln1 = bswap_64(l1);
        uint64_t ln2 = bswap_64(l2);
        printf("ln1, ln2: %llu, %llu,\n",ln1, ln2 );
        cout << huj;
    }
}

int main (int argc, char *argv[]) {
    /* argumenty wywołania programu */
    char *multicast_dotted_address;
    in_port_t local_port;

    if (argc != 3)
        err("Usage: %s multicast_dotted_address local_port");
    multicast_dotted_address = argv[1];
    local_port = (in_port_t)atoi(argv[2]);


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
    if (getsockname(bcast_sock.get_sock(), (struct sockaddr *)&wynik, &len) == -1)
        err("getsockname");

    // ten nowy czyta odp
    cout << "czytam na porcie " << ntohs(wynik.sin_port) << "\n";
    thread lol = thread(czytaj_odp, ntohs(wynik.sin_port), bcast_sock.get_sock());

    sleep(2);
    // TODO dodatkowe \0 na koncu nie powinny byc chyba
    const char * rex = "LOUDER_PLEASE 0,512,1024,1536,5632,3584,-3,ab,2,21\n";
    //write(bcast_sock.get_sock(), rex, strlen(rex) + 1);
    sendto(bcast_sock.get_sock(), rex, strlen(rex) + 1, 0, (sockaddr*)&BROAD, BROAD_SIZE);
    const char * look = "ZERO_SEVEN_COME_IN\n";
    // write(bcast_sock.get_sock(), look, strlen(look) + 1);
    sendto(bcast_sock.get_sock(), look, strlen(look) + 1, 0, (sockaddr*)&BROAD, BROAD_SIZE);

    PROGRAM_RUNNING = false;
    //READ_THREAD.join();
    lol.join();

    return 0;
}
