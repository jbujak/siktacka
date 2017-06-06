#ifndef GUI_CLIENT
#define GUI_CLIENT

int gui_init(int port_num, const char *host);

void gui_new_game(int maxx, int maxy, int players_num, char *players[]);

void gui_pixel(int x, int y, char *player);

void gui_eliminated(char *player);

#endif /* GUI_CLIENT */
