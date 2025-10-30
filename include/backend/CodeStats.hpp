// File: CodeStats.hpp
// Description: Declares utilities for computing language line counts and
//              Python function statistics within a directory tree.

#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace backend {

struct LanguageSummary {
    std::size_t fileCount{0};
    std::size_t lineCount{0};
};

struct PythonFunctionSummary {
    std::size_t functionCount{0};
    double averageLength{0.0};
    int minLength{0};
    int maxLength{0};
    double medianLength{0.0};
    std::vector<int> lengths;
};

struct CodeStatsResult {
    std::unordered_map<std::string, LanguageSummary> languageSummaries;
    PythonFunctionSummary pythonFunctions;
    std::size_t totalLines{0};
    bool withinWorkspace{true};
    bool directoryExists{true};
};

class CodeStatsAnalyzer {
public:
    CodeStatsResult analyze(const std::filesystem::path& root);

private:
    void visitFile(const std::filesystem::path& filePath, CodeStatsResult& result);
    std::size_t countLogicalLines(const std::filesystem::path& filePath);
    void analyzePythonFile(const std::filesystem::path& filePath, CodeStatsResult& result);
};

}  // namespace backend
