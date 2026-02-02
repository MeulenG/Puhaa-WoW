# Quick Start Guide

## Current Status

The native wowee client foundation is **complete and functional**! The application successfully:

✅ Opens a native Linux window (1920x1080)
✅ Creates an OpenGL 3.3+ rendering context
✅ Initializes SDL2 for window management and input
✅ Sets up ImGui for UI rendering (ready to use)
✅ Implements a complete application lifecycle

## What Works Right Now

```bash
# Build the project
cd wowee/build
cmake ..
make -j$(nproc)

# Run the application
./bin/wowee
```

The application will:
- Open a window with SDL2
- Initialize OpenGL 3.3+ with GLEW
- Set up the rendering pipeline
- Run the main game loop
- Handle input and events
- Close cleanly on window close or Escape key

## What You See

Currently, the window displays a blue gradient background (clear color: 0.2, 0.3, 0.5). This is the base rendering loop working correctly.

## Next Steps

The foundation is in place. Here's what needs implementation next (in recommended order):

### 1. Authentication System (High Priority)
**Files:** `src/auth/srp.cpp`, `src/auth/auth_handler.cpp`
**Goal:** Implement SRP6a authentication protocol

Reference the original wowee implementation:
- `/wowee/src/lib/auth/handler.js` - Auth packet flow
- `/wowee/src/lib/crypto/srp.js` - SRP implementation

Key tasks:
- Implement `LOGON_CHALLENGE` packet
- Implement `LOGON_PROOF` packet
- Port SHA1 and big integer math (already have OpenSSL)

### 2. Network Protocol (High Priority)
**Files:** `src/game/game_handler.cpp`, `src/game/opcodes.hpp`
**Goal:** Implement World of Warcraft 3.3.5a packet protocol

Reference:
- `/wowee/src/lib/game/handler.js` - Packet handlers
- `/wowee/src/lib/game/opcode.js` - Opcode definitions
- [WoWDev Wiki](https://wowdev.wiki/) - Protocol documentation

Key packets to implement:
- `SMSG_AUTH_CHALLENGE` / `CMSG_AUTH_SESSION`
- `CMSG_CHAR_ENUM` / `SMSG_CHAR_ENUM`
- `CMSG_PLAYER_LOGIN`
- Movement packets (CMSG_MOVE_*)

### 3. Asset Pipeline (Medium Priority)
**Files:** `src/pipeline/*.cpp`
**Goal:** Load and parse WoW game assets

Formats to implement:
- **BLP** (`blp_loader.cpp`) - Texture format
- **M2** (`m2_loader.cpp`) - Character/creature models
- **ADT** (`adt_loader.cpp`) - Terrain chunks
- **WMO** (`wmo_loader.cpp`) - World map objects (buildings)
- **DBC** (`dbc_loader.cpp`) - Game databases

Resources:
- [WoWDev Wiki - File Formats](https://wowdev.wiki/)
- Original parsers in `/wowee/src/lib/pipeline/`
- StormLib is already linked for MPQ archive reading

### 4. Terrain Rendering (Medium Priority)
**Files:** `src/rendering/renderer.cpp`, `src/game/world.cpp`
**Goal:** Render game world terrain

Tasks:
- Load ADT terrain chunks
- Parse height maps and texture layers
- Create OpenGL meshes from terrain data
- Implement chunk streaming based on camera position
- Add frustum culling

Shaders are ready at `assets/shaders/basic.vert` and `basic.frag`.

### 5. Character Rendering (Low Priority)
**Files:** `src/rendering/renderer.cpp`
**Goal:** Render player and NPC models

Tasks:
- Load M2 model format
- Implement skeletal animation system
- Parse animation tracks
- Implement vertex skinning in shaders
- Render character equipment

### 6. UI Screens (Low Priority)
**Files:** `src/ui/*.cpp`
**Goal:** Create game UI with ImGui

Screens to implement:
- Authentication screen (username/password input)
- Realm selection screen
- Character selection screen
- In-game UI (chat, action bars, character panel)

ImGui is already initialized and ready to use!

## Development Tips

### Adding New Features

1. **Window/Input:** Use `window->getSDLWindow()` and `Input::getInstance()`
2. **Rendering:** Add render calls in `Application::render()`
3. **Game Logic:** Add updates in `Application::update(float deltaTime)`
4. **UI:** Use ImGui in `UIManager::render()`

### Debugging

```cpp
#include "core/logger.hpp"

LOG_DEBUG("Debug message");
LOG_INFO("Info message");
LOG_WARNING("Warning message");
LOG_ERROR("Error message");
```

### State Management

The application uses state machine pattern:
```cpp
AppState::AUTHENTICATION     // Login screen
AppState::REALM_SELECTION    // Choose server
AppState::CHARACTER_SELECTION // Choose character
AppState::IN_GAME           // Playing the game
AppState::DISCONNECTED      // Connection lost
```

Change state with:
```cpp
Application::getInstance().setState(AppState::IN_GAME);
```

## Testing Without a Server

For development, you can:

1. **Mock authentication** - Skip SRP and go straight to realm selection
2. **Load local assets** - Test terrain/model rendering without network
3. **Stub packet handlers** - Return fake data for testing UI

## Performance Notes

Current configuration:
- **VSync:** Enabled (60 FPS cap)
- **Resolution:** 1920x1080 (configurable in `Application::initialize()`)
- **OpenGL:** 3.3 Core Profile
- **Rendering:** Deferred until `renderWorld()` is implemented

## Useful Resources

- **Original Wowee:** `/woweer/` directory - JavaScript reference implementation
- **WoWDev Wiki:** https://wowdev.wiki/ - File formats and protocol docs
- **TrinityCore:** https://github.com/TrinityCore/TrinityCore - Server reference
- **ImGui Demo:** Run `ImGui::ShowDemoWindow()` for UI examples

## Known Issues

None! The foundation is solid and ready for feature implementation.

## Need Help?

1. Check the original wowee codebase for JavaScript reference implementations
2. Consult WoWDev Wiki for protocol and format specifications
3. Look at TrinityCore source for server-side packet handling
4. Use `LOG_DEBUG()` extensively for troubleshooting

---

**Ready to build a native WoW client!** 🎮
