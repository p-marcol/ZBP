//
//  lexicon.cpp
//  ZBP
//
//  Created by Piotr Marcol on 09/03/2026.
//

#include <lexicon/lexicon.hpp>

#include <algorithm>
#include <cctype>
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
        // Normalize at the boundary so the DFA and minimization operate on one
        // canonical representation of each key.
        insertSorted(normalize(word));
    }

    finish();
}

bool Lexicon::contains(const value_type word) const {
    if (root_ == invalidState) {
        return false;
    }

    const std::string normalizedWord = normalize(word);
    StateId current = root_;

    for (char c : normalizedWord) {
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

Lexicon::size_type Lexicon::reachableStateCount() const noexcept {
    return reachableMetrics().stateCount;
}

Lexicon::size_type Lexicon::transitionCount() const noexcept {
    return reachableMetrics().transitionCount;
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

std::string Lexicon::normalize(const std::string &word) const {
    if (caseMode_ == CaseMode::Sensitive) {
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

Lexicon::ReachableMetrics Lexicon::reachableMetrics() const noexcept {
    if (root_ == invalidState) {
        return {};
    }

    std::vector<unsigned char> visited(states_.size(), 0);
    std::vector<StateId> stack;
    stack.reserve(states_.size());
    stack.push_back(root_);
    visited[root_] = 1;

    ReachableMetrics metrics;

    while (!stack.empty()) {
        const StateId stateId = stack.back();
        stack.pop_back();

        ++metrics.stateCount;

        const auto &transitions = states_[stateId].transitions;
        metrics.transitionCount += transitions.size();

        for (const auto &transition : transitions) {
            if (visited[transition.target] != 0) {
                continue;
            }

            visited[transition.target] = 1;
            stack.push_back(transition.target);
        }
    }

    return metrics;
}

} // namespace lexicon
