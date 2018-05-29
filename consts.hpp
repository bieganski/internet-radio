#ifndef _CONSTS_H
#define _CONSTS_H

#include <netinet/in.h>

namespace Constants {

    const size_t SESS_ID_SIZE = 8; // in bytes, size of session id

    const size_t BYTE_NUM_SIZE = 8; // in bytes, size of first byte num

    const size_t INFO_LEN = SESS_ID_SIZE + BYTE_NUM_SIZE;

    const int TTL_VALUE = 10;

    static unsigned MY_IDX = 385162; // mb385162

    // TODO odbiornik
        // static char DISCOVER_ADDR[] = "255.255.255.255"; // fixed length

    static in_port_t DATA_PORT = 20000 + (MY_IDX % 10000);

    static in_port_t CTRL_PORT = 30000 + (MY_IDX % 10000);

    static in_port_t UI_PORT = 10000 + (MY_IDX % 10000);

    // static unsigned PSIZE = 512; TODO USTAWIC TO !!!!!
    static unsigned PSIZE = 64;

    // TODO odbiornik
        // static unsigned BSIZE = 65536;

    static unsigned FSIZE = 65536 * 2;

    static unsigned RTIME = 250;
}

#endif // _CONSTS_
