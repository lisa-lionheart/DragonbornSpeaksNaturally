#pragma once

#include <string>
#include <vector>
#include "common/IPrefix.h"
#include "skse64/GameTypes.h"

struct FakeMagicFavorites {
	UInt64 vtable;
	UInt64			unk008;		// 08
	UnkFormArray	spells;		// 10
	UnkFormArray	hotkeys;	// 28
};

class TESSpellList;

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

class FavoritesMenuManager
{
	static FavoritesMenuManager* instance;

public:
	static FavoritesMenuManager* getInstance();

	void RefreshAll();

	// Handle request from the service to equip an item
	void ProcessEquipCommand(std::string equipStr);
private:

	// Scan the players favourited items and tell the service
	void RefreshFavorites(PlayerCharacter* player);
	
	// Scan the players known spells and update the service
	void RefreshSpellBook(PlayerCharacter* player);

	// Scan the players known words of powers and update the service
	void RefreshShouts(TESSpellList& spells);


	FavoritesMenuManager();
	std::vector<FavoriteMenuItem> favorites;
	std::string lastFavoritesCommand;
	std::string lastSpellBookCommand;
	std::string lastShoutsCommand;
};