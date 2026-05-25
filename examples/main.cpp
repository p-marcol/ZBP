//
//  main.cpp
//  ZBP
//
//  Created by Piotr Marcol on 09/03/2026.
//

#include <lexicon/lexicon.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace {

char32_t lowercaseCodePoint(char32_t symbol) {
    if (symbol <= 0x7FU) {
        return static_cast<char32_t>(
            std::tolower(static_cast<unsigned char>(symbol)));
    }

    switch (symbol) {
    case U'Ą':
        return U'ą';
    case U'Ć':
        return U'ć';
    case U'Ę':
        return U'ę';
    case U'Ł':
        return U'ł';
    case U'Ń':
        return U'ń';
    case U'Ó':
        return U'ó';
    case U'Ś':
        return U'ś';
    case U'Ź':
        return U'ź';
    case U'Ż':
        return U'ż';
    default:
        return symbol;
    }
}

std::u32string decodeUtf8(std::string_view text) {
    std::u32string decoded;
    decoded.reserve(text.size());

    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char firstByte = static_cast<unsigned char>(text[i]);

        if (firstByte < 0x80U) {
            decoded.push_back(static_cast<char32_t>(firstByte));
            continue;
        }

        if ((firstByte & 0xE0U) == 0xC0U && i + 1 < text.size()) {
            const unsigned char secondByte =
                static_cast<unsigned char>(text[i + 1]);
            if ((secondByte & 0xC0U) == 0x80U) {
                decoded.push_back(
                    (static_cast<char32_t>(firstByte & 0x1FU) << 6U) |
                    static_cast<char32_t>(secondByte & 0x3FU));
                ++i;
                continue;
            }
        }

        if ((firstByte & 0xF0U) == 0xE0U && i + 2 < text.size()) {
            const unsigned char secondByte =
                static_cast<unsigned char>(text[i + 1]);
            const unsigned char thirdByte =
                static_cast<unsigned char>(text[i + 2]);
            if ((secondByte & 0xC0U) == 0x80U &&
                (thirdByte & 0xC0U) == 0x80U) {
                decoded.push_back(
                    (static_cast<char32_t>(firstByte & 0x0FU) << 12U) |
                    (static_cast<char32_t>(secondByte & 0x3FU) << 6U) |
                    static_cast<char32_t>(thirdByte & 0x3FU));
                i += 2;
                continue;
            }
        }

        if ((firstByte & 0xF8U) == 0xF0U && i + 3 < text.size()) {
            const unsigned char secondByte =
                static_cast<unsigned char>(text[i + 1]);
            const unsigned char thirdByte =
                static_cast<unsigned char>(text[i + 2]);
            const unsigned char fourthByte =
                static_cast<unsigned char>(text[i + 3]);
            if ((secondByte & 0xC0U) == 0x80U &&
                (thirdByte & 0xC0U) == 0x80U &&
                (fourthByte & 0xC0U) == 0x80U) {
                decoded.push_back(
                    (static_cast<char32_t>(firstByte & 0x07U) << 18U) |
                    (static_cast<char32_t>(secondByte & 0x3FU) << 12U) |
                    (static_cast<char32_t>(thirdByte & 0x3FU) << 6U) |
                    static_cast<char32_t>(fourthByte & 0x3FU));
                i += 3;
                continue;
            }
        }

        decoded.push_back(static_cast<char32_t>(firstByte));
    }

    return decoded;
}

void printUsage(const char *programName) {
    std::cerr << "Usage: " << programName
              << " [-cs] [-f WORDS_FILE] [-dot [DOT_FILE]]\n";
}

std::vector<std::string> loadWordsFromFile(const std::string &path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Failed to open words file: " + path);
    }

    std::vector<std::string> words;
    std::size_t totalSymbols = 0;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        totalSymbols += decodeUtf8(line).size();
        words.push_back(line);
    }

    std::cout << "Loaded " << words.size() << " words from file: " << path
              << '\n';
    std::cout << "Average word length: "
              << (words.empty() ? 0.0
                                : static_cast<double>(totalSymbols) /
                                      static_cast<double>(words.size()))
              << '\n';
    std::cout << "Total characters: " << totalSymbols << '\n';
    return words;
}

std::string normalizeForMode(const std::string &word, lexicon::CaseMode mode) {
    std::u32string normalized = decodeUtf8(word);

    if (mode == lexicon::CaseMode::Insensitive) {
        for (auto &symbol : normalized) {
            symbol = lowercaseCodePoint(symbol);
        }
    }

    std::string encoded;
    encoded.reserve(word.size());

    for (const char32_t symbol : normalized) {
        if (symbol <= 0x7FU) {
            encoded.push_back(static_cast<char>(symbol));
            continue;
        }

        if (symbol <= 0x7FFU) {
            encoded.push_back(
                static_cast<char>(0xC0U | ((symbol >> 6U) & 0x1FU)));
            encoded.push_back(static_cast<char>(0x80U | (symbol & 0x3FU)));
            continue;
        }

        if (symbol <= 0xFFFFU) {
            encoded.push_back(
                static_cast<char>(0xE0U | ((symbol >> 12U) & 0x0FU)));
            encoded.push_back(
                static_cast<char>(0x80U | ((symbol >> 6U) & 0x3FU)));
            encoded.push_back(static_cast<char>(0x80U | (symbol & 0x3FU)));
            continue;
        }

        encoded.push_back(
            static_cast<char>(0xF0U | ((symbol >> 18U) & 0x07U)));
        encoded.push_back(
            static_cast<char>(0x80U | ((symbol >> 12U) & 0x3FU)));
        encoded.push_back(
            static_cast<char>(0x80U | ((symbol >> 6U) & 0x3FU)));
        encoded.push_back(static_cast<char>(0x80U | (symbol & 0x3FU)));
    }

    return encoded;
}

std::string formatBytes(std::size_t bytes) {
    constexpr double unit = 1024.0;
    const char *units[] = {"B", "KiB", "MiB", "GiB"};

    double value = static_cast<double>(bytes);
    std::size_t unitIndex = 0;
    while (value >= unit && unitIndex + 1 < std::size(units)) {
        value /= unit;
        ++unitIndex;
    }

    std::ostringstream formatted;
    if (unitIndex == 0) {
        formatted << bytes << ' ' << units[unitIndex];
    } else {
        formatted << std::fixed << std::setprecision(2) << value << ' '
                  << units[unitIndex];
    }

    return formatted.str();
}

std::size_t alignTo(std::size_t value, std::size_t alignment) {
    const std::size_t remainder = value % alignment;
    return remainder == 0 ? value : value + alignment - remainder;
}

std::size_t stringStorageEstimate(const std::string &word) {
    return sizeof(std::string) + word.capacity() + 1;
}

std::vector<std::string>
normalizedWordsForMode(const std::vector<std::string> &words,
                       lexicon::CaseMode mode) {
    std::vector<std::string> normalizedWords;
    normalizedWords.reserve(words.size());

    for (const auto &word : words) {
        normalizedWords.push_back(normalizeForMode(word, mode));
    }

    return normalizedWords;
}

std::size_t estimateSetMemoryUsage(const std::set<std::string> &words) {
    constexpr std::size_t nodeOverhead =
        3 * sizeof(void *) + sizeof(bool);

    std::size_t bytes = sizeof(words);
    for (const auto &word : words) {
        bytes += alignTo(nodeOverhead + stringStorageEstimate(word),
                         alignof(std::max_align_t));
    }
    return bytes;
}

std::size_t estimateUnorderedSetMemoryUsage(
    const std::unordered_set<std::string> &words) {
    constexpr std::size_t nodeOverhead = sizeof(void *);

    std::size_t bytes = sizeof(words);
    bytes += words.bucket_count() * sizeof(void *);

    for (const auto &word : words) {
        bytes += alignTo(nodeOverhead + stringStorageEstimate(word),
                         alignof(std::max_align_t));
    }
    return bytes;
}

std::size_t inputByteSize(const std::vector<std::string> &words) {
    std::size_t bytes = 0;
    for (const auto &word : words) {
        bytes += word.size();
    }
    return bytes;
}

std::size_t inputSymbolCount(const std::vector<std::string> &words) {
    std::size_t symbols = 0;
    for (const auto &word : words) {
        symbols += decodeUtf8(word).size();
    }
    return symbols;
}

void sortWordsForMode(std::vector<std::string> &words, lexicon::CaseMode mode) {
    std::stable_sort(words.begin(), words.end(),
                     [mode](const std::string &lhs, const std::string &rhs) {
                         return normalizeForMode(lhs, mode) <
                                normalizeForMode(rhs, mode);
                     });
}

void runDemo(const std::vector<std::string> &sourceWords,
             const std::vector<std::string> &queries, lexicon::CaseMode mode,
             const std::string &title, const std::string &dotPath) {
    std::vector<std::string> words = sourceWords;
    sortWordsForMode(words, mode);

    const std::vector<std::string> normalizedWords =
        normalizedWordsForMode(sourceWords, mode);
    const std::set<std::string> setDictionary(normalizedWords.begin(),
                                              normalizedWords.end());
    const std::unordered_set<std::string> unorderedSetDictionary(
        normalizedWords.begin(), normalizedWords.end());

    lexicon::Lexicon dictionary(mode);
    const auto buildStart = std::chrono::steady_clock::now();
    dictionary.buildFromSorted(words);
    const auto buildEnd = std::chrono::steady_clock::now();
    const auto buildDuration =
        std::chrono::duration_cast<std::chrono::microseconds>(buildEnd -
                                                              buildStart);

    std::cout << title << '\n';
    std::cout << '\n';

    std::cout << "[Input]\n";
    std::cout << "Words:             " << sourceWords.size() << '\n';
    std::cout << "Raw size:          "
              << formatBytes(inputByteSize(sourceWords)) << '\n';
    std::cout << "Characters:        " << inputSymbolCount(sourceWords)
              << '\n';
    std::cout << '\n';

    std::cout << "[std::set]\n";
    std::cout << "Unique words:      " << setDictionary.size() << '\n';
    std::cout << "Estimated memory:  "
              << formatBytes(estimateSetMemoryUsage(setDictionary)) << '\n';
    std::cout << '\n';

    std::cout << "[std::unordered_set]\n";
    std::cout << "Unique words:      "
              << unorderedSetDictionary.size() << '\n';
    std::cout << "Estimated memory:  "
              << formatBytes(
                     estimateUnorderedSetMemoryUsage(unorderedSetDictionary))
              << '\n';
    std::cout << '\n';

    std::cout << "[DAFSA before final minimization]\n";
    std::cout << "Unique words:      " << dictionary.size() << '\n';
    std::cout << "Allocated states:  "
              << dictionary.preMinimizationStateCount() << '\n';
    std::cout << "Estimated memory:  "
              << formatBytes(dictionary.preMinimizationMemoryUsageEstimate())
              << '\n';
    std::cout << '\n';

    std::cout << "[DAFSA after minimization + compaction]\n";
    std::cout << "Stored states:     " << dictionary.stateCount() << '\n';
    std::cout << "Reachable states:  " << dictionary.reachableStateCount()
              << '\n';
    std::cout << "Transitions:       " << dictionary.transitionCount() << '\n';
    std::cout << "Estimated memory:  "
              << formatBytes(dictionary.memoryUsageEstimate()) << '\n';
    std::cout << "Build time:        " << buildDuration.count() << " us\n";
    std::cout << '\n';

    std::cout << "[Iterator]\n";
    std::cout << "Words:             ";
    std::size_t printedWords = 0;
    for (const auto &word : dictionary) {
        if (printedWords > 0) {
            std::cout << ", ";
        }
        std::cout << word;
        ++printedWords;

        if (printedWords == 16 && dictionary.size() > printedWords) {
            std::cout << ", ...";
            break;
        }
    }
    std::cout << '\n';

    const auto appleIt = dictionary.find("Apple");
    std::cout << "find(\"Apple\"):   "
              << (appleIt == dictionary.end() ? "end" : *appleIt) << '\n';
    std::cout << "count(\"Apple\"):  " << dictionary.count("Apple") << '\n';
    std::cout << '\n';

    std::cout << "[Queries]\n";
    for (const auto &query : queries) {
        const auto lookupStart = std::chrono::steady_clock::now();
        const bool found = dictionary.contains(query);
        const auto lookupEnd = std::chrono::steady_clock::now();
        const auto lookupDuration =
            std::chrono::duration_cast<std::chrono::nanoseconds>(lookupEnd -
                                                                 lookupStart);

        std::cout << query << " -> "
                  << (found ? "FOUND" : "NOT FOUND") << " ("
                  << lookupDuration.count() << " ns)"
                  << '\n';
    }

    if (!dotPath.empty()) {
        if (dictionary.exportToDotFile(dotPath)) {
            std::cout << '\n' << "DOT graph saved to: " << dotPath << '\n';
        } else {
            std::cout << '\n' << "Failed to save DOT graph to: " << dotPath
                      << '\n';
        }
    }

    std::cout << '\n';
}

std::string defaultDotPath() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    const std::tm *localTime = std::localtime(&time);

    if (localTime == nullptr) {
        return "lexicon.dot";
    }

    std::ostringstream path;
    path << "graphs/" << std::put_time(localTime, "%Y%m%d_%H%M%S") << ".dot";
    return path.str();
}

} // namespace

int main(int argc, char *argv[]) {
    std::string wordsFilePath;
    std::string dotPath;
    lexicon::CaseMode mode = lexicon::CaseMode::Insensitive;
    bool exportDot = false;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];

        if (argument == "-cs") {
            mode = lexicon::CaseMode::Sensitive;
            continue;
        }

        if (argument == "-f") {
            if (i + 1 >= argc) {
                printUsage(argv[0]);
                return 1;
            }

            wordsFilePath = argv[++i];
            continue;
        }

        if (argument == "-dot") {
            exportDot = true;

            if (i + 1 < argc) {
                const std::string nextArgument = argv[i + 1];
                if (!nextArgument.empty() && nextArgument[0] != '-') {
                    dotPath = argv[++i];
                }
            }

            continue;
        }

        printUsage(argv[0]);
        return 1;
    }

    std::vector<std::string> words;
    if (wordsFilePath.empty()) {
        words = {
            "Apple",
            "Banan",
            "Banana",
            "car",
            "Pneumonoultramicroscopicsilicovolcanoconiosis",
            "Łódź",
            "Żaba"
        };
    } else {
        words = loadWordsFromFile(wordsFilePath);
    }

    const std::vector<std::string> queries = {
        "Apple",
        "apple",
        "APPLE",
        "Banana",
        "banana",
        "car",
        "Car",
        "Pneumonoultramicroscopicsilicovolcanoconiosis",
        "pneumonoultramicroscopicsilicovolcanoconiosis",
        "Łódź",
        "łódź",
        "Żaba",
        "żaba"
    };

    const std::string title =
        mode == lexicon::CaseMode::Sensitive ? "Lexicon (Sensitive)"
                                             : "Lexicon (Insensitive)";

    if (exportDot && dotPath.empty()) {
        dotPath = defaultDotPath();
        std::filesystem::create_directories("graphs");
    }

    runDemo(words, queries, mode, title, dotPath);

    return 0;
}
