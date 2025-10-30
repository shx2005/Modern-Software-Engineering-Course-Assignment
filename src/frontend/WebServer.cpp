// File: WebServer.cpp
// Description: Implements a minimal HTTP server providing REST endpoints and
//              serving static assets for the browser-based frontend.

#include "frontend/WebServer.hpp"

#include "backend/Logger.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace frontend {

namespace {
constexpr std::size_t kReadBufferSize = 4096;
}

WebServer::WebServer(backend::GameEngine& engine,
                     std::string staticDir,
                     int port)
    : m_engine(engine),
      m_staticDir(std::move(staticDir)),
      m_port(port) {}

int WebServer::port() const noexcept {
    return m_port;
}

void WebServer::run() {
    int serverSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        throw std::runtime_error("Failed to create server socket.");
    }

    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        ::close(serverSocket);
        throw std::runtime_error("Failed to set socket options.");
    }

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(static_cast<uint16_t>(m_port));

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        ::close(serverSocket);
        throw std::runtime_error("Failed to bind server socket.");
    }

    if (listen(serverSocket, 10) < 0) {
        ::close(serverSocket);
        throw std::runtime_error("Failed to listen on server socket.");
    }

    backend::Logger::instance().log("Web server listening on port " + std::to_string(m_port) + ".");
    std::cout << "Web server listening on port " << m_port << "\n";

    while (true) {
        sockaddr_in clientAddress {};
        socklen_t clientLen = sizeof(clientAddress);
        int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddress), &clientLen);
        if (clientSocket < 0) {
            std::cerr << "Failed to accept client connection.\n";
            continue;
        }

        std::thread(&WebServer::handleClient, this, clientSocket).detach();
    }
}

void WebServer::handleClient(int clientSocket) {
    std::string request;
    request.reserve(1024);

    char buffer[kReadBufferSize];
    ssize_t bytesRead = 0;

    // Read header section.
    while (request.find("\r\n\r\n") == std::string::npos) {
        bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            ::close(clientSocket);
            return;
        }
        request.append(buffer, static_cast<std::size_t>(bytesRead));
        if (request.size() > 65536) {
            sendBadRequest(clientSocket, "Request header too large.");
            return;
        }
    }

    const std::size_t headerEnd = request.find("\r\n\r\n");
    std::string headerPart = request.substr(0, headerEnd + 4);
    std::string body = request.substr(headerEnd + 4);

    std::istringstream headerStream(headerPart);
    std::string requestLine;
    if (!std::getline(headerStream, requestLine)) {
        sendBadRequest(clientSocket, "Malformed request line.");
        return;
    }

    if (!requestLine.empty() && requestLine.back() == '\r') {
        requestLine.pop_back();
    }

    std::istringstream requestLineStream(requestLine);
    std::string method;
    std::string path;
    std::string version;
    requestLineStream >> method >> path >> version;
    if (method.empty() || path.empty()) {
        sendBadRequest(clientSocket, "Missing method or path.");
        return;
    }

    std::string line;
    std::size_t contentLength = 0;
    while (std::getline(headerStream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            break;
        }
        const std::size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, colonPos);
        if (key == "Content-Length") {
            const std::string value = line.substr(colonPos + 1);
            try {
                contentLength = static_cast<std::size_t>(std::stoul(value));
            } catch (...) {
                sendBadRequest(clientSocket, "Invalid Content-Length header.");
                return;
            }
        }
    }

    // Read remaining body if not already received.
    while (body.size() < contentLength) {
        bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            ::close(clientSocket);
            return;
        }
        body.append(buffer, static_cast<std::size_t>(bytesRead));
    }

    std::string contentType = "text/plain";
    int statusCode = 200;
    std::string responseBody;

    backend::Logger::instance().log("Request: " + method + " " + path);

    try {
        if (method == "GET" && (path == "/" || path == "/index.html")) {
            responseBody = loadStaticFile("index.html", contentType);
        } else if (method == "GET" && path == "/state") {
            responseBody = buildStateJson();
            contentType = "application/json";
        } else if (method == "GET" && path.rfind("/static/", 0) == 0) {
            const std::string relativePath = path.substr(1);  // remove leading slash
            try {
                responseBody = loadStaticFile(relativePath, contentType);
            } catch (const std::exception& ex) {
                backend::Logger::instance().log(std::string("Static asset missing: ") + relativePath +
                                                " (" + ex.what() + ")");
                sendNotFound(clientSocket);
                return;
            }
        } else if (method == "POST" && path == "/move") {
            responseBody = handleApiRequest(method, path, body, contentType, statusCode);
        } else if (method == "POST" && path == "/reset") {
            responseBody = handleApiRequest(method, path, body, contentType, statusCode);
        } else if (method == "POST" && path == "/rain") {
            responseBody = handleApiRequest(method, path, body, contentType, statusCode);
        } else if (method == "POST" && path == "/pause") {
            responseBody = handleApiRequest(method, path, body, contentType, statusCode);
        } else if (method == "POST" && path == "/codestats") {
            responseBody = handleApiRequest(method, path, body, contentType, statusCode);
        } else {
            sendNotFound(clientSocket);
            backend::Logger::instance().log("Responded 404 for path " + path + ".");
            return;
        }
    } catch (const std::exception& ex) {
        backend::Logger::instance().log(std::string("Internal error while handling request: ") + ex.what());
        sendInternalError(clientSocket, ex.what());
        return;
    }

    std::ostringstream statusLine;
    statusLine << "HTTP/1.1 " << statusCode << " "
               << (statusCode == 200 ? "OK" : (statusCode == 400 ? "Bad Request" : "Error"));
    sendHttpResponse(clientSocket, statusLine.str(), responseBody, contentType);
}

void WebServer::sendHttpResponse(int clientSocket,
                                 const std::string& statusLine,
                                 const std::string& body,
                                 const std::string& contentType) {
    std::ostringstream response;
    response << statusLine << "\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "Connection: close\r\n\r\n";
    response << body;

    const std::string responseStr = response.str();
    send(clientSocket, responseStr.c_str(), responseStr.size(), 0);
    ::close(clientSocket);
}

void WebServer::sendNotFound(int clientSocket) {
    const std::string body = R"({"error":"Not Found"})";
    sendHttpResponse(clientSocket, "HTTP/1.1 404 Not Found", body, "application/json");
}

void WebServer::sendBadRequest(int clientSocket, const std::string& message) {
    const std::string body = R"({"error":")" + message + R"("})";
    sendHttpResponse(clientSocket, "HTTP/1.1 400 Bad Request", body, "application/json");
}

void WebServer::sendInternalError(int clientSocket, const std::string& message) {
    const std::string body = R"({"error":")" + message + R"("})";
    sendHttpResponse(clientSocket, "HTTP/1.1 500 Internal Server Error", body, "application/json");
}

std::string WebServer::handleApiRequest(const std::string& method,
                                        const std::string& path,
                                        const std::string& body,
                                        std::string& contentType,
                                        int& statusCode) {
    contentType = "application/json";

    if (method == "POST" && path == "/move") {
        backend::MoveDirection direction = parseDirection(body);
        bool moved = false;
        bool timeUp = false;
        {
            std::lock_guard<std::mutex> guard(m_engineMutex);
            timeUp = m_engine.isTimeUp();
            if (!timeUp && direction != backend::MoveDirection::None) {
                moved = m_engine.moveTank(direction);
            }
        }
        std::string logMessage = "Move request, body='" + body + "', moved=" +
                                 std::string(moved ? "true" : "false") + ", timeUp=" +
                                 std::string(timeUp ? "true" : "false") + ".";
        backend::Logger::instance().log(logMessage);
        std::ostringstream oss;
        oss << R"({"success":)" << ((moved && !timeUp) ? "true" : "false") << ",";
        oss << R"("timeUp":)" << (timeUp ? "true" : "false") << "}";
        return oss.str();
    }

    if (method == "POST" && path == "/reset") {
        {
            std::lock_guard<std::mutex> guard(m_engineMutex);
            const auto seed = static_cast<unsigned int>(
                std::chrono::system_clock::now().time_since_epoch().count());
            m_engine.setRandomSeed(seed);
            m_engine.reset();
        }
        backend::Logger::instance().log("Reset request completed and engine reseeded.");
        return R"({"success":true})";
    }

    if (method == "POST" && path == "/rain") {
        int spawned = 0;
        {
            std::lock_guard<std::mutex> guard(m_engineMutex);
            spawned = m_engine.spawnBonusEnvelopes(5, 10);
        }
        backend::Logger::instance().log("Rain request spawned " + std::to_string(spawned) +
                                        " bonus envelopes.");
        std::ostringstream oss;
        oss << R"({"success":true,"spawned":)" << spawned << "}";
        return oss.str();
    }

    if (method == "POST" && path == "/pause") {
        const std::string action = parseAction(body);
        bool paused = false;
        {
            std::lock_guard<std::mutex> guard(m_engineMutex);
            if (action == "pause") {
                m_engine.pause();
            } else if (action == "resume") {
                m_engine.resume();
            } else {
                m_engine.togglePause();
            }
            paused = m_engine.isPaused();
        }
        backend::Logger::instance().log("Pause request '" + action + "' -> " +
                                        std::string(paused ? "paused" : "running") + ".");
        std::ostringstream oss;
        oss << R"({"success":true,"paused":)" << (paused ? "true" : "false") << "}";
        return oss.str();
    }

    if (method == "POST" && path == "/pause") {
        const std::string action = parseAction(body);
        bool paused = false;
        {
            std::lock_guard<std::mutex> guard(m_engineMutex);
            if (action == "pause") {
                m_engine.pause();
            } else if (action == "resume") {
                m_engine.resume();
            } else {
                m_engine.togglePause();
            }
            paused = m_engine.isPaused();
        }
        backend::Logger::instance().log("Pause request '" + action + "' -> " +
                                        std::string(paused ? "paused" : "running") + ".");
        std::ostringstream oss;
        oss << R"({"success":true,"paused":)" << (paused ? "true" : "false") << "}";
        return oss.str();
    }

    if (method == "POST" && path == "/codestats") {
        const std::string directory = parseDirectory(body);
        backend::CodeStatsAnalyzer analyzer;
        const std::string targetDir = directory.empty() ? "." : directory;
        backend::CodeStatsResult stats = analyzer.analyze(targetDir);
        contentType = "application/json";
        if (!stats.withinWorkspace) {
            backend::Logger::instance().log(
                "Code stats rejected for directory '" + targetDir + "' (outside workspace).");
            statusCode = 403;
            return R"({"success":false,"error":"Directory must stay within workspace."})";
        }
        if (!stats.directoryExists) {
            backend::Logger::instance().log(
                "Code stats failed: directory '" + targetDir + "' not found.");
            statusCode = 404;
            return R"({"success":false,"error":"Directory does not exist."})";
        }
        backend::Logger::instance().log("Code stats computed for directory '" + targetDir + "'.");
        return buildCodeStatsJson(stats, targetDir);
    }

    statusCode = 404;
    backend::Logger::instance().log("API path not found: " + path);
    return R"({"error":"Unsupported API path"})";
}

std::string WebServer::buildStateJson() {
    std::lock_guard<std::mutex> guard(m_engineMutex);
    const backend::GameConfig& config = m_engine.getConfig();
    const backend::CollectionStats stats = m_engine.getStats();
    const backend::Position tankPos = m_engine.getTank().getPosition();
    const double timeLeft =
        std::max(0.0, static_cast<double>(config.timeLimitSeconds) - m_engine.elapsedSeconds());

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    oss << R"({"worldWidth":)" << config.worldWidth
        << R"(,"worldHeight":)" << config.worldHeight
        << R"(,"timeLimit":)" << config.timeLimitSeconds
        << R"(,"timeLeft":)" << timeLeft
        << R"(,"tank":{"x":)" << tankPos.x << R"(,"y":)" << tankPos.y << "},"
        << R"("stats":{"count":)" << stats.collectedCount
        << R"(,"value":)" << stats.collectedValue << "},"
        << R"("paused":)" << (m_engine.isPaused() ? "true" : "false") << ","
        << R"("envelopes":[)";

    const auto& envelopes = m_engine.getEnvelopes();
    for (std::size_t i = 0; i < envelopes.size(); ++i) {
        const auto& envelope = envelopes[i];
        const backend::Position pos = envelope.getPosition();
        const char* sizeStr = "Small";
        switch (envelope.getSize()) {
            case backend::EnvelopeSize::Small:
                sizeStr = "Small";
                break;
            case backend::EnvelopeSize::Medium:
                sizeStr = "Medium";
                break;
            case backend::EnvelopeSize::Large:
                sizeStr = "Large";
                break;
        }
        oss << R"({"id":)" << envelope.getId()
            << R"(,"x":)" << pos.x
            << R"(,"y":)" << pos.y
            << R"(,"size":")" << sizeStr << R"(",)"
            << R"("value":)" << envelope.getValue()
            << R"(,"radius":)" << envelope.getCollectionRadius() << "}";
        if (i + 1 < envelopes.size()) {
            oss << ",";
        }
    }
    oss << "]}";
    return oss.str();
}

std::string WebServer::loadStaticFile(const std::string& targetPath, std::string& contentType) {
    std::filesystem::path fullPath = std::filesystem::path(m_staticDir) / targetPath;
    if (!std::filesystem::exists(fullPath)) {
        fullPath = std::filesystem::path(targetPath);
    }

    if (!std::filesystem::exists(fullPath)) {
        throw std::runtime_error("Static file not found: " + fullPath.string());
    }

    std::ifstream file(fullPath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Unable to open static file: " + fullPath.string());
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    const std::string extension = fullPath.extension().string();
    if (extension == ".html") {
        contentType = "text/html; charset=utf-8";
    } else if (extension == ".js") {
        contentType = "application/javascript";
    } else if (extension == ".css") {
        contentType = "text/css";
    } else {
        contentType = "application/octet-stream";
    }

    return buffer.str();
}

backend::MoveDirection WebServer::parseDirection(const std::string& payload) const {
    if (payload.find("up") != std::string::npos) {
        return backend::MoveDirection::Up;
    }
    if (payload.find("down") != std::string::npos) {
        return backend::MoveDirection::Down;
    }
    if (payload.find("left") != std::string::npos) {
        return backend::MoveDirection::Left;
    }
    if (payload.find("right") != std::string::npos) {
        return backend::MoveDirection::Right;
    }
    return backend::MoveDirection::None;
}

std::string WebServer::parseAction(const std::string& payload) const {
    if (payload.find("resume") != std::string::npos) {
        return "resume";
    }
    if (payload.find("pause") != std::string::npos) {
        return "pause";
    }
    if (payload.find("toggle") != std::string::npos) {
        return "toggle";
    }
    return "toggle";
}

std::string WebServer::parseDirectory(const std::string& payload) const {
    const std::string key = "directory=";
    const std::size_t pos = payload.find(key);
    if (pos == std::string::npos) {
        return {};
    }
    const std::string raw = payload.substr(pos + key.size());
    std::string decoded;
    decoded.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '%' && i + 2 < raw.size()) {
            const std::string hex = raw.substr(i + 1, 2);
            char* endPtr = nullptr;
            const long value = std::strtol(hex.c_str(), &endPtr, 16);
            if (endPtr != nullptr && *endPtr == '\0') {
                decoded.push_back(static_cast<char>(value));
                i += 2;
                continue;
            }
        } else if (raw[i] == '+') {
            decoded.push_back(' ');
            continue;
        }
        decoded.push_back(raw[i]);
    }
    return decoded;
}

std::string WebServer::buildCodeStatsJson(const backend::CodeStatsResult& result,
                                          const std::string& directory) const {
    std::ostringstream oss;
    oss << R"({"success":true,)"
        << R"("directory":")" << directory << R"(",)"
        << R"("totalLines":)" << result.totalLines << ",";

    oss << R"("languages":[)";
    std::size_t index = 0;
    for (const auto& [lang, summary] : result.languageSummaries) {
        oss << R"({"language":")" << lang << R"(",)"
            << R"("files":)" << summary.fileCount << ","
            << R"("lines":)" << summary.lineCount << "}";
        if (++index < result.languageSummaries.size()) {
            oss << ",";
        }
    }
    oss << "],";

    const auto& pySummary = result.pythonFunctions;
    oss << R"("pythonFunctions":{)"
        << R"("count":)" << pySummary.functionCount << ","
        << R"("averageLength":)" << pySummary.averageLength << ","
        << R"("minLength":)" << pySummary.minLength << ","
        << R"("maxLength":)" << pySummary.maxLength << ","
        << R"("medianLength":)" << pySummary.medianLength << "}}";
    return oss.str();
}

}  // namespace frontend
