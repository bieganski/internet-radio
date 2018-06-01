#ifndef NETWORK_RADIO_TRANSMITTER_H
#define NETWORK_RADIO_TRANSMITTER_H

#include <string>
#include <netinet/in.h>

class Transmitter {
    std::string name;
    std::string mcast;
    in_port_t port;
    uint64_t sess_id;
    uint64_t byte0;
    uint64_t last_reply_time;

};

#endif //NETWORK_RADIO_TRANSMITTER_H
