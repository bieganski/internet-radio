#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "Menu.h"
#include "err.h"
#include "telnet_consts.hpp"

#define ACTION_LEN  20
#define QUEUE_LEN    5


using namespace std;
using namespace TelnetConstants;


int main(int argc, char *argv[]) {
    int PORT_NR;
    int sock, msg_sock;
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    socklen_t client_address_len;

    if(argc != 2) {
        printf("Usage: ./server port\n");
        exit(1);
    }

    PORT_NR = atoi(argv[1]);
    if (PORT_NR <= 0 || PORT_NR > 65535) {
        perror("wrong port number");
        exit(1);
    }

    char action[ACTION_LEN];
    ssize_t len;

    sock = socket(PF_INET, SOCK_STREAM, 0); // creating IPv4 TCP socket
    if (sock < 0) {
        perror("socket create error");
        exit(1);
    }

    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
    server_address.sin_port = htons(PORT_NR); // listening on port PORT_NUM

    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("socket option error");
        exit(1);
    }

    if (bind(sock, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        perror("socket bind error");
        exit(1);
    }

    if (listen(sock, QUEUE_LEN) < 0) {
        perror("socket listen error");
        exit(1);
    }

    printf("accepting client connections on port %hu\n", ntohs(server_address.sin_port));

    for (;;) {
        client_address_len = sizeof(client_address);
        printf("waiting for client\n");
        // get client connection from the socket
        msg_sock = accept(sock, (struct sockaddr *) &client_address, &client_address_len);
        if (msg_sock < 0) {
            perror("accept error");
            exit(1);
        }

        printf("client connected\n");
        
        // configuring telnet
        if (write(msg_sock, NEGOTIATE, strlen(NEGOTIATE)) != strlen(NEGOTIATE)) {
            perror("negotiate error");
            exit(1);
        }

        MenuManager menu; // each client got his own menu manager
        
        do {
            // menu displaying
            if (!menu.act(msg_sock, action))
                break; // client exited

            // reading response 
            len = read(msg_sock, action, sizeof(action));
        } while (len > 0);

        printf("ending connection with client\n");
        if (close(msg_sock) < 0) {
            perror("close error");
            exit(1);
        }
    }

    return 0;
}