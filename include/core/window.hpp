#pragma once

#include <string>
#include <memory>
#include <SDL2/SDL.h>

namespace wowee {
namespace core {

struct WindowConfig {
    std::string title = "Wowser Native";
    int width = 1920;
    int height = 1080;
    bool fullscreen = false;
    bool vsync = true;
    bool resizable = true;
};

class Window {
public:
    explicit Window(const WindowConfig& config);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool initialize();
    void shutdown();

    void swapBuffers();
    void pollEvents();

    bool shouldClose() const { return shouldCloseFlag; }
    void setShouldClose(bool value) { shouldCloseFlag = value; }

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    float getAspectRatio() const { return static_cast<float>(width) / height; }

    SDL_Window* getSDLWindow() const { return window; }
    SDL_GLContext getGLContext() const { return glContext; }

private:
    WindowConfig config;
    SDL_Window* window = nullptr;
    SDL_GLContext glContext = nullptr;

    int width;
    int height;
    bool shouldCloseFlag = false;
};

} // namespace core
} // namespace wowee
