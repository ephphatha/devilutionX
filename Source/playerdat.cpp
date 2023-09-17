/**
 * @file playerdat.cpp
 *
 * Implementation of all player data.
 */

#include "playerdat.hpp"

#include <algorithm>
#include <array>
#include <bitset>
#include <charconv>
#include <cstdint>
#include <vector>

#include <expected.hpp>
#include <fmt/format.h>

#include "data/file.hpp"
#include "items.h"
#include "player.h"
#include "textdat.h"
#include "utils/language.h"
#include "utils/static_vector.hpp"

namespace devilution {

namespace {

class ExperienceData {
	/** Specifies the experience point limit of each level. */
	std::vector<uint32_t> levelThresholds;

public:
	uint8_t getMaxLevel() const
	{
		return static_cast<uint8_t>(std::min<size_t>(levelThresholds.size(), std::numeric_limits<uint8_t>::max()));
	}

	DVL_REINITIALIZES void clear()
	{
		levelThresholds.clear();
	}

	[[nodiscard]] uint32_t getThresholdForLevel(unsigned level) const
	{
		if (level > 0)
			return levelThresholds[std::min<unsigned>(level - 1, getMaxLevel())];

		return 0;
	}

	void setThresholdForLevel(unsigned level, uint32_t experience)
	{
		if (level > 0) {
			if (level > levelThresholds.size()) {
				// To avoid ValidatePlayer() resetting players to 0 experience we need to use the maximum possible value here
				// As long as the file has no gaps it'll get initialised properly.
				levelThresholds.resize(level, std::numeric_limits<uint32_t>::max());
			}

			levelThresholds[static_cast<size_t>(level - 1)] = experience;
		}
	}
} ExperienceData;

enum class ExperienceColumn {
	Level,
	Experience,
	LAST = Experience
};

tl::expected<ExperienceColumn, ColumnDefinition::Error> mapExperienceColumnFromName(std::string_view name)
{
	if (name == "Level") {
		return ExperienceColumn::Level;
	}
	if (name == "Experience") {
		return ExperienceColumn::Experience;
	}
	return tl::unexpected { ColumnDefinition::Error::UnknownColumn };
}

void ReloadExperienceData()
{
	constexpr std::string_view filename = "txtdata\\Experience.tsv";
	auto dataFileResult = DataFile::load(filename);
	if (!dataFileResult.has_value()) {
		DataFile::reportFatalError(dataFileResult.error(), filename);
	}
	DataFile &dataFile = dataFileResult.value();

	constexpr unsigned ExpectedColumnCount = enum_size<ExperienceColumn>::value;

	std::array<ColumnDefinition, ExpectedColumnCount> columns;
	auto parseHeaderResult = dataFile.parseHeader<ExperienceColumn>(columns.data(), columns.data() + columns.size(), mapExperienceColumnFromName);

	if (!parseHeaderResult.has_value()) {
		DataFile::reportFatalError(parseHeaderResult.error(), filename);
	}

	ExperienceData.clear();
	for (DataFileRecord record : dataFile) {
		uint8_t level = 0;
		uint32_t experience = 0;
		bool skipRecord = false;

		FieldIterator fieldIt = record.begin();
		FieldIterator endField = record.end();
		for (auto &column : columns) {
			fieldIt += column.skipLength;

			if (fieldIt == endField) {
				DataFile::reportFatalError(DataFile::Error::NotEnoughColumns, filename);
			}

			DataFileField field = *fieldIt;

			switch (static_cast<ExperienceColumn>(column)) {
			case ExperienceColumn::Level: {
				auto parseIntResult = field.parseInt(level);

				if (!parseIntResult.has_value()) {
					if (*field == "MaxLevel") {
						skipRecord = true;
					} else {
						DataFile::reportFatalFieldError(parseIntResult.error(), filename, "Level", field);
					}
				}
			} break;

			case ExperienceColumn::Experience: {
				auto parseIntResult = field.parseInt(experience);

				if (!parseIntResult.has_value()) {
					DataFile::reportFatalFieldError(parseIntResult.error(), filename, "Experience", field);
				}
			} break;

			default:
				break;
			}

			if (skipRecord)
				break;

			++fieldIt;
		}

		if (!skipRecord)
			ExperienceData.setThresholdForLevel(level, experience);
	}
}

/** Contains the data related to each player class. */
std::array<PlayerData, enum_size<HeroClass>::value> PlayersData;

enum class PlayerDataColumn {
	Class,
	BaseStrength,
	BaseMagic,
	BaseDexterity,
	BaseVitality,
	MaximumStrength,
	MaximumMagic,
	MaximumDexterity,
	MaximumVitality,
	LifeAdjustment,
	ManaAdjustment,
	LifePerLevel,
	ManaPerLevel,
	LifePerStat,
	ManaPerStat,
	LifeItemBonus,
	ManaItemBonus,
	LAST = ManaItemBonus
};

tl::expected<PlayerDataColumn, ColumnDefinition::Error> mapPlayerDataColumnFromName(std::string_view name)
{
	if (name == "Class") {
		return PlayerDataColumn::Class;
	}
	if (name == "Base Strength") {
		return PlayerDataColumn::BaseStrength;
	}
	if (name == "Base Magic") {
		return PlayerDataColumn::BaseMagic;
	}
	if (name == "Base Dexterity") {
		return PlayerDataColumn::BaseDexterity;
	}
	if (name == "Base Vitality") {
		return PlayerDataColumn::BaseVitality;
	}
	if (name == "Maximum Strength") {
		return PlayerDataColumn::MaximumStrength;
	}
	if (name == "Maximum Magic") {
		return PlayerDataColumn::MaximumMagic;
	}
	if (name == "Maximum Dexterity") {
		return PlayerDataColumn::MaximumDexterity;
	}
	if (name == "Maximum Vitality") {
		return PlayerDataColumn::MaximumVitality;
	}
	if (name == "Base Life") {
		return PlayerDataColumn::LifeAdjustment;
	}
	if (name == "Base Mana") {
		return PlayerDataColumn::ManaAdjustment;
	}
	if (name == "Life Per Level") {
		return PlayerDataColumn::LifePerLevel;
	}
	if (name == "Mana Per Level") {
		return PlayerDataColumn::ManaPerLevel;
	}
	if (name == "Life Per Player Stat") {
		return PlayerDataColumn::LifePerStat;
	}
	if (name == "Mana Per Player Stat") {
		return PlayerDataColumn::ManaPerStat;
	}
	if (name == "Life Per Item Stat") {
		return PlayerDataColumn::LifeItemBonus;
	}
	if (name == "Mana Per Item Stat") {
		return PlayerDataColumn::ManaItemBonus;
	}
	return tl::unexpected { ColumnDefinition::Error::UnknownColumn };
}

void ReloadPlayerData()
{
	constexpr std::string_view filename = "txtdata\\CharStats.tsv";
	auto dataFileResult = DataFile::load(filename);
	if (!dataFileResult.has_value()) {
		DataFile::reportFatalError(dataFileResult.error(), filename);
	}
	DataFile &dataFile = dataFileResult.value();

	constexpr unsigned ExpectedColumnCount = enum_size<PlayerDataColumn>::value;

	std::array<ColumnDefinition, ExpectedColumnCount> columns;
	auto parseHeaderResult = dataFile.parseHeader<PlayerDataColumn>(columns.data(), columns.data() + columns.size(), mapPlayerDataColumnFromName);

	if (!parseHeaderResult.has_value()) {
		DataFile::reportFatalError(parseHeaderResult.error(), filename);
	}

	for (DataFileRecord record : dataFile) {
		HeroClass clazz = static_cast<HeroClass>(-1);
		PlayerData characterStats {};
		bool skipRecord = false;

		FieldIterator fieldIt = record.begin();
		FieldIterator endField = record.end();
		for (auto &column : columns) {
			fieldIt += column.skipLength;

			if (fieldIt == endField) {
				DataFile::reportFatalError(DataFile::Error::NotEnoughColumns, filename);
			}

			DataFileField field = *fieldIt;

			switch (static_cast<PlayerDataColumn>(column)) {
			case PlayerDataColumn::Class: {
				/* TRANSLATORS: Player Class names */
				if (*field == "Warrior") {
					clazz = HeroClass::Warrior;
					characterStats.className = N_("Warrior");
				} else if (*field == "Rogue") {
					clazz = HeroClass::Rogue;
					characterStats.className = N_("Rogue");
				} else if (*field == "Sorcerer") {
					clazz = HeroClass::Sorcerer;
					characterStats.className = N_("Sorcerer");
				} else if (*field == "Monk") {
					clazz = HeroClass::Monk;
					characterStats.className = N_("Monk");
				} else if (*field == "Bard") {
					clazz = HeroClass::Bard;
					characterStats.className = N_("Bard");
				} else if (*field == "Barbarian") {
					clazz = HeroClass::Barbarian;
					characterStats.className = N_("Barbarian");
				} else if (*field == "Expansion") {
					// Special marker line used in Diablo 2 text files to separate base game classes from expansion classes.
					skipRecord = true;
				} else {
					DataFile::reportFatalFieldError(DataFileField::Error::InvalidValue, filename, "Class", field);
				}
			} break;

			case PlayerDataColumn::BaseStrength: {
				auto parseIntResult = field.parseInt(characterStats.baseStr);
				if (!parseIntResult.has_value()) {
					DataFile::reportFatalFieldError(parseIntResult.error(), filename, "Base Strength", field);
				}
			} break;

			case PlayerDataColumn::BaseMagic: {
				auto parseIntResult = field.parseInt(characterStats.baseMag);
				if (!parseIntResult.has_value()) {
					DataFile::reportFatalFieldError(parseIntResult.error(), filename, "Base Magic", field);
				}
			} break;

			case PlayerDataColumn::BaseDexterity: {
				auto parseIntResult = field.parseInt(characterStats.baseDex);
				if (!parseIntResult.has_value()) {
					DataFile::reportFatalFieldError(parseIntResult.error(), filename, "Base Dexterity", field);
				}
			} break;

			case PlayerDataColumn::BaseVitality: {
				auto parseIntResult = field.parseInt(characterStats.baseVit);
				if (!parseIntResult.has_value()) {
					DataFile::reportFatalFieldError(parseIntResult.error(), filename, "Base Vitality", field);
				}
			} break;

			case PlayerDataColumn::MaximumStrength: {
				auto parseIntResult = field.parseInt(characterStats.maxStr);
				if (!parseIntResult.has_value()) {
					DataFile::reportFatalFieldError(parseIntResult.error(), filename, "Maximum Strength", field);
				}
			} break;

			case PlayerDataColumn::MaximumMagic: {
				auto parseIntResult = field.parseInt(characterStats.maxMag);
				if (!parseIntResult.has_value()) {
					DataFile::reportFatalFieldError(parseIntResult.error(), filename, "Maximum Magic", field);
				}
			} break;

			case PlayerDataColumn::MaximumDexterity: {
				auto parseIntResult = field.parseInt(characterStats.maxDex);
				if (!parseIntResult.has_value()) {
					DataFile::reportFatalFieldError(parseIntResult.error(), filename, "Maximum Dexterity", field);
				}
			} break;

			case PlayerDataColumn::MaximumVitality: {
				auto parseIntResult = field.parseInt(characterStats.maxVit);
				if (!parseIntResult.has_value()) {
					DataFile::reportFatalFieldError(parseIntResult.error(), filename, "Maximum Vitality", field);
				}
			} break;

			case PlayerDataColumn::LifeAdjustment: {
				auto parseIntResult = field.parseFixed6(characterStats.adjLife);
				if (!parseIntResult.has_value()) {
					DataFile::reportFatalFieldError(parseIntResult.error(), filename, "Base Life", field);
				}
			} break;

			case PlayerDataColumn::ManaAdjustment: {
				auto parseIntResult = field.parseFixed6(characterStats.adjMana);
				if (!parseIntResult.has_value()) {
					DataFile::reportFatalFieldError(parseIntResult.error(), filename, "Base Mana", field);
				}
			} break;

			case PlayerDataColumn::LifePerLevel: {
				auto parseIntResult = field.parseFixed6(characterStats.lvlLife);
				if (!parseIntResult.has_value()) {
					DataFile::reportFatalFieldError(parseIntResult.error(), filename, "Life Per Level", field);
				}
			} break;

			case PlayerDataColumn::ManaPerLevel: {
				auto parseIntResult = field.parseFixed6(characterStats.lvlMana);
				if (!parseIntResult.has_value()) {
					DataFile::reportFatalFieldError(parseIntResult.error(), filename, "Mana Per Level", field);
				}
			} break;

			case PlayerDataColumn::LifePerStat: {
				auto parseIntResult = field.parseFixed6(characterStats.chrLife);
				if (!parseIntResult.has_value()) {
					DataFile::reportFatalFieldError(parseIntResult.error(), filename, "Life Per Player Stat", field);
				}
			} break;

			case PlayerDataColumn::ManaPerStat: {
				auto parseIntResult = field.parseFixed6(characterStats.chrMana);
				if (!parseIntResult.has_value()) {
					DataFile::reportFatalFieldError(parseIntResult.error(), filename, "Mana Per Player Stat", field);
				}
			} break;

			case PlayerDataColumn::LifeItemBonus: {
				auto parseIntResult = field.parseFixed6(characterStats.itmLife);
				if (!parseIntResult.has_value()) {
					DataFile::reportFatalFieldError(parseIntResult.error(), filename, "Life Per Item Stat", field);
				}
			} break;

			case PlayerDataColumn::ManaItemBonus: {
				auto parseIntResult = field.parseFixed6(characterStats.itmMana);
				if (!parseIntResult.has_value()) {
					DataFile::reportFatalFieldError(parseIntResult.error(), filename, "Mana Per Item Stat", field);
				}
			} break;

			default:
				break;
			}

			if (skipRecord)
				break;

			++fieldIt;
		}

		if (!skipRecord)
			PlayersData[static_cast<size_t>(clazz)] = characterStats;
	}
}

const PlayerCombatData PlayersCombatData[] = {
	// clang-format off
// HeroClass                 baseToBlock, baseMeleeToHit, baseRangedToHit, baseMagicToHit,
/* HeroClass::Warrior   */ {          30,             70,              60,             50, },
/* HeroClass::Rogue     */ {          20,             50,              70,             50, },
/* HeroClass::Sorcerer  */ {          10,             50,              50,             70, },
/* HeroClass::Monk      */ {          25,             50,              50,             50, },
/* HeroClass::Bard      */ {          25,             50,              60,             60, },
/* HeroClass::Barbarian */ {          30,             50,              50,             50, }
	// clang-format on
};

const std::array<PlayerStartingLoadoutData, enum_size<HeroClass>::value> PlayersStartingLoadoutData { {
	// clang-format off
// HeroClass                 skill,                  spell,             spellLevel,     items[0].diablo,       items[0].hellfire, items[1].diablo,  items[1].hellfire, items[2].diablo, items[2].hellfire, items[3].diablo, items[3].hellfire, items[4].diablo, items[4].hellfire, gold,
/* HeroClass::Warrior   */ { SpellID::ItemRepair,    SpellID::Null,              0, { { { IDI_WARRIOR,         IDI_WARRIOR,    }, { IDI_WARRSHLD,   IDI_WARRSHLD,   }, { IDI_WARRCLUB,  IDI_WARRCLUB,   }, { IDI_HEAL,    IDI_HEAL,  }, { IDI_HEAL,      IDI_HEAL, }, }, },  100, },
/* HeroClass::Rogue     */ { SpellID::TrapDisarm,    SpellID::Null,              0, { { { IDI_ROGUE,           IDI_ROGUE,      }, { IDI_HEAL,       IDI_HEAL,       }, { IDI_HEAL,      IDI_HEAL,       }, { IDI_NONE,    IDI_NONE,  }, { IDI_NONE,      IDI_NONE, }, }, },  100, },
/* HeroClass::Sorcerer  */ { SpellID::StaffRecharge, SpellID::Fireball,          2, { { { IDI_SORCERER_DIABLO, IDI_SORCERER,   }, { IDI_MANA,       IDI_HEAL,       }, { IDI_MANA,      IDI_HEAL,       }, { IDI_NONE,    IDI_NONE,  }, { IDI_NONE,      IDI_NONE, }, }, },  100, },
/* HeroClass::Monk      */ { SpellID::Search,        SpellID::Null,              0, { { { IDI_SHORTSTAFF,      IDI_SHORTSTAFF, }, { IDI_HEAL,       IDI_HEAL,       }, { IDI_HEAL,      IDI_HEAL,       }, { IDI_NONE,    IDI_NONE,  }, { IDI_NONE,      IDI_NONE, }, }, },  100, },
/* HeroClass::Bard      */ { SpellID::Identify,      SpellID::Null,              0, { { { IDI_BARDSWORD,       IDI_BARDSWORD,  }, { IDI_BARDDAGGER, IDI_BARDDAGGER, }, { IDI_HEAL,      IDI_HEAL,       }, { IDI_HEAL,    IDI_HEAL,  }, { IDI_NONE,      IDI_NONE, }, }, },  100, },
/* HeroClass::Barbarian */ { SpellID::Rage,          SpellID::Null,              0, { { { IDI_BARBARIAN,       IDI_BARBARIAN,  }, { IDI_WARRSHLD,   IDI_WARRSHLD,   }, { IDI_HEAL,      IDI_HEAL,       }, { IDI_HEAL,    IDI_HEAL,  }, { IDI_NONE,      IDI_NONE, }, }, },  100, }
	// clang-format on
} };

} // namespace

void LoadPlayerDataFiles()
{
	ReloadExperienceData();
	ReloadPlayerData();
}

uint32_t GetNextExperienceThresholdForLevel(unsigned level)
{
	return ExperienceData.getThresholdForLevel(level);
}

uint8_t GetMaximumCharacterLevel()
{
	return ExperienceData.getMaxLevel();
}

const _sfx_id herosounds[enum_size<HeroClass>::value][enum_size<HeroSpeech>::value] = {
	// clang-format off
	{ PS_WARR1,  PS_WARR2,  PS_WARR3,  PS_WARR4,  PS_WARR5,  PS_WARR6,  PS_WARR7,  PS_WARR8,  PS_WARR9,  PS_WARR10,  PS_WARR11,  PS_WARR12,  PS_WARR13,  PS_WARR14,  PS_WARR15,  PS_WARR16,  PS_WARR17,  PS_WARR18,  PS_WARR19,  PS_WARR20,  PS_WARR21,  PS_WARR22,  PS_WARR23,  PS_WARR24,  PS_WARR25,  PS_WARR26,  PS_WARR27,  PS_WARR28,  PS_WARR29,  PS_WARR30,  PS_WARR31,  PS_WARR32,  PS_WARR33,  PS_WARR34,  PS_WARR35,  PS_WARR36,  PS_WARR37,  PS_WARR38,  PS_WARR39,  PS_WARR40,  PS_WARR41,  PS_WARR42,  PS_WARR43,  PS_WARR44,  PS_WARR45,  PS_WARR46,  PS_WARR47,  PS_WARR48,  PS_WARR49,  PS_WARR50,  PS_WARR51,  PS_WARR52,  PS_WARR53,  PS_WARR54,  PS_WARR55,  PS_WARR56,  PS_WARR57,  PS_WARR58,  PS_WARR59,  PS_WARR60,  PS_WARR61,  PS_WARR62,  PS_WARR63,  PS_WARR64,  PS_WARR65,  PS_WARR66,  PS_WARR67,  PS_WARR68,  PS_WARR69,  PS_WARR70,  PS_WARR71,  PS_WARR72,  PS_WARR73,  PS_WARR74,  PS_WARR75,  PS_WARR76,  PS_WARR77,  PS_WARR78,  PS_WARR79,  PS_WARR80,  PS_WARR81,  PS_WARR82,  PS_WARR83,  PS_WARR84,  PS_WARR85,  PS_WARR86,  PS_WARR87,  PS_WARR88,  PS_WARR89,  PS_WARR90,  PS_WARR91,  PS_WARR92,  PS_WARR93,  PS_WARR94,  PS_WARR95,  PS_WARR96B,  PS_WARR97,  PS_WARR98,  PS_WARR99,  PS_WARR100,  PS_WARR101,  PS_WARR102,  PS_DEAD    },
	{ PS_ROGUE1, PS_ROGUE2, PS_ROGUE3, PS_ROGUE4, PS_ROGUE5, PS_ROGUE6, PS_ROGUE7, PS_ROGUE8, PS_ROGUE9, PS_ROGUE10, PS_ROGUE11, PS_ROGUE12, PS_ROGUE13, PS_ROGUE14, PS_ROGUE15, PS_ROGUE16, PS_ROGUE17, PS_ROGUE18, PS_ROGUE19, PS_ROGUE20, PS_ROGUE21, PS_ROGUE22, PS_ROGUE23, PS_ROGUE24, PS_ROGUE25, PS_ROGUE26, PS_ROGUE27, PS_ROGUE28, PS_ROGUE29, PS_ROGUE30, PS_ROGUE31, PS_ROGUE32, PS_ROGUE33, PS_ROGUE34, PS_ROGUE35, PS_ROGUE36, PS_ROGUE37, PS_ROGUE38, PS_ROGUE39, PS_ROGUE40, PS_ROGUE41, PS_ROGUE42, PS_ROGUE43, PS_ROGUE44, PS_ROGUE45, PS_ROGUE46, PS_ROGUE47, PS_ROGUE48, PS_ROGUE49, PS_ROGUE50, PS_ROGUE51, PS_ROGUE52, PS_ROGUE53, PS_ROGUE54, PS_ROGUE55, PS_ROGUE56, PS_ROGUE57, PS_ROGUE58, PS_ROGUE59, PS_ROGUE60, PS_ROGUE61, PS_ROGUE62, PS_ROGUE63, PS_ROGUE64, PS_ROGUE65, PS_ROGUE66, PS_ROGUE67, PS_ROGUE68, PS_ROGUE69, PS_ROGUE70, PS_ROGUE71, PS_ROGUE72, PS_ROGUE73, PS_ROGUE74, PS_ROGUE75, PS_ROGUE76, PS_ROGUE77, PS_ROGUE78, PS_ROGUE79, PS_ROGUE80, PS_ROGUE81, PS_ROGUE82, PS_ROGUE83, PS_ROGUE84, PS_ROGUE85, PS_ROGUE86, PS_ROGUE87, PS_ROGUE88, PS_ROGUE89, PS_ROGUE90, PS_ROGUE91, PS_ROGUE92, PS_ROGUE93, PS_ROGUE94, PS_ROGUE95, PS_ROGUE96,  PS_ROGUE97, PS_ROGUE98, PS_ROGUE99, PS_ROGUE100, PS_ROGUE101, PS_ROGUE102, PS_ROGUE71 },
	{ PS_MAGE1,  PS_MAGE2,  PS_MAGE3,  PS_MAGE4,  PS_MAGE5,  PS_MAGE6,  PS_MAGE7,  PS_MAGE8,  PS_MAGE9,  PS_MAGE10,  PS_MAGE11,  PS_MAGE12,  PS_MAGE13,  PS_MAGE14,  PS_MAGE15,  PS_MAGE16,  PS_MAGE17,  PS_MAGE18,  PS_MAGE19,  PS_MAGE20,  PS_MAGE21,  PS_MAGE22,  PS_MAGE23,  PS_MAGE24,  PS_MAGE25,  PS_MAGE26,  PS_MAGE27,  PS_MAGE28,  PS_MAGE29,  PS_MAGE30,  PS_MAGE31,  PS_MAGE32,  PS_MAGE33,  PS_MAGE34,  PS_MAGE35,  PS_MAGE36,  PS_MAGE37,  PS_MAGE38,  PS_MAGE39,  PS_MAGE40,  PS_MAGE41,  PS_MAGE42,  PS_MAGE43,  PS_MAGE44,  PS_MAGE45,  PS_MAGE46,  PS_MAGE47,  PS_MAGE48,  PS_MAGE49,  PS_MAGE50,  PS_MAGE51,  PS_MAGE52,  PS_MAGE53,  PS_MAGE54,  PS_MAGE55,  PS_MAGE56,  PS_MAGE57,  PS_MAGE58,  PS_MAGE59,  PS_MAGE60,  PS_MAGE61,  PS_MAGE62,  PS_MAGE63,  PS_MAGE64,  PS_MAGE65,  PS_MAGE66,  PS_MAGE67,  PS_MAGE68,  PS_MAGE69,  PS_MAGE70,  PS_MAGE71,  PS_MAGE72,  PS_MAGE73,  PS_MAGE74,  PS_MAGE75,  PS_MAGE76,  PS_MAGE77,  PS_MAGE78,  PS_MAGE79,  PS_MAGE80,  PS_MAGE81,  PS_MAGE82,  PS_MAGE83,  PS_MAGE84,  PS_MAGE85,  PS_MAGE86,  PS_MAGE87,  PS_MAGE88,  PS_MAGE89,  PS_MAGE90,  PS_MAGE91,  PS_MAGE92,  PS_MAGE93,  PS_MAGE94,  PS_MAGE95,  PS_MAGE96,   PS_MAGE97,  PS_MAGE98,  PS_MAGE99,  PS_MAGE100,  PS_MAGE101,  PS_MAGE102,  PS_MAGE71  },
	{ PS_MONK1,  SFX_NONE,  SFX_NONE,  SFX_NONE,  SFX_NONE,  SFX_NONE,  SFX_NONE,  PS_MONK8,  PS_MONK9,  PS_MONK10,  PS_MONK11,  PS_MONK12,  PS_MONK13,  PS_MONK14,  PS_MONK15,  PS_MONK16,  SFX_NONE,  SFX_NONE,    SFX_NONE,   SFX_NONE,   SFX_NONE,   SFX_NONE,   SFX_NONE,   PS_MONK24,  SFX_NONE,   SFX_NONE,   PS_MONK27,  SFX_NONE,   PS_MONK29,  SFX_NONE,   SFX_NONE,   SFX_NONE,   SFX_NONE,   PS_MONK34,  PS_MONK35,  SFX_NONE,   SFX_NONE,   SFX_NONE,   SFX_NONE,   SFX_NONE,   SFX_NONE,   SFX_NONE,   PS_MONK43,  SFX_NONE,   SFX_NONE,   PS_MONK46,  SFX_NONE,   SFX_NONE,   PS_MONK49,  PS_MONK50,  SFX_NONE,   PS_MONK52,  SFX_NONE,   PS_MONK54,  PS_MONK55,  PS_MONK56,  SFX_NONE,   SFX_NONE,   SFX_NONE,   SFX_NONE,   PS_MONK61,  PS_MONK62,  SFX_NONE,   SFX_NONE,   SFX_NONE,   SFX_NONE,   SFX_NONE,   PS_MONK68,  PS_MONK69,  PS_MONK70,  PS_MONK71,  SFX_NONE,   SFX_NONE,   SFX_NONE,   SFX_NONE,   SFX_NONE,   SFX_NONE,   SFX_NONE,   PS_MONK79,  PS_MONK80,  SFX_NONE,   PS_MONK82,  PS_MONK83,  SFX_NONE,   SFX_NONE,   SFX_NONE,   PS_MONK87,  PS_MONK88,  PS_MONK89,  SFX_NONE,   PS_MONK91,  PS_MONK92,  SFX_NONE,   PS_MONK94,  PS_MONK95,  PS_MONK96,   PS_MONK97,  PS_MONK98,  PS_MONK99,  SFX_NONE,    SFX_NONE,    SFX_NONE,    PS_MONK71  },
	{ PS_ROGUE1, PS_ROGUE2, PS_ROGUE3, PS_ROGUE4, PS_ROGUE5, PS_ROGUE6, PS_ROGUE7, PS_ROGUE8, PS_ROGUE9, PS_ROGUE10, PS_ROGUE11, PS_ROGUE12, PS_ROGUE13, PS_ROGUE14, PS_ROGUE15, PS_ROGUE16, PS_ROGUE17, PS_ROGUE18, PS_ROGUE19, PS_ROGUE20, PS_ROGUE21, PS_ROGUE22, PS_ROGUE23, PS_ROGUE24, PS_ROGUE25, PS_ROGUE26, PS_ROGUE27, PS_ROGUE28, PS_ROGUE29, PS_ROGUE30, PS_ROGUE31, PS_ROGUE32, PS_ROGUE33, PS_ROGUE34, PS_ROGUE35, PS_ROGUE36, PS_ROGUE37, PS_ROGUE38, PS_ROGUE39, PS_ROGUE40, PS_ROGUE41, PS_ROGUE42, PS_ROGUE43, PS_ROGUE44, PS_ROGUE45, PS_ROGUE46, PS_ROGUE47, PS_ROGUE48, PS_ROGUE49, PS_ROGUE50, PS_ROGUE51, PS_ROGUE52, PS_ROGUE53, PS_ROGUE54, PS_ROGUE55, PS_ROGUE56, PS_ROGUE57, PS_ROGUE58, PS_ROGUE59, PS_ROGUE60, PS_ROGUE61, PS_ROGUE62, PS_ROGUE63, PS_ROGUE64, PS_ROGUE65, PS_ROGUE66, PS_ROGUE67, PS_ROGUE68, PS_ROGUE69, PS_ROGUE70, PS_ROGUE71, PS_ROGUE72, PS_ROGUE73, PS_ROGUE74, PS_ROGUE75, PS_ROGUE76, PS_ROGUE77, PS_ROGUE78, PS_ROGUE79, PS_ROGUE80, PS_ROGUE81, PS_ROGUE82, PS_ROGUE83, PS_ROGUE84, PS_ROGUE85, PS_ROGUE86, PS_ROGUE87, PS_ROGUE88, PS_ROGUE89, PS_ROGUE90, PS_ROGUE91, PS_ROGUE92, PS_ROGUE93, PS_ROGUE94, PS_ROGUE95, PS_ROGUE96,  PS_ROGUE97, PS_ROGUE98, PS_ROGUE99, PS_ROGUE100, PS_ROGUE101, PS_ROGUE102, PS_ROGUE71 },
	{ PS_WARR1,  PS_WARR2,  PS_WARR3,  PS_WARR4,  PS_WARR5,  PS_WARR6,  PS_WARR7,  PS_WARR8,  PS_WARR9,  PS_WARR10,  PS_WARR11,  PS_WARR12,  PS_WARR13,  PS_WARR14,  PS_WARR15,  PS_WARR16,  PS_WARR17,  PS_WARR18,  PS_WARR19,  PS_WARR20,  PS_WARR21,  PS_WARR22,  PS_WARR23,  PS_WARR24,  PS_WARR25,  PS_WARR26,  PS_WARR27,  PS_WARR28,  PS_WARR29,  PS_WARR30,  PS_WARR31,  PS_WARR32,  PS_WARR33,  PS_WARR34,  PS_WARR35,  PS_WARR36,  PS_WARR37,  PS_WARR38,  PS_WARR39,  PS_WARR40,  PS_WARR41,  PS_WARR42,  PS_WARR43,  PS_WARR44,  PS_WARR45,  PS_WARR46,  PS_WARR47,  PS_WARR48,  PS_WARR49,  PS_WARR50,  PS_WARR51,  PS_WARR52,  PS_WARR53,  PS_WARR54,  PS_WARR55,  PS_WARR56,  PS_WARR57,  PS_WARR58,  PS_WARR59,  PS_WARR60,  PS_WARR61,  PS_WARR62,  PS_WARR63,  PS_WARR64,  PS_WARR65,  PS_WARR66,  PS_WARR67,  PS_WARR68,  PS_WARR69,  PS_WARR70,  PS_WARR71,  PS_WARR72,  PS_WARR73,  PS_WARR74,  PS_WARR75,  PS_WARR76,  PS_WARR77,  PS_WARR78,  PS_WARR79,  PS_WARR80,  PS_WARR81,  PS_WARR82,  PS_WARR83,  PS_WARR84,  PS_WARR85,  PS_WARR86,  PS_WARR87,  PS_WARR88,  PS_WARR89,  PS_WARR90,  PS_WARR91,  PS_WARR92,  PS_WARR93,  PS_WARR94,  PS_WARR95,  PS_WARR96B,  PS_WARR97,  PS_WARR98,  PS_WARR99,  PS_WARR100,  PS_WARR101,  PS_WARR102,  PS_WARR71  },
	// clang-format on
};

const PlayerData &GetPlayerDataForClass(HeroClass clazz)
{
	return PlayersData[static_cast<size_t>(clazz)];
}

const PlayerCombatData &GetPlayerCombatDataForClass(HeroClass clazz)
{
	return PlayersCombatData[static_cast<size_t>(clazz)];
}

const PlayerStartingLoadoutData &GetPlayerStartingLoadoutForClass(HeroClass clazz)
{
	return PlayersStartingLoadoutData[static_cast<size_t>(clazz)];
}

/** Contains the data related to each player class. */
const PlayerSpriteData PlayersSpriteData[] = {
	// clang-format off
// HeroClass                 classPath,  stand,   walk,   attack,   bow, swHit,   block,   lightning,   fire,   magic,   death

/* HeroClass::Warrior */   { "warrior",     96,     96,      128,    96,    96,      96,          96,     96,      96,     128 },
/* HeroClass::Rogue */     { "rogue",       96,     96,      128,   128,    96,      96,          96,     96,      96,     128 },
/* HeroClass::Sorcerer */  { "sorceror",    96,     96,      128,   128,    96,      96,         128,    128,     128,     128 },
/* HeroClass::Monk */      { "monk",       112,    112,      130,   130,    98,      98,         114,    114,     114,     160 },
/* HeroClass::Bard */      { "rogue",       96,     96,      128,   128,    96,      96,          96,     96,      96,     128 },
/* HeroClass::Barbarian */ { "warrior",     96,     96,      128,    96,    96,      96,          96,     96,      96,     128 },
	// clang-format on
};

const PlayerAnimData PlayersAnimData[] = {
	// clang-format off
// HeroClass                unarmedFrames, unarmedActionFrame, unarmedShieldFrames, unarmedShieldActionFrame, swordFrames, swordActionFrame, swordShieldFrames, swordShieldActionFrame, bowFrames, bowActionFrame, axeFrames, axeActionFrame, maceFrames, maceActionFrame, maceShieldFrames, maceShieldActionFrame, staffFrames, staffActionFrame, idleFrames,  walkingFrames, blockingFrames, deathFrames, castingFrames, recoveryFrames, townIdleFrames, townWalkingFrames, castingActionFrame
/* HeroClass::Warrior */   {           16,                  9,                  16,                        9,          16,                9,                16,                      9,        16,             11,        20,             10,         16,               9,               16,                     9,          16,               11,         10,              8,              2,          20,            20,              6,             20,                 8,                 14 },
/* HeroClass::Rogue */     {           18,                 10,                  18,                       10,          18,               10,                18,                     10,        12,              7,        22,             13,         18,              10,               18,                    10,          16,               11,          8,              8,              4,          20,            16,              7,             20,                 8,                 12 },
/* HeroClass::Sorcerer */  {           20,                 12,                  16,                        9,          16,               12,                16,                     12,        20,             16,        24,             16,         16,              12,               16,                    12,          16,               12,          8,              8,              6,          20,            12,              8,             20,                 8,                  8 },
/* HeroClass::Monk */      {           12,                  7,                  12,                        7,          16,               12,                16,                     12,        20,             14,        23,             14,         16,              12,               16,                    12,          13,                8,          8,              8,              3,          20,            18,              6,             20,                 8,                 13 },
/* HeroClass::Bard */      {           18,                 10,                  18,                       10,          18,               10,                18,                     10,        12,             11,        22,             13,         18,              10,               18,                    10,          16,               11,          8,              8,              4,          20,            16,              7,             20,                 8,                 12 },
/* HeroClass::Barbarian */ {           16,                  9,                  16,                        9,          16,                9,                16,                      9,        16,             11,        20,              8,         16,               8,               16,                     8,          16,               11,         10,              8,              2,          20,            20,              6,             20,                 8,                 14 },
	// clang-format on
};

} // namespace devilution
