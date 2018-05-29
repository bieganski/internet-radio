#ifndef NETWORK_RADIO_MESSAGEPARSER_H
#define NETWORK_RADIO_MESSAGEPARSER_H

#include <regex>

enum Message {
    LOOKUP, REXMIT, REPLY, UNKNOWN
};


class MessageParser {
    std::vector <std::pair<Message, std::regex>> mess_vec;

public:
    MessageParser();

    Message parse(const char *str);
};


#endif //NETWORK_RADIO_MESSAGEPARSER_H
