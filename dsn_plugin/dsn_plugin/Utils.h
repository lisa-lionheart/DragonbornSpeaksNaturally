#pragma once
#include <string>
#include <vector>

class TESForm;
class Actor;
class SpellItem;
class BaseExtraList;

enum
{
	kSlotId_Null = -1,
	kSlotId_Default = 0, 
	kSlotId_Right = 1,
	kSlotId_Left = 2,

	kSlotId_Voice = kSlotId_Default
};


namespace Utils
{
	using std::string;
	using std::vector;

	// Format integral values as hex
	string fmt_hex(UInt32 val);
	string fmt_hex(UInt64 val);

	string fmt_flags(uint32_t val);


	string inspect(TESForm* form);


	SInt32 getSlot(string name);

	// Split a string
	vector<string> split(const string& s, char delim);

	/*
	* Get the currently equipped item or spell for this actors slot
	* slot = "left","right","voice"
	*/
	TESForm* getEquippedSlot(Actor* actor, SInt32 slotId);

	/*
	* Look in a players spell list and find the specified spell if the know it
	*/
	SpellItem* resolvePlayerSpell(Actor* actor, string spellid);


	SInt32 fmCalcItemId(TESForm* form, BaseExtraList* extraList);

	bool IsEquipmentSingleHanded(TESForm* item);
};


