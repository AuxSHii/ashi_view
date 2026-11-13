//platform specefic termnl control

#include "terminal.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

volatile bool g_terminal_interrupted = false;

/* what this code should do
  1.implmnt windows console init , get size
  2.implement posix console init , get size , trap
  3.restore the terminal for both
*/

#ifndef _WIN32 //to go btwn win and posix
#include <windows.h>
#include <conio.h>
static HANDLE hOut = INVALID_HANDLE_VALUE;
static DWORD originalOutMode = 0;
static bool s_init = false;
static UINT ogCP = 0;



static void sig_handler(int sig_num){
    (void)sig_num;
    g_terminal_interrupted = true;
    terminal_restore();
    exit(130);
}




int terminal_init(){
if(s_init) return 0;

hOut = GetStdHandle(STD_OUTPUT_HANDLE);
GetConsoleMode(hOut , &originalOutMode);  //storing og output settings of terminal on windows

DWORD newOutMode = originalOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUPUT;

//set new our mode in user terminal
//SetConsoleMode(hOut , newOutMode);


ogCP = GetConsoleOutputCP(); //utf8 cp, store og user cp
SetConsoleOutputCP(CP_UTF8);

//register handler for sigint n sigterm
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);

  s_init = true;
  return 0;
}

//fxn to restore terminal

void terminal_restore(void){
    if(!s_init) return;

    terminal_show_cursor();
    terminal_reset_color();
    fflush(stdout);

    //restoer original modes here
    SetConsoleMode(hOut ,originalOutMode);
    setConsoleOutputCP(ogCP);
    
    if (hOut != INVALID_HANDLE_VALUE && originalOutMode != 0) {
        SetConsoleMode(hOut ,originalOutMode);
    }
    if (ogCP != 0) {
        SetConsoleOutputCP(ogCP);
    }
 

    s_init = false;
}






//fxn to get terminal size

int terminal_get_size(int *cols, int *rows){
  if(hOut != INVALID_HANDLE_VALUE){
     
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if(GetConsoleScreenBufferInfo(hOut, &csbi)) {  //scrn buff info will handle out and store inside csbi adptr
        *cols = csbi.srWindow.Right - csbi.srWindow.Left +1;
        *rows = csbi.srWindow.Bottom - csbi.srWindow.Top +1;
        if(*cols > 0 && *rows > 0){
            return 0;
        }
   }
}


    // fallback to environment variables or defaults 
    const char *env_c = getenv("COLUMNS");
    const char *env_r = getenv("LINES");
    *cols = env_c ? atoi(env_c) : 80;
    *rows = env_r ? atoi(env_r) : 24;
    if (*cols <= 0) *cols = 80;
    if (*rows <= 0) *rows = 24;
    return 0;
}



//fxn for time elpased[gif playback]

uint64_t terminal_time_ms(void){
  LARGE_INTEGER count,freq;
  QueryPerformanceCounter(&count);
  QueryPerformanceFrequency(&freq);
  return (uint64_t)((count.QuadPart * 1000ULL) / freq.QuadPart);
   //in ms 
}


void terminal_sleep_ms(uint64_t ms){
  Sleep((DWORD)ms);  //make my thread sleepfor this much miliseconds time
}

//fxn taking exit key input[non blocking]

bool terminal_check_key(char *out_key){ //has the user pressed any key this ms
  if(_kbhit()) { //windows input buffer [if not empty]=> returns non-zero
    int ch = _getch(); //pull the key in buffer
    if(outkey) *outkey = (char)ch;
    return true;
  }
  return false;
}




##else //for psoix

#include <unistd>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <poll.h>

static struct termios og_termios;
static bool has_og_termios = false;
static bool s_init = false;

static void sig_handler(int sig_num){
    (void)sig_num;
    g_terminal_interrupted = true;
    terminal_restore();
    exit(130);
}



int terminal_init(){
  if (s_init) return 0;  
  

  if(isatty(STDIN_FILENO)){
    if(tcgetattr(STDIN_FILENO, &og_termios) == 0){
      has_og_termios = true;
    }
  }


  struct sigaction sa;
  sa.sa_handler = sig_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  s_init = true;
  return 0;


}


//fxn to restorer terminal

void terminal_restore(void){
  if(s_init) return;

  terminal_show_cursor();
  terminal_reset_color();
  fflush(stdout);

  if(has_og_termios){
    tcsetattr(STDIN_FILENO, TCSANOW, &og_termios);
  }
  s_init = false;
}


//fxn for gettin size
int terminal_get_size(int *cols, int *rows){

 struct winsize ws;
 if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0){
     
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
 }
    //universal fallback to env var
//read env var for row adn cols
 const char *env_c = getenv("COLUMNS");
 const char *env_r = getenv("LINES");

 *cols = env_c ? atoi(env_c) : 80;
 *rows = env_r ? atoi(env_r) : 24;

 //check for valid integer found 
 if(*cols <= 0) *cols = 80;  //if invalid then fallback to def value we defined
 if(*rows <= 0) *rows = 24;
 return 0;
}

//fxn for get time in ms

uint64_t terminal_time_ms(void){
  struct timespec ts; //posix time struct
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
 //return in ms
}

//fxn to make my thread sleep for ms
void terminal_sleep_ms(uint32_t ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}



//fxn: to get non blocking exit key for our programme
void terminal_check_key(char *out_key){
  struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN, .revents = 0 }; //posix poll fxn struct defineed declared
  if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
        char ch;
        if (read(STDIN_FILENO, &ch, 1) == 1) {
            if (out_key) *out_key = ch;
            return true;
        }
    }
    return false;
}







#endif //win 32

void terminal_hide_cursor(void)
{
    fputs("\x1b[?25l", stdout);
}

void terminal_show_cursor(void){
    fputs("\x1b[251", stdout);
}

void terminal_reset_color(void){
    fputs("\x1b[0m", stdout);
}

void terminal_clear_screen(void)
{
    fputs("\x1b[2J\x1b[H", stdout);
}

void terminal_cursor_home(void)
{
    fputs("\x1b[H", stdout);
}

void terminal_cursor_up(int rows)
{
    if (rows > 0) {
        fprintf(stdout, "\x1b[%dA\r", rows);
    }
}

