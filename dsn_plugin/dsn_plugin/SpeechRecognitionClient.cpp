#include "SpeechRecognitionClient.h"
#include "Log.h"
#include <io.h>
#include <fcntl.h>
#include <Threading.h>
#include "SkyrimType.h"
#include "Utils.h"
#include "Hooks.h"

using std::string;
using std::vector;
using std::map;


SpeechRecognitionClient::SpeechRecognitionClient(string dllPath) {

	// Startup speech recognition service
	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0);
	SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0);
	CreatePipe(&hChildStd_IN_Rd, &hChildStd_IN_Wr, &saAttr, 0);
	SetHandleInformation(hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0);

	string exePath = dllPath.substr(0, dllPath.find_last_of("\\/"))
		.append("\\DragonbornSpeaksNaturally.exe")
		.append(" --encoding UTF-8"); // Let the service set encoding of its stdin/stdout to UTF-8.
									// This can avoid non-ASCII characters (such as Chinese characters) garbled.

	Log::info("Starting speech recognition service at %s", exePath.c_str());

	LPSTR szCmdline = const_cast<char*>(exePath.c_str());
	PROCESS_INFORMATION piProcInfo;
	STARTUPINFO siStartInfo;
	BOOL bSuccess = FALSE;
	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	//siStartInfo.hStdError = g_hChildStd_OUT_Wr;
	siStartInfo.hStdOutput = hChildStd_OUT_Wr;
	siStartInfo.hStdInput = hChildStd_IN_Rd;
	siStartInfo.wShowWindow = SW_HIDE;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
	siStartInfo.dwFlags |= STARTF_USESHOWWINDOW;

	bSuccess = CreateProcess(NULL,
		szCmdline,     // command line 
		NULL,          // process security attributes 
		NULL,          // primary thread security attributes 
		TRUE,          // handles are inherited 
		0,             // creation flags 
		NULL,          // use parent's environment 
		NULL,          // use parent's current directory 
		&siStartInfo,  // STARTUPINFO pointer 
		&piProcInfo);  // receives PROCESS_INFORMATION 

	CloseHandle(piProcInfo.hProcess);
	CloseHandle(piProcInfo.hThread); extern string g_dllPath;


	if (bSuccess) {
		Log::info("Initialized speech recognition service");
		CreateThread(NULL, 0, &SpeechRecognitionClient::ThreadStart, this, 0L, NULL);
	} else {
		Log::error("Failed to initialize speech recognition service");
	}
}

SpeechRecognitionClient::~SpeechRecognitionClient()
{
}


void SpeechRecognitionClient::HandleLine(const std::string& inLine) {
	Log::debug("CLIENT << %s", inLine);
	vector<string> tokens = Utils::split(inLine, '|');
		
	string cmd = tokens[0];		
	if (commandHandlers.count(cmd) == 1) {
		auto handler = commandHandlers[cmd];
		auto func = std::get<0>(handler);
		if (std::get<1>(handler)) {
			Hooks::taskQueue.ExecuteAction(std::bind(func, tokens), true);
		}else {
			func(tokens);
		}
	}
}

DWORD SpeechRecognitionClient::ClientLoop() {
	Log::info("SpeechRecognitionClient thread started");
	
	DWORD dwRead = 0;
	char buffer[4096];
	string workingLine(4096, ' ');
	for (;;) {

		PumpThreadActions();

		BOOL bSuccess = ReadFile(this->hChildStd_OUT_Rd, buffer, sizeof(buffer), &dwRead, NULL);
		if (!bSuccess || dwRead == 0)
		{
			continue;
		}

		int newLine = -1;
		for (size_t i = 0; i < dwRead; i++) {
			if (buffer[i] == '\n') {
				HandleLine(workingLine);
				workingLine.clear();
			}
			else {
				workingLine.append(1,buffer[i]);
			}
		}
	}

	return 0;
}

void SpeechRecognitionClient::WriteLine(string line) {

	// Permit only writes from a single thread
	if (!isOwnerThread()) {
		ExecuteAction(this, &SpeechRecognitionClient::WriteLine, line, false);
		return;
	}

	DWORD dwWritten;
	Log::debug("CLIENT >> %s", line.c_str());
	line.push_back('\n');
	WriteFile(this->hChildStd_IN_Wr, line.c_str(), line.length(), &dwWritten, NULL);
	
}
