#ifndef NETWORK_RADIO_MESSAGEPARSER_H
#define NETWORK_RADIO_MESSAGEPARSER_H

#include <regex>

namespace Mess {
    enum Message {
        LOOKUP, REXMIT, REPLY, UNKNOWN
    };
}


class MessageParser {
    std::vector <std::pair<Mess::Message, std::regex>> mess_vec;

public:
    MessageParser();

    Mess::Message parse(const char *str);
};


#endif //NETWORK_RADIO_MESSAGEPARSER_H
