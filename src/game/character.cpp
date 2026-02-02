#include "game/character.hpp"

namespace wowee {
namespace game {

const char* getRaceName(Race race) {
    switch (race) {
        case Race::HUMAN:       return "Human";
        case Race::ORC:         return "Orc";
        case Race::DWARF:       return "Dwarf";
        case Race::NIGHT_ELF:   return "Night Elf";
        case Race::UNDEAD:      return "Undead";
        case Race::TAUREN:      return "Tauren";
        case Race::GNOME:       return "Gnome";
        case Race::TROLL:       return "Troll";
        case Race::GOBLIN:      return "Goblin";
        case Race::BLOOD_ELF:   return "Blood Elf";
        case Race::DRAENEI:     return "Draenei";
        default:                return "Unknown";
    }
}

const char* getClassName(Class characterClass) {
    switch (characterClass) {
        case Class::WARRIOR:        return "Warrior";
        case Class::PALADIN:        return "Paladin";
        case Class::HUNTER:         return "Hunter";
        case Class::ROGUE:          return "Rogue";
        case Class::PRIEST:         return "Priest";
        case Class::DEATH_KNIGHT:   return "Death Knight";
        case Class::SHAMAN:         return "Shaman";
        case Class::MAGE:           return "Mage";
        case Class::WARLOCK:        return "Warlock";
        case Class::DRUID:          return "Druid";
        default:                    return "Unknown";
    }
}

const char* getGenderName(Gender gender) {
    switch (gender) {
        case Gender::MALE:      return "Male";
        case Gender::FEMALE:    return "Female";
        default:                return "Unknown";
    }
}

} // namespace game
} // namespace wowee
