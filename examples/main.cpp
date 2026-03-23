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
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

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
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        words.push_back(line);
    }

    return words;
}

void appendLowercasePolishUtf8(std::string &normalized,
                               unsigned char firstByte,
                               unsigned char secondByte) {
    if (firstByte == 0xC3U && secondByte == 0x93U) {
        normalized.push_back(static_cast<char>(0xC3U));
        normalized.push_back(static_cast<char>(0xB3U));
        return;
    }

    if (firstByte == 0xC4U) {
        if (secondByte == 0x84U || secondByte == 0x86U ||
            secondByte == 0x98U) {
            normalized.push_back(static_cast<char>(firstByte));
            normalized.push_back(static_cast<char>(secondByte + 1U));
            return;
        }
    }

    if (firstByte == 0xC5U) {
        if (secondByte == 0x81U || secondByte == 0x83U ||
            secondByte == 0x9AU || secondByte == 0xB9U ||
            secondByte == 0xBBU) {
            normalized.push_back(static_cast<char>(firstByte));
            normalized.push_back(static_cast<char>(secondByte + 1U));
            return;
        }
    }

    normalized.push_back(static_cast<char>(firstByte));
    normalized.push_back(static_cast<char>(secondByte));
}

std::string normalizeForMode(const std::string &word, lexicon::CaseMode mode) {
    if (mode == lexicon::CaseMode::Sensitive) {
        return word;
    }

    std::string normalized;
    normalized.reserve(word.size());

    for (std::size_t i = 0; i < word.size(); ++i) {
        const unsigned char character = static_cast<unsigned char>(word[i]);

        if (character < 0x80U) {
            normalized.push_back(
                static_cast<char>(std::tolower(character)));
            continue;
        }

        if (i + 1 < word.size()) {
            appendLowercasePolishUtf8(
                normalized, character,
                static_cast<unsigned char>(word[i + 1]));
            ++i;
            continue;
        }

        normalized.push_back(static_cast<char>(character));
    }

    return normalized;
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

    lexicon::Lexicon dictionary(mode);
    const auto buildStart = std::chrono::steady_clock::now();
    dictionary.buildFromSorted(words);
    const auto buildEnd = std::chrono::steady_clock::now();
    const auto buildDuration =
        std::chrono::duration_cast<std::chrono::microseconds>(buildEnd -
                                                              buildStart);

    std::cout << title << '\n';
    std::cout << "Words count:      " << dictionary.size() << '\n';
    std::cout << "Allocated states: " << dictionary.stateCount() << '\n';
    std::cout << "Reachable states: " << dictionary.reachableStateCount()
              << '\n';
    std::cout << "Transitions:      " << dictionary.transitionCount() << '\n';
    std::cout << "Build time:       " << buildDuration.count() << " us\n";
    std::cout << '\n';

    for (const auto &query : queries) {
        std::cout << query << " -> "
                  << (dictionary.contains(query) ? "FOUND" : "NOT FOUND")
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
            "Banan" // Similar to "Banana" but should be sorted before it in both modes.
            "Banana",
            "car"
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
    }

    runDemo(words, queries, mode, title, dotPath);

    return 0;
}
