#include <unistd.h>
#include <iostream>
#include <regex>

#include "utils.h"
#include "consts.hpp"


using namespace Constants;
using namespace std;


extern void err(const char *mess) {
    std::cout << mess << std::endl;
    exit(1);
}

extern bool is_positive_number(const char *str) {
    std::string tmp = std::string(str);
    return tmp.find_first_not_of("0123456789") == std::string::npos;
}

/**
 * Parses given string to positive number, else exits.
 */
extern unsigned get_pos_nr_or_err(const char *str) {
    if (!is_positive_number(str))
        err("Wrong argument.");
    long res = strtol(str, nullptr, 10);
    if (res <= 0)
        err("Wrong argument.");
    return (unsigned) res;
}


