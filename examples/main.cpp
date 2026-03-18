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
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void printUsage(const char *programName) {
    std::cerr << "Usage: " << programName << " [-cs] [-f WORDS_FILE]\n";
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

std::string normalizeForMode(const std::string &word, lexicon::CaseMode mode) {
    if (mode == lexicon::CaseMode::Sensitive) {
        return word;
    }

    std::string normalized = word;
    std::transform(
        normalized.begin(), normalized.end(), normalized.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
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
             const std::string &title) {
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

    std::cout << '\n';
}

} // namespace

int main(int argc, char *argv[]) {
    std::string wordsFilePath;
    lexicon::CaseMode mode = lexicon::CaseMode::Insensitive;

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

        printUsage(argv[0]);
        return 1;
    }

    std::vector<std::string> words;
    if (wordsFilePath.empty()) {
        words = {
            "Apple",
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
        "Car"
    };

    const std::string title =
        mode == lexicon::CaseMode::Sensitive ? "Lexicon (Sensitive)"
                                             : "Lexicon (Insensitive)";
    runDemo(words, queries, mode, title);

    return 0;
}
