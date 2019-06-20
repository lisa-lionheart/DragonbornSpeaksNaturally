#include "SpeechRecognitionClient.h"
#include "ConsoleCommandRunner.h"
#include "Log.h"
#include <io.h>
#include <fcntl.h>
#include <Threading.h>

#include "FavoritesMenuManager.h"
#include "ConsoleCommandRunner.h"
#include "SkyrimType.h"


static std::vector<std::string> split(const std::string &s, char delim) {
	std::stringstream ss(s);
	std::string item;
	std::vector<std::string> tokens;
	while (std::getline(ss, item, delim)) {
		tokens.push_back(item);
	}
	return tokens;
}

SpeechRecognitionClient* SpeechRecognitionClient::instance = NULL;

SpeechRecognitionClient* SpeechRecognitionClient::getInstance() {
	if (!instance)
		instance = new SpeechRecognitionClient();
	return instance;
}

SpeechRecognitionClient::SpeechRecognitionClient()
{
}

SpeechRecognitionClient::~SpeechRecognitionClient()
{
}

void SpeechRecognitionClient::StopDialogue() {
	WriteLine("STOP_DIALOGUE");
}

void SpeechRecognitionClient::StartDialogue(DialogueList list) {
	this->currentDialogueId++;
	this->selectedIndex = -1;
	std::string command = "START_DIALOGUE|";
	command.append(std::to_string(this->currentDialogueId));
	for (size_t i = 0; i < list.lines.size(); i++) {
		command.append("|");
		command.append(list.lines[i]);
	}
	WriteLine(command);
}

int SpeechRecognitionClient::ReadSelectedIndex() {
	int t = this->selectedIndex;
	this->selectedIndex = -1;
	return t;
}

DWORD SpeechRecognitionClient::ClientLoop() {
	Log::info("SpeechRecognitionClient thread started");
	for (;;) {

		g_ClientThreadTaskQueue.PumpThreadActions();

		std::string inLine = ReadLine();
		std::vector<std::string> tokens = split(inLine, '|');
		std::string responseType = tokens[0];
		if (responseType == "DIALOGUE") {
			int dialogueId = std::stoi(tokens[1]);
			int indexId = std::stoi(tokens[2]);
			if (dialogueId == this->currentDialogueId) {
				this->selectedIndex = indexId;
			}
		}
		else if (responseType == "COMMAND") {
			std::vector<std::string> commands = split(tokens[1], ';');
			for (int i = 0; i < commands.size(); i++) {
				ConsoleCommandRunner::RunCommand(commands[i]);
			}
		}
		else if (responseType == "EQUIP") {

			// Not sure why this only in VR
			if (g_SkyrimType == VR) {

				g_GameThreadTaskQueue.ExecuteAction(std::bind(&FavoritesMenuManager::ProcessEquipCommand, FavoritesMenuManager::getInstance(), tokens[1]), false);
			}
		}
	}

	return 0;
}

std::string SpeechRecognitionClient::ReadLine() {
	DWORD dwRead = 0;
	char buffer[4096];

	std::string line(workingLine);
	for (;;) {

		g_ClientThreadTaskQueue.PumpThreadActions();

		BOOL bSuccess = ReadFile(this->hChildStd_OUT_Rd, buffer, 4096, &dwRead, NULL);
		if (bSuccess && dwRead > 0)
		{
			std::string chunk = std::string(buffer, dwRead);
			line = line.append(chunk);
			for (size_t i = 0; i < chunk.length(); i++)
			{
				if (chunk[i] == '\n')
				{
					// Append any trailing line to the next readline call
					if (i < chunk.length() - 1)
						workingLine = chunk.substr(i + 1);
					else
						workingLine = "";

					// Remove \n
					line.pop_back();
					Log::debug("CLIENT << %s", line.c_str());
					return line;
				}
			}
		}
	}

	return "";
}

void SpeechRecognitionClient::WriteLine(std::string line) {
	DWORD dwWritten;
	Log::debug("CLIENT >> %s", line.c_str());
	line.push_back('\n');
	WriteFile(this->hChildStd_IN_Wr, line.c_str(), line.length(), &dwWritten, NULL);
}

void SpeechRecognitionClient::Start() {
	extern std::string g_dllPath;

	// Startup speech recognition service
	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0);
	SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0);
	CreatePipe(&hChildStd_IN_Rd, &hChildStd_IN_Wr, &saAttr, 0);
	SetHandleInformation(hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0);

	std::string exePath = g_dllPath.substr(0, g_dllPath.find_last_of("\\/"))
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
	CloseHandle(piProcInfo.hThread);extern std::string g_dllPath;


	if (bSuccess)
	{
		Log::info("Initialized speech recognition service");
		CreateThread(NULL, 0, &SpeechRecognitionClient::ThreadStart, this, 0L, NULL);
	}
	else
	{
		Log::info("Failed to initialize speech recognition service");
	}
}
