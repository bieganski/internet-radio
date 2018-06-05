#include <unistd.h>
#include <sstream>

#include "Menu.h"
#include "telnet_consts.hpp"

using namespace TelnetConstants;

using ActionType = std::function<bool(int)>;


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

    bool act(int sockdesc, const char *action);
};



void Menu::go_up() {
    if (marked_row > 0)
        marked_row -= 1;
    display();
}

void Menu::go_down() {
    if (marked_row < rows.size() - 1)
        marked_row += 1;
    display();
}

std::string Menu::str() const {
    std::string panel = std::string(
        "------------------------------------------------------------------------\n");
    std::string not_marked = std::string("    ");
    std::string marked = std::string("  > ");
    std::stringstream res;
    res << panel << "SIK Radio\n" << panel;
    for (size_t i = 0; i < rows.size(); i++) {
        if (i == marked_row)
            res << marked;
        else
            res << not_marked;
        res << rows[i].content.c_str() << "\r\n";
    }
    res << panel;
    return res.str();
}

void Menu::display() const {
    write(sockdesc, CLEAR, strlen(CLEAR));
    std::string new_menu = this->str();
    write(sockdesc, new_menu.c_str(), new_menu.size());
}


MenuManager::MenuManager() {
    Menu menu; // main menu
    menus.push_back(menu);
    act_menu = MAIN_MENU_IDX;
    std::vector<MenuRow> rows;


    // -------------- A-MENU CREATING --------------
    ActionType print_A = [](int sockdesc) {
        write(sockdesc, "A\r\n", 3);
        return true;
    };
    rows.emplace_back(MenuRow("Opcja A", print_A));

//    ActionType change_to_B = [this, B_IDX](int sockdesc) {
//        this->act_menu = B_IDX;
//        this->menus[B_IDX].marked_row = 0;
//        this->menus[B_IDX].display(sockdesc);
//        return true;
//    };
    rows.emplace_back(MenuRow("Opcja B", change_to_B));
    ActionType exit = [](int sockdesc) {
        return false;
    };
    rows.emplace_back(MenuRow("Koniec", exit));

    menus[MAIN_MENU_IDX].rows = std::move(rows);
}

void Menu::add_station(std::string name, int sockdesc) {
    ActionType set_station = [](int sockdesc) {
        // TODO
        write(sockdesc, "A\r\n", 3);
        return true;
    };
    if (rows.empty())
        marked_row = 0;
    rows.emplace_back(MenuRow(name.c_str(), set_station));
    display();
}

void Menu::rmv_station(std::string name, int sockdesc) {
    ActionType set_station = [](int sockdesc) {
        // TODO
        write(sockdesc, "A\r\n", 3);
        return true;
    };
    if (rows.empty())
        marked_row = 0;
    rows.emplace_back(MenuRow(name.c_str(), set_station));
    display();

    for (auto row : rows) {
        if (0 == row.content.compare(name))
            rows.erase(row);
    }
}


bool Menu::act(const char *action) {
//    if(strncmp(action, ENTER, 3) == 0) {
//        Menu& cur_menu = menus[this->act_menu];
//        bool proceed = cur_menu.rows[cur_menu.marked_row].act(sockdesc);
//        return proceed;
//    }
//
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