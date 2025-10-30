// File: CodeStats.cpp
// Description: Implements code statistics extraction for multiple languages.

#include "backend/CodeStats.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <numeric>
#include <string>

namespace backend {

namespace {

bool hasExtension(const std::filesystem::path& path, const std::vector<std::string>& extensions) {
    const std::string ext = path.extension().string();
    return std::find(extensions.begin(), extensions.end(), ext) != extensions.end();
}

std::string trim(const std::string& input) {
    std::size_t start = 0;
    while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])) != 0) {
        ++start;
    }
    std::size_t end = input.size();
    while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
        --end;
    }
    return input.substr(start, end - start);
}

int leadingSpaces(const std::string& line) {
    int spaces = 0;
    while (spaces < static_cast<int>(line.size()) && line[spaces] == ' ') {
        ++spaces;
    }
    return spaces;
}

bool isDirectoryExcluded(const std::filesystem::path& path) {
    const std::string name = path.filename().string();
    if (name.empty()) {
        return false;
    }
    return name == ".git" || name == "bin" || name == "logs" || name == "node_modules";
}

}  // namespace

CodeStatsResult CodeStatsAnalyzer::analyze(const std::filesystem::path& root) {
    CodeStatsResult result;

    std::error_code ec;
    const std::filesystem::path workspace =
        std::filesystem::weakly_canonical(std::filesystem::current_path(), ec);
    if (ec) {
        result.withinWorkspace = false;
        result.directoryExists = false;
        return result;
    }

    std::filesystem::path input = root;
    if (input.empty()) {
        input = ".";
    }

    std::filesystem::path requested = workspace / input.relative_path();
    const std::filesystem::path canonicalRequested =
        std::filesystem::weakly_canonical(requested, ec);
    if (ec) {
        result.directoryExists = false;
        return result;
    }

    const std::string workspaceStr = workspace.string();
    const std::string requestedStr = canonicalRequested.string();
    const bool isSubDir =
        requestedStr.size() >= workspaceStr.size() &&
        requestedStr.compare(0, workspaceStr.size(), workspaceStr) == 0 &&
        (requestedStr.size() == workspaceStr.size() ||
         requestedStr[workspaceStr.size()] == std::filesystem::path::preferred_separator);

    if (!isSubDir) {
        result.withinWorkspace = false;
        return result;
    }

    std::filesystem::recursive_directory_iterator it(canonicalRequested, ec);
    std::filesystem::recursive_directory_iterator end;

    while (it != end) {
        if (ec) {
            break;
        }

        const auto& entry = *it;
        if (entry.is_directory()) {
            if (isDirectoryExcluded(entry.path())) {
                it.disable_recursion_pending();
            }
        } else if (entry.is_regular_file()) {
            visitFile(entry.path(), result);
        }

        ec.clear();
        ++it;
    }

    // Finalize Python summary statistics.
    auto& pySummary = result.pythonFunctions;
    if (!pySummary.lengths.empty()) {
        std::sort(pySummary.lengths.begin(), pySummary.lengths.end());
        pySummary.functionCount = pySummary.lengths.size();
        pySummary.minLength = pySummary.lengths.front();
        pySummary.maxLength = pySummary.lengths.back();
        pySummary.averageLength =
            std::accumulate(pySummary.lengths.begin(), pySummary.lengths.end(), 0.0) /
            static_cast<double>(pySummary.lengths.size());

        if (pySummary.lengths.size() % 2 == 0) {
            const std::size_t mid = pySummary.lengths.size() / 2;
            pySummary.medianLength =
                (pySummary.lengths[mid - 1] + pySummary.lengths[mid]) / 2.0;
        } else {
            pySummary.medianLength = static_cast<double>(
                pySummary.lengths[pySummary.lengths.size() / 2]);
        }
    }

    return result;
}

void CodeStatsAnalyzer::visitFile(const std::filesystem::path& filePath, CodeStatsResult& result) {
    static const std::vector<std::string> javaExt{".java"};
    static const std::vector<std::string> cppExt{
        ".c",  ".C",  ".cc",  ".cpp", ".cxx", ".h",  ".hpp", ".hh", ".hxx"};
    static const std::vector<std::string> pyExt{".py"};

    const std::string ext = filePath.extension().string();
    LanguageSummary* summary = nullptr;

    if (hasExtension(filePath, javaExt)) {
        summary = &result.languageSummaries["Java"];
    } else if (hasExtension(filePath, cppExt)) {
        summary = &result.languageSummaries["C/C++"];
    } else if (hasExtension(filePath, pyExt)) {
        summary = &result.languageSummaries["Python"];
    }

    if (summary == nullptr) {
        return;
    }

    const std::size_t lineCount = countLogicalLines(filePath);
    summary->fileCount += 1;
    summary->lineCount += lineCount;
    result.totalLines += lineCount;

    if (ext == ".py") {
        analyzePythonFile(filePath, result);
    }
}

std::size_t CodeStatsAnalyzer::countLogicalLines(const std::filesystem::path& filePath) {
    std::ifstream stream(filePath);
    if (!stream.is_open()) {
        return 0;
    }

    std::size_t count = 0;
    std::string line;
    while (std::getline(stream, line)) {
        if (!trim(line).empty()) {
            ++count;
        }
    }
    return count;
}

void CodeStatsAnalyzer::analyzePythonFile(const std::filesystem::path& filePath,
                                          CodeStatsResult& result) {
    std::ifstream stream(filePath);
    if (!stream.is_open()) {
        return;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }

    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::string trimmedLine = trim(lines[i]);
        if (trimmedLine.rfind("def ", 0) == 0) {
            const int indent = leadingSpaces(lines[i]);
            int length = 1;
            for (std::size_t j = i + 1; j < lines.size(); ++j) {
                const std::string trimmedBodyLine = trim(lines[j]);
                const int bodyIndent = leadingSpaces(lines[j]);
                if (!trimmedBodyLine.empty() && bodyIndent <= indent && trimmedBodyLine.rfind("#", 0) != 0) {
                    break;
                }
                if (!trimmedBodyLine.empty()) {
                    ++length;
                }
            }
            if (length > 0) {
                result.pythonFunctions.lengths.push_back(length);
            }
        }
    }
}

}  // namespace backend
