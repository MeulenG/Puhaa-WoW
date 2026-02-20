# Build Instructions

This project builds as a native C++ client for WoW 3.3.5a in online mode.

## 1. Install Dependencies

### Ubuntu / Debian

```bash
sudo apt update
sudo apt install -y \
  cmake build-essential pkg-config git \
  libsdl2-dev libglew-dev libglm-dev \
  libssl-dev zlib1g-dev \
  libavformat-dev libavcodec-dev libswscale-dev libavutil-dev \
  libstorm-dev libunicorn-dev
```

### Arch

```bash
sudo pacman -S --needed \
  cmake base-devel pkgconf git \
  sdl2 glew glm openssl zlib ffmpeg stormlib
```

## 2. Clone + Prepare

```bash
git clone https://github.com/MeulenG/Puhaa-WoW.git --recurse-submodules -j"$(nproc)"
cd Puhaa-WoW
```

## 3. Configure + Build

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)"
```

Binary output:

```text
build/bin/puhaa-wow
```

## 4. Provide WoW Data (Extract + Manifest)

Puhaa-WoW loads assets from an extracted loose-file tree indexed by `manifest.json` (it does not read MPQs at runtime).

### Option A: Extract into `./Data/` (recommended)

Run:

```bash
# WotLK 3.3.5a example
./extract_assets.sh /path/to/WoW/Data wotlk
```

The output includes:

```text
Data/
  manifest.json
  interface/
  sound/
  world/
  expansions/
```

### Option B: Use an existing extracted data tree

Point Puhaa-WoW at your extracted `Data/` directory:

```bash
export WOW_DATA_PATH=/path/to/extracted/Data
```

## 5. Run

```bash
./build/bin/puhaa-wow
```

### Data not found at runtime

Verify `Data/manifest.json` exists (or re-run `./extract_assets.sh ...`), or set:

```bash
export WOW_DATA_PATH=/path/to/extracted/Data
```

### Clean rebuild

```bash
rm -rf build
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)"
```
