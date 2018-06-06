#ifndef NETWORK_RADIO_GROUPSOCK_H
#define NETWORK_RADIO_GROUPSOCK_H

#include <sys/types.h>
#include <netinet/in.h>

enum Type {
    BROADCAST, MULTICAST
};

/**
 * Multicast or broadcast wrapper class.
 */
class GroupSock {
private:
    Type type;
    int sock;

public:
    GroupSock(Type t = BROADCAST, int flags = 0);

    ~GroupSock();

    static struct sockaddr_in make_addr(in_addr_t addr, in_port_t port);

    static struct sockaddr_in make_addr(const char *addr, in_port_t port);

    // all methods below return socket descriptor

    int get_sock();

    int connect(const char *addr, in_port_t port);

    int connect(in_addr_t addr, in_port_t port);

    int bind(const char *addr, in_port_t port);

    int bind(in_addr_t addr, in_port_t port);

    //
    // --------- multicast methods below ---------
    //

    // keep returned structure to drop member
    struct ip_mreq add_member(const char *multi_addr);

    void drop_member(struct ip_mreq ip);
};


#endif //NETWORK_RADIO_GROUPSOCK_H
