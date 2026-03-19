#ifndef __SLOTDEFS_H__
#define __SLOTDEFS_H__

#ifdef _WIN32

#include <windows.h>
// Define MQ_MAX_MSG_SIZE for Windows to match the mailslot max message size
// used in CreateMailslot() calls (240 bytes + safety margin).
#define MQ_MAX_MSG_SIZE 244

#else
// Linux / non-Windows

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <mqueue.h>

#include <mutex>
#include <ctype.h>
#include <cstring>
#include <cstdlib>

// POSIX message queue constants for IPC (replaces Windows mailslots)
#define MQ_MAX_MSG_SIZE 256
#define MQ_MAX_MSGS 10

// Override any previous HANDLE definition (e.g. from xsdkdefs.h void*)
// We need mqd_t for POSIX message queue IPC
#ifdef HANDLE
#undef HANDLE
#endif
#define HANDLE mqd_t
#ifdef INVALID_HANDLE_VALUE
#undef INVALID_HANDLE_VALUE
#endif
#define INVALID_HANDLE_VALUE ((mqd_t)-1)

#ifndef BOOL
#define BOOL bool
#endif
#ifndef TRUE
#define TRUE true
#endif
#ifndef FALSE
#define FALSE false
#endif

#ifndef LPSTR
#define LPSTR char*
#endif

#define CRITICAL_SECTION std::recursive_mutex

#ifndef DWORD
#define DWORD unsigned long
#endif
#define MAILSLOT_NO_MESSAGE ((DWORD)-1)

int strcmpi (const char * str1, const char * str2 );

char *strlwr(char *str);

#endif

#endif
