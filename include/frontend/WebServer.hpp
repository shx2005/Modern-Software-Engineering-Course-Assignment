// File: WebServer.hpp
// Description: Declares a lightweight HTTP server that exposes the game engine
//              state to a browser-based frontend.

#pragma once

#include "backend/GameEngine.hpp"

#include <mutex>
#include <string>

namespace frontend {

class WebServer {
public:
    WebServer(backend::GameEngine& engine,
              std::string staticDir,
              int port = 8080);

    void run();

    int port() const noexcept;

private:
    backend::GameEngine& m_engine;
    std::string m_staticDir;
    int m_port;
    std::mutex m_engineMutex;

    void handleClient(int clientSocket);
    void sendHttpResponse(int clientSocket,
                          const std::string& statusLine,
                          const std::string& body,
                          const std::string& contentType = "text/plain");
    void sendNotFound(int clientSocket);
    void sendBadRequest(int clientSocket, const std::string& message);
    void sendInternalError(int clientSocket, const std::string& message);
    std::string handleApiRequest(const std::string& method,
                                 const std::string& path,
                                 const std::string& body,
                                 std::string& contentType,
                                 int& statusCode);
    std::string buildStateJson();
    std::string loadStaticFile(const std::string& targetPath, std::string& contentType);
    backend::MoveDirection parseDirection(const std::string& payload) const;
<<<<<<< HEAD
=======
    std::string parseAction(const std::string& payload) const;
>>>>>>> bdcdecfa8616715985974d5c31139b0637afe2d3
};

}  // namespace frontend
