#include "Transmitter.h"

bool Transmitter::operator<(const Transmitter &other) const {
    return name.compare(other.name) < 0;
}

Transmitter::Transmitter(std::string &&name, std::string &&mcast,
                         in_port_t port) :
        name(std::move(name)), mcast(std::move(mcast)), port(port) {
    sess_id = 0;
    byte0 = 0;
}