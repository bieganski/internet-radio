#include <unistd.h>
#include <iostream>
#include "utils.h"


extern void err(const char *mess) {
    std::cout << mess << std::endl;
    exit(1);
}

std::string& AudioFIFO::operator[] (size_t first_byte) {
    assert(0 == first_byte % data_len);
    return std::get<1>(fifo[first_byte / data_len]);
}

void AudioFIFO::push_back(size_t first_byte, const char * data, size_t count) {
    assert(data_bytes() < fifo_size);
    if (data_bytes() + data_len > fifo_size) // need to pop first
        fifo.pop_front();
    fifo.push_back(std::make_pair(first_byte, std::string(data, count)));
    assert(data_bytes() < fifo_size);
}


//void AudioPack::to_buffer(char *buff) {
//    memcpy(buff, &session_id, sizeof(uint64_t));
//    memcpy(buff + Constants::SESS_ID_SIZE, &first_byte_num, sizeof(uint64_t));
//    memcpy(buff + Constants::INFO_LEN, audio_data, Constants::PSIZE);
//}
//
//void AudioPack::change_byte_order() { // swap between big/little endian
//    session_id = bswap_64(session_id);
//    first_byte_num = bswap_64(first_byte_num);
//}
//
//int AudioPack::send_udp(int sock, size_t nbytes) {
//    if (write(sock, audio_data, nbytes) != nbytes)
//        err("Could not write given byte amount!");
//}
//
//void AudioPack::to_str() {
//    std::cout << "jestem paczką o id " << this->session_id
//         << " i trzymam: \n" << audio_data << "KONIEC\n";
//}


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
