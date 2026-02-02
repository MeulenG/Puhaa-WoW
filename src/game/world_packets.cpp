#include "game/world_packets.hpp"
#include "game/opcodes.hpp"
#include "game/character.hpp"
#include "auth/crypto.hpp"
#include "core/logger.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace wowee {
namespace game {

network::Packet AuthSessionPacket::build(uint32_t build,
                                          const std::string& accountName,
                                          uint32_t clientSeed,
                                          const std::vector<uint8_t>& sessionKey,
                                          uint32_t serverSeed) {
    if (sessionKey.size() != 40) {
        LOG_ERROR("Invalid session key size: ", sessionKey.size(), " (expected 40)");
    }

    // Convert account name to uppercase
    std::string upperAccount = accountName;
    std::transform(upperAccount.begin(), upperAccount.end(),
                   upperAccount.begin(), ::toupper);

    LOG_INFO("Building CMSG_AUTH_SESSION for account: ", upperAccount);

    // Compute authentication hash
    auto authHash = computeAuthHash(upperAccount, clientSeed, serverSeed, sessionKey);

    LOG_DEBUG("  Build: ", build);
    LOG_DEBUG("  Client seed: 0x", std::hex, clientSeed, std::dec);
    LOG_DEBUG("  Server seed: 0x", std::hex, serverSeed, std::dec);
    LOG_DEBUG("  Auth hash: ", authHash.size(), " bytes");

    // Create packet (opcode will be added by WorldSocket)
    network::Packet packet(static_cast<uint16_t>(Opcode::CMSG_AUTH_SESSION));

    // Build number (uint32, little-endian)
    packet.writeUInt32(build);

    // Unknown uint32 (always 0)
    packet.writeUInt32(0);

    // Account name (null-terminated string)
    packet.writeString(upperAccount);

    // Unknown uint32 (always 0)
    packet.writeUInt32(0);

    // Client seed (uint32, little-endian)
    packet.writeUInt32(clientSeed);

    // Unknown fields (5x uint32, all zeros)
    for (int i = 0; i < 5; ++i) {
        packet.writeUInt32(0);
    }

    // Authentication hash (20 bytes)
    packet.writeBytes(authHash.data(), authHash.size());

    // Addon CRC (uint32, can be 0)
    packet.writeUInt32(0);

    LOG_INFO("CMSG_AUTH_SESSION packet built: ", packet.getSize(), " bytes");

    return packet;
}

std::vector<uint8_t> AuthSessionPacket::computeAuthHash(
    const std::string& accountName,
    uint32_t clientSeed,
    uint32_t serverSeed,
    const std::vector<uint8_t>& sessionKey) {

    // Build hash input:
    // account_name + [0,0,0,0] + client_seed + server_seed + session_key

    std::vector<uint8_t> hashInput;
    hashInput.reserve(accountName.size() + 4 + 4 + 4 + sessionKey.size());

    // Account name (as bytes)
    hashInput.insert(hashInput.end(), accountName.begin(), accountName.end());

    // 4 null bytes
    for (int i = 0; i < 4; ++i) {
        hashInput.push_back(0);
    }

    // Client seed (little-endian)
    hashInput.push_back(clientSeed & 0xFF);
    hashInput.push_back((clientSeed >> 8) & 0xFF);
    hashInput.push_back((clientSeed >> 16) & 0xFF);
    hashInput.push_back((clientSeed >> 24) & 0xFF);

    // Server seed (little-endian)
    hashInput.push_back(serverSeed & 0xFF);
    hashInput.push_back((serverSeed >> 8) & 0xFF);
    hashInput.push_back((serverSeed >> 16) & 0xFF);
    hashInput.push_back((serverSeed >> 24) & 0xFF);

    // Session key (40 bytes)
    hashInput.insert(hashInput.end(), sessionKey.begin(), sessionKey.end());

    LOG_DEBUG("Auth hash input: ", hashInput.size(), " bytes");

    // Compute SHA1 hash
    return auth::Crypto::sha1(hashInput);
}

bool AuthChallengeParser::parse(network::Packet& packet, AuthChallengeData& data) {
    // SMSG_AUTH_CHALLENGE format (WoW 3.3.5a):
    // uint32 unknown1 (always 1?)
    // uint32 serverSeed

    if (packet.getSize() < 8) {
        LOG_ERROR("SMSG_AUTH_CHALLENGE packet too small: ", packet.getSize(), " bytes");
        return false;
    }

    data.unknown1 = packet.readUInt32();
    data.serverSeed = packet.readUInt32();

    LOG_INFO("Parsed SMSG_AUTH_CHALLENGE:");
    LOG_INFO("  Unknown1: 0x", std::hex, data.unknown1, std::dec);
    LOG_INFO("  Server seed: 0x", std::hex, data.serverSeed, std::dec);

    // Note: 3.3.5a has additional data after this (seed2, etc.)
    // but we only need the first seed for authentication

    return true;
}

bool AuthResponseParser::parse(network::Packet& packet, AuthResponseData& response) {
    // SMSG_AUTH_RESPONSE format:
    // uint8 result

    if (packet.getSize() < 1) {
        LOG_ERROR("SMSG_AUTH_RESPONSE packet too small: ", packet.getSize(), " bytes");
        return false;
    }

    uint8_t resultCode = packet.readUInt8();
    response.result = static_cast<AuthResult>(resultCode);

    LOG_INFO("Parsed SMSG_AUTH_RESPONSE: ", getAuthResultString(response.result));

    return true;
}

const char* getAuthResultString(AuthResult result) {
    switch (result) {
        case AuthResult::OK:
            return "OK - Authentication successful";
        case AuthResult::FAILED:
            return "FAILED - Authentication failed";
        case AuthResult::REJECT:
            return "REJECT - Connection rejected";
        case AuthResult::BAD_SERVER_PROOF:
            return "BAD_SERVER_PROOF - Invalid server proof";
        case AuthResult::UNAVAILABLE:
            return "UNAVAILABLE - Server unavailable";
        case AuthResult::SYSTEM_ERROR:
            return "SYSTEM_ERROR - System error occurred";
        case AuthResult::BILLING_ERROR:
            return "BILLING_ERROR - Billing error";
        case AuthResult::BILLING_EXPIRED:
            return "BILLING_EXPIRED - Subscription expired";
        case AuthResult::VERSION_MISMATCH:
            return "VERSION_MISMATCH - Client version mismatch";
        case AuthResult::UNKNOWN_ACCOUNT:
            return "UNKNOWN_ACCOUNT - Account not found";
        case AuthResult::INCORRECT_PASSWORD:
            return "INCORRECT_PASSWORD - Wrong password";
        case AuthResult::SESSION_EXPIRED:
            return "SESSION_EXPIRED - Session has expired";
        case AuthResult::SERVER_SHUTTING_DOWN:
            return "SERVER_SHUTTING_DOWN - Server is shutting down";
        case AuthResult::ALREADY_LOGGING_IN:
            return "ALREADY_LOGGING_IN - Already logging in";
        case AuthResult::LOGIN_SERVER_NOT_FOUND:
            return "LOGIN_SERVER_NOT_FOUND - Can't contact login server";
        case AuthResult::WAIT_QUEUE:
            return "WAIT_QUEUE - Waiting in queue";
        case AuthResult::BANNED:
            return "BANNED - Account is banned";
        case AuthResult::ALREADY_ONLINE:
            return "ALREADY_ONLINE - Character already logged in";
        case AuthResult::NO_TIME:
            return "NO_TIME - No game time remaining";
        case AuthResult::DB_BUSY:
            return "DB_BUSY - Database is busy";
        case AuthResult::SUSPENDED:
            return "SUSPENDED - Account is suspended";
        case AuthResult::PARENTAL_CONTROL:
            return "PARENTAL_CONTROL - Parental controls active";
        case AuthResult::LOCKED_ENFORCED:
            return "LOCKED_ENFORCED - Account is locked";
        default:
            return "UNKNOWN - Unknown result code";
    }
}

network::Packet CharEnumPacket::build() {
    // CMSG_CHAR_ENUM has no body - just the opcode
    network::Packet packet(static_cast<uint16_t>(Opcode::CMSG_CHAR_ENUM));

    LOG_DEBUG("Built CMSG_CHAR_ENUM packet (no body)");

    return packet;
}

bool CharEnumParser::parse(network::Packet& packet, CharEnumResponse& response) {
    // Read character count
    uint8_t count = packet.readUInt8();

    LOG_INFO("Parsing SMSG_CHAR_ENUM: ", (int)count, " characters");

    response.characters.clear();
    response.characters.reserve(count);

    for (uint8_t i = 0; i < count; ++i) {
        Character character;

        // Read GUID (8 bytes, little-endian)
        character.guid = packet.readUInt64();

        // Read name (null-terminated string)
        character.name = packet.readString();

        // Read race, class, gender
        character.race = static_cast<Race>(packet.readUInt8());
        character.characterClass = static_cast<Class>(packet.readUInt8());
        character.gender = static_cast<Gender>(packet.readUInt8());

        // Read appearance data
        character.appearanceBytes = packet.readUInt32();
        character.facialFeatures = packet.readUInt8();

        // Read level
        character.level = packet.readUInt8();

        // Read location
        character.zoneId = packet.readUInt32();
        character.mapId = packet.readUInt32();
        character.x = packet.readFloat();
        character.y = packet.readFloat();
        character.z = packet.readFloat();

        // Read affiliations
        character.guildId = packet.readUInt32();

        // Read flags
        character.flags = packet.readUInt32();

        // Skip customization flag (uint32) and unknown byte
        packet.readUInt32();  // Customization
        packet.readUInt8();   // Unknown

        // Read pet data (always present, even if no pet)
        character.pet.displayModel = packet.readUInt32();
        character.pet.level = packet.readUInt32();
        character.pet.family = packet.readUInt32();

        // Read equipment (23 items)
        character.equipment.reserve(23);
        for (int j = 0; j < 23; ++j) {
            EquipmentItem item;
            item.displayModel = packet.readUInt32();
            item.inventoryType = packet.readUInt8();
            item.enchantment = packet.readUInt32();
            character.equipment.push_back(item);
        }

        LOG_INFO("  Character ", (int)(i + 1), ": ", character.name);
        LOG_INFO("    GUID: 0x", std::hex, character.guid, std::dec);
        LOG_INFO("    ", getRaceName(character.race), " ",
                 getClassName(character.characterClass), " (",
                 getGenderName(character.gender), ")");
        LOG_INFO("    Level: ", (int)character.level);
        LOG_INFO("    Location: Zone ", character.zoneId, ", Map ", character.mapId);
        LOG_INFO("    Position: (", character.x, ", ", character.y, ", ", character.z, ")");
        if (character.hasGuild()) {
            LOG_INFO("    Guild ID: ", character.guildId);
        }
        if (character.hasPet()) {
            LOG_INFO("    Pet: Model ", character.pet.displayModel,
                     ", Level ", character.pet.level);
        }

        response.characters.push_back(character);
    }

    LOG_INFO("Successfully parsed ", response.characters.size(), " characters");

    return true;
}

network::Packet PlayerLoginPacket::build(uint64_t characterGuid) {
    network::Packet packet(static_cast<uint16_t>(Opcode::CMSG_PLAYER_LOGIN));

    // Write character GUID (8 bytes, little-endian)
    packet.writeUInt64(characterGuid);

    LOG_INFO("Built CMSG_PLAYER_LOGIN packet");
    LOG_INFO("  Character GUID: 0x", std::hex, characterGuid, std::dec);

    return packet;
}

bool LoginVerifyWorldParser::parse(network::Packet& packet, LoginVerifyWorldData& data) {
    // SMSG_LOGIN_VERIFY_WORLD format (WoW 3.3.5a):
    // uint32 mapId
    // float x, y, z (position)
    // float orientation

    if (packet.getSize() < 20) {
        LOG_ERROR("SMSG_LOGIN_VERIFY_WORLD packet too small: ", packet.getSize(), " bytes");
        return false;
    }

    data.mapId = packet.readUInt32();
    data.x = packet.readFloat();
    data.y = packet.readFloat();
    data.z = packet.readFloat();
    data.orientation = packet.readFloat();

    LOG_INFO("Parsed SMSG_LOGIN_VERIFY_WORLD:");
    LOG_INFO("  Map ID: ", data.mapId);
    LOG_INFO("  Position: (", data.x, ", ", data.y, ", ", data.z, ")");
    LOG_INFO("  Orientation: ", data.orientation, " radians");

    return true;
}

bool AccountDataTimesParser::parse(network::Packet& packet, AccountDataTimesData& data) {
    // SMSG_ACCOUNT_DATA_TIMES format (WoW 3.3.5a):
    // uint32 serverTime (Unix timestamp)
    // uint8 unknown (always 1?)
    // uint32[8] accountDataTimes (timestamps for each data slot)

    if (packet.getSize() < 37) {
        LOG_ERROR("SMSG_ACCOUNT_DATA_TIMES packet too small: ", packet.getSize(), " bytes");
        return false;
    }

    data.serverTime = packet.readUInt32();
    data.unknown = packet.readUInt8();

    LOG_DEBUG("Parsed SMSG_ACCOUNT_DATA_TIMES:");
    LOG_DEBUG("  Server time: ", data.serverTime);
    LOG_DEBUG("  Unknown: ", (int)data.unknown);

    for (int i = 0; i < 8; ++i) {
        data.accountDataTimes[i] = packet.readUInt32();
        if (data.accountDataTimes[i] != 0) {
            LOG_DEBUG("  Data slot ", i, ": ", data.accountDataTimes[i]);
        }
    }

    return true;
}

bool MotdParser::parse(network::Packet& packet, MotdData& data) {
    // SMSG_MOTD format (WoW 3.3.5a):
    // uint32 lineCount
    // string[lineCount] lines (null-terminated strings)

    if (packet.getSize() < 4) {
        LOG_ERROR("SMSG_MOTD packet too small: ", packet.getSize(), " bytes");
        return false;
    }

    uint32_t lineCount = packet.readUInt32();

    LOG_INFO("Parsed SMSG_MOTD:");
    LOG_INFO("  Line count: ", lineCount);

    data.lines.clear();
    data.lines.reserve(lineCount);

    for (uint32_t i = 0; i < lineCount; ++i) {
        std::string line = packet.readString();
        data.lines.push_back(line);
        LOG_INFO("  [", i + 1, "] ", line);
    }

    return true;
}

network::Packet PingPacket::build(uint32_t sequence, uint32_t latency) {
    network::Packet packet(static_cast<uint16_t>(Opcode::CMSG_PING));

    // Write sequence number (uint32, little-endian)
    packet.writeUInt32(sequence);

    // Write latency (uint32, little-endian, in milliseconds)
    packet.writeUInt32(latency);

    LOG_DEBUG("Built CMSG_PING packet");
    LOG_DEBUG("  Sequence: ", sequence);
    LOG_DEBUG("  Latency: ", latency, " ms");

    return packet;
}

bool PongParser::parse(network::Packet& packet, PongData& data) {
    // SMSG_PONG format (WoW 3.3.5a):
    // uint32 sequence (echoed from CMSG_PING)

    if (packet.getSize() < 4) {
        LOG_ERROR("SMSG_PONG packet too small: ", packet.getSize(), " bytes");
        return false;
    }

    data.sequence = packet.readUInt32();

    LOG_DEBUG("Parsed SMSG_PONG:");
    LOG_DEBUG("  Sequence: ", data.sequence);

    return true;
}

network::Packet MovementPacket::build(Opcode opcode, const MovementInfo& info) {
    network::Packet packet(static_cast<uint16_t>(opcode));

    // Movement packet format (WoW 3.3.5a):
    // uint32 flags
    // uint16 flags2
    // uint32 time
    // float x, y, z
    // float orientation

    // Write movement flags
    packet.writeUInt32(info.flags);
    packet.writeUInt16(info.flags2);

    // Write timestamp
    packet.writeUInt32(info.time);

    // Write position
    packet.writeBytes(reinterpret_cast<const uint8_t*>(&info.x), sizeof(float));
    packet.writeBytes(reinterpret_cast<const uint8_t*>(&info.y), sizeof(float));
    packet.writeBytes(reinterpret_cast<const uint8_t*>(&info.z), sizeof(float));

    // Write orientation
    packet.writeBytes(reinterpret_cast<const uint8_t*>(&info.orientation), sizeof(float));

    // Write pitch if swimming/flying
    if (info.hasFlag(MovementFlags::SWIMMING) || info.hasFlag(MovementFlags::FLYING)) {
        packet.writeBytes(reinterpret_cast<const uint8_t*>(&info.pitch), sizeof(float));
    }

    // Write fall time if falling
    if (info.hasFlag(MovementFlags::FALLING)) {
        packet.writeUInt32(info.fallTime);
        packet.writeBytes(reinterpret_cast<const uint8_t*>(&info.jumpVelocity), sizeof(float));

        // Extended fall data if far falling
        if (info.hasFlag(MovementFlags::FALLINGFAR)) {
            packet.writeBytes(reinterpret_cast<const uint8_t*>(&info.jumpSinAngle), sizeof(float));
            packet.writeBytes(reinterpret_cast<const uint8_t*>(&info.jumpCosAngle), sizeof(float));
            packet.writeBytes(reinterpret_cast<const uint8_t*>(&info.jumpXYSpeed), sizeof(float));
        }
    }

    LOG_DEBUG("Built movement packet: opcode=0x", std::hex, static_cast<uint16_t>(opcode), std::dec);
    LOG_DEBUG("  Flags: 0x", std::hex, info.flags, std::dec);
    LOG_DEBUG("  Position: (", info.x, ", ", info.y, ", ", info.z, ")");
    LOG_DEBUG("  Orientation: ", info.orientation);

    return packet;
}

uint64_t UpdateObjectParser::readPackedGuid(network::Packet& packet) {
    // Read packed GUID format:
    // First byte is a mask indicating which bytes are present
    uint8_t mask = packet.readUInt8();

    if (mask == 0) {
        return 0;
    }

    uint64_t guid = 0;
    for (int i = 0; i < 8; ++i) {
        if (mask & (1 << i)) {
            uint8_t byte = packet.readUInt8();
            guid |= (static_cast<uint64_t>(byte) << (i * 8));
        }
    }

    return guid;
}

bool UpdateObjectParser::parseMovementBlock(network::Packet& packet, UpdateBlock& block) {
    // Skip movement flags and other movement data for now
    // This is a simplified implementation

    // Read movement flags (not used yet)
    /*uint32_t flags =*/ packet.readUInt32();
    /*uint16_t flags2 =*/ packet.readUInt16();

    // Read timestamp (not used yet)
    /*uint32_t time =*/ packet.readUInt32();

    // Read position
    block.x = packet.readFloat();
    block.y = packet.readFloat();
    block.z = packet.readFloat();
    block.orientation = packet.readFloat();

    block.hasMovement = true;

    LOG_DEBUG("  Movement: (", block.x, ", ", block.y, ", ", block.z, "), orientation=", block.orientation);

    // TODO: Parse additional movement fields based on flags
    // For now, we'll skip them to keep this simple

    return true;
}

bool UpdateObjectParser::parseUpdateFields(network::Packet& packet, UpdateBlock& block) {
    // Read number of blocks (each block is 32 fields = 32 bits)
    uint8_t blockCount = packet.readUInt8();

    if (blockCount == 0) {
        return true; // No fields to update
    }

    LOG_DEBUG("  Parsing ", (int)blockCount, " field blocks");

    // Read update mask
    std::vector<uint32_t> updateMask(blockCount);
    for (int i = 0; i < blockCount; ++i) {
        updateMask[i] = packet.readUInt32();
    }

    // Read field values for each bit set in mask
    for (int blockIdx = 0; blockIdx < blockCount; ++blockIdx) {
        uint32_t mask = updateMask[blockIdx];

        for (int bit = 0; bit < 32; ++bit) {
            if (mask & (1 << bit)) {
                uint16_t fieldIndex = blockIdx * 32 + bit;
                uint32_t value = packet.readUInt32();
                block.fields[fieldIndex] = value;

                LOG_DEBUG("    Field[", fieldIndex, "] = 0x", std::hex, value, std::dec);
            }
        }
    }

    LOG_DEBUG("  Parsed ", block.fields.size(), " fields");

    return true;
}

bool UpdateObjectParser::parseUpdateBlock(network::Packet& packet, UpdateBlock& block) {
    // Read update type
    uint8_t updateTypeVal = packet.readUInt8();
    block.updateType = static_cast<UpdateType>(updateTypeVal);

    LOG_DEBUG("Update block: type=", (int)updateTypeVal);

    switch (block.updateType) {
        case UpdateType::VALUES: {
            // Partial update - changed fields only
            block.guid = readPackedGuid(packet);
            LOG_DEBUG("  VALUES update for GUID: 0x", std::hex, block.guid, std::dec);

            return parseUpdateFields(packet, block);
        }

        case UpdateType::MOVEMENT: {
            // Movement update
            block.guid = readPackedGuid(packet);
            LOG_DEBUG("  MOVEMENT update for GUID: 0x", std::hex, block.guid, std::dec);

            return parseMovementBlock(packet, block);
        }

        case UpdateType::CREATE_OBJECT:
        case UpdateType::CREATE_OBJECT2: {
            // Create new object with full data
            block.guid = readPackedGuid(packet);
            LOG_DEBUG("  CREATE_OBJECT for GUID: 0x", std::hex, block.guid, std::dec);

            // Read object type
            uint8_t objectTypeVal = packet.readUInt8();
            block.objectType = static_cast<ObjectType>(objectTypeVal);
            LOG_DEBUG("  Object type: ", (int)objectTypeVal);

            // Parse movement if present
            bool hasMovement = parseMovementBlock(packet, block);
            if (!hasMovement) {
                return false;
            }

            // Parse update fields
            return parseUpdateFields(packet, block);
        }

        case UpdateType::OUT_OF_RANGE_OBJECTS: {
            // Objects leaving view range - handled differently
            LOG_DEBUG("  OUT_OF_RANGE_OBJECTS (skipping in block parser)");
            return true;
        }

        case UpdateType::NEAR_OBJECTS: {
            // Objects entering view range - handled differently
            LOG_DEBUG("  NEAR_OBJECTS (skipping in block parser)");
            return true;
        }

        default:
            LOG_WARNING("Unknown update type: ", (int)updateTypeVal);
            return false;
    }
}

bool UpdateObjectParser::parse(network::Packet& packet, UpdateObjectData& data) {
    LOG_INFO("Parsing SMSG_UPDATE_OBJECT");

    // Read block count
    data.blockCount = packet.readUInt32();
    LOG_INFO("  Block count: ", data.blockCount);

    // Check for out-of-range objects first
    if (packet.getReadPos() + 1 <= packet.getSize()) {
        uint8_t firstByte = packet.readUInt8();

        if (firstByte == static_cast<uint8_t>(UpdateType::OUT_OF_RANGE_OBJECTS)) {
            // Read out-of-range GUID count
            uint32_t count = packet.readUInt32();
            LOG_INFO("  Out-of-range objects: ", count);

            for (uint32_t i = 0; i < count; ++i) {
                uint64_t guid = readPackedGuid(packet);
                data.outOfRangeGuids.push_back(guid);
                LOG_DEBUG("    Out of range: 0x", std::hex, guid, std::dec);
            }

            // Done - packet may have more blocks after this
            // Reset read position to after the first byte if needed
        } else {
            // Not out-of-range, rewind
            packet.setReadPos(packet.getReadPos() - 1);
        }
    }

    // Parse update blocks
    data.blocks.reserve(data.blockCount);

    for (uint32_t i = 0; i < data.blockCount; ++i) {
        LOG_DEBUG("Parsing block ", i + 1, " / ", data.blockCount);

        UpdateBlock block;
        if (!parseUpdateBlock(packet, block)) {
            LOG_ERROR("Failed to parse update block ", i + 1);
            return false;
        }

        data.blocks.push_back(block);
    }

    LOG_INFO("Successfully parsed ", data.blocks.size(), " update blocks");

    return true;
}

bool DestroyObjectParser::parse(network::Packet& packet, DestroyObjectData& data) {
    // SMSG_DESTROY_OBJECT format:
    // uint64 guid
    // uint8 isDeath (0 = despawn, 1 = death)

    if (packet.getSize() < 9) {
        LOG_ERROR("SMSG_DESTROY_OBJECT packet too small: ", packet.getSize(), " bytes");
        return false;
    }

    data.guid = packet.readUInt64();
    data.isDeath = (packet.readUInt8() != 0);

    LOG_INFO("Parsed SMSG_DESTROY_OBJECT:");
    LOG_INFO("  GUID: 0x", std::hex, data.guid, std::dec);
    LOG_INFO("  Is death: ", data.isDeath ? "yes" : "no");

    return true;
}

network::Packet MessageChatPacket::build(ChatType type,
                                          ChatLanguage language,
                                          const std::string& message,
                                          const std::string& target) {
    network::Packet packet(static_cast<uint16_t>(Opcode::CMSG_MESSAGECHAT));

    // Write chat type
    packet.writeUInt32(static_cast<uint32_t>(type));

    // Write language
    packet.writeUInt32(static_cast<uint32_t>(language));

    // Write target (for whispers) or channel name
    if (type == ChatType::WHISPER) {
        packet.writeString(target);
    } else if (type == ChatType::CHANNEL) {
        packet.writeString(target);  // Channel name
    }

    // Write message
    packet.writeString(message);

    LOG_DEBUG("Built CMSG_MESSAGECHAT packet");
    LOG_DEBUG("  Type: ", static_cast<int>(type));
    LOG_DEBUG("  Language: ", static_cast<int>(language));
    LOG_DEBUG("  Message: ", message);

    return packet;
}

bool MessageChatParser::parse(network::Packet& packet, MessageChatData& data) {
    // SMSG_MESSAGECHAT format (WoW 3.3.5a):
    // uint8 type
    // uint32 language
    // uint64 senderGuid
    // uint32 unknown (always 0)
    // [type-specific data]
    // uint32 messageLength
    // string message
    // uint8 chatTag

    if (packet.getSize() < 15) {
        LOG_ERROR("SMSG_MESSAGECHAT packet too small: ", packet.getSize(), " bytes");
        return false;
    }

    // Read chat type
    uint8_t typeVal = packet.readUInt8();
    data.type = static_cast<ChatType>(typeVal);

    // Read language
    uint32_t langVal = packet.readUInt32();
    data.language = static_cast<ChatLanguage>(langVal);

    // Read sender GUID
    data.senderGuid = packet.readUInt64();

    // Read unknown field
    packet.readUInt32();

    // Type-specific data
    switch (data.type) {
        case ChatType::MONSTER_SAY:
        case ChatType::MONSTER_YELL:
        case ChatType::MONSTER_EMOTE: {
            // Read sender name length + name
            uint32_t nameLen = packet.readUInt32();
            if (nameLen > 0 && nameLen < 256) {
                std::vector<char> nameBuffer(nameLen);
                for (uint32_t i = 0; i < nameLen; ++i) {
                    nameBuffer[i] = static_cast<char>(packet.readUInt8());
                }
                data.senderName = std::string(nameBuffer.begin(), nameBuffer.end());
            }

            // Read receiver GUID (usually 0 for monsters)
            data.receiverGuid = packet.readUInt64();
            break;
        }

        case ChatType::WHISPER_INFORM: {
            // Read receiver name
            data.receiverName = packet.readString();
            break;
        }

        case ChatType::CHANNEL: {
            // Read channel name
            data.channelName = packet.readString();
            break;
        }

        case ChatType::ACHIEVEMENT:
        case ChatType::GUILD_ACHIEVEMENT: {
            // Read achievement ID
            packet.readUInt32();
            break;
        }

        default:
            // No additional data for most types
            break;
    }

    // Read message length
    uint32_t messageLen = packet.readUInt32();

    // Read message
    if (messageLen > 0 && messageLen < 8192) {
        std::vector<char> msgBuffer(messageLen);
        for (uint32_t i = 0; i < messageLen; ++i) {
            msgBuffer[i] = static_cast<char>(packet.readUInt8());
        }
        data.message = std::string(msgBuffer.begin(), msgBuffer.end());
    }

    // Read chat tag
    data.chatTag = packet.readUInt8();

    LOG_DEBUG("Parsed SMSG_MESSAGECHAT:");
    LOG_DEBUG("  Type: ", getChatTypeString(data.type));
    LOG_DEBUG("  Language: ", static_cast<int>(data.language));
    LOG_DEBUG("  Sender GUID: 0x", std::hex, data.senderGuid, std::dec);
    if (!data.senderName.empty()) {
        LOG_DEBUG("  Sender name: ", data.senderName);
    }
    if (!data.channelName.empty()) {
        LOG_DEBUG("  Channel: ", data.channelName);
    }
    LOG_DEBUG("  Message: ", data.message);
    LOG_DEBUG("  Chat tag: 0x", std::hex, (int)data.chatTag, std::dec);

    return true;
}

const char* getChatTypeString(ChatType type) {
    switch (type) {
        case ChatType::SAY: return "SAY";
        case ChatType::PARTY: return "PARTY";
        case ChatType::RAID: return "RAID";
        case ChatType::GUILD: return "GUILD";
        case ChatType::OFFICER: return "OFFICER";
        case ChatType::YELL: return "YELL";
        case ChatType::WHISPER: return "WHISPER";
        case ChatType::WHISPER_INFORM: return "WHISPER_INFORM";
        case ChatType::EMOTE: return "EMOTE";
        case ChatType::TEXT_EMOTE: return "TEXT_EMOTE";
        case ChatType::SYSTEM: return "SYSTEM";
        case ChatType::MONSTER_SAY: return "MONSTER_SAY";
        case ChatType::MONSTER_YELL: return "MONSTER_YELL";
        case ChatType::MONSTER_EMOTE: return "MONSTER_EMOTE";
        case ChatType::CHANNEL: return "CHANNEL";
        case ChatType::CHANNEL_JOIN: return "CHANNEL_JOIN";
        case ChatType::CHANNEL_LEAVE: return "CHANNEL_LEAVE";
        case ChatType::CHANNEL_LIST: return "CHANNEL_LIST";
        case ChatType::CHANNEL_NOTICE: return "CHANNEL_NOTICE";
        case ChatType::CHANNEL_NOTICE_USER: return "CHANNEL_NOTICE_USER";
        case ChatType::AFK: return "AFK";
        case ChatType::DND: return "DND";
        case ChatType::IGNORED: return "IGNORED";
        case ChatType::SKILL: return "SKILL";
        case ChatType::LOOT: return "LOOT";
        case ChatType::BATTLEGROUND: return "BATTLEGROUND";
        case ChatType::BATTLEGROUND_LEADER: return "BATTLEGROUND_LEADER";
        case ChatType::RAID_LEADER: return "RAID_LEADER";
        case ChatType::RAID_WARNING: return "RAID_WARNING";
        case ChatType::ACHIEVEMENT: return "ACHIEVEMENT";
        case ChatType::GUILD_ACHIEVEMENT: return "GUILD_ACHIEVEMENT";
        default: return "UNKNOWN";
    }
}

} // namespace game
} // namespace wowee
