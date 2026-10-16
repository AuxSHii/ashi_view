//platform specefic termnl control

#include <terminal.h>

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

int init_terminal(){
if(s_init) return 0;

hOut = GetStdHandle(STD_OUTPUT_HANDLE);
GetConsoleMode(hOut , &originalOutMode);  //storing og output settings of terminal on windows

DWORD newOutMode = originalOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUPUT;

//set new our mode in user terminal
SetConsoleMode(hOut , newOutMode);
}
