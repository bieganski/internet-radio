#include <unistd.h>
#include <iostream>
#include "utils.h"


using namespace Constants;

/*
 *  ------- AUDIOFIFO IMPLEMENTATION --------
 */

std::string &AudioFIFO::operator[](size_t first_byte) {
    assert(0 == first_byte % data_len);
    return std::get<1>(fifo[first_byte / data_len]);
}

void AudioFIFO::push_back(size_t first_byte, const char *data, size_t count) {
    assert(data_bytes() < fifo_size);
    if (data_bytes() + data_len > fifo_size) // need to pop first
        fifo.pop_front();
    fifo.push_back(std::make_pair(first_byte, std::string(data, count)));
    assert(data_bytes() < fifo_size);
}


/*
 *  ------- GROUPSOCK IMPLEMENTATION --------
 */

GroupSock::GroupSock(Type t) : type(t) {
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        err("socket error!");

    if (type == BROADCAST) {
        int optval = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *) &optval,
                       sizeof optval) < 0)
            err("setsockopt broadcast set error!");
        // TTL set
        optval = Constants::TTL_VALUE;
        if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void *) &optval,
                       sizeof optval) < 0)
            err("setsockopt multicast TTL set error!");
    }
    else if (type == MULTICAST) {
        ; // no setsockopt but need to join to any group
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

struct sockaddr_in
GroupSock::make_addr(const char *addr, in_port_t port) const {
    struct sockaddr_in addr_in;
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(port);
    if (inet_aton(addr, &addr_in.sin_addr) == 0)
        err("inet_aton error!");
    return addr_in;
}


struct sockaddr_in GroupSock::make_addr(in_addr_t addr, in_port_t port) const {
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
    return ip; // TODO pointer potrzebny?
}

void GroupSock::drop_member(struct ip_mreq ip) {
    assert(type == MULTICAST);
    if (setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                   (void *) &ip, sizeof ip) < 0)
        err("setsockopt");
}


/**
 *  ------- HELP FUNCTIONS IMPLEMENTATION --------
 */


extern void err(const char *mess) {
    std::cout << mess << std::endl;
    exit(1);
}

extern bool is_positive_number(const char *str) {
    std::string tmp = std::string(str);
    return tmp.find_first_not_of("0123456789") == std::string::npos;
}

/**
 * Parses given string to positive number, else exits.
 */
extern unsigned get_pos_nr_or_err(const char *str) {
    if (!is_positive_number(str))
        err("Wrong argument.");
    long res = strtol(str, nullptr, 10);
    if (res <= 0)
        err("Wrong argument.");
    return (unsigned) res;
}


