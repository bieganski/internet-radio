#ifndef _MENU_H
#define _MENU_H

#include <vector>
#include <string.h>
#include <string>
#include <functional>

/**
 * Each menu row has it's own class keeping std::function
 * object with instructions after beinig chosed.
 */
class MenuRow {
public:
	std::string content;
	std::function<bool (int)> act; // executed after being chosed
	MenuRow(const char *text, std::function<bool (int)> act) : 
		content{std::string(text)}, act{std::move(act)} {};
};

/**
 * Simply keeps each option and remembers marked row.
 */
class Menu {
private:
	std::string marking;
	std::vector<MenuRow> rows;
	size_t marked_row;
	std::string str() const;

	friend class MenuManager;
public:
	Menu() : marking{std::string("* ")}, marked_row{0} {};
	void go_up();
	void go_down();
	void display(int sockdesc) const;
};

/*
 * Processes users' requests, maintains menus.
 */  
class MenuManager {
private:
	std::vector<Menu> menus;
	size_t act_menu; // index in 'menus' vector
public:
	MenuManager();
	// returns false if client disconnected
	bool act(int sockdesc, char *action);
};

#endif //_MENU_H

