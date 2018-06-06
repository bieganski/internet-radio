#ifndef NETWORK_RADIO_RECV_CONSTS_HPP
#define NETWORK_RADIO_RECV_CONSTS_HPP

#include <netinet/in.h>

namespace Recv_Consts {

    const size_t SESS_ID_SIZE = 8; // in bytes, size of session id

    const size_t BYTE_NUM_SIZE = 8; // in bytes, size of first byte num

    const size_t INFO_LEN = SESS_ID_SIZE + BYTE_NUM_SIZE;

    static unsigned MY_IDX = 385162; // mb385162

    static char DISCOVER_ADDR[] = "255.255.255.255"; // fixed length

    static in_port_t CTRL_PORT = (in_port_t) (30000 + (MY_IDX % 10000));

    static in_port_t UI_PORT = (in_port_t) (10000 + (MY_IDX % 10000));

    static unsigned BSIZE = 65536;

    static unsigned RTIME = 250;

    const size_t NAME_LEN = 64;
}

#endif //NETWORK_RADIO_RECV_CONSTS_HPP
