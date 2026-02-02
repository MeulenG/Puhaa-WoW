# Architecture Overview

## System Design

Wowee follows a modular architecture with clear separation of concerns:

```
┌─────────────────────────────────────────────┐
│           Application (main loop)            │
│  - State management (auth/realms/game)      │
│  - Update cycle (60 FPS)                    │
│  - Event dispatch                           │
└──────────────┬──────────────────────────────┘
               │
       ┌───────┴────────┐
       │                │
┌──────▼──────┐  ┌─────▼──────┐
│   Window    │  │   Input    │
│  (SDL2)     │  │ (Keyboard/ │
│             │  │   Mouse)   │
└──────┬──────┘  └─────┬──────┘
       │                │
       └───────┬────────┘
               │
    ┌──────────┴──────────┐
    │                     │
┌───▼────────┐   ┌───────▼──────┐
│  Renderer  │   │  UI Manager  │
│ (OpenGL)   │   │   (ImGui)    │
└───┬────────┘   └──────────────┘
    │
    ├─ Camera
    ├─ Scene Graph
    ├─ Shaders
    ├─ Meshes
    └─ Textures
```

## Core Systems

### 1. Application Layer (`src/core/`)

**Application** - Main controller
- Owns all subsystems
- Manages application state
- Runs update/render loop
- Handles lifecycle (init/shutdown)

**Window** - SDL2 wrapper
- Creates window and OpenGL context
- Handles resize events
- Manages VSync and fullscreen

**Input** - Input management
- Keyboard state tracking
- Mouse position and buttons
- Mouse locking for camera control

**Logger** - Logging system
- Thread-safe logging
- Multiple log levels (DEBUG, INFO, WARNING, ERROR, FATAL)
- Timestamp formatting

### 2. Rendering System (`src/rendering/`)

**Renderer** - Main rendering coordinator
- Manages OpenGL state
- Coordinates frame rendering
- Owns camera and scene

**Camera** - View/projection matrices
- Position and orientation
- FOV and aspect ratio
- View frustum (for culling)

**Scene** - Scene graph
- Mesh collection
- Spatial organization
- Visibility determination

**Shader** - GLSL program wrapper
- Loads vertex/fragment shaders
- Uniform management
- Compilation and linking

**Mesh** - Geometry container
- Vertex buffer (position, normal, texcoord)
- Index buffer
- VAO/VBO/EBO management

**Texture** - Texture management
- Loading (will support BLP format)
- OpenGL texture object
- Mipmap generation

**Material** - Surface properties
- Shader assignment
- Texture binding
- Color/properties

### 3. Networking (`src/network/`)

**Socket** (Abstract base class)
- Connection interface
- Packet send/receive
- Callback system

**TCPSocket** - Linux TCP sockets
- Non-blocking I/O
- Raw TCP (replaces WebSocket)
- Packet framing

**Packet** - Binary data container
- Read/write primitives
- Byte order handling
- Opcode management

### 4. Authentication (`src/auth/`)

**AuthHandler** - Auth server protocol
- Connects to port 3724
- SRP authentication flow
- Session key generation

**SRP** - Secure Remote Password
- SRP6a algorithm
- Big integer math
- Salt and verifier generation

**Crypto** - Cryptographic functions
- SHA1 hashing (OpenSSL)
- Random number generation
- Encryption helpers

### 5. Game Logic (`src/game/`)

**GameHandler** - World server protocol
- Connects to port 8129
- Packet handlers for all opcodes
- Session management

**World** - Game world state
- Map loading
- Entity management
- Terrain streaming

**Player** - Player character
- Position and movement
- Stats and inventory
- Action queue

**Entity** - Game entities
- NPCs and creatures
- Base entity functionality
- GUID management

**Opcodes** - Protocol definitions
- Client→Server opcodes (CMSG_*)
- Server→Client opcodes (SMSG_*)
- WoW 3.3.5a specific

### 6. Asset Pipeline (`src/pipeline/`)

**MPQManager** - Archive management
- Loads .mpq files (via StormLib)
- File lookup
- Data extraction

**BLPLoader** - Texture parser
- BLP format (Blizzard texture format)
- DXT compression support
- Mipmap extraction

**M2Loader** - Model parser
- Character/creature models
- Skeletal animation data
- Bone hierarchies
- Animation sequences

**WMOLoader** - World object parser
- Buildings and structures
- Static geometry
- Portal system
- Doodad placement

**ADTLoader** - Terrain parser
- 16x16 chunks per map
- Height map data
- Texture layers (up to 4)
- Liquid data (water/lava)
- Object placement

**DBCLoader** - Database parser
- Game data tables
- Creature/spell/item definitions
- Map and area information

### 7. UI System (`src/ui/`)

**UIManager** - ImGui coordinator
- ImGui initialization
- Event handling
- Render dispatch

**AuthScreen** - Login interface
- Username/password input
- Server address configuration
- Connection status

**RealmScreen** - Server selection
- Realm list display
- Population info
- Realm type (PvP/PvE/RP)

**CharacterScreen** - Character selection
- Character list with 3D preview
- Create/delete characters
- Enter world button

**GameScreen** - In-game UI
- Chat window
- Action bars
- Character stats
- Minimap

## Data Flow Examples

### Authentication Flow
```
User Input (username/password)
    ↓
AuthHandler::authenticate()
    ↓
SRP::calculateVerifier()
    ↓
TCPSocket::send(LOGON_CHALLENGE)
    ↓
Server Response (LOGON_CHALLENGE)
    ↓
AuthHandler receives packet
    ↓
SRP::calculateProof()
    ↓
TCPSocket::send(LOGON_PROOF)
    ↓
Server Response (LOGON_PROOF) → Success
    ↓
Application::setState(REALM_SELECTION)
```

### Rendering Flow
```
Application::render()
    ↓
Renderer::beginFrame()
    ├─ glClearColor() - Clear screen
    └─ glClear() - Clear buffers
    ↓
Renderer::renderWorld(world)
    ├─ Update camera matrices
    ├─ Frustum culling
    ├─ For each visible chunk:
    │   ├─ Bind shader
    │   ├─ Set uniforms (matrices, lighting)
    │   ├─ Bind textures
    │   └─ Mesh::draw() → glDrawElements()
    └─ For each entity:
        ├─ Calculate bone transforms
        └─ Render skinned mesh
    ↓
UIManager::render()
    ├─ ImGui::NewFrame()
    ├─ Render current UI screen
    └─ ImGui::Render()
    ↓
Renderer::endFrame()
    ↓
Window::swapBuffers()
```

### Asset Loading Flow
```
World::loadMap(mapId)
    ↓
MPQManager::readFile("World/Maps/{map}/map.adt")
    ↓
ADTLoader::load(adtData)
    ├─ Parse MCNK chunks (terrain)
    ├─ Parse MCLY chunks (textures)
    ├─ Parse MCVT chunks (vertices)
    └─ Parse MCNR chunks (normals)
    ↓
For each texture reference:
    MPQManager::readFile(texturePath)
    ↓
    BLPLoader::load(blpData)
    ↓
    Texture::loadFromMemory(imageData)
    ↓
Create Mesh from vertices/normals/texcoords
    ↓
Add to Scene
    ↓
Renderer draws in next frame
```

## Threading Model

Currently **single-threaded**:
- Main thread: Window events, update, render
- Network I/O: Non-blocking in main thread
- Asset loading: Synchronous in main thread

**Future multi-threading opportunities:**
- Asset loading thread pool (background texture/model loading)
- Network thread (dedicated for socket I/O)
- Physics thread (if collision detection is added)

## Memory Management

- **Smart pointers:** Used throughout (std::unique_ptr, std::shared_ptr)
- **RAII:** All resources (OpenGL, SDL) cleaned up automatically
- **No manual memory management:** No raw new/delete
- **OpenGL resources:** Wrapped in classes with proper destructors

## Performance Considerations

### Rendering
- **Frustum culling:** Only render visible chunks
- **Batching:** Group draw calls by material
- **LOD:** Distance-based level of detail (TODO)
- **Occlusion:** Portal-based visibility (WMO system)

### Asset Streaming
- **Lazy loading:** Load chunks as player moves
- **Unloading:** Free distant chunks
- **Caching:** Keep frequently used assets in memory

### Network
- **Non-blocking I/O:** Never stall main thread
- **Packet buffering:** Handle multiple packets per frame
- **Compression:** Some packets are compressed (TODO)

## Error Handling

- **Logging:** All errors logged with context
- **Graceful degradation:** Missing assets show placeholder
- **State recovery:** Network disconnect → back to auth screen
- **No crashes:** Exceptions caught at application level

## Configuration

Currently hardcoded, future config system:
- Window size and fullscreen
- Graphics quality settings
- Server addresses
- Keybindings
- Audio volume

## Testing Strategy

**Unit Testing** (TODO):
- Packet serialization/deserialization
- SRP math functions
- Asset parsers with sample files

**Integration Testing** (TODO):
- Full auth flow against test server
- Realm list retrieval
- Character selection

**Manual Testing:**
- Visual verification of rendering
- Performance profiling
- Memory leak checking (valgrind)

## Build System

**CMake:**
- Modular target structure
- Automatic dependency discovery
- Cross-platform (Linux focus, but portable)
- Out-of-source builds

**Dependencies:**
- SDL2 (system)
- OpenGL/GLEW (system)
- OpenSSL (system)
- GLM (system or header-only)
- ImGui (submodule in extern/)
- StormLib (system, optional)

## Code Style

- **C++17 standard**
- **Namespaces:** wowee::core, wowee::rendering, etc.
- **Naming:** PascalCase for classes, camelCase for functions/variables
- **Headers:** .hpp extension
- **Includes:** Relative to project root

---

This architecture provides a solid foundation for a full-featured native WoW client!
