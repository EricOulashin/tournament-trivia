#ifndef TRIVLOG_H_DEF
#define TRIVLOG_H_DEF

#include <cstdio>
#include <cstdarg>
#include <cstring>

// Simple logging utility for trivsrv.
// Logging is enabled by the presence of a file called "debug.on" in the
// working directory.  When enabled, messages are appended to "trivsrv.log".
// When disabled, log calls are silently discarded.
//
// Usage:  trivlog("trivsrv: something happened, value=%d\n", val);

inline bool trivlog_enabled()
{
	static int cached = -1;
	if (cached == -1)
	{
		FILE* f = fopen("debug.on", "r");
		if (f) { fclose(f); cached = 1; }
		else cached = 0;
	}
	return cached == 1;
}

inline void trivlog(const char* fmt, ...)
{
	if (!trivlog_enabled())
		return;
	FILE* f = fopen("trivsrv.log", "a");
	if (!f)
		return;
	va_list args;
	va_start(args, fmt);
	vfprintf(f, fmt, args);
	va_end(args);
	fclose(f);
}

#endif
