#ifndef NETWORK_RADIO_AUDIOFIFO_H
#define NETWORK_RADIO_AUDIOFIFO_H

#include <cstring>
#include <deque>
#include <set>
#include <sys/types.h>

class AudioFIFO {
private:
    std::deque<std::pair<uint64_t, std::string>> fifo; // pair: first byte and data
    size_t data_len; // initialised with PSIZE
    size_t fifo_size; // initialised with FSIZE

    size_t data_bytes() const { return data_len * fifo.size(); }

    ssize_t first() const; // first byte sent kept in queue

    ssize_t last() const; // last first byte sent

public:
    AudioFIFO(size_t data_len, size_t fifo_len) : data_len(data_len),
                                                  fifo_size(fifo_len) {}

    ssize_t idx(uint64_t first_byte);

    std::string &operator[](size_t first_byte); // must be 'data_len' multiple

    void push_back(uint64_t first_byte, const char *data, size_t count);

    std::set<uint64_t> insert_pack(uint64_t first_byte, const char *data,
                                      size_t count);

    bool complete() const; // contains consistent bytes sequence

    void clean();

    void reinit(size_t data_len); // fifo size doesn't change

    bool playing_possible() const; // at least BYTE0 + ⌊BSIZE*3/4⌋ present in queue
};

#endif //NETWORK_RADIO_AUDIOFIFO_H
