#include "core/application.hpp"
#include "core/logger.hpp"
#include <exception>

int main(int argc, char* argv[]) {
    try {
        wowee::core::Logger::getInstance().setLogLevel(wowee::core::LogLevel::DEBUG);
        LOG_INFO("=== Wowser Native Client ===");
        LOG_INFO("Starting application...");

        wowee::core::Application app;

        if (!app.initialize()) {
            LOG_FATAL("Failed to initialize application");
            return 1;
        }

        app.run();
        app.shutdown();

        LOG_INFO("Application exited successfully");
        return 0;
    }
    catch (const std::exception& e) {
        LOG_FATAL("Unhandled exception: ", e.what());
        return 1;
    }
    catch (...) {
        LOG_FATAL("Unknown exception occurred");
        return 1;
    }
}
