// File: main.cpp
// Description: Bootstraps the backend engine and launches the HTTP server
//              powering the browser-based tank red envelope game.

#include "backend/GameEngine.hpp"
#include "backend/Logger.hpp"
#include "frontend/LayoutManager.hpp"
#include "frontend/WebServer.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int sanitizePort(int port) {
    if (port < 1024 || port > 65535) {
        std::cerr << "Requested port " << port
                  << " is outside the permitted range (1024-65535). Using default 8080.\n";
        return 8080;
    }
    return port;
}

int resolvePort(int argc, char* argv[]) {
    int port = 8080;

    if (const char* envPort = std::getenv("TANK_GAME_PORT")) {
        try {
            port = std::stoi(envPort);
        } catch (...) {
            std::cerr << "Invalid TANK_GAME_PORT value; falling back to default 8080.\n";
            port = 8080;
        }
    }

    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            std::cerr << "Invalid command-line port; retaining previous value " << port << ".\n";
        }
    }

    return sanitizePort(port);
}

}  // namespace

int main(int argc, char* argv[]) {
    backend::Logger::instance().initialize("logs/server.log");
    backend::Logger::instance().log("Initializing tank red envelope game.");

    backend::GameConfig config;
    config.worldWidth = 30;
    config.worldHeight = 20;
    config.initialEnvelopeCount = 12;
    config.timeLimitSeconds = 60;

    backend::GameEngine engine(config);
    const auto seed =
        static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count());
    engine.setRandomSeed(seed);
    engine.reset();
    backend::Logger::instance().log("Engine seeded with value " + std::to_string(seed) + ".");

    const int port = resolvePort(argc, argv);
    backend::Logger::instance().log("Resolved HTTP port " + std::to_string(port) + ".");
    frontend::LayoutManager layoutManager;
    layoutManager.initialize();
    frontend::WebServer server(engine, layoutManager, "web", port);

    try {
        backend::Logger::instance().log("Starting web server event loop.");
        server.run();
    } catch (const std::exception& ex) {
        backend::Logger::instance().log(std::string("Server terminated: ") + ex.what());
        std::cerr << "Server terminated: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
