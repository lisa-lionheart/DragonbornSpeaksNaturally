#pragma once
#include "common/IPrefix.h"

#include <vector>
#include <queue>
#include <string>
#include <windows.h> 
#include <sstream>
#include <mutex>


#include <Threading.h>

typedef std::vector<std::string> TokenList;
typedef std::function<void(const TokenList&)> TokenHandlerFunc;

/*
	Manage the c# service, creates the proccess handles IO
*/
class SpeechRecognitionClient : public TaskQueue
{

public:

	// Start the service
	SpeechRecognitionClient(std::string dllPath);

	~SpeechRecognitionClient();
	

	// Send a command to the service
	void WriteLine(std::string str);



	// Register interest in a handling a game command
	// func: The function to execute
	// bool: Should be defered to execution in a game thread, otherwise on the client thread
	template<class T>
	void RegisterHandler(T* inst, const char* command, void (T::*func)(const TokenList&), bool executeGameThread) {
		TokenHandlerFunc handler = std::bind(func, inst, std::placeholders::_1);
		commandHandlers[command] = std::make_tuple(handler, executeGameThread);
	}
	

private:


	std::map<std::string, std::tuple<TokenHandlerFunc,bool>> commandHandlers;

	HANDLE hChildStd_IN_Rd = NULL;
	HANDLE hChildStd_IN_Wr = NULL;
	HANDLE hChildStd_OUT_Rd = NULL;
	HANDLE hChildStd_OUT_Wr = NULL;
	
	DWORD ClientLoop();
	

	static DWORD WINAPI ThreadStart(void* a) {
		return ((SpeechRecognitionClient*)a)->ClientLoop();
	}	
	void HandleLine(const std::string& line);
};

