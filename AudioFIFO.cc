#include <cstring>
#include <cassert>
#include <sys/types.h>

#include "AudioFIFO.h"

std::string &AudioFIFO::operator[](size_t first_byte) {
    assert(0 == first_byte % data_len);
    return std::get<1>(fifo[first_byte / data_len]);
}

void AudioFIFO::push_back(size_t first_byte, const char *data, size_t count) {
    assert(data_bytes() < fifo_size);
    if (data_bytes() + data_len > fifo_size) // need to pop first
        fifo.pop_front();
    fifo.push_back(std::make_pair(first_byte, std::string(data, count)));
    assert(data_bytes() < fifo_size);
}