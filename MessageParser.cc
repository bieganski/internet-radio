#include "MessageParser.h"

MessageParser::MessageParser() {
    // that regexes aren't completely proper, need to be checked more precisely
    std::string ip_reg = std::string(
        "(([0-9]{1,2}|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}"
        "([0-9]{1,2}|1[0-9]{2}|2[0-4][0-9]|25[0-5])");
    std::string port_reg = std::string("[0-9]{1,5}");
    mess_vec.emplace_back(
        std::make_pair(Mess::LOOKUP, std::regex("ZERO_SEVEN_COME_IN\\n")));
    std::stringstream reply_ss;
    reply_ss << "BOREWICZ_HERE " << ip_reg << " " << port_reg << " .{1,64}\\n";
    mess_vec.emplace_back(
        std::make_pair(Mess::REPLY, std::regex(reply_ss.str()))
    );
    mess_vec.emplace_back(
        std::make_pair(Mess::REXMIT, std::regex("LOUDER_PLEASE [0-9](,.+)*\\n")));
}

Mess::Message MessageParser::parse(const char *str) {
    std::string string = std::string(str);
    for (auto pair : mess_vec) {
        if (regex_match(string, std::get<1>(pair))) {
            return std::get<0>(pair);
        }
    }
    return Mess::UNKNOWN;
}