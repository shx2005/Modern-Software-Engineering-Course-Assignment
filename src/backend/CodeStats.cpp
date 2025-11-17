// File: CodeStats.cpp
// Description: Implements code statistics extraction for multiple languages.

#include "backend/CodeStats.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <numeric>
#include <string>
#include <unordered_set>

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

struct LineMetrics {
    std::size_t logical{0};
    std::size_t blank{0};
    std::size_t comment{0};
};

LineMetrics computeLineMetrics(const std::filesystem::path& filePath,
                               const std::string& language,
                               bool includeBlank,
                               bool includeComment) {
    LineMetrics metrics;
    std::ifstream stream(filePath);
    if (!stream.is_open()) {
        return metrics;
    }

    std::string line;
    bool inBlockComment = false;
    while (std::getline(stream, line)) {
        const std::string trimmed = trim(line);
        if (trimmed.empty()) {
            if (includeBlank) {
                metrics.blank += 1;
            }
            continue;
        }

        bool isCommentLine = false;
        if (language == "Python") {
            if (!trimmed.empty() && trimmed[0] == '#') {
                isCommentLine = true;
            }
        } else {
            if (inBlockComment) {
                isCommentLine = true;
                if (trimmed.find("*/") != std::string::npos) {
                    inBlockComment = false;
                }
            } else {
                if (trimmed.rfind("//", 0) == 0) {
                    isCommentLine = true;
                } else if (trimmed.rfind("/*", 0) == 0) {
                    isCommentLine = true;
                    if (trimmed.find("*/", 0) == std::string::npos) {
                        inBlockComment = true;
                    }
                } else {
                    const std::size_t blockPos = trimmed.find("/*");
                    if (blockPos != std::string::npos &&
                        trimmed.find("*/", blockPos + 2) == std::string::npos) {
                        inBlockComment = true;
                    }
                }

                if (!isCommentLine && trimmed.rfind("*", 0) == 0) {
                    isCommentLine = true;
                }
            }
        }

        if (includeComment && isCommentLine) {
            metrics.comment += 1;
        }

        if (!isCommentLine) {
            metrics.logical += 1;
        }
    }

    return metrics;
}

int countChar(const std::string& text, char target) {
    return static_cast<int>(std::count(text.begin(), text.end(), target));
}

std::string toLowerCopy(std::string value) {
    std::transform(value.begin(),
                   value.end(),
                   value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool isControlKeyword(const std::string& token) {
    static const std::unordered_set<std::string> keywords{
        "if",    "for",   "while", "switch", "catch",   "return", "else",
        "class", "struct","enum",  "case",   "default", "using",  "typedef"};
    return keywords.find(token) != keywords.end();
}

bool looksLikeFunctionSignature(const std::string& signature) {
    const std::string trimmedSignature = trim(signature);
    if (trimmedSignature.empty()) {
        return false;
    }
    const std::size_t parenOpen = trimmedSignature.find('(');
    const std::size_t parenClose = trimmedSignature.find(')');
    if (parenOpen == std::string::npos || parenClose == std::string::npos || parenClose < parenOpen) {
        return false;
    }
    if (trimmedSignature[0] == '#' || trimmedSignature.back() == ';') {
        return false;
    }
    const std::string lower = toLowerCopy(trimmedSignature);
    const std::size_t firstSpace = lower.find_first_of(" \t(");
    if (firstSpace != std::string::npos) {
        const std::string head = lower.substr(0, firstSpace);
        if (isControlKeyword(head)) {
            return false;
        }
    }
    if (lower.find(" operator") != std::string::npos) {
        return true;
    }
    if (lower.find(" namespace ") != std::string::npos || lower.rfind("namespace", 0) == 0) {
        return false;
    }
    return true;
}

std::string extractFunctionName(const std::string& signature) {
    const std::size_t parenPos = signature.find('(');
    if (parenPos == std::string::npos) {
        return "anonymous";
    }
    std::string left = trim(signature.substr(0, parenPos));
    if (left.empty()) {
        return "anonymous";
    }
    const std::size_t tokenStart = left.find_last_of(" \t:*&");
    std::string candidate =
        tokenStart == std::string::npos ? left : left.substr(tokenStart + 1);
    if (candidate.empty()) {
        candidate = "anonymous";
    }
    return candidate;
}

std::string stripInlineComment(const std::string& line) {
    const std::size_t commentPos = line.find("//");
    if (commentPos != std::string::npos) {
        return line.substr(0, commentPos);
    }
    return line;
}

}  // namespace

CodeStatsResult CodeStatsAnalyzer::analyze(const std::filesystem::path& root,
                                           const CodeStatsOptions& options) {
    CodeStatsResult result;
    result.includeBlankLines = options.includeBlankLines;
    result.includeCommentLines = options.includeCommentLines;

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
            visitFile(entry.path(), result, options);
        }

        ec.clear();
        ++it;
    }

    for (auto& [_, summary] : result.languageSummaries) {
        auto& fnSummary = summary.functions;
        if (fnSummary.lengths.empty()) {
            fnSummary.functionCount = 0;
            fnSummary.minLength = 0;
            fnSummary.maxLength = 0;
            fnSummary.averageLength = 0.0;
            fnSummary.medianLength = 0.0;
            continue;
        }
        std::sort(fnSummary.lengths.begin(), fnSummary.lengths.end());
        fnSummary.functionCount = fnSummary.lengths.size();
        fnSummary.minLength = fnSummary.lengths.front();
        fnSummary.maxLength = fnSummary.lengths.back();
        fnSummary.averageLength =
            std::accumulate(fnSummary.lengths.begin(), fnSummary.lengths.end(), 0.0) /
            static_cast<double>(fnSummary.lengths.size());

        if (fnSummary.lengths.size() % 2 == 0) {
            const std::size_t mid = fnSummary.lengths.size() / 2;
            fnSummary.medianLength =
                (fnSummary.lengths[mid - 1] + fnSummary.lengths[mid]) / 2.0;
        } else {
            fnSummary.medianLength = static_cast<double>(
                fnSummary.lengths[fnSummary.lengths.size() / 2]);
        }
    }

    if (!options.languages.empty()) {
        for (const auto& language : options.languages) {
            result.languageSummaries.try_emplace(language, LanguageSummary{});
            result.includedLanguages.insert(language);
        }
    }

    return result;
}

void CodeStatsAnalyzer::visitFile(const std::filesystem::path& filePath,
                                  CodeStatsResult& result,
                                  const CodeStatsOptions& options) {
    static const std::vector<std::string> cExt{".c"};
    static const std::vector<std::string> cppExt{
        ".C",  ".cc", ".cpp", ".cxx", ".h", ".hpp", ".hh", ".hxx"};
    static const std::vector<std::string> csharpExt{".cs"};
    static const std::vector<std::string> javaExt{".java"};
    static const std::vector<std::string> pyExt{".py"};

    std::string languageKey;

    if (hasExtension(filePath, cExt)) {
        languageKey = "C";
    } else if (hasExtension(filePath, cppExt)) {
        languageKey = "C++";
    } else if (hasExtension(filePath, csharpExt)) {
        languageKey = "C#";
    } else if (hasExtension(filePath, javaExt)) {
        languageKey = "Java";
    } else if (hasExtension(filePath, pyExt)) {
        languageKey = "Python";
    }

    if (languageKey.empty()) {
        return;
    }

    if (!options.languages.empty() && options.languages.find(languageKey) == options.languages.end()) {
        return;
    }

    result.includedLanguages.insert(languageKey);
    LanguageSummary& languageSummary = result.languageSummaries[languageKey];

    const LineMetrics metrics =
        computeLineMetrics(filePath, languageKey, options.includeBlankLines, options.includeCommentLines);
    languageSummary.fileCount += 1;
    languageSummary.lineCount += metrics.logical;
    result.totalLines += metrics.logical;
    if (options.includeBlankLines) {
        languageSummary.blankLineCount += metrics.blank;
        result.totalBlankLines += metrics.blank;
    }
    if (options.includeCommentLines) {
        languageSummary.commentLineCount += metrics.comment;
        result.totalCommentLines += metrics.comment;
    }

    if (languageKey == "Python") {
        analyzePythonFile(filePath, languageSummary);
    } else if (languageKey == "C" || languageKey == "C++" || languageKey == "C#" || languageKey == "Java") {
        analyzeBraceLanguageFile(filePath, languageKey, languageSummary);
    }
}

void CodeStatsAnalyzer::analyzePythonFile(const std::filesystem::path& filePath,
                                          LanguageSummary& summary) {
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
            const std::size_t definitionLine = i + 1;
            std::string functionName;
            {
                const std::size_t nameStart = trimmedLine.find(' ') + 1;
                std::size_t nameEnd = trimmedLine.find('(', nameStart);
                if (nameEnd == std::string::npos) {
                    nameEnd = trimmedLine.find(':', nameStart);
                }
                if (nameEnd != std::string::npos && nameEnd > nameStart) {
                    functionName = trimmedLine.substr(nameStart, nameEnd - nameStart);
                } else {
                    functionName = "unknown";
                }
            }

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
                summary.functions.lengths.push_back(length);
                FunctionDetail detail{};
                detail.language = "Python";
                detail.name = functionName;
                detail.filePath = filePath;
                detail.lineNumber = definitionLine;
                detail.length = length;
                summary.functions.details.push_back(detail);
            }
        }
    }
}

void CodeStatsAnalyzer::analyzeBraceLanguageFile(const std::filesystem::path& filePath,
                                                 const std::string& language,
                                                 LanguageSummary& summary) {
    std::ifstream stream(filePath);
    if (!stream.is_open()) {
        return;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }

    std::string signatureBuffer;
    int signatureStartLine = 0;
    int pendingSignatureLines = 0;
    bool awaitingBody = false;
    bool insideFunction = false;
    int braceDepth = 0;
    int functionLength = 0;
    int functionStartLine = 0;
    std::string functionName;

    auto resetSignature = [&]() {
        signatureBuffer.clear();
        signatureStartLine = 0;
        pendingSignatureLines = 0;
        awaitingBody = false;
    };

    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::string codeLine = stripInlineComment(lines[i]);
        const std::string trimmed = trim(codeLine);

        if (!insideFunction) {
            if (trimmed.empty()) {
                if (!awaitingBody) {
                    resetSignature();
                }
                continue;
            }

            if (signatureBuffer.empty()) {
                signatureStartLine = static_cast<int>(i + 1);
            }

            signatureBuffer += " " + codeLine;
            pendingSignatureLines += 1;

            if (signatureBuffer.find('(') != std::string::npos) {
                awaitingBody = true;
            }

            if (awaitingBody && codeLine.find('{') != std::string::npos) {
                if (looksLikeFunctionSignature(signatureBuffer)) {
                    insideFunction = true;
                    functionStartLine = signatureStartLine;
                    functionName = extractFunctionName(signatureBuffer);
                    functionLength = pendingSignatureLines;
                    braceDepth = countChar(codeLine, '{') - countChar(codeLine, '}');
                    if (braceDepth <= 0) {
                        FunctionDetail detail{};
                        detail.language = language;
                        detail.name = functionName;
                        detail.filePath = filePath;
                        detail.lineNumber = static_cast<std::size_t>(functionStartLine);
                        detail.length = functionLength;
                        summary.functions.lengths.push_back(functionLength);
                        summary.functions.details.push_back(detail);
                        insideFunction = false;
                        resetSignature();
                        functionLength = 0;
                        functionName.clear();
                    }
                } else {
                    resetSignature();
                }
            } else if (!awaitingBody && codeLine.find(';') != std::string::npos) {
                resetSignature();
            }
            continue;
        }

        if (!trimmed.empty()) {
            functionLength += 1;
        }

        braceDepth += countChar(codeLine, '{');
        braceDepth -= countChar(codeLine, '}');

        if (braceDepth <= 0) {
            FunctionDetail detail{};
            detail.language = language;
            detail.name = functionName.empty() ? "anonymous" : functionName;
            detail.filePath = filePath;
            detail.lineNumber = static_cast<std::size_t>(functionStartLine);
            detail.length = functionLength;
            summary.functions.lengths.push_back(functionLength);
            summary.functions.details.push_back(detail);

            insideFunction = false;
            resetSignature();
            braceDepth = 0;
            functionLength = 0;
            functionName.clear();
        }
    }
}

}  // namespace backend
