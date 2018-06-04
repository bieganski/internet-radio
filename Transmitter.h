#ifndef NETWORK_RADIO_TRANSMITTER_H
#define NETWORK_RADIO_TRANSMITTER_H

#include <cstring>
#include <string>
#include <netinet/in.h>
#include "recv_consts.hpp"

class Transmitter {
public:
    std::string name;
    std::string mcast;
    in_port_t port;
    uint64_t sess_id;
    uint64_t byte0;
    uint64_t last_reply_time;

    Transmitter(std::string name, std::string mcast, in_port_t port) = delete;

    Transmitter(std::string &&name, std::string &&mcast, in_port_t port);

    bool operator<(const Transmitter &other) const;
};

#endif //NETWORK_RADIO_TRANSMITTER_H
