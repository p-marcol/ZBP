//
//  lexicon.h
//  ZBP
//
//  Created by Piotr Marcol on 09/03/2026.
//

#ifndef lexicon_h
#define lexicon_h

#include <cstddef>
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
    [[nodiscard]] bool contains(const value_type word) const;

    /**
     * @brief Returns whether the lexicon contains any words.
     */
    [[nodiscard]] bool empty() const noexcept;

    /**
     * @brief Returns the number of distinct inserted words.
     */
    [[nodiscard]] size_type size() const noexcept;

    /**
     * @brief Returns the number of allocated states stored by the builder.
     *
     * This is a structural metric useful for debugging and comparisons. It does
     * not attempt to compact the internal storage after state merging, so it
     * can be larger than the number of states reachable in the final DFA.
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
     * @brief Removes all states and returns the object to its initial state.
     */
    void clear();

  private:
    using StateId = std::size_t;
    static constexpr StateId invalidState = static_cast<StateId>(-1);

    struct Transition {
        char symbol{};
        StateId target{invalidState};

        [[nodiscard]] bool operator==(const Transition &other) const noexcept;
    };

    struct State {
        bool isFinal = false;
        std::vector<Transition> transitions;
    };

    struct UncheckedNode {
        StateId parent{invalidState};
        char symbol{};
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
    void insertSorted(const std::string &word);

    /**
     * @brief Finalizes the automaton by minimizing the remaining unchecked path.
     */
    void finish();

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
    void replaceTransitionTarget(StateId parent, char symbol,
                                 StateId newTarget);

    /**
     * @brief Keeps transitions ordered to allow binary search in contains().
     */
    static void sortTransitions(State &state);

    /**
     * @brief Computes the common prefix length for two strings.
     */
    [[nodiscard]] static std::size_t commonPrefixLength(std::string_view a,
                                                        std::string_view b);

    /**
     * @brief Appends a new state to the internal storage.
     */
    [[nodiscard]] StateId createState(bool isFinal);

    /**
     * @brief Normalizes one lookup key before build/search.
     */
    [[nodiscard]] std::string normalize(const std::string &word) const;

    /**
     * @brief Counts reachable states and transitions from the root.
     */
    [[nodiscard]] ReachableMetrics reachableMetrics() const noexcept;

    std::vector<State> states_;
    std::unordered_map<StateSignature, StateId, SignatureHash> registry_;
    std::vector<UncheckedNode> uncheckedPath_;

    std::string previousWord_;
    size_type wordCount_ = 0;
    bool finalized_ = false;
    StateId root_ = invalidState;
    // The mode is fixed at construction because the DFA is built from
    // normalized keys; switching later would make its contents inconsistent.
    CaseMode caseMode_ = CaseMode::Insensitive;
};

}; // namespace lexicon

#endif /* lexicon_h */
