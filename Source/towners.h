/**
 * @file towners.h
 *
 * Interface of functionality for loading and spawning towners.
 */
#pragma once

#include "utils/stdcompat/string_view.hpp"
#include <cstdint>
#include <memory>

#include "items.h"
#include "player.h"
#include "quests.h"
#include "utils/stdcompat/cstddef.hpp"

namespace devilution {

#define NUM_TOWNERS 16

enum class TownerType : uint8_t {
	Smith,
	Healer,
	DeadGuy,
	Tavern,
	StoryTeller,
	Drunk,
	Witch,
	Barmaid,
	PegBoy,
	Cow,
	Farmer,
	Girl,
	CowFarmer,
	LAST = CowFarmer,
};

struct Towner {
	OptionalOwnedClxSpriteList ownedAnim;
	OptionalClxSpriteList anim;
	/** Specifies the animation frame sequence. */
	const uint8_t *animOrder; // unowned
	void (*talk)(Player &player, Towner &towner);

	string_view name;

	/** Tile position of NPC */
	Point position;
	/** Randomly chosen topic for discussion (picked when loading into town) */
	_speech_id gossip;
	uint16_t _tAnimWidth;
	/** Tick length of each frame in the current animation */
	int16_t _tAnimDelay;
	/** Increases by one each game tick, counting how close we are to _pAnimDelay */
	int16_t _tAnimCnt;
	/** Number of frames in current animation */
	uint8_t _tAnimLen;
	/** Current frame of animation. */
	uint8_t _tAnimFrame;
	uint8_t _tAnimFrameCnt;
	uint8_t animOrderSize;
	TownerType _ttype;

	ClxSprite currentSprite() const
	{
		return (*anim)[_tAnimFrame];
	}
};

extern Towner Towners[NUM_TOWNERS];
/**
 * @brief Maps from a TownerType value to a pointer to the Towner object, if they have been initialised
 * @param type enum constant identifying the towner
 * @return Pointer to the Towner or nullptr if they are not available
 */
Towner *GetTowner(TownerType type);

void InitTowners();
void FreeTownerGFX();
void ProcessTowners();
void TalkToTowner(Player &player, int t);

void UpdateGirlAnimAfterQuestComplete();
void UpdateCowFarmerAnimAfterQuestComplete();

#ifdef _DEBUG
bool DebugTalkToTowner(std::string targetName);
#endif

bool HasQuestDialog(TownerType towner, quest_id quest);
_speech_id GetQuestDialog(TownerType towner, quest_id quest);
void SetQuestDialog(TownerType towner, quest_id quest, _speech_id speech);

} // namespace devilution
