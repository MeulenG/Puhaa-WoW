#include "auth/auth_opcodes.hpp"

namespace wowee {
namespace auth {

const char* getAuthResultString(AuthResult result) {
    switch (result) {
        case AuthResult::SUCCESS: return "Success";
        case AuthResult::UNKNOWN0: return "Unknown Error 0";
        case AuthResult::UNKNOWN1: return "Unknown Error 1";
        case AuthResult::ACCOUNT_BANNED: return "Account Banned";
        case AuthResult::ACCOUNT_INVALID: return "Account Invalid";
        case AuthResult::PASSWORD_INVALID: return "Password Invalid";
        case AuthResult::ALREADY_ONLINE: return "Already Online";
        case AuthResult::OUT_OF_CREDIT: return "Out of Credit";
        case AuthResult::BUSY: return "Server Busy";
        case AuthResult::BUILD_INVALID: return "Build Invalid";
        case AuthResult::BUILD_UPDATE: return "Build Update Required";
        case AuthResult::INVALID_SERVER: return "Invalid Server";
        case AuthResult::ACCOUNT_SUSPENDED: return "Account Suspended";
        case AuthResult::ACCESS_DENIED: return "Access Denied";
        case AuthResult::SURVEY: return "Survey Required";
        case AuthResult::PARENTAL_CONTROL: return "Parental Control";
        case AuthResult::LOCK_ENFORCED: return "Lock Enforced";
        case AuthResult::TRIAL_EXPIRED: return "Trial Expired";
        case AuthResult::BATTLE_NET: return "Battle.net Error";
        default: return "Unknown";
    }
}

} // namespace auth
} // namespace wowee
