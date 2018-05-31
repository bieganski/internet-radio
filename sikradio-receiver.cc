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

#define BSIZE         1024

#include "menu.h"
#include "telnet_consts.hpp"
#include "utils.h"
#include "consts.hpp"

#include "GroupSock.h"
#include "MessageParser.h"

using namespace std;
using namespace Constants;
using namespace TelnetConstants;

std::atomic<bool> PROGRAM_RUNNING(true);



void read_data(const char * multi_addr, in_port_t port) {
    GroupSock data_multi(Type::MULTICAST);
    struct ip_mreq ip = data_multi.add_member(multi_addr);
    data_multi.bind(INADDR_ANY, port);

    ssize_t rcv_len;
    char buffer[BSIZE];

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


void czytaj_odp() {
    GroupSock sock{};
    sock.bind(INADDR_ANY, 0);

    ssize_t len;
    char huj[10000];
    while (PROGRAM_RUNNING) {
        len = read(sock.get_sock(), huj, 10000);
        assert(len >= 0);
        cout << "dostalem rexmita: ";
        uint64_t l1, l2;
        memcpy(&l1, huj, 8);
        memcpy(&l2, huj + 8, 8);
        uint64_t ln1 = bswap_64(l1);
        uint64_t ln2 = bswap_64(l2);
        printf("ln1, ln2: %llu, %llu,\n",ln1, ln2 );
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


    // ten nowy czyta odp
    thread lol = thread(czytaj_odp);

    // TEN WYSYLA TEST NA BROAD
    GroupSock bcast_sock{};
    bcast_sock.connect(INADDR_BROADCAST, CTRL_PORT);
    sleep(2);
    const char * rex = "LOUDER_PLEASE 0,512,1024,1536,5632,3584,-3,ab,2,21\n";
    write(bcast_sock.get_sock(), rex, strlen(rex) + 1);

    const char * look = "ZERO_SEVEN_COME_IN\n";
    write(bcast_sock.get_sock(), look, strlen(look) + 1);


    PROGRAM_RUNNING = false;
    //READ_THREAD.join();
    lol.join();

    return 0;
}
