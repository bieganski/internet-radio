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


class Menu {
private:
    std::string &SHRD_ACT_STATION;
    std::map<std::string, Transmitter> &SHRD_TRANSMITTERS;

public:
    Menu(std::string &SHRD_ACT_STATION,
         std::map<std::string, Transmitter> &SHRD_TRANSMITTERS)
        : SHRD_ACT_STATION(SHRD_ACT_STATION),
          SHRD_TRANSMITTERS(SHRD_TRANSMITTERS) {};

    std::string str() const;
};

#endif //_MENU_H

