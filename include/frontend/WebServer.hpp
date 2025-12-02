// File: WebServer.hpp
// Description: Declares a lightweight HTTP server that exposes the game engine
//              state to a browser-based frontend.

#pragma once

#include "backend/CodeStats.hpp"
#include "backend/CodeStatsFacade.hpp"
#include "backend/Attendance.hpp"
#include "backend/GameEngine.hpp"

#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace frontend {

class LayoutManager;

class WebServer {
public:
    WebServer(backend::GameEngine& engine,
              LayoutManager& layoutManager,
              std::string staticDir,
              int port = 8080);

    void run();

    int port() const noexcept;

private:
    backend::GameEngine& m_engine;
    LayoutManager& m_layoutManager;
    backend::CodeStatsFacade m_codeStatsFacade;
    std::unique_ptr<backend::AttendanceRepository> m_attendanceRepo;
    std::size_t m_attendanceCursor{0};
    std::string m_staticDir;
    int m_port;
    std::mutex m_engineMutex;

    void handleClient(int clientSocket);
    void sendHttpResponse(int clientSocket,
                          const std::string& statusLine,
                          const std::string& body,
                          const std::string& contentType = "text/plain",
                          const std::vector<std::pair<std::string, std::string>>& extraHeaders = {});
    void sendNotFound(int clientSocket);
    void sendBadRequest(int clientSocket, const std::string& message);
    void sendInternalError(int clientSocket, const std::string& message);
    std::string handleAttendanceMark(const std::string& body,
                                     std::string& contentType,
                                     int& statusCode);
    std::string handleApiRequest(const std::string& method,
                                 const std::string& path,
                                 const std::string& body,
                                 std::string& contentType,
                                 int& statusCode);
    std::string buildStateJson();
    std::string loadStaticFile(const std::string& targetPath, std::string& contentType);
    backend::MoveDirection parseDirection(const std::string& payload) const;
    std::string parseAction(const std::string& payload) const;
    std::string parseDirectory(const std::string& payload) const;
    std::string parseFormValue(const std::string& payload, const std::string& key) const;
    std::unordered_set<std::string> parseLanguages(const std::string& payload) const;
    bool parseBooleanFlag(const std::string& payload, const std::string& key) const;
    std::string parseFormat(const std::string& payload) const;
    std::string decodeFormValue(const std::string& value) const;
    std::string buildCodeStatsJson(const backend::CodeStatsResult& result,
                                   const std::string& directory,
                                   const backend::CodeStatsOptions& options) const;
    std::string buildCsvReport(const backend::CodeStatsResult& result) const;
    std::string buildJsonReport(const backend::CodeStatsResult& result) const;
    std::string buildXlsxReport(const backend::CodeStatsResult& result) const;
    std::string buildLayoutSettingsJson(const std::string& userId);
};

}  // namespace frontend
