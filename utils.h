#ifndef _UTILS_H
#define _UTILS_H

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <deque>
#include <byteswap.h>
#include <cassert>
#include <netinet/in.h>

#include "consts.hpp"

enum Type {
    BROADCAST, MULTICAST
};

class AudioFIFO {
private:
    std::deque<std::pair<uint64_t, std::string>> fifo; // pair: first byte and data
    size_t data_len; // initialised with PSIZE
    size_t fifo_size; // initialised with FSIZE

    size_t data_bytes() const { return data_len * fifo.size(); }

public:
    AudioFIFO(size_t data_len, size_t fifo_len) : data_len(data_len),
                                                  fifo_size(fifo_len) {}

    std::string &operator[](size_t num);

    void push_back(size_t first_byte, const char *data, size_t count);
};

/**
 * Multicast or broadcast wrapper class.
 */
class GroupSock {
private:
    Type type;
    int sock;

    struct sockaddr_in make_addr(uint32_t addr, in_port_t port) const;

    struct sockaddr_in make_addr(const char *addr, in_port_t port) const;

public:
    GroupSock(Type t);

    GroupSock() : type(BROADCAST) {}

    ~GroupSock();

    // all methods below return socket descriptor

    int get_sock();

    int connect(const char *addr, in_port_t port);

    int connect(in_addr_t addr, in_port_t port);

    int bind(const char *addr, in_port_t port);

    int bind(in_addr_t addr, in_port_t port);


    // Multicast methods below

    // keep returned structure to drop member
    struct ip_mreq add_member(const char *multi_addr);

    void drop_member(struct ip_mreq ip);
};


extern void err(const char *mess);

extern bool is_positive_number(const char *str);

extern unsigned get_pos_nr_or_err(const char *str);

#endif // _UTILS_H