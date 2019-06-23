#pragma once
#include "common/IPrefix.h"
#include <cstdlib>
#include <string>
#include <fstream>


class Log
{
	std::ofstream log_file;

	Log();
	static Log* instance;

	void write(int level, const char* format, va_list& args);

public:
	static Log* get();

	static void trace(const char* message, ...);
	static void debug(const char* message, ...);
	static void info(const char* message, ...);
	static void warn(const char* message, ...);
	static void error(const char* message, ...);
};




// Trace method enters and exits, useful for random crashes 
#ifdef _METHOD_TRACE
class MethodTracer {
	const char* name;
public:
	inline MethodTracer(const char* _name) : name(_name) {
		Log::trace("Function %s enters", name);
	}

	inline ~MethodTracer() {
		Log::trace("Function %s exits", name);
	}

};
#define TRACE_METHOD() MethodTracer _tracer(__FUNCTION__)
#else
#define TRACE_METHOD()
#endif // DEBUG