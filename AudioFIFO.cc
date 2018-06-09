#include <string>
#include <cassert>
#include <iostream>
#include <tuple>
#include <vector>
#include <sstream>
#include <algorithm>
#include <sys/types.h>

#include "AudioFIFO.h"

std::string &AudioFIFO::operator[](size_t idx) {
    assert(!fifo.empty() && fifo.size() > idx);
    return std::get<1>(fifo[idx]);
}

// returns packages that needs rexmit
std::set<uint64_t> AudioFIFO::insert_pack(uint64_t first_byte,
                                          const char *data, size_t count) {
    std::set<uint64_t> res;
    assert(0 == first_byte % data_len);
    assert(count == data_len);
    //std::cout << "FIFO: wrzucam fb:" << first_byte << "\n";
    if (fifo.empty()) {
        push_back(first_byte, data, count);
        return res;
    }

    if (idx(first_byte) >= 0) {
        // exists in queue, probably rexmit being pushed
        std::get<1>(fifo[idx(first_byte)]) = std::string(data, count);
        return res;
    }

    while (last() != first_byte) {
        if (first_byte - last() != data_len) { // there is a gap
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

    return res;
}


void AudioFIFO::push_back(uint64_t first_byte, const char *data, size_t count) {
    assert(data_bytes() <= fifo_size);
    if (count != data_len) {
        std::cout << "count: " << count << ", data: " << data_len << "\n";
        assert(false);
    }
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

bool AudioFIFO::empty() {
    return fifo.empty();
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
        if (std::get<1>(i).empty())
            return false;
    return true;
}

void AudioFIFO::clear() {
    fifo.clear();
}

void AudioFIFO::reinit(size_t data_len) {
    assert(fifo.empty());
    this->data_len = data_len;
}

bool AudioFIFO::playing_possible() const {
    if (fifo.empty())
        return false;
    return last() >= first() + (fifo_size * 3 / 4);
}

std::pair<uint64_t, std::string> AudioFIFO::str() {
    assert(this->complete());
    uint64_t first_byte = std::get<0>(fifo[0]);
    return str(first_byte);
}

std::pair<uint64_t, std::string> AudioFIFO::str(uint64_t first_byte) {
    assert(this->complete());
    std::stringstream ss;
    ssize_t fb_idx = this->idx(first_byte);
    if (fb_idx < 0) {
        return std::make_pair((uint64_t) 0, std::string{}); // empty string -end
    }
    for (size_t i = (size_t) fb_idx; i < fifo.size(); i++)
        ss << std::get<1>(fifo[i]);
    return std::make_pair(std::get<0>(fifo[fifo.size() - 1]), ss.str());
}