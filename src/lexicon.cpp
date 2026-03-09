//
//  lexicon.cpp
//  ZBP
//
//  Created by Piotr Marcol on 09/03/2026.
//

#include "../include/lexicon/lexicon.hpp"

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <utility>

namespace lexicon {

bool Lexicon::Transition::operator==(const Transition &other) const noexcept {
    return symbol == other.symbol && target == other.target;
}

bool Lexicon::StateSignature::operator==(
    const StateSignature &other) const noexcept {
    return isFinal == other.isFinal && transitions == other.transitions;
}

std::size_t
Lexicon::SignatureHash::operator()(const StateSignature &sig) const noexcept {
    std::size_t h = std::hash<bool>{}(sig.isFinal);

    for (const auto &transition : sig.transitions) {
        h ^= std::hash<char>{}(transition.symbol) + 0x9e3779b9U + (h << 6U) +
             (h >> 2U);
        h ^= std::hash<StateId>{}(transition.target) + 0x9e3779b9U + (h << 6U) +
             (h >> 2U);
    }

    return h;
}

void Lexicon::buildFromSorted(const std::vector<std::string> &words) {
    clear();

    root_ = createState(false);
    for (const auto &word : words) {
        insertSorted(word);
    }

    finish();
}

bool Lexicon::contains(const value_type word) const {
    if (root_ == invalidState) {
        return false;
    }

    StateId current = root_;

    for (char c : word) {
        const auto &transitions = states_[current].transitions;

        const auto it =
            std::lower_bound(transitions.begin(), transitions.end(), c,
                             [](const Transition &transition, char symbol) {
                                 return transition.symbol < symbol;
                             });
        if (it == transitions.end() || it->symbol != c) {
            return false;
        }

        current = it->target;
    }

    return states_[current].isFinal;
}

bool Lexicon::empty() const noexcept { return wordCount_ == 0; }

Lexicon::size_type Lexicon::size() const noexcept { return wordCount_; }

Lexicon::size_type Lexicon::stateCount() const noexcept {
    return states_.size();
}

void Lexicon::clear() {
    states_.clear();
    registry_.clear();
    uncheckedPath_.clear();
    previousWord_.clear();
    wordCount_ = 0;
    finalized_ = false;
    root_ = invalidState;
}

void Lexicon::insertSorted(const std::string &word) {
    if (finalized_) {
        throw std::logic_error("Cannot insert words after finish().");
    }

    if (!previousWord_.empty() && word < previousWord_) {
        throw std::invalid_argument(
            "Words must be inserted in lexicographical order.");
    }

    if (word == previousWord_) {
        return;
    }

    const std::size_t commonPrefix = commonPrefixLength(previousWord_, word);

    minimizeFrom(commonPrefix);

    StateId current = root_;
    if (commonPrefix > 0) {
        current = uncheckedPath_[commonPrefix - 1].child;
    }

    for (std::size_t i = commonPrefix; i < word.size(); ++i) {
        const StateId next = createState(false);

        states_[current].transitions.push_back(Transition{word[i], next});
        uncheckedPath_.push_back(UncheckedNode{current, word[i], next});

        current = next;
    }

    states_[current].isFinal = true;
    previousWord_ = word;
    ++wordCount_;
}

void Lexicon::finish() {
    if (finalized_) {
        return;
    }

    minimizeFrom(0);
    finalized_ = true;
}

void Lexicon::minimizeFrom(std::size_t prefixLen) {
    while (uncheckedPath_.size() > prefixLen) {
        const UncheckedNode node = uncheckedPath_.back();
        uncheckedPath_.pop_back();

        const StateId canonicalChild = replaceOrRegister(node.child);
        replaceTransitionTarget(node.parent, node.symbol, canonicalChild);
    }

    if (root_ != invalidState) {
        sortTransitions(states_[root_]);
    }
}

Lexicon::StateId Lexicon::replaceOrRegister(StateId stateId) {
    sortTransitions(states_[stateId]);

    StateSignature signature;
    signature.isFinal = states_[stateId].isFinal;
    signature.transitions = states_[stateId].transitions;

    const auto it = registry_.find(signature);
    if (it != registry_.end()) {
        return it->second;
    }

    registry_.emplace(std::move(signature), stateId);
    return stateId;
}

void Lexicon::replaceTransitionTarget(StateId parent, char symbol,
                                      StateId newTarget) {
    auto &transitions = states_[parent].transitions;

    for (auto &transition : transitions) {
        if (transition.symbol == symbol) {
            transition.target = newTarget;
            return;
        }
    }

    throw std::logic_error("Transition to replace was not found.");
}

void Lexicon::sortTransitions(State &state) {
    std::sort(state.transitions.begin(), state.transitions.end(),
              [](const Transition &lhs, const Transition &rhs) {
                  return lhs.symbol < rhs.symbol;
              });
}

std::size_t Lexicon::commonPrefixLength(std::string_view a,
                                        std::string_view b) {
    const std::size_t limit = std::min(a.size(), b.size());

    std::size_t i = 0;
    while (i < limit && a[i] == b[i]) {
        ++i;
    }

    return i;
}

Lexicon::StateId Lexicon::createState(bool isFinal) {
    states_.push_back(State{isFinal, {}});
    return states_.size() - 1;
}

} // namespace lexicon
