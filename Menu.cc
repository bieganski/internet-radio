#include <unistd.h>
#include <sstream>
#include <cassert>
#include <iostream>

#include "Menu.h"
#include "telnet_consts.hpp"

using namespace TelnetConstants;

using ActionType = std::function<bool()>;


void Menu::set_station() {
    act_stat_mut.lock();
    SHRD_ACT_STATION = rows[marked_row];
    act_stat_mut.unlock();
    STATION_CHANGED = true;
    display();
}


void Menu::go_up() {
    if (marked_row > 0) {
        marked_row -= 1;
        set_station();
    }
}


void Menu::go_down() {
    if (marked_row < rows.size() - 1) {
        marked_row += 1;
        set_station();
    }
}

std::string Menu::str() const {
    std::string panel = std::string(
        "------------------------------------------------------------------------\r\n");
    std::string not_marked = std::string("    ");
    std::string marked = std::string("  > ");
    std::stringstream res;
    res << panel << "SIK Radio\r\n" << panel;
    act_stat_mut.lock();
    std::string act_station = SHRD_ACT_STATION;
    act_stat_mut.unlock();
    for (size_t i = 0; i < rows.size(); i++) {
        if (act_station.empty() && i == 0)
            res << marked;
        else if (rows[i] == act_station)
            res << marked;
        else
            res << not_marked;
        res << rows[i].c_str() << "\r\n";
    }
    res << panel;
    return res.str();
}

void Menu::display() {
    trans_mut.lock();
    rows.clear();
    for (const auto &i : SHRD_TRANSMITTERS) {
        rows.emplace_back(std::string(std::get<0>(i)));
    }
    trans_mut.unlock();
    std::string new_menu = this->str();
    socks_mut.lock();
    for (auto i : socks) {
        std::cout << "MENU: rysuje do socketa " << i << "\n";
        write(i, CLEAR, strlen(CLEAR));
        write(i, new_menu.c_str(), new_menu.size());
    }
    socks_mut.unlock();
}

void Menu::add_station(std::string name) {
    assert(!name.empty());
    if (rows.empty())
        marked_row = 0;
    display();
}

void Menu::rmv_station(std::string name) {
    marked_row = 0;
    display();
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