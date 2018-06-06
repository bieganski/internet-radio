#include <unistd.h>
#include <sstream>
#include <cassert>

#include "Menu.h"
#include "telnet_consts.hpp"

using namespace TelnetConstants;

using ActionType = std::function<bool(int)>;

void Menu::go_up() {
    if (marked_row > 0) {
        marked_row -= 1;
        act_stat_mut.lock();
        SHRD_ACT_STATION = rows[marked_row].content;
        act_stat_mut.unlock();
        STATION_CHANGED = true;
        display();
    }
}

void Menu::go_down() {
    if (marked_row < rows.size() - 1) {
        marked_row += 1;
        act_stat_mut.lock();
        SHRD_ACT_STATION = rows[marked_row].content;
        act_stat_mut.unlock();
        STATION_CHANGED = true;
        display();
    }
}

std::string Menu::str() const {
    std::string panel = std::string(
        "------------------------------------------------------------------------\n");
    std::string not_marked = std::string("    ");
    std::string marked = std::string("  > ");
    std::stringstream res;
    res << panel << "SIK Radio\n" << panel;
    for (size_t i = 0; i < rows.size(); i++) {
        if (i == marked_row)
            res << marked;
        else
            res << not_marked;
        res << rows[i].content.c_str() << "\r\n";
    }
    res << panel;
    return res.str();
}

void Menu::display() const {
    socks_mut.lock();
    for (auto i : socks) {
        write(i, CLEAR, strlen(CLEAR));
        std::string new_menu = this->str();
        write(i, new_menu.c_str(), new_menu.size());
    }
    socks_mut.unlock();
}

void Menu::add_station(std::string name) {
    assert(!name.empty());
    ActionType set_station = [&](int sockdesc) {
        act_stat_mut.lock();
        SHRD_ACT_STATION = rows[marked_row].content;
        act_stat_mut.unlock();
        STATION_CHANGED = true;
        display();
        return true;
    };
    if (rows.empty())
        marked_row = 0;
    rows.emplace_back(MenuRow(name.c_str(), set_station));
    if (NAME == name) {
        marked_row = rows.size() - 1; // lastly added
        act_stat_mut.lock();
        SHRD_ACT_STATION = rows[marked_row].content;
        act_stat_mut.unlock();
        STATION_CHANGED = true;
    }
    else if (NAME.empty()) { // no preferences, start playing
        act_stat_mut.lock();
        SHRD_ACT_STATION = name;
        act_stat_mut.unlock();
        STATION_CHANGED = true;
    }
    display();
}

void Menu::rmv_station(std::string name) {
    MenuRow &mr = rows[marked_row];
    if (name == mr.content) {
        // deleting actually played station
        act_stat_mut.lock();
        if (rows.size() == 1)
            SHRD_ACT_STATION = std::string(); // that was last station, now none
        else
            SHRD_ACT_STATION = rows[0].content;
        act_stat_mut.unlock();
        STATION_CHANGED = true;
        display();
    }
}

bool Menu::act(const char *action) {
    if (strncmp(action, UP, 3) == 0) {
        go_up();
    }
    else if (strncmp(action, DOWN, 3) == 0) {
        go_down();
    }
    else { ; // ignore bad request
    }
    return true;
}