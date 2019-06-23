#include "ConsoleCommandRunner.h"
#include "skse64/GameAPI.h"
#include "skse64/GameMenus.h"
#include "skse64/GameData.h"
#include "skse64/GameTypes.h"
#include "skse64/GameTypes.h"
#include "DSNMenuManager.h"
#include "KeyCode.hpp"
#include "WindowUtils.hpp"
#include "StringUtils.hpp"
#include "Log.h"
#include "Utils.h"
#include "Equipper.h"
#include "Hooks.h"
#include <algorithm>
#include <map>
#include "Threading.h"
#include "ThreadAsserts.h"
#include "SpeechRecognitionClient.h"


using std::vector;
using std::string;


ConsoleCommandRunner::ConsoleCommandRunner(SpeechRecognitionClient* client) {
	
	client->RegisterHandler(this, "COMMAND", &ConsoleCommandRunner::HandleCommands, false);

	commands.insert_or_assign("press", &ConsoleCommandRunner::CustomCommandPress);
	commands.insert_or_assign("tapkey", &ConsoleCommandRunner::CustomCommandTapKey);
	commands.insert_or_assign("holdkey", &ConsoleCommandRunner::CustomCommandHoldKey);
	commands.insert_or_assign("releasekey", &ConsoleCommandRunner::CustomCommandReleaseKey);
	commands.insert_or_assign("sleep", &ConsoleCommandRunner::CustomCommandSleep);
	commands.insert_or_assign("switchwindow", &ConsoleCommandRunner::CustomCommandSwitchWindow);
	commands.insert_or_assign("trycast", &ConsoleCommandRunner::CustomCommandTryCast);
	commands.insert_or_assign("pushequip", &ConsoleCommandRunner::CustomCommandPushEquip);
	commands.insert_or_assign("popequip", &ConsoleCommandRunner::CustomCommandPopEquip);
}

void ConsoleCommandRunner::HandleCommands(const TokenList& tokens) {
	vector<string> commands = Utils::split(tokens[1], ';');
	for (int i = 0; i < commands.size(); i++) {
		RunCommand(commands[i]);
	}
}

void ConsoleCommandRunner::RunCommand(string command) {

	ASSERT_IS_CLIENT_THREAD();

	// Run the custom command on the client thread because they do things
	// like sleep and send window key presses and we dont want to lock up
	// the game loop
	// 
	// How ever if it is a vanilla command we should run it on a game thread
	// to prevent multi threading issue (in the hook_loop callback)
	// we block the execution however to ensure a mixture of custom and vanilla
	// comands are executed sequentually
	if (!ConsoleCommandRunner::TryRunCustomCommand(command)) {
		Hooks::taskQueue.ExecuteAction(this, &ConsoleCommandRunner::RunVanillaCommand, command, true);
	}
}

void ConsoleCommandRunner::RunVanillaCommand(string command) {

	ASSERT_IS_GAME_THREAD();

	if (!consoleMenu) {
		Log::info("Trying to create Console menu");
		consoleMenu = DSNMenuManager::GetOrCreateMenu("Console");
		if (!consoleMenu) {
			Log::error("Unable to find Console menu");
			return;
		}
	}

	Log::debug("Invoking command: %s", command.c_str());

	GFxValue methodName;
	methodName.type = GFxValue::kType_String;
	methodName.data.string = "ExecuteCommand";
	GFxValue resphash;
	resphash.type = GFxValue::kType_Number;
	resphash.data.number = -1;
	GFxValue commandVal;
	commandVal.type = GFxValue::kType_String;
	commandVal.data.string = command.c_str();
	GFxValue args[3];
	args[0] = methodName;
	args[1] = resphash;
	args[2] = commandVal;

	GFxValue resp;
	consoleMenu->view->Invoke("flash.external.ExternalInterface.call", &resp, args, 3);
	
}

bool ConsoleCommandRunner::TryRunCustomCommand(const string& command) {
	ASSERT_IS_CLIENT_THREAD();

	std::vector<std::string> params = splitParams(command);
	
	if (params.empty()) {
		return false;
	}
	
	std::string action = params[0];	
	
	if(commands.count(action)) {
		(this->*commands[action])(params);
		return true;	
	}


	return false;
}


void ConsoleCommandRunner::CustomCommandPress(const TokenList& params) {
	std::vector<UInt32 /*key*/> keyDown;
	std::map<UInt32 /*time*/, UInt32 /*key*/> keyUp;

	// command: press <key> <time> <key> <time> ...
	//           [0]   [1]   [2]    [3]   [4]
	for (size_t i = 1; i + 1 < params.size(); i += 2) {
		const std::string &keyStr = params[i];
		
		UInt32 key = 0;

		if (keyStr.empty()) {
			continue;
		}

		key = GetKeyScanCode(keyStr);
		if (key == 0) {
			continue;
		}
		
		UInt32 time = kDefaultKeyPressTime;
		if (params.size() >= (i+1)) {
			time = strtol(params[i + 1].c_str(), NULL, 10);
		}

		if (time == 0) {
			continue;
		}

		keyDown.push_back(key);

		// Map is used to sort by time.
		// Avoiding map key conflicts.
		// Although it changes the time, it is more convenient than sorting by myself.
		while (keyUp.find(time) != keyUp.end()) {
			time++;
		}
		keyUp[time] = key;
	}

	// send KEY_DOWN
	for (auto itr = keyDown.begin(); itr != keyDown.end(); itr++) {
		SendKeyDown(*itr);
	}

	// send KEY_UP
	int totalSleepTime = 0;
	for (auto itr = keyUp.begin(); itr != keyUp.end(); itr++) {
		int sleepTime = itr->first - totalSleepTime;
		Sleep(sleepTime);
		totalSleepTime += sleepTime;

		SendKeyUp(itr->second);
	}
}

void ConsoleCommandRunner::CustomCommandTapKey(const TokenList& params) {
	std::vector<std::string> newParams = { "press" };
	for (auto itr = ++params.begin(); itr != params.end(); itr++) {
		newParams.push_back(*itr);
		newParams.push_back(std::to_string(kDefaultKeyPressTime));
	}
	CustomCommandPress(newParams);
}

void ConsoleCommandRunner::CustomCommandHoldKey(const TokenList& params) {
	for (auto itr = ++params.begin(); itr != params.end(); itr++) {
		UInt32 key = GetKeyScanCode(*itr);
		if (key != 0) {
			SendKeyDown(key);
		}
	}
}

void ConsoleCommandRunner::CustomCommandReleaseKey(const TokenList& params) {
	for (auto itr = ++params.begin(); itr != params.end(); itr++) {
		UInt32 key = GetKeyScanCode(*itr);
		if (key != 0) {
			SendKeyUp(key);
		}
	}
}

void ConsoleCommandRunner::CustomCommandSleep(const TokenList& params) {
	if (params.size() < 2) {
		return;
	}

	string time = params[1];
	time_t millisecond = 0;

	if (time.size() > 2 && time[0] == '0' && (time[1] == 'x' || time[1] == 'X')) {
		// hex
		millisecond = strtol(time.substr(2).c_str(), NULL, 16);
	}
	else if ('0' <= time[0] && time[0] <= '9') {
		// dec
		millisecond = strtol(time.c_str(), NULL, 10);
	}

	if (millisecond > 0) {
		Sleep(millisecond);
	}
}

void ConsoleCommandRunner::CustomCommandSwitchWindow(const TokenList& params) {
	HWND window = NULL;
	DWORD pid = 0;
	std::string windowTitle;

	if (params.size() < 2) {
		pid = GetCurrentProcessId();
	}
	else {
		windowTitle = params[1];
		for (size_t i = 2; i < params.size(); i++) {
			windowTitle += ' ';
			windowTitle += params[i];
		}
		//Log::info("title: " + windowTitle);

		pid = GetProcessIDByName(windowTitle.c_str());
	}

	if (pid != 0) {
		window = FindMainWindow(pid);
		//Log::info("pid: "+std::to_string(pid) + ", window: " + std::to_string((uint64_t)window));
	}

	if (window == NULL && !windowTitle.empty()) {
		window = FindWindow(NULL, windowTitle.c_str());
		//Log::info("window: " + std::to_string((uint64_t)window));

		if (window == NULL) {
			window = FindWindow(windowTitle.c_str(), NULL);
			//Log::info("window: " + std::to_string((uint64_t)window));
		}
	}

	if (window != NULL) {
		SwitchToThisWindow(window, true);
	}
	else {
		Log::info("Cannot find windows with title/executable: %s", windowTitle.c_str());
	}
}
// trycast <spellid> <slot>
void ConsoleCommandRunner::CustomCommandTryCast(const TokenList& params) {

	if (params.size() != 3) {
		Log::error("trycast: Expecting 2 parameters got %d", params.size());
		return;
	}

	PlayerCharacter* player = *g_thePlayer.GetPtr();
	if (!player) {
		Log::error("trycast: No player");
		return;
	}

	// Look up the spell to make sure the player knows it
	SpellItem* spell = Utils::resolvePlayerSpell(player, params[1]);
	if (spell == NULL) {
		Log::error("trycast: Player does not know that spell");
		return;
	}
	else {
		Log::info("Spell is %s", Utils::inspect(spell).c_str());
	}

	std::string slotName = params[2];
	SInt32 slotId = Utils::getSlot(slotName);
	if (slotId == kSlotId_Null) {
		Log::error("trycast: Invalid slot '%'", slotName.c_str());
		return;
	}

	// The item previously inhabiting the slot
	TESForm* saveSlotItem = Utils::getEquippedSlot(player, slotId);

	Log::info("Current item in slot %s is %s ", slotName.c_str(), Utils::inspect(saveSlotItem).c_str());

	// TODO: Disable animation
	RunCommand("player.equipspell " + Utils::fmt_hex(spell->formID) + " " + slotName);

	Sleep(50); // Give time to switch spell

	Log::info("Spell cast time=%d", spell->data.castTime);

	// Extra time since even insta-cast spells need a second or two to work
	const int CAST_TIME_MODIFIER = 2000;

	// Milliseconds to hold the button down to cast
	int castTime = (int)(spell->data.castTime * 1000) + CAST_TIME_MODIFIER;

	if (slotId == kSlotId_Left) {
		RunCommand("press rightclick " + std::to_string(castTime));
	}
	else if (slotId == kSlotId_Right) {
		RunCommand("press leftclick " + std::to_string(castTime));
	}
	else if (slotId == kSlotId_Voice) {
		RunCommand("press z " + std::to_string(castTime));
	}

	Sleep(1000);

	if (saveSlotItem != NULL) {
		Log::debug("Restoring old state: %s", Utils::inspect(saveSlotItem).c_str());
		switch (saveSlotItem->formType) {
		case FormType::kFormType_Spell:
			RunCommand("player.equipspell " + Utils::fmt_hex(saveSlotItem->formID) + " " + slotName);
			break;
		case FormType::kFormType_Shout:
			RunCommand("player.equipshout " + Utils::fmt_hex(saveSlotItem->formID));
			break;
		case FormType::kFormType_Weapon:
			// TODO: Get the right instance of the item rather than the first matching
			Hooks::taskQueue.ExecuteAction(std::bind(papyrusActor::EquipItemEx, player, saveSlotItem, slotId, false, false), true);
			break;
		default:
			Log::error("trycast: Don't know how to restore previous state: %s", Utils::inspect(saveSlotItem).c_str());
		}
	}
	else {
		//TODO: Clear equipped spell
	}

	Log::debug("trycast exits");
}

void ConsoleCommandRunner::CustomCommandPushEquip(const TokenList& params) {


	PlayerCharacter* player = *g_thePlayer.GetPtr();
	if (!player) {
		Log::error("pushequip: No player");
		return;
	}

	std::string slotName = params[1];
	SInt32 slotId = Utils::getSlot(slotName);
	if (slotId == kSlotId_Null) {
		Log::error("pushequip: Invalid slot '%s'", slotName.c_str());
		return;
	}

	// The item previously inhabiting the slot
	TESForm* saveSlotItem = Utils::getEquippedSlot(player, slotId);

	Log::info("Current item in slot %s is %s", slotName.c_str(), Utils::inspect(saveSlotItem).c_str());

	equipStack.push(saveSlotItem);
}

void ConsoleCommandRunner::CustomCommandPopEquip(const TokenList& params) {

	PlayerCharacter* player = *g_thePlayer.GetPtr();
	if (!player) {
		Log::error("popequip: No player");
		return;
	}

	std::string slotName = params[1];
	SInt32 slotId = Utils::getSlot(slotName);
	if (slotId == kSlotId_Null) {
		Log::error("popequip: Invalid slot '%s'", slotName.c_str());
		return;
	}

	if (equipStack.size() == 0) {
		Log::error("popequip: Nothing pushed onto stack!");
		return;
	}

	TESForm* popped = equipStack.top();
	equipStack.pop();

	Log::debug("popequip: Popped from stack %s into %s", Utils::inspect(popped).c_str(), slotName.c_str());
	switch (popped->formType) {
	case FormType::kFormType_Spell:
		RunCommand("player.equipspell " + Utils::fmt_hex(popped->formID) + " " + slotName);
		break;
	case FormType::kFormType_Shout:
		RunCommand("player.equipshout " + Utils::fmt_hex(popped->formID));
		break;
	case FormType::kFormType_Weapon:
		// TODO: Get the right instance of the item rather than the first matching
		Hooks::taskQueue.ExecuteAction(std::bind(papyrusActor::EquipItemEx, player, popped, slotId, false, false), true);
		break;
	default:
		Log::error("popequip: Don't know how to restore previous state: %s", Utils::inspect(popped).c_str());
	}
}