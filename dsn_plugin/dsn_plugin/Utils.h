#pragma once
#include <string>

class TESForm;
class Actor;
class SpellItem;


enum
{
	kSlotId_Null = -1,
	kSlotId_Default = 0, 
	kSlotId_Right = 1,
	kSlotId_Left = 2,

	kSlotId_Voice = kSlotId_Default
};


class Utils
{

public:
	static std::string fmt_hex(uint32_t val);

	static std::string fmt_flags(uint32_t val);


	static std::string inspect(TESForm* form);


	static SInt32 getSlot(std::string name);

	/*
	* Get the currently equipped item or spell for this actors slot
	* slot = "left","right","voice"
	*/
	static TESForm* getEquippedSlot(Actor* actor, SInt32 slotId);

	/*
	* Look in a players spell list and find the specified spell if the know it
	*/
	static SpellItem* resolvePlayerSpell(Actor* actor, std::string spellid);
};

