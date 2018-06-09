#include <unistd.h>
#include <iostream>
#include <regex>
#include <arpa/inet.h>

#include "utils.h"

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


extern bool proper_ip(const char *str) {
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, str, &(sa.sin_addr));
    return result != 0;
}