// File: CodeStats.hpp
// Description: Declares utilities for computing language line counts and
//              Python function statistics within a directory tree.

#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace backend {

struct FunctionDetail {
    std::string name;
    std::string language;
    std::filesystem::path filePath;
    std::size_t lineNumber{0};
    int length{0};
};

struct FunctionSummary {
    std::size_t functionCount{0};
    double averageLength{0.0};
    int minLength{0};
    int maxLength{0};
    double medianLength{0.0};
    std::vector<int> lengths;
    std::vector<FunctionDetail> details;
};

struct LanguageSummary {
    std::size_t fileCount{0};
    std::size_t lineCount{0};
    std::size_t blankLineCount{0};
    std::size_t commentLineCount{0};
    FunctionSummary functions;
};

struct CodeStatsResult {
    std::unordered_map<std::string, LanguageSummary> languageSummaries;
    std::size_t totalLines{0};
    std::size_t totalBlankLines{0};
    std::size_t totalCommentLines{0};
    bool withinWorkspace{true};
    bool directoryExists{true};
    bool includeBlankLines{false};
    bool includeCommentLines{false};
    std::unordered_set<std::string> includedLanguages;
};

struct CodeStatsOptions {
    std::unordered_set<std::string> languages;
    bool includeBlankLines{false};
    bool includeCommentLines{false};
};

class CodeStatsAnalyzer {
public:
    CodeStatsResult analyze(const std::filesystem::path& root,
                            const CodeStatsOptions& options = CodeStatsOptions{});

private:
    void visitFile(const std::filesystem::path& filePath,
                   CodeStatsResult& result,
                   const CodeStatsOptions& options);
    void analyzePythonFile(const std::filesystem::path& filePath, LanguageSummary& summary);
    void analyzeBraceLanguageFile(const std::filesystem::path& filePath,
                                  const std::string& language,
                                  LanguageSummary& summary);
};

}  // namespace backend
