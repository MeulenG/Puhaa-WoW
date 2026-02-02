#include "audio/music_manager.hpp"
#include "pipeline/asset_manager.hpp"
#include "core/logger.hpp"
#include <fstream>
#include <cstdlib>
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

namespace wowee {
namespace audio {

MusicManager::MusicManager() {
    tempFilePath = "/tmp/wowee_music.mp3";
}

MusicManager::~MusicManager() {
    shutdown();
}

bool MusicManager::initialize(pipeline::AssetManager* assets) {
    assetManager = assets;
    LOG_INFO("Music manager initialized");
    return true;
}

void MusicManager::shutdown() {
    stopCurrentProcess();
    // Clean up temp file
    std::remove(tempFilePath.c_str());
}

void MusicManager::playMusic(const std::string& mpqPath, bool loop) {
    if (!assetManager) return;
    if (mpqPath == currentTrack && playing) return;

    // Read music file from MPQ
    auto data = assetManager->readFile(mpqPath);
    if (data.empty()) {
        LOG_WARNING("Music: Could not read: ", mpqPath);
        return;
    }

    // Stop current playback
    stopCurrentProcess();

    // Write to temp file
    std::ofstream out(tempFilePath, std::ios::binary);
    if (!out) {
        LOG_ERROR("Music: Could not write temp file");
        return;
    }
    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    out.close();

    // Play with ffplay in background
    pid_t pid = fork();
    if (pid == 0) {
        // Child process — create new process group so we can kill all children
        setpgid(0, 0);
        // Redirect output to /dev/null
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);

        if (loop) {
            execlp("ffplay", "ffplay", "-nodisp", "-autoexit", "-loop", "0",
                   "-volume", "30", tempFilePath.c_str(), nullptr);
        } else {
            execlp("ffplay", "ffplay", "-nodisp", "-autoexit",
                   "-volume", "30", tempFilePath.c_str(), nullptr);
        }
        _exit(1);  // exec failed
    } else if (pid > 0) {
        playerPid = pid;
        playing = true;
        currentTrack = mpqPath;
        LOG_INFO("Music: Playing ", mpqPath);
    } else {
        LOG_ERROR("Music: fork() failed");
    }
}

void MusicManager::stopMusic(float fadeMs) {
    (void)fadeMs;  // ffplay doesn't support fade easily
    stopCurrentProcess();
    playing = false;
    currentTrack.clear();
}

void MusicManager::crossfadeTo(const std::string& mpqPath, float fadeMs) {
    if (mpqPath == currentTrack && playing) return;

    // Simple implementation: stop and start (no actual crossfade with subprocess)
    if (fadeMs > 0 && playing) {
        crossfading = true;
        pendingTrack = mpqPath;
        fadeTimer = 0.0f;
        fadeDuration = fadeMs / 1000.0f;
        stopCurrentProcess();
    } else {
        playMusic(mpqPath);
    }
}

void MusicManager::update(float deltaTime) {
    // Check if player process is still running
    if (playerPid > 0) {
        int status;
        pid_t result = waitpid(playerPid, &status, WNOHANG);
        if (result == playerPid) {
            // Process ended
            playerPid = -1;
            playing = false;
        }
    }

    // Handle crossfade
    if (crossfading) {
        fadeTimer += deltaTime;
        if (fadeTimer >= fadeDuration * 0.3f) {
            // Start new track after brief pause
            crossfading = false;
            playMusic(pendingTrack);
            pendingTrack.clear();
        }
    }
}

void MusicManager::stopCurrentProcess() {
    if (playerPid > 0) {
        // Kill the entire process group (ffplay may spawn children)
        kill(-playerPid, SIGTERM);
        kill(playerPid, SIGTERM);
        int status;
        waitpid(playerPid, &status, 0);
        playerPid = -1;
        playing = false;
    }
}

} // namespace audio
} // namespace wowee
