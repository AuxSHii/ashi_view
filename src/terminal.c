//platform specefic termnl control

#include "terminal.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>


/* what this code should do
  1.implmnt windows console init , get size
  2.implement posix console init , get size , trap
  3.restore the terminal for both
*/

#ifndef _WIN32 //to go btwn win and posix
#include <windows.h>
static HANDLE hOut;
static DWORD originalOutMode = 0;
static bool s_init = false;


static void sig_handler(int sig_num){
    (void)sig_num;
}




int init_terminal(){
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


//fxn to get terminal size

int terminal_get_size(int *cols, int *rows){
     
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if(GetConsoleScreenBufferInfo(hOut, &csbi)) {  //scrn buff info will handle out and store inside csbi adptr
        *cols = csbi.srWindow.Right - csbi.srWindow.Left +1;
        *rows = csbi.srWindow.Bottom - csbi.srWindow.Top +1;
        if(*cols > 0 && *rows > 0){
            return 0;
        }
   }
}






##else //for psoix

int init_terminal(){
  
  struct sigaction sa;

  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  s_init = true;
  return 0;


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









