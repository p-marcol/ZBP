//
//  lexicon.h
//  ZBP
//
//  Created by Piotr Marcol on 09/03/2026.
//

#ifndef lexicon_h
#define lexicon_h

#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace lexicon {

enum class CaseMode {
    Sensitive,
    Insensitive
};

/**
 * @brief Minimal deterministic finite automaton built from a sorted word list.
 *
 * The class implements incremental construction of a minimal DFA for whole-word
 * lookup. Input words must be provided in lexicographical order. Duplicate
 * entries are ignored.
 */
class Lexicon {
  public:
    using value_type = std::string;
    using size_type = std::size_t;

    class const_iterator {
      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::string;
        using difference_type = std::ptrdiff_t;
        using reference = const value_type &;
        using pointer = const value_type *;

        const_iterator() = default;

        [[nodiscard]] reference operator*() const noexcept;
        [[nodiscard]] pointer operator->() const noexcept;

        const_iterator &operator++();
        const_iterator operator++(int);

        friend bool operator==(const const_iterator &lhs,
                               const const_iterator &rhs) noexcept;

        friend bool operator!=(const const_iterator &lhs,
                               const const_iterator &rhs) noexcept {
            return !(lhs == rhs);
        }

      private:
        friend class Lexicon;

        struct Frame {
            std::size_t state = 0;
            std::size_t nextTransition = 0;
            std::size_t pathLength = 0;
            bool yielded = false;
        };

        explicit const_iterator(const Lexicon *lexicon);
        const_iterator(const Lexicon *lexicon, const std::u32string &word);

        void advanceToNextWord();
        [[nodiscard]] bool isEnd() const noexcept;

        const Lexicon *lexicon_ = nullptr;
        std::vector<Frame> stack_;
        std::u32string currentSymbols_;
        std::string currentWord_;
    };

    using iterator = const_iterator;

    explicit Lexicon(CaseMode mode = CaseMode::Insensitive)
        : caseMode_(mode) {}

    /**
     * @brief Rebuilds the automaton from a lexicographically sorted word list.
     *
     * The current contents are discarded before construction starts. Words that
     * compare equal to the previous entry are skipped.
     *
     * @param words Source dictionary ordered lexicographically for the current
     * case mode.
     * @throws std::invalid_argument If the input is not sorted.
     */
    void buildFromSorted(const std::vector<std::string> &words);

    /**
     * @brief Checks whether the exact word exists in the lexicon.
     *
     * Prefixes are not treated as matches unless they were inserted as complete
     * words.
     *
     * @param word Query word.
     * @return true when the word is accepted by the automaton.
     */
    [[nodiscard]] bool contains(const value_type &word) const;

    /**
     * @brief Returns an iterator to a word, or end() when it is absent.
     */
    [[nodiscard]] const_iterator find(const value_type &word) const;

    /**
     * @brief Returns 1 if the word exists, otherwise 0.
     */
    [[nodiscard]] size_type count(const value_type &word) const;

    /**
     * @brief Returns an iterator to the first stored word.
     */
    [[nodiscard]] const_iterator begin() const;

    /**
     * @brief Returns the past-the-end iterator.
     */
    [[nodiscard]] const_iterator end() const noexcept;

    /**
     * @brief Returns an iterator to the first stored word.
     */
    [[nodiscard]] const_iterator cbegin() const;

    /**
     * @brief Returns the past-the-end iterator.
     */
    [[nodiscard]] const_iterator cend() const noexcept;

    /**
     * @brief Returns whether the lexicon contains any words.
     */
    [[nodiscard]] bool empty() const noexcept;

    /**
     * @brief Returns the number of distinct inserted words.
     */
    [[nodiscard]] size_type size() const noexcept;

    /**
     * @brief Returns the number of states stored by the current automaton.
     *
     * After build finalization this matches the compacted reachable DFA.
     */
    [[nodiscard]] size_type stateCount() const noexcept;

    /**
     * @brief Returns the number of states reachable from the root state.
     */
    [[nodiscard]] size_type reachableStateCount() const noexcept;

    /**
     * @brief Returns the number of outgoing transitions from reachable states.
     */
    [[nodiscard]] size_type transitionCount() const noexcept;

    /**
     * @brief Returns allocated states recorded before final minimization.
     */
    [[nodiscard]] size_type preMinimizationStateCount() const noexcept;

    /**
     * @brief Estimates memory recorded before final minimization in bytes.
     */
    [[nodiscard]] size_type preMinimizationMemoryUsageEstimate() const noexcept;

    /**
     * @brief Estimates memory used by the reachable DFA in bytes.
     *
     * Counts only states and transitions reachable from the root. It excludes
     * unreachable builder storage and implementation-specific allocator
     * bookkeeping.
     */
    [[nodiscard]] size_type memoryUsageEstimate() const noexcept;

    /**
     * @brief Exports the reachable part of the automaton as Graphviz DOT.
     */
    [[nodiscard]] std::string exportToDot() const;

    /**
     * @brief Writes the Graphviz DOT representation to a file.
     */
    bool exportToDotFile(const std::string &path) const;

    /**
     * @brief Removes all states and returns the object to its initial state.
     */
    void clear();

  private:
    using StateId = std::size_t;
    using Symbol = char32_t;
    using SymbolString = std::u32string;
    static constexpr StateId invalidState = static_cast<StateId>(-1);

    struct Transition {
        Symbol symbol{};
        StateId target{invalidState};

        [[nodiscard]] bool operator==(const Transition &other) const noexcept;
    };

    struct State {
        bool isFinal = false;
        std::vector<Transition> transitions;
    };

    struct UncheckedNode {
        StateId parent{invalidState};
        Symbol symbol{};
        StateId child{invalidState};
    };

    struct StateSignature {
        bool isFinal = false;
        std::vector<Transition> transitions;

        [[nodiscard]] bool
        operator==(const StateSignature &other) const noexcept;
    };

    struct SignatureHash {
        [[nodiscard]] std::size_t
        operator()(const StateSignature &sig) const noexcept;
    };

    struct ReachableMetrics {
        size_type stateCount = 0;
        size_type transitionCount = 0;
    };

    /**
     * @brief Inserts one word into the incremental builder.
     *
     * The word must not be smaller than the previous inserted word.
     */
    void insertSorted(const SymbolString &word);

    /**
     * @brief Finalizes the automaton by minimizing the remaining unchecked path.
     */
    void finish();

    /**
     * @brief Removes states that are no longer reachable after minimization.
     */
    void compactReachableStates();

    /**
     * @brief Minimizes unchecked suffix states that are deeper than prefixLen.
     *
     * @param prefixLen Length of the shared prefix between consecutive words.
     */
    void minimizeFrom(std::size_t prefixLen);

    /**
     * @brief Reuses an equivalent canonical state when possible.
     *
     * @param stateId Candidate state.
     * @return Canonical state identifier.
     */
    [[nodiscard]] StateId replaceOrRegister(StateId stateId);

    /**
     * @brief Redirects one outgoing edge to a canonical child state.
     */
    void replaceTransitionTarget(StateId parent, Symbol symbol,
                                 StateId newTarget);

    /**
     * @brief Keeps transitions ordered to allow binary search in contains().
     */
    static void sortTransitions(State &state);

    /**
     * @brief Computes the common prefix length for two strings.
     */
    [[nodiscard]] static std::size_t commonPrefixLength(std::u32string_view a,
                                                        std::u32string_view b);

    /**
     * @brief Appends a new state to the internal storage.
     */
    [[nodiscard]] StateId createState(bool isFinal);

    /**
     * @brief Normalizes one lookup key before build/search.
     */
    [[nodiscard]] SymbolString normalize(const std::string &word) const;

    /**
     * @brief Counts reachable states and transitions from the root.
     */
    [[nodiscard]] ReachableMetrics reachableMetrics() const noexcept;

    /**
     * @brief Traverses only states reachable from root_ using iterative DFS.
     */
    [[nodiscard]] std::vector<StateId> reachableStatesDepthFirst() const;

    /**
     * @brief Estimates memory currently held by allocated builder storage.
     */
    [[nodiscard]] size_type allocatedMemoryUsageEstimate() const noexcept;

    std::vector<State> states_;
    std::unordered_map<StateSignature, StateId, SignatureHash> registry_;
    std::vector<UncheckedNode> uncheckedPath_;

    SymbolString previousWord_;
    size_type wordCount_ = 0;
    size_type preMinimizationStateCount_ = 0;
    size_type preMinimizationMemoryUsageEstimate_ = 0;
    bool finalized_ = false;
    StateId root_ = invalidState;
    // The mode is fixed at construction because the DFA is built from
    // normalized keys; switching later would make its contents inconsistent.
    CaseMode caseMode_ = CaseMode::Insensitive;
};

}; // namespace lexicon

#endif /* lexicon_h */
