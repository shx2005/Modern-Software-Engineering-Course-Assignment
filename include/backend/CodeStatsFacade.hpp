// File: CodeStatsFacade.hpp
// Description: Declares façade and bridging interfaces that expose code
//              statistics services to multiple language consumers.

#pragma once

#include "backend/CodeStats.hpp"

#include <filesystem>
#include <string>

namespace backend {

// Facade consolidating CodeStatsAnalyzer to serve distinct language requests.
class CodeStatsFacade {
public:
    CodeStatsFacade();

    CodeStatsResult analyzeAll(const std::filesystem::path& root,
                               const CodeStatsOptions& options = CodeStatsOptions{});
    LanguageSummary analyzeCppOnly(const std::filesystem::path& root);
    LanguageSummary analyzeJavaOnly(const std::filesystem::path& root);

    // Placeholder for future Java bridge integration (e.g., JNI).
    LanguageSummary analyzeJavaFromContext(const std::string& rootIdentifier);

    void printLongestFunction(const CodeStatsResult& result) const;
    void printShortestFunction(const CodeStatsResult& result) const;

private:
    CodeStatsAnalyzer m_analyzer;
};

// Plain-old-data summary exposed via C-compatible ABI.
struct LanguageStatsC {
    std::size_t fileCount;
    std::size_t lineCount;
};

extern "C" {
    LanguageStatsC get_cpp_code_stats(const char* directory);
    LanguageStatsC get_java_code_stats(const char* directory);
    void print_longest_function(const char* directory);
    void print_shortest_function(const char* directory);
}

}  // namespace backend
