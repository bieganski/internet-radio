#ifndef NETWORK_RADIO_AUDIOFIFO_H
#define NETWORK_RADIO_AUDIOFIFO_H

#include <cstring>
#include <deque>
#include <sys/types.h>

class AudioFIFO {
private:
    std::deque<std::pair<uint64_t, std::string>> fifo; // pair: first byte and data
    size_t data_len; // initialised with PSIZE
    size_t fifo_size; // initialised with FSIZE

    size_t data_bytes() const { return data_len * fifo.size(); }

public:
    AudioFIFO(size_t data_len, size_t fifo_len) : data_len(data_len),
                                                  fifo_size(fifo_len) {}

    std::string &operator[](size_t first_byte); // must be 'data_len' multiple

    void push_back(size_t first_byte, const char *data, size_t count);
};

#endif //NETWORK_RADIO_AUDIOFIFO_H
