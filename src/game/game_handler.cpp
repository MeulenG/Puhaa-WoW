#include "game/game_handler.hpp"
#include "game/opcodes.hpp"
#include "network/world_socket.hpp"
#include "network/packet.hpp"
#include "core/logger.hpp"
#include <algorithm>
#include <cctype>
#include <random>
#include <chrono>

namespace wowee {
namespace game {

GameHandler::GameHandler() {
    LOG_DEBUG("GameHandler created");
}

GameHandler::~GameHandler() {
    disconnect();
}

bool GameHandler::connect(const std::string& host,
                          uint16_t port,
                          const std::vector<uint8_t>& sessionKey,
                          const std::string& accountName,
                          uint32_t build) {

    if (sessionKey.size() != 40) {
        LOG_ERROR("Invalid session key size: ", sessionKey.size(), " (expected 40)");
        fail("Invalid session key");
        return false;
    }

    LOG_INFO("========================================");
    LOG_INFO("   CONNECTING TO WORLD SERVER");
    LOG_INFO("========================================");
    LOG_INFO("Host: ", host);
    LOG_INFO("Port: ", port);
    LOG_INFO("Account: ", accountName);
    LOG_INFO("Build: ", build);

    // Store authentication data
    this->sessionKey = sessionKey;
    this->accountName = accountName;
    this->build = build;

    // Generate random client seed
    this->clientSeed = generateClientSeed();
    LOG_DEBUG("Generated client seed: 0x", std::hex, clientSeed, std::dec);

    // Create world socket
    socket = std::make_unique<network::WorldSocket>();

    // Set up packet callback
    socket->setPacketCallback([this](const network::Packet& packet) {
        network::Packet mutablePacket = packet;
        handlePacket(mutablePacket);
    });

    // Connect to world server
    setState(WorldState::CONNECTING);

    if (!socket->connect(host, port)) {
        LOG_ERROR("Failed to connect to world server");
        fail("Connection failed");
        return false;
    }

    setState(WorldState::CONNECTED);
    LOG_INFO("Connected to world server, waiting for SMSG_AUTH_CHALLENGE...");

    return true;
}

void GameHandler::disconnect() {
    if (socket) {
        socket->disconnect();
        socket.reset();
    }
    setState(WorldState::DISCONNECTED);
    LOG_INFO("Disconnected from world server");
}

bool GameHandler::isConnected() const {
    return socket && socket->isConnected();
}

void GameHandler::update(float deltaTime) {
    if (!socket) {
        return;
    }

    // Update socket (processes incoming data and triggers callbacks)
    socket->update();

    // Validate target still exists
    if (targetGuid != 0 && !entityManager.hasEntity(targetGuid)) {
        clearTarget();
    }

    // Send periodic heartbeat if in world
    if (state == WorldState::IN_WORLD) {
        timeSinceLastPing += deltaTime;

        if (timeSinceLastPing >= pingInterval) {
            sendPing();
            timeSinceLastPing = 0.0f;
        }
    }
}

void GameHandler::handlePacket(network::Packet& packet) {
    if (packet.getSize() < 1) {
        LOG_WARNING("Received empty packet");
        return;
    }

    uint16_t opcode = packet.getOpcode();

    LOG_DEBUG("Received world packet: opcode=0x", std::hex, opcode, std::dec,
              " size=", packet.getSize(), " bytes");

    // Route packet based on opcode
    Opcode opcodeEnum = static_cast<Opcode>(opcode);

    switch (opcodeEnum) {
        case Opcode::SMSG_AUTH_CHALLENGE:
            if (state == WorldState::CONNECTED) {
                handleAuthChallenge(packet);
            } else {
                LOG_WARNING("Unexpected SMSG_AUTH_CHALLENGE in state: ", (int)state);
            }
            break;

        case Opcode::SMSG_AUTH_RESPONSE:
            if (state == WorldState::AUTH_SENT) {
                handleAuthResponse(packet);
            } else {
                LOG_WARNING("Unexpected SMSG_AUTH_RESPONSE in state: ", (int)state);
            }
            break;

        case Opcode::SMSG_CHAR_ENUM:
            if (state == WorldState::CHAR_LIST_REQUESTED) {
                handleCharEnum(packet);
            } else {
                LOG_WARNING("Unexpected SMSG_CHAR_ENUM in state: ", (int)state);
            }
            break;

        case Opcode::SMSG_LOGIN_VERIFY_WORLD:
            if (state == WorldState::ENTERING_WORLD) {
                handleLoginVerifyWorld(packet);
            } else {
                LOG_WARNING("Unexpected SMSG_LOGIN_VERIFY_WORLD in state: ", (int)state);
            }
            break;

        case Opcode::SMSG_ACCOUNT_DATA_TIMES:
            // Can be received at any time after authentication
            handleAccountDataTimes(packet);
            break;

        case Opcode::SMSG_MOTD:
            // Can be received at any time after entering world
            handleMotd(packet);
            break;

        case Opcode::SMSG_PONG:
            // Can be received at any time after entering world
            handlePong(packet);
            break;

        case Opcode::SMSG_UPDATE_OBJECT:
            // Can be received after entering world
            if (state == WorldState::IN_WORLD) {
                handleUpdateObject(packet);
            }
            break;

        case Opcode::SMSG_DESTROY_OBJECT:
            // Can be received after entering world
            if (state == WorldState::IN_WORLD) {
                handleDestroyObject(packet);
            }
            break;

        case Opcode::SMSG_MESSAGECHAT:
            // Can be received after entering world
            if (state == WorldState::IN_WORLD) {
                handleMessageChat(packet);
            }
            break;

        default:
            LOG_WARNING("Unhandled world opcode: 0x", std::hex, opcode, std::dec);
            break;
    }
}

void GameHandler::handleAuthChallenge(network::Packet& packet) {
    LOG_INFO("Handling SMSG_AUTH_CHALLENGE");

    AuthChallengeData challenge;
    if (!AuthChallengeParser::parse(packet, challenge)) {
        fail("Failed to parse SMSG_AUTH_CHALLENGE");
        return;
    }

    if (!challenge.isValid()) {
        fail("Invalid auth challenge data");
        return;
    }

    // Store server seed
    serverSeed = challenge.serverSeed;
    LOG_DEBUG("Server seed: 0x", std::hex, serverSeed, std::dec);

    setState(WorldState::CHALLENGE_RECEIVED);

    // Send authentication session
    sendAuthSession();
}

void GameHandler::sendAuthSession() {
    LOG_INFO("Sending CMSG_AUTH_SESSION");

    // Build authentication packet
    auto packet = AuthSessionPacket::build(
        build,
        accountName,
        clientSeed,
        sessionKey,
        serverSeed
    );

    LOG_DEBUG("CMSG_AUTH_SESSION packet size: ", packet.getSize(), " bytes");

    // Send packet (NOT encrypted yet)
    socket->send(packet);

    // CRITICAL: Initialize encryption AFTER sending AUTH_SESSION
    // but BEFORE receiving AUTH_RESPONSE
    LOG_INFO("Initializing RC4 header encryption...");
    socket->initEncryption(sessionKey);

    setState(WorldState::AUTH_SENT);
    LOG_INFO("CMSG_AUTH_SESSION sent, encryption initialized, waiting for response...");
}

void GameHandler::handleAuthResponse(network::Packet& packet) {
    LOG_INFO("Handling SMSG_AUTH_RESPONSE");

    AuthResponseData response;
    if (!AuthResponseParser::parse(packet, response)) {
        fail("Failed to parse SMSG_AUTH_RESPONSE");
        return;
    }

    if (!response.isSuccess()) {
        std::string reason = std::string("Authentication failed: ") +
                           getAuthResultString(response.result);
        fail(reason);
        return;
    }

    // Authentication successful!
    setState(WorldState::AUTHENTICATED);

    LOG_INFO("========================================");
    LOG_INFO("   WORLD AUTHENTICATION SUCCESSFUL!");
    LOG_INFO("========================================");
    LOG_INFO("Connected to world server");
    LOG_INFO("Ready for character operations");

    setState(WorldState::READY);

    // Call success callback
    if (onSuccess) {
        onSuccess();
    }
}

void GameHandler::requestCharacterList() {
    if (state != WorldState::READY && state != WorldState::AUTHENTICATED) {
        LOG_WARNING("Cannot request character list in state: ", (int)state);
        return;
    }

    LOG_INFO("Requesting character list from server...");

    // Build CMSG_CHAR_ENUM packet (no body, just opcode)
    auto packet = CharEnumPacket::build();

    // Send packet
    socket->send(packet);

    setState(WorldState::CHAR_LIST_REQUESTED);
    LOG_INFO("CMSG_CHAR_ENUM sent, waiting for character list...");
}

void GameHandler::handleCharEnum(network::Packet& packet) {
    LOG_INFO("Handling SMSG_CHAR_ENUM");

    CharEnumResponse response;
    if (!CharEnumParser::parse(packet, response)) {
        fail("Failed to parse SMSG_CHAR_ENUM");
        return;
    }

    // Store characters
    characters = response.characters;

    setState(WorldState::CHAR_LIST_RECEIVED);

    LOG_INFO("========================================");
    LOG_INFO("   CHARACTER LIST RECEIVED");
    LOG_INFO("========================================");
    LOG_INFO("Found ", characters.size(), " character(s)");

    if (characters.empty()) {
        LOG_INFO("No characters on this account");
    } else {
        LOG_INFO("Characters:");
        for (size_t i = 0; i < characters.size(); ++i) {
            const auto& character = characters[i];
            LOG_INFO("  [", i + 1, "] ", character.name);
            LOG_INFO("      GUID: 0x", std::hex, character.guid, std::dec);
            LOG_INFO("      ", getRaceName(character.race), " ",
                     getClassName(character.characterClass));
            LOG_INFO("      Level ", (int)character.level);
        }
    }

    LOG_INFO("Ready to select character");
}

void GameHandler::selectCharacter(uint64_t characterGuid) {
    if (state != WorldState::CHAR_LIST_RECEIVED) {
        LOG_WARNING("Cannot select character in state: ", (int)state);
        return;
    }

    LOG_INFO("========================================");
    LOG_INFO("   ENTERING WORLD");
    LOG_INFO("========================================");
    LOG_INFO("Character GUID: 0x", std::hex, characterGuid, std::dec);

    // Find character name for logging
    for (const auto& character : characters) {
        if (character.guid == characterGuid) {
            LOG_INFO("Character: ", character.name);
            LOG_INFO("Level ", (int)character.level, " ",
                     getRaceName(character.race), " ",
                     getClassName(character.characterClass));
            break;
        }
    }

    // Build CMSG_PLAYER_LOGIN packet
    auto packet = PlayerLoginPacket::build(characterGuid);

    // Send packet
    socket->send(packet);

    setState(WorldState::ENTERING_WORLD);
    LOG_INFO("CMSG_PLAYER_LOGIN sent, entering world...");
}

void GameHandler::handleLoginVerifyWorld(network::Packet& packet) {
    LOG_INFO("Handling SMSG_LOGIN_VERIFY_WORLD");

    LoginVerifyWorldData data;
    if (!LoginVerifyWorldParser::parse(packet, data)) {
        fail("Failed to parse SMSG_LOGIN_VERIFY_WORLD");
        return;
    }

    if (!data.isValid()) {
        fail("Invalid world entry data");
        return;
    }

    // Successfully entered the world!
    setState(WorldState::IN_WORLD);

    LOG_INFO("========================================");
    LOG_INFO("   SUCCESSFULLY ENTERED WORLD!");
    LOG_INFO("========================================");
    LOG_INFO("Map ID: ", data.mapId);
    LOG_INFO("Position: (", data.x, ", ", data.y, ", ", data.z, ")");
    LOG_INFO("Orientation: ", data.orientation, " radians");
    LOG_INFO("Player is now in the game world");

    // Initialize movement info with world entry position
    movementInfo.x = data.x;
    movementInfo.y = data.y;
    movementInfo.z = data.z;
    movementInfo.orientation = data.orientation;
    movementInfo.flags = 0;
    movementInfo.flags2 = 0;
    movementInfo.time = 0;
}

void GameHandler::handleAccountDataTimes(network::Packet& packet) {
    LOG_DEBUG("Handling SMSG_ACCOUNT_DATA_TIMES");

    AccountDataTimesData data;
    if (!AccountDataTimesParser::parse(packet, data)) {
        LOG_WARNING("Failed to parse SMSG_ACCOUNT_DATA_TIMES");
        return;
    }

    LOG_DEBUG("Account data times received (server time: ", data.serverTime, ")");
}

void GameHandler::handleMotd(network::Packet& packet) {
    LOG_INFO("Handling SMSG_MOTD");

    MotdData data;
    if (!MotdParser::parse(packet, data)) {
        LOG_WARNING("Failed to parse SMSG_MOTD");
        return;
    }

    if (!data.isEmpty()) {
        LOG_INFO("========================================");
        LOG_INFO("   MESSAGE OF THE DAY");
        LOG_INFO("========================================");
        for (const auto& line : data.lines) {
            LOG_INFO(line);
        }
        LOG_INFO("========================================");
    }
}

void GameHandler::sendPing() {
    if (state != WorldState::IN_WORLD) {
        return;
    }

    // Increment sequence number
    pingSequence++;

    LOG_DEBUG("Sending CMSG_PING (heartbeat)");
    LOG_DEBUG("  Sequence: ", pingSequence);

    // Build and send ping packet
    auto packet = PingPacket::build(pingSequence, lastLatency);
    socket->send(packet);
}

void GameHandler::handlePong(network::Packet& packet) {
    LOG_DEBUG("Handling SMSG_PONG");

    PongData data;
    if (!PongParser::parse(packet, data)) {
        LOG_WARNING("Failed to parse SMSG_PONG");
        return;
    }

    // Verify sequence matches
    if (data.sequence != pingSequence) {
        LOG_WARNING("SMSG_PONG sequence mismatch: expected ", pingSequence,
                    ", got ", data.sequence);
        return;
    }

    LOG_DEBUG("Heartbeat acknowledged (sequence: ", data.sequence, ")");
}

void GameHandler::sendMovement(Opcode opcode) {
    if (state != WorldState::IN_WORLD) {
        LOG_WARNING("Cannot send movement in state: ", (int)state);
        return;
    }

    // Update movement time
    movementInfo.time = ++movementTime;

    // Update movement flags based on opcode
    switch (opcode) {
        case Opcode::CMSG_MOVE_START_FORWARD:
            movementInfo.flags |= static_cast<uint32_t>(MovementFlags::FORWARD);
            break;
        case Opcode::CMSG_MOVE_START_BACKWARD:
            movementInfo.flags |= static_cast<uint32_t>(MovementFlags::BACKWARD);
            break;
        case Opcode::CMSG_MOVE_STOP:
            movementInfo.flags &= ~(static_cast<uint32_t>(MovementFlags::FORWARD) |
                                    static_cast<uint32_t>(MovementFlags::BACKWARD));
            break;
        case Opcode::CMSG_MOVE_START_STRAFE_LEFT:
            movementInfo.flags |= static_cast<uint32_t>(MovementFlags::STRAFE_LEFT);
            break;
        case Opcode::CMSG_MOVE_START_STRAFE_RIGHT:
            movementInfo.flags |= static_cast<uint32_t>(MovementFlags::STRAFE_RIGHT);
            break;
        case Opcode::CMSG_MOVE_STOP_STRAFE:
            movementInfo.flags &= ~(static_cast<uint32_t>(MovementFlags::STRAFE_LEFT) |
                                    static_cast<uint32_t>(MovementFlags::STRAFE_RIGHT));
            break;
        case Opcode::CMSG_MOVE_JUMP:
            movementInfo.flags |= static_cast<uint32_t>(MovementFlags::FALLING);
            break;
        case Opcode::CMSG_MOVE_START_TURN_LEFT:
            movementInfo.flags |= static_cast<uint32_t>(MovementFlags::TURN_LEFT);
            break;
        case Opcode::CMSG_MOVE_START_TURN_RIGHT:
            movementInfo.flags |= static_cast<uint32_t>(MovementFlags::TURN_RIGHT);
            break;
        case Opcode::CMSG_MOVE_STOP_TURN:
            movementInfo.flags &= ~(static_cast<uint32_t>(MovementFlags::TURN_LEFT) |
                                    static_cast<uint32_t>(MovementFlags::TURN_RIGHT));
            break;
        case Opcode::CMSG_MOVE_FALL_LAND:
            movementInfo.flags &= ~static_cast<uint32_t>(MovementFlags::FALLING);
            break;
        case Opcode::CMSG_MOVE_HEARTBEAT:
            // No flag changes — just sends current position
            break;
        default:
            break;
    }

    LOG_DEBUG("Sending movement packet: opcode=0x", std::hex,
              static_cast<uint16_t>(opcode), std::dec);

    // Build and send movement packet
    auto packet = MovementPacket::build(opcode, movementInfo);
    socket->send(packet);
}

void GameHandler::setPosition(float x, float y, float z) {
    movementInfo.x = x;
    movementInfo.y = y;
    movementInfo.z = z;
}

void GameHandler::setOrientation(float orientation) {
    movementInfo.orientation = orientation;
}

void GameHandler::handleUpdateObject(network::Packet& packet) {
    LOG_INFO("Handling SMSG_UPDATE_OBJECT");

    UpdateObjectData data;
    if (!UpdateObjectParser::parse(packet, data)) {
        LOG_WARNING("Failed to parse SMSG_UPDATE_OBJECT");
        return;
    }

    // Process out-of-range objects first
    for (uint64_t guid : data.outOfRangeGuids) {
        if (entityManager.hasEntity(guid)) {
            LOG_INFO("Entity went out of range: 0x", std::hex, guid, std::dec);
            entityManager.removeEntity(guid);
        }
    }

    // Process update blocks
    for (const auto& block : data.blocks) {
        switch (block.updateType) {
            case UpdateType::CREATE_OBJECT:
            case UpdateType::CREATE_OBJECT2: {
                // Create new entity
                std::shared_ptr<Entity> entity;

                switch (block.objectType) {
                    case ObjectType::PLAYER:
                        entity = std::make_shared<Player>(block.guid);
                        LOG_INFO("Created player entity: 0x", std::hex, block.guid, std::dec);
                        break;

                    case ObjectType::UNIT:
                        entity = std::make_shared<Unit>(block.guid);
                        LOG_INFO("Created unit entity: 0x", std::hex, block.guid, std::dec);
                        break;

                    case ObjectType::GAMEOBJECT:
                        entity = std::make_shared<GameObject>(block.guid);
                        LOG_INFO("Created gameobject entity: 0x", std::hex, block.guid, std::dec);
                        break;

                    default:
                        entity = std::make_shared<Entity>(block.guid);
                        entity->setType(block.objectType);
                        LOG_INFO("Created generic entity: 0x", std::hex, block.guid, std::dec,
                                 ", type=", static_cast<int>(block.objectType));
                        break;
                }

                // Set position from movement block
                if (block.hasMovement) {
                    entity->setPosition(block.x, block.y, block.z, block.orientation);
                    LOG_DEBUG("  Position: (", block.x, ", ", block.y, ", ", block.z, ")");
                }

                // Set fields
                for (const auto& field : block.fields) {
                    entity->setField(field.first, field.second);
                }

                // Add to manager
                entityManager.addEntity(block.guid, entity);
                break;
            }

            case UpdateType::VALUES: {
                // Update existing entity fields
                auto entity = entityManager.getEntity(block.guid);
                if (entity) {
                    for (const auto& field : block.fields) {
                        entity->setField(field.first, field.second);
                    }
                    LOG_DEBUG("Updated entity fields: 0x", std::hex, block.guid, std::dec);
                } else {
                    LOG_WARNING("VALUES update for unknown entity: 0x", std::hex, block.guid, std::dec);
                }
                break;
            }

            case UpdateType::MOVEMENT: {
                // Update entity position
                auto entity = entityManager.getEntity(block.guid);
                if (entity) {
                    entity->setPosition(block.x, block.y, block.z, block.orientation);
                    LOG_DEBUG("Updated entity position: 0x", std::hex, block.guid, std::dec);
                } else {
                    LOG_WARNING("MOVEMENT update for unknown entity: 0x", std::hex, block.guid, std::dec);
                }
                break;
            }

            default:
                break;
        }
    }

    tabCycleStale = true;
    LOG_INFO("Entity count: ", entityManager.getEntityCount());
}

void GameHandler::handleDestroyObject(network::Packet& packet) {
    LOG_INFO("Handling SMSG_DESTROY_OBJECT");

    DestroyObjectData data;
    if (!DestroyObjectParser::parse(packet, data)) {
        LOG_WARNING("Failed to parse SMSG_DESTROY_OBJECT");
        return;
    }

    // Remove entity
    if (entityManager.hasEntity(data.guid)) {
        entityManager.removeEntity(data.guid);
        LOG_INFO("Destroyed entity: 0x", std::hex, data.guid, std::dec,
                 " (", (data.isDeath ? "death" : "despawn"), ")");
    } else {
        LOG_WARNING("Destroy object for unknown entity: 0x", std::hex, data.guid, std::dec);
    }

    tabCycleStale = true;
    LOG_INFO("Entity count: ", entityManager.getEntityCount());
}

void GameHandler::sendChatMessage(ChatType type, const std::string& message, const std::string& target) {
    if (state != WorldState::IN_WORLD) {
        LOG_WARNING("Cannot send chat in state: ", (int)state);
        return;
    }

    if (message.empty()) {
        LOG_WARNING("Cannot send empty chat message");
        return;
    }

    LOG_INFO("Sending chat message: [", getChatTypeString(type), "] ", message);

    // Determine language based on character (for now, use COMMON)
    ChatLanguage language = ChatLanguage::COMMON;

    // Build and send packet
    auto packet = MessageChatPacket::build(type, language, message, target);
    socket->send(packet);
}

void GameHandler::handleMessageChat(network::Packet& packet) {
    LOG_DEBUG("Handling SMSG_MESSAGECHAT");

    MessageChatData data;
    if (!MessageChatParser::parse(packet, data)) {
        LOG_WARNING("Failed to parse SMSG_MESSAGECHAT");
        return;
    }

    // Add to chat history
    chatHistory.push_back(data);

    // Limit chat history size
    if (chatHistory.size() > maxChatHistory) {
        chatHistory.erase(chatHistory.begin());
    }

    // Log the message
    std::string senderInfo;
    if (!data.senderName.empty()) {
        senderInfo = data.senderName;
    } else if (data.senderGuid != 0) {
        // Try to find entity name
        auto entity = entityManager.getEntity(data.senderGuid);
        if (entity && entity->getType() == ObjectType::PLAYER) {
            auto player = std::dynamic_pointer_cast<Player>(entity);
            if (player && !player->getName().empty()) {
                senderInfo = player->getName();
            } else {
                senderInfo = "Player-" + std::to_string(data.senderGuid);
            }
        } else {
            senderInfo = "Unknown-" + std::to_string(data.senderGuid);
        }
    } else {
        senderInfo = "System";
    }

    std::string channelInfo;
    if (!data.channelName.empty()) {
        channelInfo = "[" + data.channelName + "] ";
    }

    LOG_INFO("========================================");
    LOG_INFO(" CHAT [", getChatTypeString(data.type), "]");
    LOG_INFO("========================================");
    LOG_INFO(channelInfo, senderInfo, ": ", data.message);
    LOG_INFO("========================================");
}

void GameHandler::setTarget(uint64_t guid) {
    if (guid == targetGuid) return;
    targetGuid = guid;
    if (guid != 0) {
        LOG_INFO("Target set: 0x", std::hex, guid, std::dec);
    }
}

void GameHandler::clearTarget() {
    if (targetGuid != 0) {
        LOG_INFO("Target cleared");
    }
    targetGuid = 0;
    tabCycleIndex = -1;
    tabCycleStale = true;
}

std::shared_ptr<Entity> GameHandler::getTarget() const {
    if (targetGuid == 0) return nullptr;
    return entityManager.getEntity(targetGuid);
}

void GameHandler::tabTarget(float playerX, float playerY, float playerZ) {
    // Rebuild cycle list if stale
    if (tabCycleStale) {
        tabCycleList.clear();
        tabCycleIndex = -1;

        struct EntityDist {
            uint64_t guid;
            float distance;
        };
        std::vector<EntityDist> sortable;

        for (const auto& [guid, entity] : entityManager.getEntities()) {
            auto t = entity->getType();
            if (t != ObjectType::UNIT && t != ObjectType::PLAYER) continue;
            float dx = entity->getX() - playerX;
            float dy = entity->getY() - playerY;
            float dz = entity->getZ() - playerZ;
            float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            sortable.push_back({guid, dist});
        }

        std::sort(sortable.begin(), sortable.end(),
                  [](const EntityDist& a, const EntityDist& b) { return a.distance < b.distance; });

        for (const auto& ed : sortable) {
            tabCycleList.push_back(ed.guid);
        }
        tabCycleStale = false;
    }

    if (tabCycleList.empty()) {
        clearTarget();
        return;
    }

    tabCycleIndex = (tabCycleIndex + 1) % static_cast<int>(tabCycleList.size());
    setTarget(tabCycleList[tabCycleIndex]);
}

void GameHandler::addLocalChatMessage(const MessageChatData& msg) {
    chatHistory.push_back(msg);
    if (chatHistory.size() > maxChatHistory) {
        chatHistory.erase(chatHistory.begin());
    }
}

std::vector<MessageChatData> GameHandler::getChatHistory(size_t maxMessages) const {
    if (maxMessages == 0 || maxMessages >= chatHistory.size()) {
        return chatHistory;
    }

    // Return last N messages
    return std::vector<MessageChatData>(
        chatHistory.end() - maxMessages,
        chatHistory.end()
    );
}

uint32_t GameHandler::generateClientSeed() {
    // Generate cryptographically random seed
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(1, 0xFFFFFFFF);
    return dis(gen);
}

void GameHandler::setState(WorldState newState) {
    if (state != newState) {
        LOG_DEBUG("World state: ", (int)state, " -> ", (int)newState);
        state = newState;
    }
}

void GameHandler::fail(const std::string& reason) {
    LOG_ERROR("World connection failed: ", reason);
    setState(WorldState::FAILED);

    if (onFailure) {
        onFailure(reason);
    }
}

} // namespace game
} // namespace wowee
