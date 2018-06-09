#ifndef TELNET_CONSTS_H
#define TELNET_CONSTS_H

namespace TelnetConstants {
    // DO LINEMODE, WILL ECHO

	static const char NEGOTIATE[] = "\377\375\042\377\373\001";

	static const char CLEAR[] = {27, 99, 0};

	static const char UP[] = {27, 91, 65, 0};

	static const char DOWN[] = {27, 91, 66, 0};

	static const char ENTER[] = {13, 0};
}

#endif // _TELNET_CONSTS_
