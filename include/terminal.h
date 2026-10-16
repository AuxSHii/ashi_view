//declare fxns to control terminals interface for progrm

int terminal_init(void); //to vt processng on win , utf-8 mode , install signal hadnlers

void terminal_restore(void); //to restore terminal to og config/sizer after user exits the prgrm

int terminal_get_size(int *cols, int *rows); //write size[row/col] of currnt termnl to ptr

//cursor helper fxns

terminal_hide_cursor(void);
terminal_show_cursor(void);
terminal_reset_color(void);
terminal_clear_screen(void);
terminal_cursor_up(int rows);

extern volatile bool g_terminal_interrupted;//flag for signal traps

