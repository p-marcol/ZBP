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

class Lexicon {
  public:
    using value_type = std::string;
    using size_type = std::size_t;

    Lexicon() = default;

    void buildFromSorted(const std::vector<std::string> &words);

    [[nodiscard]] bool contains(const value_type word) const;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] size_type size() const noexcept;
    [[nodiscard]] size_type stateCount() const noexcept;

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

    void insertSorted(const std::string &word);
    void finish();
    void minimizeFrom(std::size_t prefixLen);
    [[nodiscard]] StateId replaceOrRegister(StateId stateId);
    void replaceTransitionTarget(StateId parent, char symbol,
                                 StateId newTarget);

    static void sortTransitions(State &state);
    [[nodiscard]] static std::size_t commonPrefixLength(std::string_view a,
                                                        std::string_view b);

    [[nodiscard]] StateId createState(bool isFinal);

    std::vector<State> states_;
    std::unordered_map<StateSignature, StateId, SignatureHash> registry_;
    std::vector<UncheckedNode> uncheckedPath_;

    std::string previousWord_;
    size_type wordCount_ = 0;
    bool finalized_ = false;
    StateId root_ = invalidState;
};

}; // namespace lexicon

#endif /* lexicon_h */
