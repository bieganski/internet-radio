#include <unistd.h>
#include <sstream>

#include "menu.h"
#include "telnet_consts.hpp"

using namespace TelnetConstants;

using ActionType = std::function<bool(int)>;

void Menu::go_up() {
    if (marked_row > 0)
        marked_row -= 1;
}

void Menu::go_down() {
    if (marked_row < rows.size() - 1)
        marked_row += 1;
}

std::string Menu::str() const {
    std::stringstream res;
    for (size_t i = 0; i < rows.size(); i++) {
        if (i == marked_row)
            res << marking;
        res << rows[i].content.c_str() << "\r\n";
    }
    return res.str();
}

void Menu::display(int sockdesc) const {
    write(sockdesc, CLEAR, strlen(CLEAR));
    std::string new_menu = this->str();
    write(sockdesc, new_menu.c_str(), new_menu.size());
}

MenuManager::MenuManager() {
    Menu menuA, menuB;
    menus.push_back(menuA);
    menus.push_back(menuB);
    size_t A_IDX = 0, B_IDX = 1;
    act_menu = A_IDX;
    std::vector<MenuRow> rowsA, rowsB;


    // -------------- A-MENU CREATING --------------
    ActionType print_A = [](int sockdesc) {
        write(sockdesc, "A\r\n", 3);
        return true;
    };
    rowsA.push_back(MenuRow("Opcja A", print_A));

    ActionType change_to_B = [this, B_IDX](int sockdesc) {
        this->act_menu = B_IDX;
        this->menus[B_IDX].marked_row = 0;
        this->menus[B_IDX].display(sockdesc);
        return true;
    };
    rowsA.push_back(MenuRow("Opcja B", change_to_B));
    ActionType exit = [](int sockdesc) {
        return false;
    };
    rowsA.push_back(MenuRow("Koniec", exit));

    menus[A_IDX].rows = std::move(rowsA);


    // -------------- B-MENU CREATING --------------
    ActionType print_B1 = [](int sockdesc) {
        write(sockdesc, "B1\r\n", 4);
        return true;
    };
    rowsB.push_back(MenuRow("Opcja B1", print_B1));

    ActionType print_B2 = [](int sockdesc) {
        write(sockdesc, "B2\r\n", 4);
        return true;
    };
    rowsB.push_back(MenuRow("Opcja B2", print_B2));

    ActionType get_back = [this, A_IDX](int sockdesc) {
        this->act_menu = A_IDX;
        this->menus[A_IDX].marked_row = 0;
        this->menus[A_IDX].display(sockdesc);
        return true;
    };
    rowsB.push_back(MenuRow("Wstecz", get_back));

    menus[B_IDX].rows = std::move(rowsB);
}


bool MenuManager::act(int sockdesc, char *action) {
    if (strncmp(action, ENTER, 3) == 0) {
        Menu &cur_menu = menus[this->act_menu];
        bool proceed = cur_menu.rows[cur_menu.marked_row].act(sockdesc);
        return proceed;
    }
    else if (strncmp(action, UP, 3) == 0) {
        menus[act_menu].go_up();
    }
    else if (strncmp(action, DOWN, 3) == 0) {
        menus[act_menu].go_down();
    }
    else { ; // ignore bad request
    }
    menus[act_menu].display(sockdesc);
    return true;
}