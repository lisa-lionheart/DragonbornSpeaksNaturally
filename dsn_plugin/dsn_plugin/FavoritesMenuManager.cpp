#include "FavoritesMenuManager.h"
#include "Log.h"
#include "SkyrimType.h"
#include "Equipper.h"
#include "SpeechRecognitionClient.h"
#include "skse64/GameAPI.h"
#include "skse64/GameRTTI.h"
#include "skse64/GameData.h"
#include "skse64/GameExtraData.h"
#include "skse64/GameTypes.h"
#include "skse64/PapyrusActor.h"
#include "skse64/GameInput.h"
#include "skse64/HashUtil.h"

#include <sstream>
#include <fstream>

#include "Utils.h"

FavoritesMenuManager* FavoritesMenuManager::instance = NULL;

FavoritesMenuManager* FavoritesMenuManager::getInstance() {
	if (!instance)
		instance = new FavoritesMenuManager();
	return instance;
}

FavoritesMenuManager::FavoritesMenuManager(){}


SInt32 fmCalcItemId(TESForm * form, BaseExtraList * extraList)
{
	if (!form || !extraList)
		return 0;

	const char * name = extraList->GetDisplayName(form);

	// No name in extra data? Use base form name
	if (!name)
	{
		TESFullName* pFullName = DYNAMIC_CAST(form, TESForm, TESFullName);
		if (pFullName)
			name = pFullName->name.data;
	}

	if (!name)
		return 0;

	return (SInt32)HashUtil::CRC32(name, form->formID & 0x00FFFFFF);
}

bool IsEquipmentSingleHanded(TESForm *item) {
	BGSEquipType * equipType = DYNAMIC_CAST(item, TESForm, BGSEquipType);
	if (!equipType)
		return false;

	BGSEquipSlot * equipSlot = equipType->GetEquipSlot();
	if (!equipSlot)
		return false;

	return (equipSlot == GetLeftHandSlot() || equipSlot == GetRightHandSlot());
}

std::vector<FavoriteMenuItem> ExtractFavorites(InventoryEntryData * inv) {
	std::vector<FavoriteMenuItem> menuItems;
	ExtendDataList* pExtendList = inv->extendDataList;

	if (pExtendList)
	{
		bool isHanded = IsEquipmentSingleHanded(inv->type);
		TESFullName *baseFullname = DYNAMIC_CAST(inv->type, TESForm, TESFullName);
		std::string baseName = std::string(baseFullname->name.data);
		SInt32 n = 0;
		BaseExtraList* itemExtraDataList = pExtendList->GetNthItem(n);
		while (itemExtraDataList)
		{
			// Check if item is hotkeyed
			if (ExtraHotkey * extraHotkey = static_cast<ExtraHotkey*>(itemExtraDataList->GetByType(kExtraData_Hotkey)))
			{
				std::string name = baseName;
				const char* modifiedName = itemExtraDataList->GetDisplayName(inv->type);
				if (modifiedName) {
					name = std::string(modifiedName);
				}
				SInt32 itemId = fmCalcItemId(inv->type, itemExtraDataList);

				if (ExtraTextDisplayData * textDisplayData = static_cast<ExtraTextDisplayData*>(itemExtraDataList->GetByType(kExtraData_TextDisplayData)))
				{
					if (textDisplayData->name.data) {
						name = std::string(textDisplayData->name.data);
						const char* displayName = itemExtraDataList->GetDisplayName(inv->type);
						itemId = (SInt32)HashUtil::CRC32(displayName, inv->type->formID & 0x00FFFFFF);
					}
				}

				FavoriteMenuItem entry = {
					inv->type->formID,
					itemId,
					name,
					1, // Equipment
					isHanded };

				menuItems.push_back(entry);
			}
			n++;
			itemExtraDataList = pExtendList->GetNthItem(n);
		}
	}

	return menuItems;
}

void FavoritesMenuManager::RefreshAll() {
	ASSERT_IS_GAME_THREAD();

	PlayerCharacter* player = (*g_thePlayer);
	if (player != NULL) {
		RefreshFavorites(player);
		RefreshSpellBook(player);

		TESActorBase* actorBase = DYNAMIC_CAST(player->baseForm, TESForm, TESActorBase);
		if (actorBase == NULL) {
			Log::error("Player is not TESActorBase? WTF?");
		} else {
			RefreshShouts(actorBase->spellList);
		}


		Log::debug("Left hand slot: " + Utils::inspect(Utils::getEquippedSlot(player, kSlotId_Left)));
		Log::debug("Right hand slot: " + Utils::inspect(Utils::getEquippedSlot(player, kSlotId_Right)));
		Log::debug("Voice slot: " + Utils::inspect(Utils::getEquippedSlot(player, kSlotId_Voice)));
	}
}

void FavoritesMenuManager::RefreshSpellBook(PlayerCharacter* player) {

	PlayerCharacter::SpellArray& spells = player->addedSpells;

	Log::info("Refreshing player spell book");

	std::string command = "SPELLBOOK|";

	for (UInt32 i = 0; i < spells.Length() ; i++) {
		SpellItem* spell = spells.Get(i);
		const char* typeString = NULL;
		switch (spell->data.type) {

		case SpellItem::kTypeSpell:
			typeString = "SPELL";
			break;
		case SpellItem::kTypePower:
		case SpellItem::kTypeLesserPower:
			typeString = "POWER";
			break;
		}

		if (typeString != NULL) {
			Log::debug("Adding: " + std::string(spell->fullName.name.data) + " type: " + typeString + " flags: " + Utils::fmt_flags(spell->data.unk00.flags));
			command += Utils::fmt_hex(spell->formID) + "," + typeString;
			command += "," + std::string(spell->fullName.GetName());
			switch (spell->data.castType)
			{
			case 1:
				command += ",SINGLE";
				break;
			case 2:
				command += ",CONTINOUS";
				break;
			default:
				command += ",UNKNOWN";
			}
			command += "|";
		} else {
			Log::debug("Ignoring: " + std::string(spell->fullName.GetName()));
		}
	}

	if (lastSpellBookCommand != command) {
		Log::info(command);
		SpeechRecognitionClient::getInstance()->WriteLine(command);
		lastSpellBookCommand = command;
	}	
}

/*
* Sends shout data if has changed
* Format SHOUTS|<formid>,<name>,<WOOP1>,[<WOOP2>,[<WOOP3>]]|....
*/
void FavoritesMenuManager::RefreshShouts(TESSpellList& spellList) {

	Log::info("Refreshing player shouts");

	std::string command = "SHOUTS|";

	for (UInt32 i = 0; i < spellList.GetShoutCount(); i++) {
		TESShout* shout = spellList.GetNthShout(i);
		std::string name(shout->fullName.name.data);

		Log::debug("Adding shout: " + name);
		command += Utils::fmt_hex(shout->formID) + "," + name;

		for (int j = 0; j < TESShout::Words::kNumWords; j++) {
			TESWordOfPower* word = shout->words[j].word;
			if ((word->flags & TESForm::kFlagPlayerKnows) == TESForm::kFlagPlayerKnows) {
				std::string wordName(word->fullName.GetName());
				command += "," + wordName;
			}
		}
		command += "|";
	}

	if (lastShoutsCommand != command) {
		Log::info(command);
		SpeechRecognitionClient::getInstance()->WriteLine(command);
		lastShoutsCommand = command;
	}
}

void FavoritesMenuManager::RefreshFavorites(PlayerCharacter* player) {

		// Clear current favorites
		favorites.clear();

		// Equipment
		ExtraContainerChanges* pContainerChanges = static_cast<ExtraContainerChanges*>(player->extraData.GetByType(kExtraData_ContainerChanges));
		if (pContainerChanges && pContainerChanges->data) {
			for (EntryDataList::Iterator it = pContainerChanges->data->objList->Begin(); !it.End(); ++it)
			{
				InventoryEntryData *inv = it.Get();
				TESForm* type = inv->type;
				if (type) {
					HotkeyData data = pContainerChanges->FindHotkey(type);
					if (data.pHotkey)
					{
						std::vector<FavoriteMenuItem> itemEntries = ExtractFavorites(inv);
						for (int i = 0; i < itemEntries.size(); i++) {
							favorites.push_back(itemEntries[i]);
						}
					}
				}
			}
		}

		// Spells/Shouts
		FakeMagicFavorites * magicFavorites = (FakeMagicFavorites*)MagicFavorites::GetSingleton();
		if (magicFavorites) {
			UnkFormArray spellArray = magicFavorites->spells;
			for (int i = 0; i < spellArray.count; i++) {
				TESForm* spellForm = spellArray.entries[i];
				if (spellForm) {
					SpellItem *spellItem = DYNAMIC_CAST(spellForm, TESForm, SpellItem);
					if (spellItem) {
						FavoriteMenuItem entry = {
							spellForm->formID,
							0,
							std::string(spellItem->fullName.name.data),
							2, // Spell
							true };
						favorites.push_back(entry);
					}

					TESShout *shout = DYNAMIC_CAST(spellForm, TESForm, TESShout);
					if (shout) {

						FavoriteMenuItem entry = {
						shout->formID,
						0,
						std::string(shout->fullName.name.data),
						3, // Shout
						false };

						favorites.push_back(entry);
					}
				}
			}
		}

		std::string command = "FAVORITES";
		for (int i = 0; i < favorites.size(); i++) {
			FavoriteMenuItem favorite = favorites[i];
			command += "|" + favorite.fullname + "," + std::to_string(favorite.TESFormId) + "," + std::to_string(favorite.itemId) + "," + std::to_string(favorite.isHanded) + "," + std::to_string(favorite.itemType);
		}

		if (lastFavoritesCommand != command) {
			SpeechRecognitionClient::getInstance()->WriteLine(command);
			lastFavoritesCommand = command;
		}
	}


static std::vector<std::string> split(const std::string &s, char delim) {
	std::stringstream ss(s);
	std::string item;
	std::vector<std::string> tokens;
	while (std::getline(ss, item, delim)) {
		tokens.push_back(item);
	}
	return tokens;
}

EquipItem parseEquipItem(std::string command) {
	std::vector<std::string> tokens = split(command, ';');

	char* formId = (char*)tokens[0].c_str();
	char* end = formId + strlen(formId);

	EquipItem item = {
		(UInt32)std::strtoul(formId, &end, 10),
		(SInt32)std::atoi(tokens[1].c_str()),
		(UInt8)std::atoi(tokens[2].c_str()),
		(SInt32)std::atoi(tokens[3].c_str())
	};

	return item;
}



void FavoritesMenuManager::ProcessEquipCommands() {

	ASSERT_IS_GAME_THREAD();

	PlayerCharacter *player = (*g_thePlayer);
	SpeechRecognitionClient *client = SpeechRecognitionClient::getInstance();
	EquipManager *equipManager = EquipManager::GetSingleton();
	std::string equipStr = client->PopEquip();
	if (player && equipManager && equipStr != "") {
		EquipItem equipItem = parseEquipItem(equipStr);
		TESForm * form = LookupFormByID(equipItem.TESFormId);
		std::string hand = equipItem.hand == 1 ? "right" : "left";
		if (form) {
			switch (equipItem.itemType) {
			case 1: // Item
				Equipper::EquipItem(player, form, equipItem.itemId, equipItem.hand);
				break;
			case 2: // Spell
				if (equipItem.hand == 0) {
					client->EnqueueCommand("player.equipspell " + Utils::fmt_hex(equipItem.itemId) + " left");
					client->EnqueueCommand("player.equipspell " + Utils::fmt_hex(equipItem.itemId) + " right");
				} else {
					client->EnqueueCommand("player.equipspell " + Utils::fmt_hex(equipItem.itemId) + " " + hand);
				}
				break;
			case 3: // Shout
				client->EnqueueCommand("player.equipshout " + Utils::fmt_hex(equipItem.itemId));
				PlayerControls * controls = PlayerControls::GetSingleton();
				break;
			}
		}

	}
}



