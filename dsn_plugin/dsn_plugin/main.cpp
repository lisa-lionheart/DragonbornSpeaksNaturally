#include "Hooks.h"
#include "SpeechRecognitionClient.h"
#include "ConsoleCommandRunner.h"
#include "Log.h"
#include "common/IPrefix.h"
#include "skse64_common/BranchTrampoline.h"
#include "skse64_common/Relocation.h"
#include <Windows.h>
#include "VersionCheck.h"
#include "SkyrimType.h"



extern "C"	{
	BOOL WINAPI DllMain(
		HINSTANCE hinstDLL,  // handle to DLL module
		DWORD fdwReason,     // reason for calling function
		LPVOID lpReserved)  // reserved
	{
		// Get the path to the DLL file
		char DllPath[MAX_PATH] = { 0 };
		GetModuleFileName(hinstDLL, DllPath, _countof(DllPath));
		std::string dllPath = std::string(DllPath);

		// Perform actions based on the reason for calling.
		switch (fdwReason)
		{
		case DLL_PROCESS_ATTACH:

			// Initialize once for each new process.
			// Return FALSE to fail DLL load.

			if (!VersionCheck::IsCompatibleExeVersion()) {
				return TRUE;
			}

			Hooks::Start(dllPath, hinstDLL);

			break;

		case DLL_THREAD_ATTACH:
			// Do thread-specific initialization.
			break;

		case DLL_THREAD_DETACH:
			// Do thread-specific cleanup.
			break;

		case DLL_PROCESS_DETACH:
			// Perform any necessary cleanup.
			break;
		}
		return TRUE;  // Successful DLL_PROCESS_ATTACH.
	}
};