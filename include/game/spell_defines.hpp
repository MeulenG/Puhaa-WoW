#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace wowee {
namespace game {

/**
 * Aura slot data for buff/debuff tracking
 */
struct AuraSlot {
    uint32_t spellId = 0;
    uint8_t flags = 0;         // Active, positive/negative, etc.
    uint8_t level = 0;
    uint8_t charges = 0;
    int32_t durationMs = -1;
    int32_t maxDurationMs = -1;
    uint64_t casterGuid = 0;

    bool isEmpty() const { return spellId == 0; }
};

/**
 * Action bar slot
 */
struct ActionBarSlot {
    enum Type : uint8_t { EMPTY = 0, SPELL = 1, ITEM = 2, MACRO = 3 };
    Type type = EMPTY;
    uint32_t id = 0;              // spellId, itemId, or macroId
    float cooldownRemaining = 0.0f;
    float cooldownTotal = 0.0f;

    bool isReady() const { return cooldownRemaining <= 0.0f; }
    bool isEmpty() const { return type == EMPTY; }
};

/**
 * Floating combat text entry
 */
struct CombatTextEntry {
    enum Type : uint8_t {
        MELEE_DAMAGE, SPELL_DAMAGE, HEAL, MISS, DODGE, PARRY, BLOCK,
        CRIT_DAMAGE, CRIT_HEAL, PERIODIC_DAMAGE, PERIODIC_HEAL, ENVIRONMENTAL
    };
    Type type;
    int32_t amount = 0;
    uint32_t spellId = 0;
    float age = 0.0f;           // Seconds since creation (for fadeout)
    bool isPlayerSource = false; // True if player dealt this

    static constexpr float LIFETIME = 2.5f;
    bool isExpired() const { return age >= LIFETIME; }
};

/**
 * Spell cooldown entry received from server
 */
struct SpellCooldownEntry {
    uint32_t spellId;
    uint16_t itemId;
    uint16_t categoryId;
    uint32_t cooldownMs;
    uint32_t categoryCooldownMs;
};

} // namespace game
} // namespace wowee
