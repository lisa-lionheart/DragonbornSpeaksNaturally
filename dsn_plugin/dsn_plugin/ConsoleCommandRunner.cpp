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

static IMenu* consoleMenu = NULL;

std::unordered_map<std::string/* name */, std::function<void(std::vector<std::string>)>/* func */> ConsoleCommandRunner::customCmdList;

void ConsoleCommandRunner::RunCommand(std::string command) {

	// Bad things will happen if we try and do this from a background thread so pass it of
	// and wait for it to be executed
	if (!isGameThread()) {
		// Don't know what thread this would be if not the client thread
		ASSERT_IS_CLIENT_THREAD();
		Log::debug("RunCommand: Calling to game thread for " + command);
		executeOnGameThread(std::bind(ConsoleCommandRunner::RunCommand, command), true);
		Log::debug("RunCommnad: Done with async");
		return;
	}

	if (!consoleMenu) {
		Log::info("Trying to create Console menu");
		consoleMenu = DSNMenuManager::GetOrCreateMenu("Console");
	}

	if (consoleMenu != NULL) {
		Log::debug("Invoking command:" + command);

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
	else
	{
		Log::info("Unable to find Console menu");
	}
}

bool ConsoleCommandRunner::TryRunCustomCommand(const std::string & command) {
	ASSERT_IS_CLIENT_THREAD();

	std::vector<std::string> params = splitParams(command);
	
	if (params.empty()) {
		return false;
	}
	
	std::string action = params[0];
	stringToLower(action);

	auto itr = customCmdList.find(action);
	if (itr != customCmdList.end()) {
		Log::debug("Custom command: " + command);
		itr->second(params);
		return true;
	}

	return false;
}

void ConsoleCommandRunner::RegisterCustomCommands() {
	// Register custom commands
	customCmdList["press"] = CustomCommandPress;
	customCmdList["tapkey"] = CustomCommandTapKey;
	customCmdList["holdkey"] = CustomCommandHoldKey;
	customCmdList["releasekey"] = CustomCommandReleaseKey;
	customCmdList["sleep"] = CustomCommandSleep;
	customCmdList["switchwindow"] = CustomCommandSwitchWindow;
	customCmdList["trycast"] = CustomCommandTryCast;
}

void ConsoleCommandRunner::CustomCommandPress(std::vector<std::string> params) {
	std::vector<UInt32 /*key*/> keyDown;
	std::map<UInt32 /*time*/, UInt32 /*key*/> keyUp;

	// If time does not exist, set as kDefaultKeyPressTime milliseconds
	if ((params.size() - 1) % 2 > 0) {
		params.push_back(std::to_string(kDefaultKeyPressTime));
	}

	// command: press <key> <time> <key> <time> ...
	//           [0]   [1]   [2]    [3]   [4]
	for (size_t i = 1; i + 1 < params.size(); i += 2) {
		const std::string &keyStr = params[i];
		const std::string &timeStr = params[i + 1];
		UInt32 key = 0;
		UInt32 time = 0;

		if (keyStr.empty()) {
			continue;
		}

		key = GetKeyScanCode(keyStr);
		if (key == 0) {
			continue;
		}

		time = strtol(timeStr.c_str(), NULL, 10);
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

void ConsoleCommandRunner::CustomCommandTapKey(std::vector<std::string> params) {
	std::vector<std::string> newParams = { "press" };
	for (auto itr = ++params.begin(); itr != params.end(); itr++) {
		newParams.push_back(*itr);
		newParams.push_back(std::to_string(kDefaultKeyPressTime));
	}
	CustomCommandPress(newParams);
}

void ConsoleCommandRunner::CustomCommandHoldKey(std::vector<std::string> params) {
	for (auto itr = ++params.begin(); itr != params.end(); itr++) {
		UInt32 key = GetKeyScanCode(*itr);
		if (key != 0) {
			SendKeyDown(key);
		}
	}
}

void ConsoleCommandRunner::CustomCommandReleaseKey(std::vector<std::string> params) {
	for (auto itr = ++params.begin(); itr != params.end(); itr++) {
		UInt32 key = GetKeyScanCode(*itr);
		if (key != 0) {
			SendKeyUp(key);
		}
	}
}

void ConsoleCommandRunner::CustomCommandSleep(std::vector<std::string> params) {
	if (params.size() < 2) {
		return;
	}

	std::string &time = params[1];
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

void ConsoleCommandRunner::CustomCommandSwitchWindow(std::vector<std::string> params) {
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
		Log::info("Cannot find windows with title/executable: " + windowTitle);
	}
}
// trycast <spellid> <slot>
void ConsoleCommandRunner::CustomCommandTryCast(std::vector<std::string> params) {

	if (params.size() != 3) {
		Log::error("trycast: Expecting 2 parameters got " + std::to_string(params.size()));
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
		Log::info("Spell is " + Utils::inspect(spell));
	}

	std::string slotName = params[2];
	SInt32 slotId = Utils::getSlot(slotName);
	if (slotId == kSlotId_Null) {
		Log::error("trycast: Invalid slot '" + slotName + "'");
		return;
	}

	// The item previously inhabiting the slot
	TESForm* saveSlotItem = Utils::getEquippedSlot(player, slotId);

	Log::info("Current item in slot " + slotName + " is " + Utils::inspect(saveSlotItem));

	// TODO: Disable animation
	RunCommand("player.equipspell " + Utils::fmt_hex(spell->formID) + " " + slotName);

	Sleep(50); // Give time to switch spell

	Log::info("Spell cast time=" + std::to_string(spell->data.castTime));

	// Extra time since even insta-cast spells need a second or two to work
	const int CAST_TIME_MODIFIER = 2000;

	// Milliseconds to hold the button down to cast
	int castTime = (int)(spell->data.castTime * 1000) + CAST_TIME_MODIFIER;

	if (slotId == kSlotId_Left) {
		TryRunCustomCommand("press rightclick " + std::to_string(castTime));
	}
	else if (slotId == kSlotId_Right) {
		TryRunCustomCommand("press leftclick " + std::to_string(castTime));
	}
	else if (slotId == kSlotId_Voice) {
		TryRunCustomCommand("press z " + std::to_string(castTime));
	}

	Sleep(1000);

	if (saveSlotItem != NULL) {
		Log::debug("Restoring old state: " + Utils::inspect(saveSlotItem));
		switch (saveSlotItem->formType) {
		case FormType::kFormType_Spell:
			RunCommand("player.equipspell " + Utils::fmt_hex(saveSlotItem->formID) + " " + slotName);
			break;
		case FormType::kFormType_Shout:
			RunCommand("player.equipshout " + Utils::fmt_hex(saveSlotItem->formID));
			break;
		case FormType::kFormType_Weapon:
			// TODO: Get the right instance of the item rather than the first matching
			executeOnGameThread(std::bind(papyrusActor::EquipItemEx, player, saveSlotItem, slotId, false, false), true);
			break;
		default:
			Log::error("trycast: Don't know how to restore previous state: " + Utils::inspect(saveSlotItem));
		}
	}
	else {
		//TODO: Clear equipped spell
	}

	Log::debug("trycast exits");
}