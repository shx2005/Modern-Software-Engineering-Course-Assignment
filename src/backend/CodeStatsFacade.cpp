// File: CodeStatsFacade.cpp
// Description: Provides façade implementations for serving code statistics
//              to various consumer interfaces (C/C++ and Java placeholders).

#include "backend/CodeStatsFacade.hpp"

#include "backend/Logger.hpp"

#include <iostream>
#include <sstream>
#include <utility>

namespace backend {

namespace {

LanguageSummary findSummaryForKey(const CodeStatsResult& result,
                                  const std::string& key) {
    const auto it = result.languageSummaries.find(key);
    if (it != result.languageSummaries.end()) {
        return it->second;
    }
    return {};
}

LanguageStatsC toLanguageStatsC(const LanguageSummary& summary) {
    LanguageStatsC cSummary{};
    cSummary.fileCount = summary.fileCount;
    cSummary.lineCount = summary.lineCount;
    return cSummary;
}

}  // namespace

CodeStatsFacade::CodeStatsFacade() = default;

CodeStatsResult CodeStatsFacade::analyzeAll(const std::filesystem::path& root,
                                            const CodeStatsOptions& options) {
    return m_analyzer.analyze(root, options);
}

LanguageSummary CodeStatsFacade::analyzeCppOnly(const std::filesystem::path& root) {
    const CodeStatsResult result = analyzeAll(root);
    LanguageSummary summary{};
    const auto accumulate = [&](const std::string& key) {
        const auto it = result.languageSummaries.find(key);
        if (it != result.languageSummaries.end()) {
            summary.fileCount += it->second.fileCount;
            summary.lineCount += it->second.lineCount;
            summary.blankLineCount += it->second.blankLineCount;
            summary.commentLineCount += it->second.commentLineCount;
        }
    };
    accumulate("C");
    accumulate("C++");
    return summary;
}

LanguageSummary CodeStatsFacade::analyzeJavaOnly(const std::filesystem::path& root) {
    const CodeStatsResult result = analyzeAll(root);
    return findSummaryForKey(result, "Java");
}

LanguageSummary CodeStatsFacade::analyzeJavaFromContext(const std::string& rootIdentifier) {
    // TODO: Bridge Java runtime context into filesystem paths or packaged sources.
    (void)rootIdentifier;
    return {};
}

std::string CodeStatsFacade::printLongestFunction(const CodeStatsResult& result) const {
    const FunctionDetail* best = nullptr;
    for (const auto& [language, summary] : result.languageSummaries) {
        (void)language;
        for (const auto& detail : summary.functions.details) {
            if (best == nullptr || detail.length > best->length) {
                best = &detail;
            }
        }
    }
    if (best == nullptr) {
        return {};
    }
    std::ostringstream oss;
    oss << "最长函数 " << best->name << " ("
        << best->length << " 行, 语言: " << best->language << ") - 文件: "
        << best->filePath.string() << " (第 " << best->lineNumber << " 行)";
    return oss.str();
}

std::string CodeStatsFacade::printShortestFunction(const CodeStatsResult& result) const {
    const FunctionDetail* best = nullptr;
    for (const auto& [language, summary] : result.languageSummaries) {
        (void)language;
        for (const auto& detail : summary.functions.details) {
            if (best == nullptr || detail.length < best->length) {
                best = &detail;
            }
        }
    }
    if (best == nullptr) {
        return {};
    }
    std::ostringstream oss;
    oss << "最短函数 " << best->name << " ("
        << best->length << " 行, 语言: " << best->language << ") - 文件: "
        << best->filePath.string() << " (第 " << best->lineNumber << " 行)";
    return oss.str();
}

LanguageStatsC get_cpp_code_stats(const char* directory) {
    const std::filesystem::path rootPath = directory != nullptr ? directory : ".";
    CodeStatsFacade facade;
    const LanguageSummary summary = facade.analyzeCppOnly(rootPath);
    return toLanguageStatsC(summary);
}

LanguageStatsC get_java_code_stats(const char* directory) {
    const std::filesystem::path rootPath = directory != nullptr ? directory : ".";
    CodeStatsFacade facade;
    const LanguageSummary summary = facade.analyzeJavaOnly(rootPath);
    return toLanguageStatsC(summary);
}

void print_longest_function(const char* directory) {
    const std::filesystem::path rootPath = directory != nullptr ? directory : ".";
    CodeStatsFacade facade;
    const CodeStatsResult result = facade.analyzeAll(rootPath);
    const std::string summary = facade.printLongestFunction(result);
    if (!summary.empty()) {
        std::cout << summary << std::endl;
    }
}

void print_shortest_function(const char* directory) {
    const std::filesystem::path rootPath = directory != nullptr ? directory : ".";
    CodeStatsFacade facade;
    const CodeStatsResult result = facade.analyzeAll(rootPath);
    const std::string summary = facade.printShortestFunction(result);
    if (!summary.empty()) {
        std::cout << summary << std::endl;
    }
}

}  // namespace backend
