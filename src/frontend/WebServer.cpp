// File: WebServer.cpp
// Description: Implements a minimal HTTP server providing REST endpoints and
//              serving static assets for the browser-based frontend.

#include "frontend/WebServer.hpp"

#include "backend/Logger.hpp"
#include "frontend/LayoutManager.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <unordered_set>

namespace frontend {

namespace {
constexpr std::size_t kReadBufferSize = 4096;

std::string jsonEscape(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    for (char ch : input) {
        switch (ch) {
            case '\\':
                output += "\\\\";
                break;
            case '"':
                output += "\\\"";
                break;
            case '\b':
                output += "\\b";
                break;
            case '\f':
                output += "\\f";
                break;
            case '\n':
                output += "\\n";
                break;
            case '\r':
                output += "\\r";
                break;
            case '\t':
                output += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    std::ostringstream oss;
                    oss << "\\u"
                        << std::hex << std::uppercase << std::setfill('0') << std::setw(4)
                        << static_cast<int>(static_cast<unsigned char>(ch));
                    output += oss.str();
                } else {
                    output += ch;
                }
                break;
        }
    }
    return output;
}

std::string xmlEscape(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    for (char ch : input) {
        switch (ch) {
            case '&':
                output += "&amp;";
                break;
            case '<':
                output += "&lt;";
                break;
            case '>':
                output += "&gt;";
                break;
            case '"':
                output += "&quot;";
                break;
            case '\'':
                output += "&apos;";
                break;
            default:
                output.push_back(ch);
                break;
        }
    }
    return output;
}

struct LanguageRow {
    std::string name;
    std::size_t files;
    std::size_t lines;
    std::size_t blankLines;
    std::size_t commentLines;
    std::size_t functionCount;
    int minFunctionLength;
    int maxFunctionLength;
    double averageFunctionLength;
    double medianFunctionLength;
};

std::vector<LanguageRow> collectLanguageRows(const backend::CodeStatsResult& result) {
    std::vector<LanguageRow> rows;
    rows.reserve(result.languageSummaries.size());
    for (const auto& [name, summary] : result.languageSummaries) {
        rows.push_back(LanguageRow{
            name,
            summary.fileCount,
            summary.lineCount,
            summary.blankLineCount,
            summary.commentLineCount,
            summary.functions.functionCount,
            summary.functions.minLength,
            summary.functions.maxLength,
            summary.functions.averageLength,
            summary.functions.medianLength,
        });
    }
    std::sort(rows.begin(), rows.end(),
              [](const LanguageRow& a, const LanguageRow& b) { return a.name < b.name; });
    return rows;
}

std::string buildSheetXml(const std::vector<LanguageRow>& rows,
                          bool includeBlank,
                          bool includeComments) {
    auto cellRef = [](int row, int col) {
        std::string ref;
        int c = col;
        while (c >= 0) {
            ref.insert(ref.begin(), static_cast<char>('A' + (c % 26)));
            c = c / 26 - 1;
        }
        ref += std::to_string(row);
        return ref;
    };

    std::vector<std::string> headers{"Language", "Files", "Lines"};
    if (includeBlank) {
        headers.push_back("Blank Lines");
    }
    if (includeComments) {
        headers.push_back("Comment Lines");
    }
    headers.push_back("Functions");
    headers.push_back("Min Fn Lines");
    headers.push_back("Max Fn Lines");
    headers.push_back("Avg Fn Lines");
    headers.push_back("Median Fn Lines");

    std::ostringstream oss;
    oss << R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
        << R"(<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">)"
        << "<sheetData>";

    auto appendStringCell = [&](int rowIdx, int colIdx, const std::string& value) {
        oss << "<c r=\"" << cellRef(rowIdx, colIdx) << "\" t=\"inlineStr\"><is><t>"
            << xmlEscape(value) << "</t></is></c>";
    };
    auto appendNumberCell = [&](int rowIdx, int colIdx, std::size_t value) {
        oss << "<c r=\"" << cellRef(rowIdx, colIdx) << "\"><v>" << value << "</v></c>";
    };
    auto appendDoubleCell = [&](int rowIdx, int colIdx, double value) {
        std::ostringstream cell;
        cell << std::fixed << std::setprecision(2) << value;
        oss << "<c r=\"" << cellRef(rowIdx, colIdx) << "\"><v>" << cell.str() << "</v></c>";
    };

    oss << "<row r=\"1\">";
    for (std::size_t col = 0; col < headers.size(); ++col) {
        appendStringCell(1, static_cast<int>(col), headers[col]);
    }
    oss << "</row>";

    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto& row = rows[i];
        const int excelRow = static_cast<int>(i + 2);
        oss << "<row r=\"" << excelRow << "\">";
        appendStringCell(excelRow, 0, row.name);
        appendNumberCell(excelRow, 1, row.files);
        appendNumberCell(excelRow, 2, row.lines);
        int colOffset = 3;
        if (includeBlank) {
            appendNumberCell(excelRow, colOffset, row.blankLines);
            ++colOffset;
        }
        if (includeComments) {
            appendNumberCell(excelRow, colOffset, row.commentLines);
            ++colOffset;
        }
        appendNumberCell(excelRow, colOffset, row.functionCount);
        ++colOffset;
        appendNumberCell(excelRow, colOffset,
                         static_cast<std::size_t>(std::max(0, row.minFunctionLength)));
        ++colOffset;
        appendNumberCell(excelRow, colOffset,
                         static_cast<std::size_t>(std::max(0, row.maxFunctionLength)));
        ++colOffset;
        appendDoubleCell(excelRow, colOffset, row.averageFunctionLength);
        ++colOffset;
        appendDoubleCell(excelRow, colOffset, row.medianFunctionLength);
        oss << "</row>";
    }

    oss << "</sheetData></worksheet>";
    return oss.str();
}

std::uint32_t crc32(const std::string& data) {
    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> tbl {};
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t crc = i;
            for (int j = 0; j < 8; ++j) {
                if (crc & 1U) {
                    crc = (crc >> 1U) ^ 0xEDB88320U;
                } else {
                    crc >>= 1U;
                }
            }
            tbl[i] = crc;
        }
        return tbl;
    }();

    std::uint32_t crc = 0xFFFFFFFFU;
    for (unsigned char ch : data) {
        crc = (crc >> 8U) ^ table[(crc ^ ch) & 0xFFU];
    }
    return crc ^ 0xFFFFFFFFU;
}

void writeLE16(std::string& buffer, std::uint16_t value) {
    buffer.push_back(static_cast<char>(value & 0xFF));
    buffer.push_back(static_cast<char>((value >> 8) & 0xFF));
}

void writeLE32(std::string& buffer, std::uint32_t value) {
    buffer.push_back(static_cast<char>(value & 0xFF));
    buffer.push_back(static_cast<char>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<char>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<char>((value >> 24) & 0xFF));
}

class ZipBuilder {
public:
    void addFile(const std::string& name, const std::string& content) {
        Entry entry;
        entry.name = name;
        entry.content = content;
        entry.crc = crc32(content);
        entry.compressedSize = static_cast<std::uint32_t>(content.size());
        entry.uncompressedSize = static_cast<std::uint32_t>(content.size());
        entries.push_back(entry);
    }

    std::string finalize() {
        std::string output;
        std::string centralDirectory;

        for (auto& entry : entries) {
            entry.localHeaderOffset = static_cast<std::uint32_t>(output.size());
            output.append("\x50\x4b\x03\x04", 4);
            writeLE16(output, 20);
            writeLE16(output, 0);
            writeLE16(output, 0);
            writeLE16(output, 0);
            writeLE16(output, 0);
            writeLE32(output, entry.crc);
            writeLE32(output, entry.compressedSize);
            writeLE32(output, entry.uncompressedSize);
            writeLE16(output, static_cast<std::uint16_t>(entry.name.size()));
            writeLE16(output, 0);
            output += entry.name;
            output += entry.content;
        }

        for (const auto& entry : entries) {
            centralDirectory.append("\x50\x4b\x01\x02", 4);
            writeLE16(centralDirectory, 20);
            writeLE16(centralDirectory, 20);
            writeLE16(centralDirectory, 0);
            writeLE16(centralDirectory, 0);
            writeLE16(centralDirectory, 0);
            writeLE16(centralDirectory, 0);
            writeLE32(centralDirectory, entry.crc);
            writeLE32(centralDirectory, entry.compressedSize);
            writeLE32(centralDirectory, entry.uncompressedSize);
            writeLE16(centralDirectory, static_cast<std::uint16_t>(entry.name.size()));
            writeLE16(centralDirectory, 0);
            writeLE16(centralDirectory, 0);
            writeLE16(centralDirectory, 0);
            writeLE16(centralDirectory, 0);
            writeLE32(centralDirectory, 0);
            writeLE32(centralDirectory, entry.localHeaderOffset);
            centralDirectory += entry.name;
        }

        const std::uint32_t centralDirOffset = static_cast<std::uint32_t>(output.size());
        output += centralDirectory;

        output.append("\x50\x4b\x05\x06", 4);
        writeLE16(output, 0);
        writeLE16(output, 0);
        writeLE16(output, static_cast<std::uint16_t>(entries.size()));
        writeLE16(output, static_cast<std::uint16_t>(entries.size()));
        writeLE32(output, static_cast<std::uint32_t>(centralDirectory.size()));
        writeLE32(output, centralDirOffset);
        writeLE16(output, 0);

        return output;
    }

private:
    struct Entry {
        std::string name;
        std::string content;
        std::uint32_t crc;
        std::uint32_t compressedSize;
        std::uint32_t uncompressedSize;
        std::uint32_t localHeaderOffset;
    };

    std::vector<Entry> entries;
};
}  // anonymous namespace

}  // namespace frontend

using frontend::LayoutManager;
using frontend::WebServer;

WebServer::WebServer(backend::GameEngine& engine,
                     LayoutManager& layoutManager,
                     std::string staticDir,
                     int port)
    : m_engine(engine),
      m_layoutManager(layoutManager),
      m_codeStatsFacade(),
      m_attendanceRepo(backend::createAttendanceRepository()),
      m_attendanceCursor(0),
      m_staticDir(std::move(staticDir)),
      m_port(port) {}

int WebServer::port() const noexcept {
    return m_port;
}

void WebServer::run() {
    int serverSocket = -1;
    int attempt = 0;
    int currentPort = m_port;
    constexpr int kMaxAttempts = 10;

    while (attempt < kMaxAttempts) {
        serverSocket = ::socket(AF_INET, SOCK_STREAM, 0);
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
        address.sin_port = htons(static_cast<uint16_t>(currentPort));

        if (bind(serverSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) {
            if (listen(serverSocket, 10) < 0) {
                ::close(serverSocket);
                throw std::runtime_error("Failed to listen on server socket.");
            }
            if (currentPort != m_port) {
                backend::Logger::instance().log("Requested port " + std::to_string(m_port) +
                                                " unavailable; using fallback port " +
                                                std::to_string(currentPort) + ".");
            }
            m_port = currentPort;
            break;
        }

        ::close(serverSocket);
        serverSocket = -1;
        ++attempt;
        ++currentPort;
    }

    if (serverSocket < 0) {
        throw std::runtime_error("Failed to bind server socket after multiple attempts.");
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
        } else if (method == "POST" && path == "/codestats/export") {
            const std::string directory = parseDirectory(body);
            const std::string targetDir = directory.empty() ? "." : directory;
            backend::CodeStatsOptions options;
            options.languages = parseLanguages(body);
            options.includeBlankLines = parseBooleanFlag(body, "includeBlank");
            options.includeCommentLines = parseBooleanFlag(body, "includeComments");
            const std::string format = parseFormat(body);
            if (format.empty() || format == "none") {
                sendBadRequest(clientSocket, "Invalid export format.");
                return;
            }

            backend::CodeStatsResult stats = m_codeStatsFacade.analyzeAll(targetDir, options);
            if (!stats.withinWorkspace) {
                backend::Logger::instance().log(
                    "Code stats export rejected for directory '" + targetDir + "' (outside workspace).");
                sendHttpResponse(clientSocket,
                                 "HTTP/1.1 403 Forbidden",
                                 R"({"success":false,"error":"Directory must stay within workspace."})",
                                 "application/json");
                return;
            }
            if (!stats.directoryExists) {
                backend::Logger::instance().log(
                    "Code stats export failed: directory '" + targetDir + "' not found.");
                sendHttpResponse(clientSocket,
                                 "HTTP/1.1 404 Not Found",
                                 R"({"success":false,"error":"Directory does not exist."})",
                                 "application/json");
                return;
            }

            std::string payload;
            std::string mime;
            std::string filename;
            if (format == "csv") {
                payload = buildCsvReport(stats);
                mime = "text/csv; charset=utf-8";
                filename = "code-report.csv";
            } else if (format == "json") {
                payload = buildJsonReport(stats);
                mime = "application/json";
                filename = "code-report.json";
            } else if (format == "xlsx") {
                payload = buildXlsxReport(stats);
                mime = "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
                filename = "code-report.xlsx";
            } else {
                sendBadRequest(clientSocket, "Unsupported export format.");
                return;
            }

            backend::Logger::instance().log("Code stats export (" + format + ") prepared for directory '" +
                                            targetDir + "'.");
            std::vector<std::pair<std::string, std::string>> headers{
                {"Content-Disposition", "attachment; filename=\"" + filename + "\""}};
            sendHttpResponse(clientSocket, "HTTP/1.1 200 OK", payload, mime, headers);
            return;
        } else if (method == "GET" && path == "/attendance/roster") {
            responseBody = handleApiRequest(method, path, body, contentType, statusCode);
        } else if (method == "GET" && path == "/attendance/next") {
            responseBody = handleApiRequest(method, path, body, contentType, statusCode);
        } else if (method == "POST" && path == "/attendance/mark") {
            responseBody = handleAttendanceMark(body, contentType, statusCode);
        } else if (method == "POST" && path == "/layout") {
            responseBody = handleApiRequest(method, path, body, contentType, statusCode);
        } else if (method == "POST" && path == "/print_longest_function") {
            responseBody = handleApiRequest(method, path, body, contentType, statusCode);
        } else if (method == "POST" && path == "/print_shortest_function") {
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
                                 const std::string& contentType,
                                 const std::vector<std::pair<std::string, std::string>>& extraHeaders) {
    std::ostringstream response;
    response << statusLine << "\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    for (const auto& header : extraHeaders) {
        response << header.first << ": " << header.second << "\r\n";
    }
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
    statusCode = 200;

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
    } else if (method == "POST" && path == "/reset") {
        {
            std::lock_guard<std::mutex> guard(m_engineMutex);
            const auto seed = static_cast<unsigned int>(
                std::chrono::system_clock::now().time_since_epoch().count());
            m_engine.setRandomSeed(seed);
            m_engine.reset();
        }
        backend::Logger::instance().log("Reset request completed and engine reseeded.");
        return R"({"success":true})";
    } else if (method == "POST" && path == "/rain") {
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
    } else if (method == "POST" && path == "/pause") {
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
    } else if (method == "POST" && path == "/codestats") {
        const std::string directory = parseDirectory(body);
        const std::string targetDir = directory.empty() ? "." : directory;
        backend::CodeStatsOptions options;
        options.languages = parseLanguages(body);
        options.includeBlankLines = parseBooleanFlag(body, "includeBlank");
        options.includeCommentLines = parseBooleanFlag(body, "includeComments");
        backend::CodeStatsResult stats = m_codeStatsFacade.analyzeAll(targetDir, options);
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
        return buildCodeStatsJson(stats, targetDir, options);
    } else if (method == "POST" &&
               (path == "/print_longest_function" || path == "/print_shortest_function")) {
        const std::string directory = parseDirectory(body);
        const std::string targetDir = directory.empty() ? "." : directory;
        backend::CodeStatsOptions options;
        options.languages = parseLanguages(body);
        options.includeBlankLines = parseBooleanFlag(body, "includeBlank");
        options.includeCommentLines = parseBooleanFlag(body, "includeComments");
        backend::CodeStatsResult stats = m_codeStatsFacade.analyzeAll(targetDir, options);
        contentType = "application/json";
        if (!stats.withinWorkspace) {
            backend::Logger::instance().log(
                "Function print rejected for directory '" + targetDir + "' (outside workspace).");
            statusCode = 403;
            return R"({"success":false,"error":"目录必须位于工作空间内"})";
        }
        if (!stats.directoryExists) {
            backend::Logger::instance().log(
                "Function print failed: directory '" + targetDir + "' not found.");
            statusCode = 404;
            return R"({"success":false,"error":"目录不存在"})";
        }
        if (path == "/print_longest_function") {
            const std::string summary = m_codeStatsFacade.printLongestFunction(stats);
            if (summary.empty()) {
                return R"({"success":false,"message":"未检测到函数数据，无法打印最长函数"})";
            }
            backend::Logger::instance().log(
                "Longest function summary delivered for directory '" + targetDir + "'.");
            return std::string(R"({"success":true,"message":")") + jsonEscape(summary) + "\"}";
        }
        const std::string summary = m_codeStatsFacade.printShortestFunction(stats);
        if (summary.empty()) {
            return R"({"success":false,"message":"未检测到函数数据，无法打印最短函数"})";
        }
        backend::Logger::instance().log(
            "Shortest function summary delivered for directory '" + targetDir + "'.");
        return std::string(R"({"success":true,"message":")") + jsonEscape(summary) + "\"}";
    } else if (method == "POST" && path == "/layout") {
        backend::Logger::instance().log("Layout customization requested.");
        contentType = "application/json";
        statusCode = 202;
        const std::string userId = "default";  // TODO: Extract from payload.
        (void)body;
        const std::string serialized = buildLayoutSettingsJson(userId);
        return R"({"success":false,"message":"Layout manager not yet implemented","settings":")" +
               serialized + R"("})";
    } else if (method == "GET" && path == "/attendance/roster") {
        contentType = "application/json";
        if (!m_attendanceRepo) {
            statusCode = 500;
            return R"({"success":false,"error":"Attendance repository not configured"})";
        }
        const std::vector<backend::Student> students = m_attendanceRepo->listStudents();
        std::ostringstream oss;
        oss << R"({"success":true,"students":[)";
        for (std::size_t i = 0; i < students.size(); ++i) {
            const backend::Student& s = students[i];
            oss << R"({"id":")" << jsonEscape(s.studentId) << R"(","name":")"
                << jsonEscape(s.name) << R"("})";
            if (i + 1 < students.size()) {
                oss << ",";
            }
        }
        oss << "]}"
            ;
        return oss.str();
    } else if (method == "GET" && path == "/attendance/next") {
        contentType = "application/json";
        if (!m_attendanceRepo) {
            statusCode = 500;
            return R"({"success":false,"error":"Attendance repository not configured"})";
        }
        const std::vector<backend::Student> students = m_attendanceRepo->listStudents();
        if (students.empty()) {
            return R"({"success":true,"empty":true})";
        }
        if (m_attendanceCursor >= students.size()) {
            m_attendanceCursor = 0;
        }
        const backend::Student& student = students[m_attendanceCursor++];
        std::ostringstream oss;
        oss << R"({"success":true,"student":{"id":")" << jsonEscape(student.studentId)
            << R"(","name":")" << jsonEscape(student.name) << R"("}})";
        return oss.str();
    } else if (method == "POST" && path == "/attendance/mark") {
        contentType = "application/json";
        statusCode = 501;
        return R"({"success":false,"error":"Attendance mark not implemented"})";
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
    return parseFormValue(payload, "directory");
}

std::string WebServer::parseFormValue(const std::string& payload,
                                      const std::string& key) const {
    if (key.empty()) {
        return {};
    }
    const std::string needle = key + "=";
    const std::size_t pos = payload.find(needle);
    if (pos == std::string::npos) {
        return {};
    }
    std::size_t endPos = payload.find('&', pos + needle.size());
    std::string raw;
    if (endPos == std::string::npos) {
        raw = payload.substr(pos + needle.size());
    } else {
        raw = payload.substr(pos + needle.size(), endPos - (pos + needle.size()));
    }
    return decodeFormValue(raw);
}

std::unordered_set<std::string> WebServer::parseLanguages(const std::string& payload) const {
    const std::string key = "languages=";
    const std::size_t pos = payload.find(key);
    if (pos == std::string::npos) {
        return {};
    }
    const std::size_t endPos = payload.find('&', pos + key.size());
    const std::string raw =
        endPos == std::string::npos ? payload.substr(pos + key.size())
                                    : payload.substr(pos + key.size(), endPos - (pos + key.size()));
    const std::string decoded = decodeFormValue(raw);

    std::unordered_set<std::string> languages;
    std::size_t start = 0;
    while (start < decoded.size()) {
        const std::size_t comma = decoded.find(',', start);
        const std::string token = decoded.substr(
            start, comma == std::string::npos ? std::string::npos : comma - start);
        if (!token.empty()) {
            std::string lowerToken;
            lowerToken.reserve(token.size());
            for (char ch : token) {
                lowerToken.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            }
            if (lowerToken == "c" || lowerToken == "ansi-c") {
                languages.insert("C");
            } else if (lowerToken == "cpp" || lowerToken == "c++" || lowerToken == "cxx") {
                languages.insert("C++");
            } else if (lowerToken == "c#" || lowerToken == "csharp" || lowerToken == "cs") {
                languages.insert("C#");
            } else if (lowerToken == "java") {
                languages.insert("Java");
            } else if (lowerToken == "python" || lowerToken == "py" || lowerToken == "python3") {
                languages.insert("Python");
            }
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return languages;
}

std::string WebServer::parseFormat(const std::string& payload) const {
    const std::string key = "format=";
    const std::size_t pos = payload.find(key);
    if (pos == std::string::npos) {
        return "none";
    }
    const std::size_t endPos = payload.find('&', pos + key.size());
    const std::string raw =
        endPos == std::string::npos ? payload.substr(pos + key.size())
                                    : payload.substr(pos + key.size(), endPos - (pos + key.size()));
    std::string value = decodeFormValue(raw);
    std::string lower;
    lower.reserve(value.size());
    for (char ch : value) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    if (lower.empty() || lower == "none") {
        return "none";
    }
    if (lower == "csv" || lower == "json" || lower == "xlsx") {
        return lower;
    }
    return {};
}

bool WebServer::parseBooleanFlag(const std::string& payload, const std::string& key) const {
    const std::string prefix = key + "=";
    const std::size_t pos = payload.find(prefix);
    if (pos == std::string::npos) {
        return false;
    }
    const std::size_t endPos = payload.find('&', pos + prefix.size());
    const std::string raw =
        endPos == std::string::npos ? payload.substr(pos + prefix.size())
                                    : payload.substr(pos + prefix.size(), endPos - (pos + prefix.size()));
    const std::string value = decodeFormValue(raw);
    std::string lowerValue;
    lowerValue.reserve(value.size());
    for (char ch : value) {
        lowerValue.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    if (lowerValue == "1" || lowerValue == "true" || lowerValue == "on" || lowerValue == "yes") {
        return true;
    }
    return false;
}

std::string WebServer::decodeFormValue(const std::string& value) const {
    std::string decoded;
    decoded.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            const std::string hex = value.substr(i + 1, 2);
            char* endPtr = nullptr;
            const long numeric = std::strtol(hex.c_str(), &endPtr, 16);
            if (endPtr != nullptr && *endPtr == '\0') {
                decoded.push_back(static_cast<char>(numeric));
                i += 2;
                continue;
            }
        } else if (value[i] == '+') {
            decoded.push_back(' ');
            continue;
        }
        decoded.push_back(value[i]);
    }
    return decoded;
}

std::string WebServer::buildCodeStatsJson(const backend::CodeStatsResult& result,
                                          const std::string& directory,
                                          const backend::CodeStatsOptions& options) const {
    std::vector<std::string> included(result.includedLanguages.begin(), result.includedLanguages.end());
    std::sort(included.begin(), included.end());
    std::ostringstream oss;
    oss << R"({"success":true,)"
        << R"("directory":")" << jsonEscape(directory) << R"(",)"
        << R"("totalLines":)" << result.totalLines;
    if (result.includeBlankLines) {
        oss << R"(,"totalBlankLines":)" << result.totalBlankLines;
    }
    if (result.includeCommentLines) {
        oss << R"(,"totalCommentLines":)" << result.totalCommentLines;
    }
    oss << R"(,"includeBlank":)" << (result.includeBlankLines ? "true" : "false")
        << R"(,"includeComments":)" << (result.includeCommentLines ? "true" : "false") << ",";

    oss << R"("includedLanguages":[)";
    for (std::size_t i = 0; i < included.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << "\"" << jsonEscape(included[i]) << "\"";
    }
    oss << "],";

    oss << R"("languages":[)";
    std::size_t emitted = 0;
    for (const auto& [lang, summary] : result.languageSummaries) {
        if (emitted > 0) {
            oss << ",";
        }
        oss << R"({"language":")" << jsonEscape(lang) << R"(",)"
            << R"("files":)" << summary.fileCount << ","
            << R"("lines":)" << summary.lineCount;
        if (result.includeBlankLines) {
            oss << R"(,"blankLines":)" << summary.blankLineCount;
        }
        if (result.includeCommentLines) {
            oss << R"(,"commentLines":)" << summary.commentLineCount;
        }
        oss << R"(,"functions":{"count":)" << summary.functions.functionCount << ","
            << R"("min":)" << summary.functions.minLength << ","
            << R"("max":)" << summary.functions.maxLength << ","
            << R"("average":)" << summary.functions.averageLength << ","
            << R"("median":)" << summary.functions.medianLength << "}}";
        ++emitted;
    }
        if (emitted == 0 && !options.languages.empty()) {
            bool first = true;
            for (const auto& lang : options.languages) {
                const auto it = result.languageSummaries.find(lang);
                backend::LanguageSummary summary{};
                if (it != result.languageSummaries.end()) {
                    summary = it->second;
                }
                if (!first) {
                    oss << ",";
                }
                oss << R"({"language":")" << jsonEscape(lang) << R"(",)"
                    << R"("files":)" << summary.fileCount << ","
                    << R"("lines":)" << summary.lineCount;
                if (result.includeBlankLines) {
                    oss << R"(,"blankLines":)" << summary.blankLineCount;
                }
            if (result.includeCommentLines) {
                oss << R"(,"commentLines":)" << summary.commentLineCount;
            }
            oss << R"(,"functions":{"count":)" << summary.functions.functionCount << ","
                << R"("min":)" << summary.functions.minLength << ","
                << R"("max":)" << summary.functions.maxLength << ","
                << R"("average":)" << summary.functions.averageLength << ","
                << R"("median":)" << summary.functions.medianLength << "}}";
            first = false;
        }
    }
    oss << "]}";
    return oss.str();
}

std::string WebServer::buildCsvReport(const backend::CodeStatsResult& result) const {
    const auto rows = collectLanguageRows(result);
    std::ostringstream oss;
    oss << "\xEF\xBB\xBF";
    oss << "Language,Files,Lines";
    if (result.includeBlankLines) {
        oss << ",BlankLines";
    }
    if (result.includeCommentLines) {
        oss << ",CommentLines";
    }
    oss << ",Functions,MinFunctionLength,MaxFunctionLength,AverageFunctionLength,MedianFunctionLength\n";
    for (const auto& row : rows) {
        oss << "\"" << row.name << "\""
            << "," << row.files << "," << row.lines;
        if (result.includeBlankLines) {
            oss << "," << row.blankLines;
        }
        if (result.includeCommentLines) {
            oss << "," << row.commentLines;
        }
        oss << "," << row.functionCount
            << "," << row.minFunctionLength
            << "," << row.maxFunctionLength;
        oss << "," << std::fixed << std::setprecision(2) << row.averageFunctionLength;
        oss << "," << std::fixed << std::setprecision(2) << row.medianFunctionLength;
        oss.unsetf(std::ios::floatfield);
        oss << std::setprecision(6);
        oss << "\n";
    }
    return oss.str();
}

std::string WebServer::buildJsonReport(const backend::CodeStatsResult& result) const {
    const auto rows = collectLanguageRows(result);
    std::ostringstream oss;
    oss << R"({"languages":[)";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << R"({"language":")" << jsonEscape(rows[i].name) << R"(",)"
            << R"("files":)" << rows[i].files << ","
            << R"("lines":)" << rows[i].lines;
        if (result.includeBlankLines) {
            oss << R"(,"blankLines":)" << rows[i].blankLines;
        }
        if (result.includeCommentLines) {
            oss << R"(,"commentLines":)" << rows[i].commentLines;
        }
        oss << R"(,"functions":{"count":)" << rows[i].functionCount << ","
            << R"("min":)" << rows[i].minFunctionLength << ","
            << R"("max":)" << rows[i].maxFunctionLength << ","
            << R"("average":)" << rows[i].averageFunctionLength << ","
            << R"("median":)" << rows[i].medianFunctionLength << "}";
        oss << "}";
    }
    oss << "]}";
    return oss.str();
}

std::string WebServer::buildXlsxReport(const backend::CodeStatsResult& result) const {
    const auto sheetXml = buildSheetXml(collectLanguageRows(result),
                                        result.includeBlankLines,
                                        result.includeCommentLines);

    ZipBuilder builder;
    builder.addFile("[Content_Types].xml",
                    R"(<?xml version="1.0" encoding="UTF-8"?>)"
                    R"(<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">)"
                    R"(<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>)"
                    R"(<Default Extension="xml" ContentType="application/xml"/>)"
                    R"(<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>)"
                    R"(<Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>)"
                    R"(</Types>)");

    builder.addFile("_rels/.rels",
                    R"(<?xml version="1.0" encoding="UTF-8"?>)"
                    R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
                    R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>)"
                    R"(</Relationships>)");

    builder.addFile("xl/workbook.xml",
                    R"(<?xml version="1.0" encoding="UTF-8"?>)"
                    R"(<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" )"
                    R"(xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">)"
                    R"(<sheets><sheet name="Languages" sheetId="1" r:id="rId1"/></sheets></workbook>)");

    builder.addFile("xl/_rels/workbook.xml.rels",
                    R"(<?xml version="1.0" encoding="UTF-8"?>)"
                    R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
                    R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>)"
                    R"(</Relationships>)");

    builder.addFile("xl/worksheets/sheet1.xml", sheetXml);

    return builder.finalize();
}

std::string WebServer::buildLayoutSettingsJson(const std::string& userId) {
    return m_layoutManager.exportPreferences(userId);
}
