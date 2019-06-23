
#include <sstream>

#include "Log.h"
#include "common/IPrefix.h"
#include "skse64/GameInput.h"
#include "skse64/GameTypes.h"
#include "skse64/GameMenus.h"


#include "SpeechRecognitionClient.h"
#include "GameStateController.h"
#include "Hooks.h"

void GameStateController::SendStateChange() {
	TRACE_METHOD();

	std::ostringstream msg;
	msg << "GAMESTATE|";
	msg << currentMenu << "|";
	msg << isInCombat << "|";
	msg << isWeaponDrawn << "|";
	msg << isSneaking;

	client->WriteLine(msg.str());
}

GameStateController::GameStateController(SpeechRecognitionClient* _client) {
	TRACE_METHOD();
	client = _client;

	Hooks::taskQueue.AddPoll(this, &GameStateController::UpdatePlayerState);

	mm = MenuManager::GetSingleton();
	mm->menuOpenCloseEventDispatcher.AddEventSink(this);

	filter.insert("TweenMenu"); // Inbetween menu when you want to open magic, inventory etc
	filter.insert("MapMarkerTest3D"); // The map menu
	filter.insert("StatsMenu");
	filter.insert("FavoritesMenu");
	filter.insert("Journal Menu");
	filter.insert("Inventory Menu");
	filter.insert("Dialogue Menu");
}

// Monitor opening and closing of menus
EventResult GameStateController::ReceiveEvent(MenuOpenCloseEvent* evn, EventDispatcher<MenuOpenCloseEvent>* dispatcher) {
	TRACE_METHOD();

	std::string newState = currentMenu;

	if (evn->opening) {
		std::string menuName(evn->menuName.c_str());
		if (filter.find(menuName) != filter.end()) {
			newState = menuName;
		}
		else {
			Log::debug("Ignored %s", menuName.c_str());
		}
	}
	else {
		std::string menuName(evn->menuName.c_str());
		if (menuName == currentMenu) {
			newState = "None";
		}
		Log::debug("Closed %s", menuName.c_str());
	}

	if (newState != currentMenu) {
		Log::info("Menu State: %s -> %s", currentMenu.c_str(), newState.c_str());
		currentMenu = newState;
		SendStateChange();
	}
	return kEvent_Continue;
}

void GameStateController::UpdatePlayerState() {

	//TRACE_METHOD();

	PlayerCharacter* player = (*g_thePlayer);
	if (player == NULL) {
		return;
	}

	bool change = false;


	// Breaks, makes the players hands become invisible??
	//if (player->IsInCombat() != isInCombat) {
	//	isInCombat = player->IsInCombat();
	//	change = true;
	//} 


	if (player->actorState.IsWeaponDrawn() != isWeaponDrawn) {
		isWeaponDrawn = !!player->actorState.IsWeaponDrawn();
		change = true;
	}

	bool sneak = (player->actorState.flags08 & ActorState::kState_Sneaking) != 0;
	if (sneak != isSneaking) {
		isSneaking = sneak;
		change = true;
	}

	if (change) {
		SendStateChange();
	}
}