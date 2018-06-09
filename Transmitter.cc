#include "Transmitter.h"

Transmitter::Transmitter(std::string mcast, in_port_t port) : mcast(mcast),
                                                              port(port) {
    last_reply_time = (uint64_t) time(nullptr);
}