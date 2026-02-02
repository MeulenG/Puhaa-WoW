#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

namespace wowee {
namespace pipeline { class AssetManager; }
namespace rendering { class CharacterRenderer; }
namespace game {

class EntityManager;

struct NpcSpawnDef {
    std::string name;
    std::string m2Path;
    uint32_t level;
    uint32_t health;
    glm::vec3 glPosition;   // GL world coords (pre-converted)
    float rotation;          // radians around Z
    float scale;
    bool isCritter;          // critters don't do humanoid emotes
};

struct NpcInstance {
    uint64_t guid;
    uint32_t renderInstanceId;
    float emoteTimer;        // countdown to next random emote
    float emoteEndTimer;     // countdown until emote animation finishes
    bool isEmoting;
    bool isCritter;
};

class NpcManager {
public:
    void initialize(pipeline::AssetManager* am,
                    rendering::CharacterRenderer* cr,
                    EntityManager& em,
                    const glm::vec3& playerSpawnGL);
    void update(float deltaTime, rendering::CharacterRenderer* cr);

private:
    void loadCreatureModel(pipeline::AssetManager* am,
                           rendering::CharacterRenderer* cr,
                           const std::string& m2Path,
                           uint32_t modelId);

    std::vector<NpcInstance> npcs;
    std::unordered_map<std::string, uint32_t> loadedModels; // path -> modelId
    uint64_t nextGuid = 0xF1300000DEAD0001ULL;
    uint32_t nextModelId = 100;
};

} // namespace game
} // namespace wowee
