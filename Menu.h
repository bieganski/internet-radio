#ifndef _MENU_H
#define _MENU_H

#include <vector>
#include <cstring>
#include <string>
#include <mutex>
#include <atomic>
#include <map>
#include <functional>

#include "Transmitter.h"


/**
 * Keeps each option and remembers marked row.
 */
class Menu {
private:
    std::vector<std::string> rows;
    size_t marked_row;
    std::vector<int> &socks;
    std::mutex &socks_mut;
    std::atomic<bool> &STATION_CHANGED;
    std::string &SHRD_ACT_STATION;
    std::mutex &act_stat_mut;
    std::string &NAME;
    std::map<std::string, Transmitter> &SHRD_TRANSMITTERS; // name identifies transmitter
    std::mutex &trans_mut;

    std::string str() const;

    void set_station();

public:
    Menu(std::vector<int> &socks, std::mutex &socks_mut,
         std::atomic<bool> &STATION_CHANGED, std::string &SHRD_ACT_STATION,
         std::mutex &act_stat_mut, std::string &NAME,
         std::map<std::string, Transmitter> &SHRD_TRANSMITTERS,
         std::mutex &trans_mut) :
        marked_row(0),
        socks(socks),
        socks_mut(socks_mut),
        STATION_CHANGED(STATION_CHANGED),
        SHRD_ACT_STATION(SHRD_ACT_STATION),
        act_stat_mut(act_stat_mut),
        NAME(NAME),
        SHRD_TRANSMITTERS(SHRD_TRANSMITTERS),
        trans_mut(trans_mut) {};

    void go_up();

    void go_down();

    void display();

    void add_station(std::string name);

    void rmv_station(std::string name);

    bool act(const char *action);
};

#endif //_MENU_H

