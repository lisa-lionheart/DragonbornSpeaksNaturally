#pragma once
#include "SpeechRecognitionClient.h"
#include <vector>
#include <string>
#include "skse64_common/Relocation.h"
#include "common/IPrefix.h"

#include <functional>

void Hooks_Inject(void);


// Are we running on the game thread
bool isGameThread();

// Queue an action to run on the game thread
void executeOnGameThread(const std::function<void(void)>& code, bool wait);