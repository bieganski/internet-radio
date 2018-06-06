#include <string>
#include <cassert>
#include <iostream>
#include <tuple>
#include <vector>
#include <algorithm>
#include <sys/types.h>

#include "AudioFIFO.h"

std::string &AudioFIFO::operator[](size_t first_byte) {
    assert(0 == first_byte % data_len);
    return std::get<1>(fifo[first_byte / data_len]);
}

// returns packages that needs rexmit
std::set<uint64_t> AudioFIFO::insert_pack(uint64_t first_byte,
                                          const char *data, size_t count) {
    std::set<uint64_t> res;
    assert(0 == first_byte % data_len);
    assert(count == data_len);
    std::cout << "FIFO: wrzucam fb:" << first_byte << "\n";
    if (fifo.empty()) {
        push_back(first_byte, data, count);
        return res;
    }
    while (last() != first_byte) {
        if (first_byte - last() != data_len) {
            push_back(last() + data_len, "", 0);
            res.insert(last() + data_len);
        }
        else {
            // uff, gap fulfilled, now push data from argument
            push_back(first_byte, data, count);
        }
    }

    // there may be too much elements in rexmit vector, need to check it
    for (auto it = res.begin(); it != res.end(); ++it) {
        if (*it == first()) {
            res.erase(res.begin(), it);
        }
    }
    std::cout << "FIFO: w kolejce są:\n";
    for (auto it: fifo) {
        std::cout << it.first;
    }
    return res;
}


void AudioFIFO::push_back(uint64_t first_byte, const char *data, size_t count) {
    assert(data_bytes() <= fifo_size);
    assert(count == data_len);
    assert(0 == first_byte % data_len);

    if (data_bytes() + data_len > fifo_size) // need to pop first
        fifo.pop_front();
    if (!fifo.empty())
        assert(last() + data_len == first_byte);
    fifo.emplace_back(std::make_pair(first_byte, std::string(data, count)));
    assert(data_bytes() <= fifo_size);
}

uint64_t AudioFIFO::last() const {
    assert(!fifo.empty());
    return std::get<0>(fifo[fifo.size() - 1]);
}

uint64_t AudioFIFO::first() const {
    assert(!fifo.empty());
    return last() - data_bytes();
}


ssize_t AudioFIFO::idx(uint64_t first_byte) {
    if (0 != first_byte % data_len || fifo.empty())
        return -1;
    uint64_t last = this->last();
    uint64_t first = this->first();

    if (first_byte > last || first_byte < first)
        return -1;
    return (first_byte - first) / data_len;
}

bool AudioFIFO::complete() const {
    for (auto i : fifo)
        if (std::get<std::string>(i).empty())
            return false;
    return true;
}

void AudioFIFO::clean() {
    fifo.clear();
}

void AudioFIFO::reinit(size_t data_len) {
    this->data_len = data_len;
}

bool AudioFIFO::playing_possible() const {
    if (fifo.empty())
        return false;
    return last() >= first() + (fifo_size * 3 / 4);
}