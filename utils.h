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

#include "consts.hpp"

//class AudioPack {
//private:
//    uint64_t session_id;
//    uint64_t first_byte_num;
//    char *audio_data;
//public:
//    AudioPack(uint64_t id, uint64_t fb, const char * data, ssize_t len) : session_id(id),
//                                                       first_byte_num(fb) {
//        strncpy(audio_data, data, len);
//    }
//    void to_buffer(char *buff);
//
//    void change_byte_order();
//
//    int send_udp(int sock, size_t nbytes);
//
//    void to_str();
//};

class AudioFIFO {
private:
    std::deque<std::pair<uint64_t, std::string>> fifo; // pair: first byte and data
    size_t data_len; // initialised with PSIZE
    size_t fifo_size; // initialised with FSIZE

    size_t data_bytes() const { return data_len * fifo.size(); }

public:
    AudioFIFO(size_t data_len, size_t fifo_len) : data_len(data_len),
                                                  fifo_size(fifo_len) {}

    std::string& operator[] (size_t num);

    void push_back(size_t first_byte, const char * data, size_t count);
};


extern void err(const char *mess);

extern bool is_positive_number(const char *str);

extern unsigned get_pos_nr_or_err(const char *str);

extern int set_brcst_sock(const char *addr, const unsigned short port);


#endif // _UTILS_H