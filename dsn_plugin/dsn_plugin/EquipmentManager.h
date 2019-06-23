#pragma once

#include "SpeechRecognitionClient.h"

#include <string>
#include <vector>

class ConsoleCommandRunner;
class TESSpellList;
class SpeechRecognitionClient;
class InventoryEntryData;
class PlayerCharacter;

class EquipmentManager
{

	struct FavoriteMenuItem {
		UInt32 TESFormId;
		SInt32 itemId;
		std::string fullname;
		UInt8 itemType;
		bool isHanded;	// True if user must specify "left" or "right" in equip commands
	};

	struct EquipItem {
		UInt32 TESFormId;
		UInt32 itemId;
		UInt8 itemType;
		SInt32 hand; // 0 = no hand specified, 1 = right hand, 2 = left hand
	};

public:
	EquipmentManager(SpeechRecognitionClient* client, ConsoleCommandRunner* console);
	void RefreshAll();

	// Handle request from the service to equip an item
	void ProcessEquipCommand(const TokenList& tokens);
private:

	std::vector<FavoriteMenuItem> ExtractFavorites(InventoryEntryData* inv);
	EquipItem parseEquipItem(std::string command);

	// Scan the players favourited items and tell the service
	void RefreshFavorites(PlayerCharacter* player);
	
	// Scan the players known spells and update the service
	void RefreshSpellBook(PlayerCharacter* player);

	// Scan the players known words of powers and update the service
	void RefreshShouts(TESSpellList& spells);

	SpeechRecognitionClient* client;
	ConsoleCommandRunner* console;
	
	std::vector<FavoriteMenuItem> favorites;
	std::string lastFavoritesCommand;
	std::string lastSpellBookCommand;
	std::string lastShoutsCommand;
};