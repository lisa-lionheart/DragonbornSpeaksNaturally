#pragma once
#include "SpeechRecognitionClient.h"
#include <vector>
#include <string>
#include "skse64_common/Relocation.h"
#include "common/IPrefix.h"

#include <Threading.h>


namespace Hooks {
	extern TaskQueue taskQueue;
	extern SpeechRecognitionClient* speechClient;

	void Start(std::string dllPath, HINSTANCE hInstDll);

	void Inject();
}