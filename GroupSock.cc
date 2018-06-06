#include <unistd.h>
#include <iostream>
#include <cassert>
#include <arpa/inet.h>
#include <fcntl.h>

#include "GroupSock.h"
#include "utils.h"


GroupSock::GroupSock(Type t, int flags) : type(t) {
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        err("socket error!");
    flags = fcntl(sock, F_GETFL);
    flags |= flags;
    fcntl(sock, F_SETFL, flags);

    int optval = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *) &optval,
                   sizeof(int)) < 0)
        err("setsockopt(SO_REUSEADDR) failed");

    if (type == BROADCAST) {
        optval = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *) &optval,
                       sizeof optval) < 0)
            err("setsockopt broadcast set error!");
        // TTL set
        optval = 10;
        if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void *) &optval,
                       sizeof optval) < 0)
            err("setsockopt multicast TTL set error!");
    }
    else if (type == MULTICAST) { ; // need to join to any multicast group
    }
    else { ; // nothing here at this moment
        assert(false);
    }
}

GroupSock::~GroupSock() {
    if (close(sock) == -1) {
        err("close");
    }
}

struct sockaddr_in GroupSock::make_addr(const char *addr, in_port_t port) {
    struct sockaddr_in addr_in;
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(port);
    if (inet_aton(addr, &addr_in.sin_addr) == 0)
        err("inet_aton error!");
    return addr_in;
}


struct sockaddr_in GroupSock::make_addr(in_addr_t addr, in_port_t port) {
    struct in_addr ip_addr;
    ip_addr.s_addr = addr;
    return make_addr(inet_ntoa(ip_addr), port);
}


int GroupSock::get_sock() {
    return sock;
}


int GroupSock::connect(const char *addr, in_port_t port) {
    struct sockaddr_in addr_in = make_addr(addr, port);
    if (::connect(sock, (struct sockaddr *) &addr_in, sizeof addr_in) < 0)
        err("connect error!");
    return sock;
}


int GroupSock::connect(in_addr_t addr, in_port_t port) {
    struct sockaddr_in addr_in = make_addr(addr, port);
    if (::connect(sock, (struct sockaddr *) &addr_in, sizeof addr_in) < 0)
        err("connect error!");
    return sock;
}

int GroupSock::bind(const char *addr, in_port_t port) {
    struct sockaddr_in addr_in = make_addr(addr, port);
    if (::bind(sock, (struct sockaddr *) &addr_in, sizeof addr_in) < 0)
        err("bind error!");
    return sock;
}


int GroupSock::bind(in_addr_t addr, in_port_t port) {
    struct sockaddr_in addr_in = make_addr(addr, port);
    if (::bind(sock, (struct sockaddr *) &addr_in, sizeof addr_in) < 0)
        err("bind error!");
    return sock;
}

struct ip_mreq GroupSock::add_member(const char *multi_addr) {
    assert(type == MULTICAST);
    struct ip_mreq ip;
    ip.imr_interface.s_addr = htonl(INADDR_ANY);
    if (inet_aton(multi_addr, &ip.imr_multiaddr) == 0)
        err("inet_aton");
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   (void *) &ip, sizeof(ip_mreq)) < 0)
        err("setsockopt");
    return ip;
}

void GroupSock::drop_member(struct ip_mreq ip) {
    assert(type == MULTICAST);
    if (setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                   (void *) &ip, sizeof ip) < 0)
        std::cout << "drop not existing member\n";
}