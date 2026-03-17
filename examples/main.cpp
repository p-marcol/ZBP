//
//  main.cpp
//  ZBP
//
//  Created by Piotr Marcol on 09/03/2026.
//

#include <lexicon/lexicon.hpp>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>


int main() {
    std::vector<std::string> words = {
        "car",
        "card",
        "care",
        "careful",
        "cat",
        "dog",
        "dot"
    };

    std::sort(words.begin(), words.end());

    lexicon::Lexicon dictionary;
    dictionary.buildFromSorted(words);

    std::cout << "Words count:  " << dictionary.size() << '\n';
    std::cout << "States count: " << dictionary.stateCount() << '\n';
    std::cout << '\n';

    const std::vector<std::string> queries = {
        "car",
        "care",
        "carefully",
        "cat",
        "cow",
        "dog",
        "do"
    };

    for (const auto& query : queries) {
        std::cout << query << " -> "
                  << (dictionary.contains(query) ? "FOUND" : "NOT FOUND")
                  << '\n';
    }

    return 0;
}
