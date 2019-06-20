#include "Log.h"
#include <sstream>
#include <time.h>
#include <stdarg.h>
#include <shlobj_core.h> 

static int THRESHOLD = 0;

// Maximum message length
static const int MAX_MESSAGE = 2048;

using namespace std;

Log::Log() 
{
	string name = "DragonbornSpeaksNaturallyPlugin.log";

	char myDocuments[MAX_PATH];
	if (SHGetFolderPathA(NULL, CSIDL_MYDOCUMENTS, NULL, 0, myDocuments) != S_OK) {
		ASSERT("Could not get My Documetns folder", false);
		return;
	}	
	string fileName = string(myDocuments) + "\\DragonbornSpeaksNaturally\\" + name;
	log_file = ofstream(fileName, ios_base::out |ios_base::app);
}

Log* Log::instance = NULL;

const char* LEVEL_NAMES[] = {
	"TARCE",
	"DEBUG",
	" INFO",
	" WARN",
	"ERROR"
};


Log* Log::get() {
	if (!instance)
		instance = new Log();
	return instance;
}

void Log::write(int level, const char* message, va_list& args) {
	
		time_t rawtime;
		time(&rawtime);
		tm* t = localtime(&rawtime);

		char buffer[MAX_MESSAGE];
		// Print the time and the log level
		int timeLen = snprintf(&buffer[0], MAX_MESSAGE, "%02d:%02d:%02d %s: ", t->tm_hour, t->tm_min, t->tm_sec, LEVEL_NAMES[level]);

		// Print the message formatted
		vsnprintf_s(&buffer[timeLen], MAX_MESSAGE - timeLen, MAX_MESSAGE - timeLen, message, args);

		log_file << buffer << std::endl;
		log_file.flush();
}


void Log::trace(const char* message, ...) {
	if (0 >= THRESHOLD) {
		va_list args;
		va_start(args, message);
		Log::get()->write(0, message, args);
		va_end(args);
	}
}

void Log::debug(const char* message, ...) {
	if (1 >= THRESHOLD) {
		va_list args;
		va_start(args, message);
		Log::get()->write(1, message, args);
		va_end(args);
	}
}
void Log::info(const char* message, ...) {
	if (2 >= THRESHOLD) {
		va_list args;
		va_start(args, message);
		Log::get()->write(3, message, args);
		va_end(args);
	}
}
void Log::warn(const char* message, ...) {
	if (3 >= THRESHOLD) {
		va_list args;
		va_start(args, message);
		Log::get()->write(4, message, args);
		va_end(args);
	}
}
void Log::error(const char* message, ...) {
	if (4 >= THRESHOLD) {
		va_list args;
		va_start(args, message);
		Log::get()->write(4, message, args);
		va_end(args);
	}
}