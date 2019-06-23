#pragma once
#include<set>
#include "skse64/GameEvents.h"


class MenuManager;
class SpeechRecognitionClient;

/*
* Monitor the state of the game and update the service so that it can choose which
* Activations should be avalible
*/
class GameStateController : public BSTEventSink<MenuOpenCloseEvent> {

	SpeechRecognitionClient* client;
	std::set<std::string> filter;
	MenuManager* mm;

	std::string currentMenu = "None";

	bool isInCombat = false;
	bool isWeaponDrawn = false;
	bool isSneaking = false;


	void SendStateChange();
public:
	GameStateController(SpeechRecognitionClient* client);

	// Monitor opening and closing of menus
	EventResult ReceiveEvent(MenuOpenCloseEvent* evn, EventDispatcher<MenuOpenCloseEvent>* dispatcher) override;

	void UpdatePlayerState();
};