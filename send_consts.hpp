#ifndef NETWORK_RADIO_SEND_CONSTS_HPP
#define NETWORK_RADIO_SEND_CONSTS_HPP

namespace Send_Consts {

    const size_t SESS_ID_SIZE = 8; // in bytes, size of session id

    const size_t BYTE_NUM_SIZE = 8; // in bytes, size of first byte num

    const size_t INFO_LEN = SESS_ID_SIZE + BYTE_NUM_SIZE;

    static unsigned int MY_IDX = 385162; // mb385162

    static in_port_t DATA_PORT = (in_port_t) (20000 + (MY_IDX % 10000));

    static in_port_t CTRL_PORT = (in_port_t) (30000 + (MY_IDX % 10000));

    static unsigned PSIZE = 512;

    static unsigned FSIZE = 65536 * 2;

    static unsigned RTIME = 250;
}

#endif //NETWORK_RADIO_SEND_CONSTS_HPP
