#pragma once
#include "common/IPrefix.h"
#include <cstdlib>
#include <string>
#include <fstream>

class Log
{
	std::ofstream log_file;

	Log();
	~Log();
	static Log* instance;

	void write(int level, std::string message);

public:
	static Log* get();

	
	static void debug(std::string message);
	static void info(std::string message);
	static void warn(std::string message);
	static void error(std::string message);


	static void address(std::string message, uintptr_t addr);
	static void hex(std::string message, uintptr_t addr);
};

