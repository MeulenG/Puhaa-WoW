#pragma once

#include <cstdint>

namespace wowee {
namespace auth {

// Authentication server opcodes
enum class AuthOpcode : uint8_t {
    LOGON_CHALLENGE = 0x00,
    LOGON_PROOF = 0x01,
    RECONNECT_CHALLENGE = 0x02,
    RECONNECT_PROOF = 0x03,
    REALM_LIST = 0x10,
};

// LOGON_CHALLENGE response status codes
enum class AuthResult : uint8_t {
    SUCCESS = 0x00,
    UNKNOWN0 = 0x01,
    UNKNOWN1 = 0x02,
    ACCOUNT_BANNED = 0x03,
    ACCOUNT_INVALID = 0x04,
    PASSWORD_INVALID = 0x05,
    ALREADY_ONLINE = 0x06,
    OUT_OF_CREDIT = 0x07,
    BUSY = 0x08,
    BUILD_INVALID = 0x09,
    BUILD_UPDATE = 0x0A,
    INVALID_SERVER = 0x0B,
    ACCOUNT_SUSPENDED = 0x0C,
    ACCESS_DENIED = 0x0D,
    SURVEY = 0x0E,
    PARENTAL_CONTROL = 0x0F,
    LOCK_ENFORCED = 0x10,
    TRIAL_EXPIRED = 0x11,
    BATTLE_NET = 0x12,
};

const char* getAuthResultString(AuthResult result);

} // namespace auth
} // namespace wowee
