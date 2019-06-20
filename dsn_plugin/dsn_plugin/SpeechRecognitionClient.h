#pragma once
#include "common/IPrefix.h"

#include <vector>
#include <queue>
#include <string>
#include <windows.h> 
#include <sstream>
#include <mutex>

struct DialogueList
{
	std::vector<std::string> lines;
};

/*
	Manage the c# service, creates the proccess handles IO
*/
class SpeechRecognitionClient
{
public:
	static SpeechRecognitionClient* getInstance();

	~SpeechRecognitionClient();
	
	// Start the service
	void Start();

	// Send a command to the service
	void WriteLine(std::string str);

	// Dialogue was closed
	void StopDialogue();
	// Start the speach recognition
	void StartDialogue(DialogueList list);

	// Return the response to the last dialogue call
	// If not response return -1
	// If goodbye phrase then -2
	// Otherwise the index of the option selected
	int ReadSelectedIndex();
		
private:
	static SpeechRecognitionClient* instance;

	HANDLE hChildStd_IN_Rd = NULL;
	HANDLE hChildStd_IN_Wr = NULL;
	HANDLE hChildStd_OUT_Rd = NULL;
	HANDLE hChildStd_OUT_Wr = NULL;
	
	DWORD ClientLoop();

	int selectedIndex = -1;
	int currentDialogueId = 0;

	std::string workingLine;

	static DWORD WINAPI ThreadStart(void* a) {
		return ((SpeechRecognitionClient*)a)->ClientLoop();
	}
	
	SpeechRecognitionClient();

	std::string ReadLine();
};

