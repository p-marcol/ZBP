//
//  lexicon.cpp
//  ZBP
//
//  Created by Piotr Marcol on 09/03/2026.
//

#include <lexicon/lexicon.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace lexicon {

namespace {

// Case folding is intentionally limited to ASCII and Polish letters used by
// the dictionary; full Unicode case folding would require a dedicated library.
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

// Decode UTF-8 input into Unicode code points so the DFA treats multi-byte
// letters such as 'ł' or 'ż' as single transition symbols.
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
                const char32_t symbol =
                    (static_cast<char32_t>(firstByte & 0x1FU) << 6U) |
                    static_cast<char32_t>(secondByte & 0x3FU);
                decoded.push_back(symbol);
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
                const char32_t symbol =
                    (static_cast<char32_t>(firstByte & 0x0FU) << 12U) |
                    (static_cast<char32_t>(secondByte & 0x3FU) << 6U) |
                    static_cast<char32_t>(thirdByte & 0x3FU);
                decoded.push_back(symbol);
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
                const char32_t symbol =
                    (static_cast<char32_t>(firstByte & 0x07U) << 18U) |
                    (static_cast<char32_t>(secondByte & 0x3FU) << 12U) |
                    (static_cast<char32_t>(thirdByte & 0x3FU) << 6U) |
                    static_cast<char32_t>(fourthByte & 0x3FU);
                decoded.push_back(symbol);
                i += 3;
                continue;
            }
        }

        // Preserve invalid or incomplete UTF-8 bytes as standalone symbols
        // instead of throwing, keeping lookup behavior deterministic.
        decoded.push_back(static_cast<char32_t>(firstByte));
    }

    return decoded;
}

// Encode one Unicode code point back to UTF-8. DOT labels are textual, while
// the automaton stores transition symbols internally as char32_t.
void appendUtf8(std::string &text, char32_t symbol) {
    if (symbol <= 0x7FU) {
        text.push_back(static_cast<char>(symbol));
        return;
    }

    if (symbol <= 0x7FFU) {
        text.push_back(static_cast<char>(0xC0U | ((symbol >> 6U) & 0x1FU)));
        text.push_back(static_cast<char>(0x80U | (symbol & 0x3FU)));
        return;
    }

    if (symbol <= 0xFFFFU) {
        text.push_back(static_cast<char>(0xE0U | ((symbol >> 12U) & 0x0FU)));
        text.push_back(static_cast<char>(0x80U | ((symbol >> 6U) & 0x3FU)));
        text.push_back(static_cast<char>(0x80U | (symbol & 0x3FU)));
        return;
    }

    text.push_back(static_cast<char>(0xF0U | ((symbol >> 18U) & 0x07U)));
    text.push_back(static_cast<char>(0x80U | ((symbol >> 12U) & 0x3FU)));
    text.push_back(static_cast<char>(0x80U | ((symbol >> 6U) & 0x3FU)));
    text.push_back(static_cast<char>(0x80U | (symbol & 0x3FU)));
}

std::string encodeUtf8(char32_t symbol) {
    std::string encoded;
    encoded.reserve(4);
    appendUtf8(encoded, symbol);
    return encoded;
}

std::string encodeUtf8(std::u32string_view symbols) {
    std::string encoded;
    encoded.reserve(symbols.size());

    for (const char32_t symbol : symbols) {
        appendUtf8(encoded, symbol);
    }

    return encoded;
}

// Graphviz labels use quotes and backslashes as syntax, so those bytes must be
// escaped after the symbol is encoded as UTF-8 text.
std::string escapeDotLabel(char32_t symbol) {
    const std::string encoded = encodeUtf8(symbol);
    std::string escaped;
    escaped.reserve(encoded.size());

    for (const char byte : encoded) {
        if (byte == '\\' || byte == '"') {
            escaped.push_back('\\');
        }
        escaped.push_back(byte);
    }

    return escaped;
}

} // namespace

Lexicon::const_iterator::const_iterator(const Lexicon *lexicon)
    : lexicon_(lexicon) {
    if (lexicon_ == nullptr || lexicon_->root_ == invalidState) {
        lexicon_ = nullptr;
        return;
    }

    stack_.push_back(Frame{lexicon_->root_, 0, 0, false});
    advanceToNextWord();
}

Lexicon::const_iterator::const_iterator(const Lexicon *lexicon,
                                        const std::u32string &word)
    : lexicon_(lexicon), currentSymbols_(word) {
    if (lexicon_ == nullptr || lexicon_->root_ == invalidState) {
        lexicon_ = nullptr;
        return;
    }

    StateId current = lexicon_->root_;
    stack_.push_back(Frame{current, 0, 0, true});

    for (std::size_t i = 0; i < word.size(); ++i) {
        const Symbol symbol = word[i];
        const auto &transitions = lexicon_->states_[current].transitions;
        const auto it = std::lower_bound(
            transitions.begin(), transitions.end(), symbol,
            [](const Transition &transition, Symbol transitionSymbol) {
                return transition.symbol < transitionSymbol;
            });

        if (it == transitions.end() || it->symbol != symbol) {
            lexicon_ = nullptr;
            stack_.clear();
            currentSymbols_.clear();
            return;
        }

        stack_.back().nextTransition =
            static_cast<std::size_t>(std::distance(transitions.begin(), it)) +
            1;
        current = it->target;
        stack_.push_back(Frame{current, 0, i + 1, true});
    }

    if (!lexicon_->states_[current].isFinal) {
        lexicon_ = nullptr;
        stack_.clear();
        currentSymbols_.clear();
        return;
    }

    stack_.back().yielded = false;
    advanceToNextWord();
}

Lexicon::const_iterator::reference
Lexicon::const_iterator::operator*() const noexcept {
    return currentWord_;
}

Lexicon::const_iterator::pointer
Lexicon::const_iterator::operator->() const noexcept {
    return &currentWord_;
}

Lexicon::const_iterator &Lexicon::const_iterator::operator++() {
    if (!isEnd()) {
        advanceToNextWord();
    }
    return *this;
}

Lexicon::const_iterator Lexicon::const_iterator::operator++(int) {
    const_iterator previous = *this;
    ++(*this);
    return previous;
}

bool operator==(const Lexicon::const_iterator &lhs,
                const Lexicon::const_iterator &rhs) noexcept {
    if (lhs.isEnd() || rhs.isEnd()) {
        return lhs.isEnd() && rhs.isEnd();
    }

    return lhs.lexicon_ == rhs.lexicon_ && lhs.currentWord_ == rhs.currentWord_;
}

void Lexicon::const_iterator::advanceToNextWord() {
    while (lexicon_ != nullptr && !stack_.empty()) {
        Frame &frame = stack_.back();
        const State &state = lexicon_->states_[frame.state];

        if (!frame.yielded) {
            frame.yielded = true;
            if (state.isFinal) {
                currentWord_ = encodeUtf8(currentSymbols_);
                return;
            }
        }

        if (frame.nextTransition < state.transitions.size()) {
            const Transition &transition =
                state.transitions[frame.nextTransition++];
            currentSymbols_.push_back(transition.symbol);
            stack_.push_back(
                Frame{transition.target, 0, currentSymbols_.size(), false});
            continue;
        }

        stack_.pop_back();
        if (stack_.empty()) {
            currentSymbols_.clear();
        } else {
            currentSymbols_.resize(stack_.back().pathLength);
        }
    }

    lexicon_ = nullptr;
    stack_.clear();
    currentSymbols_.clear();
    currentWord_.clear();
}

bool Lexicon::const_iterator::isEnd() const noexcept {
    return lexicon_ == nullptr;
}

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
        h ^= std::hash<Symbol>{}(transition.symbol) + 0x9e3779b9U +
             (h << 6U) + (h >> 2U);
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

bool Lexicon::contains(const value_type &word) const {
    if (root_ == invalidState) {
        return false;
    }

    const SymbolString normalizedWord = normalize(word);
    StateId current = root_;

    for (const Symbol symbol : normalizedWord) {
        const auto &transitions = states_[current].transitions;

        const auto it = std::lower_bound(
            transitions.begin(), transitions.end(), symbol,
            [](const Transition &transition, Symbol transitionSymbol) {
                return transition.symbol < transitionSymbol;
            });
        if (it == transitions.end() || it->symbol != symbol) {
            return false;
        }

        current = it->target;
    }

    return states_[current].isFinal;
}

Lexicon::const_iterator Lexicon::find(const value_type &word) const {
    return const_iterator(this, normalize(word));
}

Lexicon::size_type Lexicon::count(const value_type &word) const {
    return contains(word) ? 1 : 0;
}

Lexicon::const_iterator Lexicon::begin() const {
    return const_iterator(this);
}

Lexicon::const_iterator Lexicon::end() const noexcept {
    return const_iterator();
}

Lexicon::const_iterator Lexicon::cbegin() const {
    return begin();
}

Lexicon::const_iterator Lexicon::cend() const noexcept {
    return end();
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

Lexicon::size_type Lexicon::preMinimizationStateCount() const noexcept {
    return preMinimizationStateCount_;
}

Lexicon::size_type
Lexicon::preMinimizationMemoryUsageEstimate() const noexcept {
    return preMinimizationMemoryUsageEstimate_;
}

Lexicon::size_type Lexicon::memoryUsageEstimate() const noexcept {
    if (root_ == invalidState) {
        return sizeof(*this);
    }

    size_type bytes = sizeof(*this);
    for (const StateId stateId : reachableStatesDepthFirst()) {
        const auto &transitions = states_[stateId].transitions;
        bytes += sizeof(State);
        bytes += transitions.size() * sizeof(Transition);
    }

    return bytes;
}

std::string Lexicon::exportToDot() const {
    std::ostringstream dot;
    dot << "digraph Lexicon {\n";
    dot << "    rankdir=LR;\n";
    dot << "    node [shape=circle];\n";

    if (root_ == invalidState) {
        dot << "}\n";
        return dot.str();
    }

    std::vector<StateId> reachableStates = reachableStatesDepthFirst();
    std::sort(reachableStates.begin(), reachableStates.end());

    dot << "    start [shape=point];\n";
    dot << "    start -> s" << root_ << ";\n";

    for (const StateId stateId : reachableStates) {
        dot << "    s" << stateId
            << " [shape=" << (states_[stateId].isFinal ? "doublecircle"
                                                        : "circle")
            << "];\n";
    }

    for (const StateId stateId : reachableStates) {
        for (const auto &transition : states_[stateId].transitions) {
            dot << "    s" << stateId << " -> s" << transition.target
                << " [label=\"" << escapeDotLabel(transition.symbol)
                << "\"];\n";
        }
    }

    dot << "}\n";
    return dot.str();
}

bool Lexicon::exportToDotFile(const std::string &path) const {
    std::ofstream output(path);
    if (!output) {
        return false;
    }

    output << exportToDot();
    return static_cast<bool>(output);
}

void Lexicon::clear() {
    states_.clear();
    registry_.clear();
    uncheckedPath_.clear();
    previousWord_.clear();
    wordCount_ = 0;
    preMinimizationStateCount_ = 0;
    preMinimizationMemoryUsageEstimate_ = 0;
    finalized_ = false;
    root_ = invalidState;
}

void Lexicon::insertSorted(const SymbolString &word) {
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

    preMinimizationStateCount_ = states_.size();
    preMinimizationMemoryUsageEstimate_ = allocatedMemoryUsageEstimate();

    minimizeFrom(0);
    compactReachableStates();
    registry_.clear();
    registry_.rehash(0);
    uncheckedPath_.clear();
    uncheckedPath_.shrink_to_fit();
    previousWord_.clear();
    previousWord_.shrink_to_fit();
    finalized_ = true;
}

void Lexicon::compactReachableStates() {
    if (root_ == invalidState) {
        states_.clear();
        return;
    }

    const std::vector<StateId> reachableStates = reachableStatesDepthFirst();
    std::vector<StateId> oldToNew(states_.size(), invalidState);
    for (std::size_t i = 0; i < reachableStates.size(); ++i) {
        oldToNew[reachableStates[i]] = i;
    }

    std::vector<State> compactedStates;
    compactedStates.reserve(reachableStates.size());

    for (const StateId oldStateId : reachableStates) {
        State compactedState = std::move(states_[oldStateId]);
        for (auto &transition : compactedState.transitions) {
            transition.target = oldToNew[transition.target];
        }
        compactedState.transitions.shrink_to_fit();
        compactedStates.push_back(std::move(compactedState));
    }

    root_ = oldToNew[root_];
    states_ = std::move(compactedStates);
    states_.shrink_to_fit();
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

void Lexicon::replaceTransitionTarget(StateId parent, Symbol symbol,
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

std::size_t Lexicon::commonPrefixLength(std::u32string_view a,
                                        std::u32string_view b) {
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

Lexicon::SymbolString Lexicon::normalize(const std::string &word) const {
    SymbolString normalized = decodeUtf8(word);

    if (caseMode_ == CaseMode::Sensitive) {
        return normalized;
    }

    for (auto &symbol : normalized) {
        symbol = lowercaseCodePoint(symbol);
    }

    return normalized;
}

Lexicon::ReachableMetrics Lexicon::reachableMetrics() const noexcept {
    ReachableMetrics metrics;

    for (const StateId stateId : reachableStatesDepthFirst()) {
        ++metrics.stateCount;

        const auto &transitions = states_[stateId].transitions;
        metrics.transitionCount += transitions.size();
    }

    return metrics;
}

std::vector<Lexicon::StateId> Lexicon::reachableStatesDepthFirst() const {
    std::vector<StateId> reachableStates;
    if (root_ == invalidState) {
        return reachableStates;
    }

    std::vector<unsigned char> visited(states_.size(), 0);
    std::vector<StateId> stack;
    reachableStates.reserve(states_.size());
    stack.reserve(states_.size());

    stack.push_back(root_);
    visited[root_] = 1;

    // Iterative DFS avoids recursion depth issues for long words while still
    // making the graph walk explicit for metrics, DOT export and compaction.
    while (!stack.empty()) {
        const StateId stateId = stack.back();
        stack.pop_back();
        reachableStates.push_back(stateId);

        for (const auto &transition : states_[stateId].transitions) {
            if (visited[transition.target] != 0) {
                continue;
            }

            visited[transition.target] = 1;
            stack.push_back(transition.target);
        }
    }

    return reachableStates;
}

Lexicon::size_type Lexicon::allocatedMemoryUsageEstimate() const noexcept {
    size_type bytes = sizeof(*this);

    bytes += states_.capacity() * sizeof(State);
    for (const auto &state : states_) {
        bytes += state.transitions.capacity() * sizeof(Transition);
    }

    bytes += uncheckedPath_.capacity() * sizeof(UncheckedNode);
    bytes += previousWord_.capacity() * sizeof(Symbol);

    using RegistryValue = decltype(registry_)::value_type;
    bytes += registry_.bucket_count() * sizeof(void *);
    bytes += registry_.size() * sizeof(RegistryValue);
    for (const auto &[signature, stateId] : registry_) {
        (void)stateId;
        bytes += signature.transitions.capacity() * sizeof(Transition);
    }

    return bytes;
}

} // namespace lexicon
