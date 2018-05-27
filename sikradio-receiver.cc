#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <byteswap.h>
#include "utils.h"

#define BSIZE         1024
#define REPEAT_COUNT  30

int main (int argc, char *argv[]) {
    /* argumenty wywołania programu */
    char *multicast_dotted_address;
    in_port_t local_port;

    /* zmienne i struktury opisujące gniazda */
    int sock;
    struct sockaddr_in local_address;
    struct ip_mreq ip_mreq;

    /* zmienne obsługujące komunikację */
    char buffer[BSIZE];
    ssize_t rcv_len;
    int i;

    /* parsowanie argumentów programu */
    if (argc != 3)
        err("Usage: %s multicast_dotted_address local_port");
    multicast_dotted_address = argv[1];
    local_port = (in_port_t)atoi(argv[2]);

    /* otworzenie gniazda */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        err("socket");

    /* podpięcie się do grupy rozsyłania (ang. multicast) */
    ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (inet_aton(multicast_dotted_address, &ip_mreq.imr_multiaddr) == 0)
        err("inet_aton");
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&ip_mreq, sizeof ip_mreq) < 0)
        err("setsockopt");

    /* podpięcie się pod lokalny adres i port */
    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    local_address.sin_port = htons(local_port);
    if (bind(sock, (struct sockaddr *)&local_address, sizeof local_address) < 0)
        err("bind");

    /* czytanie tego, co odebrano */
    for (i = 0; i < REPEAT_COUNT; ++i) {
        rcv_len = read(sock, buffer, sizeof buffer);
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
            for(int i =16; i < rcv_len -16; i++)
                putchar(buffer[i]);
        }
    }
    /* w taki sposób można odpiąć się od grupy rozsyłania */
    if (setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void*)&ip_mreq, sizeof ip_mreq) < 0)
        err("setsockopt");

    /* koniec */
    close(sock);
    exit(EXIT_SUCCESS);
}
