#include "common/IPrefix.h"
#include "SkyrimType.h"
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
#include <string>
#include <vector>

#include "Log.h"
#include "Equipper.h"
#include "SpeechRecognitionClient.h"
#include "EquipmentManager.h"
#include "ConsoleCommandRunner.h"
#include "Threading.h"
#include "Utils.h"
#include "ThreadAsserts.h"


using std::string;
using std::vector;
using std::to_string;
using namespace Utils;

struct FakeMagicFavorites {
	UInt64 vtable;
	UInt64			unk008;		// 08
	UnkFormArray	spells;		// 10
	UnkFormArray	hotkeys;	// 28
};

EquipmentManager::EquipmentManager(SpeechRecognitionClient* _client, ConsoleCommandRunner* _console) {
	TRACE_METHOD();
	console = _console;
	client = _client;
	client->RegisterHandler(this, "EQUIP", &EquipmentManager::ProcessEquipCommand, true);
}


vector<EquipmentManager::FavoriteMenuItem> EquipmentManager::ExtractFavorites(InventoryEntryData * inv) {
	vector<FavoriteMenuItem> menuItems;
	ExtendDataList* pExtendList = inv->extendDataList;

	if (pExtendList)
	{
		bool isHanded = IsEquipmentSingleHanded(inv->type);
		TESFullName *baseFullname = DYNAMIC_CAST(inv->type, TESForm, TESFullName);
		string baseName = string(baseFullname->name.data);
		SInt32 n = 0;
		BaseExtraList* itemExtraDataList = pExtendList->GetNthItem(n);
		while (itemExtraDataList)
		{
			// Check if item is hotkeyed
			if (ExtraHotkey * extraHotkey = static_cast<ExtraHotkey*>(itemExtraDataList->GetByType(kExtraData_Hotkey)))
			{
				string name = baseName;
				const char* modifiedName = itemExtraDataList->GetDisplayName(inv->type);
				if (modifiedName) {
					name = string(modifiedName);
				}
				SInt32 itemId = fmCalcItemId(inv->type, itemExtraDataList);

				if (ExtraTextDisplayData * textDisplayData = static_cast<ExtraTextDisplayData*>(itemExtraDataList->GetByType(kExtraData_TextDisplayData)))
				{
					if (textDisplayData->name.data) {
						name = string(textDisplayData->name.data);
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

void EquipmentManager::RefreshAll() {
	TRACE_METHOD();
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
	}
}

void EquipmentManager::RefreshSpellBook(PlayerCharacter* player) {
	TRACE_METHOD();

	PlayerCharacter::SpellArray& spells = player->addedSpells;

	Log::info("Refreshing player spell book");

	string command = "SPELLBOOK|";

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
			Log::debug("Adding: %s type: %s flags: %s", spell->fullName.name.data, typeString, fmt_flags(spell->data.unk00.flags).c_str());
			command += fmt_hex(spell->formID) + "," + typeString;
			command += "," + string(spell->fullName.GetName());
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
			Log::debug("Ignoring: %s", spell->fullName.GetName());
		}
	}

	if (lastSpellBookCommand != command) {
		client->WriteLine(command);
		lastSpellBookCommand = command;
	}	
}

/*
* Sends shout data if has changed
* Format SHOUTS|<formid>,<name>,<WOOP1>,[<WOOP2>,[<WOOP3>]]|....
*/
void EquipmentManager::RefreshShouts(TESSpellList& spellList) {
	TRACE_METHOD();
	
	string command = "SHOUTS|";

	for (UInt32 i = 0; i < spellList.GetShoutCount(); i++) {
		TESShout* shout = spellList.GetNthShout(i);
		string name(shout->fullName.name.data);

		Log::debug("Adding shout: %s", name.c_str());
		command += fmt_hex(shout->formID) + "," + name;

		for (int j = 0; j < TESShout::Words::kNumWords; j++) {
			TESWordOfPower* word = shout->words[j].word;
			if ((word->flags & TESForm::kFlagPlayerKnows) == TESForm::kFlagPlayerKnows) {
				string wordName(word->fullName.GetName());
				command += "," + wordName;
			}
		}
		command += "|";
	}

	if (lastShoutsCommand != command) {
		client->WriteLine(command);
		lastShoutsCommand = command;
	}
}

void EquipmentManager::RefreshFavorites(PlayerCharacter* player) {
	TRACE_METHOD();

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
					vector<FavoriteMenuItem> itemEntries = ExtractFavorites(inv);
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
						string(spellItem->fullName.name.data),
						2, // Spell
						true };
					favorites.push_back(entry);
				}

				TESShout *shout = DYNAMIC_CAST(spellForm, TESForm, TESShout);
				if (shout) {

					FavoriteMenuItem entry = {
					shout->formID,
					0,
					string(shout->fullName.name.data),
					3, // Shout
					false };

					favorites.push_back(entry);
				}
			}
		}
	}

	string command = "FAVORITES";
	for (int i = 0; i < favorites.size(); i++) {
		FavoriteMenuItem favorite = favorites[i];
		command += "|" + favorite.fullname + "," + to_string(favorite.TESFormId) + "," + to_string(favorite.itemId) + "," + to_string(favorite.isHanded) + "," + to_string(favorite.itemType);
	}

	if (lastFavoritesCommand != command) {
		client->WriteLine(command);
		lastFavoritesCommand = command;
	}
}

EquipmentManager::EquipItem EquipmentManager::parseEquipItem(string command) {
	vector<string> tokens = split(command, ';');

	char* formId = (char*)tokens[0].c_str();
	char* end = formId + strlen(formId);

	EquipItem item = {
		(UInt32)strtoul(formId, &end, 10),
		(SInt32)atoi(tokens[1].c_str()),
		(UInt8)atoi(tokens[2].c_str()),
		(SInt32)atoi(tokens[3].c_str())
	};

	return item;
}



void EquipmentManager::ProcessEquipCommand(const TokenList& tokens) {
	
	ASSERT_IS_GAME_THREAD();
	string equipStr = tokens[1];

	PlayerCharacter *player = (*g_thePlayer);
	EquipManager *equipManager = EquipManager::GetSingleton();
	
	if (player && equipManager && equipStr != "") {
		EquipItem equipItem = parseEquipItem(equipStr);
		TESForm * form = LookupFormByID(equipItem.TESFormId);
		string hand = equipItem.hand == 1 ? "right" : "left";
		if (form) {
			switch (equipItem.itemType) {
			case 1: // Item
				Equipper::EquipItem(player, form, equipItem.itemId, equipItem.hand);
				break;
			case 2: // Spell
				if (equipItem.hand == 0) {
					console->RunVanillaCommand("player.equipspell " + fmt_hex(equipItem.itemId) + " left");
					console->RunVanillaCommand("player.equipspell " + fmt_hex(equipItem.itemId) + " right");
				} else {
					console->RunVanillaCommand("player.equipspell " + fmt_hex(equipItem.itemId) + " " + hand);
				}
				break;
			case 3: // Shout
				console->RunVanillaCommand("player.equipshout " + fmt_hex(equipItem.itemId));
				PlayerControls * controls = PlayerControls::GetSingleton();
				break;
			}
		}

	}
}



