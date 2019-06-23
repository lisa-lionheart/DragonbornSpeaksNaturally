#pragma once
#include "common/IPrefix.h"
#include "skse64/GameMenus.h"
#include <string>
#include <vector>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <functional>
#include <Windows.h>
#include <stack>
#include <map>



class TESForm;
class SpeechRecognitionClient;
typedef std::vector<std::string> TokenList;

class ConsoleCommandRunner
{
public:

	ConsoleCommandRunner(SpeechRecognitionClient*);


	// Run either, will determine which of the further two messages
	// should be called
	//
	// You should not call this from a game thread
	void RunCommand(std::string command);


	// Run a Skyrim console command
	void RunVanillaCommand(std::string command);

private:

	void HandleCommands(const TokenList& tokens);

	// Try run a custom command.
	// Returns true if the command was run successful.
	// Returns false if the command is not a custom command and the caller
	// should add the command to another queue.
	bool TryRunCustomCommand(const std::string& command);

	//
	// Add a new command:
	//         press <key name or DirectInput scan code> [millisecond] ...
	//
	// Description:
	//         Simulate pressing the specified key the specified milliseconds.
	//         Used to cast skills or dragon shouts or do other actions.
	//         If time is omitted, the time will be set as kDefaultKeyPressTime.
    //
	// Tips:
	//      1. If you want to use scan code 0-9, please add the prefix 0x.
	//         A single digit without 0x prefix will be considered a digit key instead of a key code.
	//         Exampke: 0x01 is esc, but 1 is the number key 1.
	//
	//      2. Only the currently active window can receive the simulated key.
	//         Activate the Skyrim window by calling switchwindow command if necessary.
	//
	//      3. Only one key can be represented when the time is omitted.
	//         To tap more keys without time, use the tapkey command.
	//
	//      4. Mouse event is supported. Use the following key names:
	//             leftmousebutton  rightmousebutton  middlemousebutton
	//             mousewheelup     mousewheeldown
	//             mousebutton4     mousebutton5
	//         However, the cursor must be inside the Skyrim window when simulating a mouse click,
	//         otherwise the other window will receive the click event instead of Skyrim.
	//
	//
	// Reference:
	//         Windows DirectInput scan codes:
    //                 https://www.creationkit.com/index.php?title=Input_Script#DXScanCodes
	//         Avaliable key names:
    //                 https://github.com/YihaoPeng/DragonbornSpeaksNaturally/blob/master/dsn_plugin/dsn_plugin/KeyCode.hpp
	//
	// Example:
	//         press m
	//         press Z 1000
	//         press 44 50
	//         press 0x2C 100
	//         press esc
	//
	//         ; Press 3 keys at the same time (ctrl + alt + a):
	//         press  ctrl 500  alt 500  a 400
	//
	//         ; left hand magic
	//         press  leftmousebutton 1000
	//
	void CustomCommandPress(const TokenList& params);

	//
	// Add a new command:
	//         tapkey <key name or DirectInput Scan Code> ...
	//
	// Description:
	//         It's a shortcut to the press command (All pressing time are set to kDefaultKeyPressTime milliseconds).
	//
	// Example:
	//         ; Press 3 keys at the same time (ctrl + alt + a):
	//         tapkey ctrl alt a
	//
	void CustomCommandTapKey(const TokenList& params);

	//
	// Add two new command:
	//         holdkey <key name or DirectInput Scan Code> ...
	//         releasekey <key name or DirectInput Scan Code> ...
	//
	// Description:
	//         Manual control the press and release of keys.
	//
	// Example:
	//         ; typing with shift hold:
	//         tapkey ~; sleep 100; holdkey shift; tapkey t e; tapkey s t; releasekey shift; sleep 100; tapkey t e; tapkey s t
	//
	//         ; casting magic with double hands
	//         holdkey leftmousebutton; sleep 1000; holdkey rightmousebutton; sleep 5000; releasekey leftmousebutton; sleep 3000; releasekey rightmousebutton
	//
	void CustomCommandHoldKey(const TokenList& params);
	void CustomCommandReleaseKey(const TokenList& params);

	//
	// Add a new command:
	//        sleep millisecond
	// 
	// Description:
	//        Delays the subsequent commands the specified milliseconds.
	//        Used to form an automatic action script with the press command.
	//        Both custom commands and Skyrim commands can be delayed.
	// 
	// Example:
	//         sleep 5000
	//         press z; sleep 1000; press LeftMouseButton 2000
	//         press ctrl; sleep 5000; press ctrl
	//
	//         ; Casting two dragon shouts one after another:
	//         player.cast 0003f9ed player voice; sleep 3000; player.cast 00013f3a player voice
	//
	void CustomCommandSleep(const TokenList& params);

	//
	// Add a new command:
	//         switchwindow [window title|executable name]
	// 
	// Description:
	//         Activate the specified window.
	//         Used to switch to the correct window before running the press command.
	//         Omitting the parameters will activate the current Skyrim window.
	//
	// TODO:
	//         Move the mouse cursor to the center of the active window.
	// 
	// Example:
	//         switchwindow
	//         switchwindow Notepad++
	//         switchwindow notepad.exe
	//
	//         ; Activate the Skyrim window and type in the console:
	//         switchwindow; sleep 50; tapkey ~; sleep 50; tapkey s a v e enter; sleep 50; tapkey ~
	//
	void CustomCommandSwitchWindow(const TokenList& params);




	//
	// Add new commands:
	//         pushequip [left|right|voice]
	//         popequip [left|right|voice]
	//
	// Description:
	//         Records the current state of the contents of the specified slot
	//         onto a stack (FILO) you can then pop it back after doing something else
	//         With the players hands
	// Example: Store and cast
	//		   pushequip right
	//         player.equipspell 4564575467 right
	//         sleep 100
	//         press leftmouse 2000
	//         popequip right
	// Example2: Swap hands
	//         pushequip left
	//         pushequip right
	//         popequip left
	//         popequip right
	void CustomCommandPushEquip(const TokenList& params);
	void CustomCommandPopEquip(const TokenList& params);

	//
	// Add a new command:
	//         trycast [spellid] [left|right|voice]
	//
	// Description:
	//         Check the player knows the spell
	//         Store current slot spell/item/shout
	//         Equip specified spell, cast
	//         Then return slot to previous state
	// 
	// Example:
	//			trycast 00012FD0 right ; Shoot a firebolt from the right hand if the player is able
	void CustomCommandTryCast(const TokenList& params);

private:
	
	std::map<std::string, void (ConsoleCommandRunner::*)(const TokenList& tokens)> commands;
	const UInt32 kDefaultKeyPressTime = 50;
	std::stack<TESForm*> equipStack;
	IMenu* consoleMenu = NULL;
};
