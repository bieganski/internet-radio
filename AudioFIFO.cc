#include <string>
#include <cassert>
#include <tuple>
#include <vector>
#include <algorithm>
#include <sys/types.h>

#include "AudioFIFO.h"

std::string &AudioFIFO::operator[](size_t first_byte) {
    assert(0 == first_byte % data_len);
    return std::get<1>(fifo[first_byte / data_len]);
}

void AudioFIFO::push_back(size_t first_byte, const char *data, size_t count) {
    std::vector<int> res;
    assert(data_bytes() < fifo_size);
    assert(0 == first_byte % data_len);

    // inserting new data packet may cause need to delete some
    // old packets or make place for expected rexmit.

    // lowest fisrt byte that may be in fifo after inserting
    // IT`S NOT DATA_LEN MULTIPLY
    uint64_t lowest_nr = std::max((size_t)0, first_byte + data_len - fifo_size);
    if (data_bytes() + data_len > fifo_size) // need to pop first
        fifo.pop_front();
    if (!fifo.empty())
        assert(this->last() + data_len == first_byte);


    fifo.emplace_back(std::make_pair(first_byte, std::string(data, count)));
    assert(data_bytes() < fifo_size);
}

ssize_t AudioFIFO::last() {
    return fifo.empty() ? -1 : std::get<0>(fifo[fifo.size() - 1]);
}

ssize_t AudioFIFO::first() {
    return fifo.empty() ? -1 : last() - data_bytes();
}

ssize_t AudioFIFO::idx(size_t first_byte) {
    if (0 != first_byte % data_len || fifo.empty())
        return -1;
    ssize_t last = this->last();
    ssize_t first = this->first();
    if (last < 0)
        return -1;
    if (first_byte > last || first_byte < first)
        return -1;
    return (first_byte - first) / data_len;
}

bool AudioFIFO::complete() {
    for (auto i : fifo)
        if (std::get<std::string>(i).empty())
            return false;
    return true;
}

void AudioFIFO::insert_dummy() {
    fifo.emplace_back(std::make_pair(this->last() + data_len, std::string()));
}