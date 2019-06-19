#include "Log.h"
#include <sstream>
#include <time.h>

static int THRESHOLD = 0;

Log::Log() :
	log_file("dragonborn_speaks.log", std::ios_base::out | std::ios_base::app)
{

}

Log::~Log()
{
}

Log* Log::instance = NULL;

const char* LEVEL_NAMES[] = {
	"DEBUG",
	"INFO",
	"WARN",
	"ERROR"
};


Log* Log::get() {
	if (!instance)
		instance = new Log();
	return instance;
}

void Log::write(int level, std::string message) {
	if (level >= THRESHOLD) {
		time_t rawtime;
		time(&rawtime);
		tm* t = localtime(&rawtime);

		char buffer[2048];
		sprintf_s(buffer, "%02d:%02d:%02d %s: %s", t->tm_hour, t->tm_min, t->tm_sec, LEVEL_NAMES[level], message.c_str());

		log_file << buffer << std::endl;
		log_file.flush();
	}
}

void Log::debug(std::string message) {
	Log::get()->write(0, message);
}
void Log::info(std::string message) {
	Log::get()->write(1, message);
}
void Log::warn(std::string message) {
	Log::get()->write(2, message);
}
void Log::error(std::string message) {
	Log::get()->write(3, message);
}

void Log::address(std::string message, uintptr_t addr) {
	std::stringstream ss;
	ss << std::hex << addr;
	const std::string s = ss.str();
	Log::info(message.append(s));
}

void Log::hex(std::string message, uintptr_t addr) {
	std::stringstream ss;
	ss << std::hex << addr;
	const std::string s = ss.str();
	Log::info(message.append(s));
}