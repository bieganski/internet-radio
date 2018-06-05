#ifndef _MENU_H
#define _MENU_H

#include <vector>
#include <cstring>
#include <string>
#include <functional>

/**
 * Each menu row has it's own class keeping std::function
 * object with instructions after beinig chosed.
 */
class MenuRow {
public:
    std::string content;
    std::function<bool(int)> act; // executed after being chosed
    MenuRow(const char *text, std::function<bool(int)> act) :
        content{std::string(text)}, act{std::move(act)} {};
};

/**
 * Simply keeps each option and remembers marked row.
 */
class Menu {
private:
    std::vector<MenuRow> rows;
    size_t marked_row;
    int sock;

    std::string str() const;

    friend class MenuManager;

public:
    Menu(int sock) : sock(sock), marked_row(0) {};

    void go_up();

    void go_down();

    void display() const;

    void add_station(std::string name, int sockdesc);

    void rmv_station(std::string name, int sockdesc);

    bool act(const char *action);
};

#endif //_MENU_H

