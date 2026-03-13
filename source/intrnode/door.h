/*    DOOR.H
      Copyright 2003 Evan Elias

      This is a header for the ObjectDoor Door Client.
*/

#ifndef DOORHDEF
#define DOORHDEF

// Determine the platforms (Windows vs Linux vs DOS, and XSDK vs OpenDoors)
#if defined(WIN32) || defined(__WIN32__) || defined(_WIN32)
#define WINDOOR
#ifdef __CONSOLE__
#define XSDK32
//#pragma message 32-bit w/ Synchronet XSDK
#define LIMITED_XSDK
#include <xsdk.h>
#else
#define OD32
//#pragma message 32-bit w/ OpenDoors
#include "../intrnode/opendoor.h"
#endif

#elif defined(LINUX)
// Linux platform
#ifdef __CONSOLE__
#define XSDK32
#define LIMITED_XSDK
#include "xsdk.h"
#else
#define OD32
#include "../intrnode/opendoor.h"
#endif
#include <cstdio>
#include <ctime>

#else
// DOS platform
#define OD16
#define DOSDOOR
#ifndef LPSTR
#define LPSTR char*
#endif
#include "opendoor.h"
#endif

#if !defined(__CONIO_H) && !defined(__COLORS)
const short    BLACK = 0;
const short    BLUE = 1;
const short    GREEN = 2;
const short    CYAN = 3;
const short    RED = 4;
const short    MAGENTA = 5;
const short    BROWN = 6;
const short    YELLOW = 14;
const short    WHITE = 15;
const short    DARKGRAY = 8;
#endif

const short 	 LWHITE = 7;
const short 	 LBLUE = 9;
const short 	 LGREEN = 10;
const short 	 LCYAN = 11;
const short 	 LRED = 12;
const short 	 LMAGENTA = 13;


// Door-related functions found in plat*.cpp

#ifdef OD32
#ifndef LINUX
void startup(LPSTR);
#else
void startup(int, char*[]);
#endif
#else
void startup(int, char*[]);
#endif

void setupExitFunction();
char inputKey();
void local(char *, short=7, short=1);
void newline();
void printChar(char);
void center(char *, short=7, short=1);
void showAnsi(char *);
void backspace(short);
void pausePrompt(short=0, short=0);
void checkCarrier();
void checkTimeLeft();
void clearScreen();
bool hasAnsi();
short isSysop();
char* getRealName();
char* getAlias();
char getGender();
short getNode();
short getPlatform();
bool inactiveCheck(time_t, short=1);
bool checkNodeMsg();
void listOnlineUsers();
void platExit(short);

#endif
