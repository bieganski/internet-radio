#include <unistd.h>
#include <sstream>
#include <cassert>
#include <iostream>

#include "Menu.h"
#include "telnet_consts.hpp"

using namespace TelnetConstants;

using ActionType = std::function<bool()>;

std::string Menu::str() const {
    std::string panel = std::string(
        "------------------------------------------------------------------------\r\n");
    std::string not_marked = std::string("    ");
    std::string marked = std::string("  > ");
    std::stringstream res;
    res << panel << "SIK Radio\r\n" << panel;

    for (auto it = SHRD_TRANSMITTERS.begin(); it != SHRD_TRANSMITTERS.end(); ++it) {
        if (SHRD_ACT_STATION.empty() && it == SHRD_TRANSMITTERS.begin())
            res << marked;
        else if (SHRD_ACT_STATION == std::get<0>(*it))
            res << marked;
        else
            res << not_marked;
        res << std::get<0>(*it) << "\r\n";
    }
    res << panel;
    return res.str();
}
