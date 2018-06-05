#ifndef _MENU_H
#define _MENU_H

#include <vector>
#include <cstring>
#include <string>
#include <mutex>
#include <atomic>
#include <functional>

/**
 * Each menu row has it's own class keeping std::function
 * object with instructions after beinig chosed.
 */
class MenuRow {
public:
    std::string content;
    std::function<bool(int)> act; // executed after being chosed
    MenuRow(const char *text, std::function<bool(int)> act) :
        content{std::string(text)}, act{std::move(act)} {};
};

/**
 * Simply keeps each option and remembers marked row.
 */
class Menu {
private:
    std::vector<MenuRow> rows;
    size_t marked_row;
    std::vector<int> &socks;
    std::mutex &socks_mut;

    std::atomic<bool> &STATION_CHANGED;
    std::string &SHRD_ACT_STATION;
    std::mutex &act_stat_mut;
    std::string &NAME;

    std::string str() const;

public:
    Menu(std::vector<int> &socks, std::mutex &socks_mut,
         std::atomic<bool> &STATION_CHANGED, std::string &SHRD_ACT_STATION,
         std::mutex &act_stat_mut, std::string &NAME) :
        socks_mut(socks_mut),
        socks(socks),
        marked_row(0),
        STATION_CHANGED(STATION_CHANGED),
        SHRD_ACT_STATION(SHRD_ACT_STATION),
        act_stat_mut(act_stat_mut), NAME(NAME) {};

    void go_up();

    void go_down();

    void display() const;

    void add_station(std::string name);

    void rmv_station(std::string name);

    bool act(const char *action);
};

#endif //_MENU_H

