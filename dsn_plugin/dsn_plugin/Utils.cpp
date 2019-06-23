
#include "skse64/GameAPI.h"
#include "skse64/GameRTTI.h"
#include "skse64/GameData.h"
#include "Utils.h"
#include "Hooks.h"


using std::string;
using std::vector;
using std::stringstream;



namespace Utils {
	const char* formTypeNames[];


	string fmt_hex(UInt32 val) {
		char buffer[9];
		sprintf_s(buffer, "%08X", val);
		return buffer;
	}

	string fmt_hex(UInt64 val) {
		char buffer[17];
		sprintf_s(buffer, "%016I64X", val);
		return buffer;
	}


	string fmt_flags(uint32_t n) {
		string r;
		while (n != 0) {
			r = (n % 2 == 0 ? "0" : "1") + r;
			n /= 2;
		}
		return r;
	}

	string inspect(TESForm* form) {
		if (!form)
			return "NULL";

		string data = "TESForm(";
		data += "id=" + fmt_hex(form->formID);
		data += ",type=";
		data += formTypeNames[form->formType];

		TESFullName* fullname = DYNAMIC_CAST(form, TESForm, TESFullName);
		if (fullname != NULL) {
			data += ",fullname=";
			data += fullname->name.data;
		}
		data += ")";
		return data;
	}


	SInt32 getSlot(string name) {
		if (name.find("left") != -1) {
			return kSlotId_Left;
		}

		if (name.find("right") != -1) {
			return kSlotId_Right;
		}

		if (name.find("voice") != -1) {
			return kSlotId_Voice;
		}

		return kSlotId_Null;
	}

	TESForm* getEquippedSlot(Actor* actor, SInt32 slot) {
		TESForm* currentItemOrSpell = NULL;
		if (slot == kSlotId_Voice) {
			currentItemOrSpell = actor->equippedShout;
		}
		else if (slot == kSlotId_Right) {
			currentItemOrSpell = actor->GetEquippedObject(false);
			//if (!currentItemOrSpell) {
			//	currentItemOrSpell = actor->rightHandSpell;
			//}
		}
		else if (slot == kSlotId_Left) {
			currentItemOrSpell = actor->GetEquippedObject(true);
			//if (!currentItemOrSpell) {
			//	currentItemOrSpell = actor->leftHandSpell;
			//}
		}
		return currentItemOrSpell;
	}

	SpellItem* resolvePlayerSpell(Actor* actor, string spellId) {
		NULL;
		for (int i = 0; i < actor->addedSpells.Length(); i++) {
			SpellItem* maybe = actor->addedSpells.Get(i);
			if (fmt_hex(maybe->formID) == spellId) {
				return maybe;
			}
		}
		return NULL;
	}

	vector<string> split(const string& s, char delim) {
		stringstream ss(s);
		string item;
		vector<string> tokens;
		while (getline(ss, item, delim)) {
			tokens.push_back(item);
		}
		return tokens;
	}


	SInt32 fmCalcItemId(TESForm* form, BaseExtraList* extraList)
	{
		if (!form || !extraList)
			return 0;

		const char* name = extraList->GetDisplayName(form);

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

	bool IsEquipmentSingleHanded(TESForm* item) {
		BGSEquipType* equipType = DYNAMIC_CAST(item, TESForm, BGSEquipType);
		if (!equipType)
			return false;

		BGSEquipSlot* equipSlot = equipType->GetEquipSlot();
		if (!equipSlot)
			return false;

		return (equipSlot == GetLeftHandSlot() || equipSlot == GetRightHandSlot());
	}

	const char* formTypeNames[] = {
		"kFormType_None",
		"kFormType_TES4",
		"kFormType_Group",
		"kFormType_GMST",
		"kFormType_Keyword",
		"kFormType_LocationRef",
		"kFormType_Action",
		"kFormType_TextureSet",
		"kFormType_MenuIcon",
		"kFormType_Global",
		"kFormType_Class",
		"kFormType_Faction",
		"kFormType_HeadPart",
		"kFormType_Eyes",
		"kFormType_Race",
		"kFormType_Sound",
		"kFormType_AcousticSpace",
		"kFormType_Skill",
		"kFormType_EffectSetting",
		"kFormType_Script",
		"kFormType_LandTexture",
		"kFormType_Enchantment",
		"kFormType_Spell",
		"kFormType_ScrollItem",
		"kFormType_Activator",
		"kFormType_TalkingActivator",
		"kFormType_Armor",
		"kFormType_Book",
		"kFormType_Container",
		"kFormType_Door",
		"kFormType_Ingredient",
		"kFormType_Light",
		"kFormType_Misc",
		"kFormType_Apparatus",
		"kFormType_Static",
		"kFormType_StaticCollection",
		"kFormType_MovableStatic",
		"kFormType_Grass",
		"kFormType_Tree",
		"kFormType_Flora",
		"kFormType_Furniture",
		"kFormType_Weapon",
		"kFormType_Ammo",
		"kFormType_NPC",
		"kFormType_LeveledCharacter",
		"kFormType_Key",
		"kFormType_Potion",
		"kFormType_IdleMarker",
		"kFormType_Note",
		"kFormType_ConstructibleObject",
		"kFormType_Projectile",
		"kFormType_Hazard",
		"kFormType_SoulGem",
		"kFormType_LeveledItem",
		"kFormType_Weather",
		"kFormType_Climate",
		"kFormType_SPGD",
		"kFormType_ReferenceEffect",
		"kFormType_Region",
		"kFormType_NAVI",
		"kFormType_Cell",
		"kFormType_Reference",
		"kFormType_Character",
		"kFormType_Missile",
		"kFormType_Arrow",
		"kFormType_Grenade",
		"kFormType_BeamProj",
		"kFormType_FlameProj",
		"kFormType_ConeProj",
		"kFormType_BarrierProj",
		"kFormType_PHZD",
		"kFormType_WorldSpace",
		"kFormType_Land",
		"kFormType_NAVM",
		"kFormType_TLOD",
		"kFormType_Topic",
		"kFormType_TopicInfo",
		"kFormType_Quest",
		"kFormType_Idle",
		"kFormType_Package",
		"kFormType_CombatStyle",
		"kFormType_LoadScreen",
		"kFormType_LeveledSpell",
		"kFormType_ANIO",
		"kFormType_Water",
		"kFormType_EffectShader",
		"kFormType_TOFT",
		"kFormType_Explosion",
		"kFormType_Debris",
		"kFormType_ImageSpace",
		"kFormType_ImageSpaceMod",
		"kFormType_List",
		"kFormType_Perk",
		"kFormType_BodyPartData",
		"kFormType_AddonNode",
		"kFormType_ActorValueInfo",
		"kFormType_CameraShot",
		"kFormType_CameraPath",
		"kFormType_VoiceType",
		"kFormType_MaterialType",
		"kFormType_ImpactData",
		"kFormType_ImpactDataSet",
		"kFormType_ARMA",
		"kFormType_EncounterZone",
		"kFormType_Location",
		"kFormType_Message",
		"kFormType_Ragdoll",
		"kFormType_DOBJ",
		"kFormType_LightingTemplate",
		"kFormType_MusicType",
		"kFormType_Footstep",
		"kFormType_FootstepSet",
		"kFormType_StoryBranchNode",
		"kFormType_StoryQuestNode",
		"kFormType_StoryEventNode",
		"kFormType_DialogueBranch",
		"kFormType_MusicTrack",
		"kFormType_DLVW",
		"kFormType_WordOfPower",
		"kFormType_Shout",
		"kFormType_EquipSlot",
		"kFormType_Relationship",
		"kFormType_Scene",
		"kFormType_AssociationType",
		"kFormType_Outfit",
		"kFormType_Art",
		"kFormType_Material",
		"kFormType_MovementType",
		"kFormType_SoundDescriptor",
		"kFormType_DualCastData",
		"kFormType_SoundCategory",
		"kFormType_SoundOutput",
		"kFormType_CollisionLayer",
		"kFormType_ColorForm",
		"kFormType_ReverbParam",
		"kFormType_LensFlare",
		"kFormType_Unk88",
		"kFormType_VolumetricLighting",
		"kFormType_Unk8A",
		"kFormType_Alias",
		"kFormType_ReferenceAlias",
		"kFormType_LocationAlias",
		"kFormType_ActiveMagicEffect"
	};
}