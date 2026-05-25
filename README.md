# ZBP Lexicon

`ZBP` is a small C++20 library for building a lexicon backed by a minimal
deterministic finite automaton (DFA). It is designed for exact whole-word
membership checks with compact shared suffixes.

## What The Library Provides

- Incremental construction of a minimal DFA from a sorted word list
- Configurable case-sensitive or case-insensitive lookup
- Exact lookup through `lexicon::Lexicon::contains`, `find` and `count`
- STL-style iteration over stored words
- Diagnostic metrics for states, transitions and estimated memory usage
- Graphviz DOT export for visualizing the reachable automaton
- A small example program in `examples/main.cpp`

## Project Layout

- `include/lexicon/lexicon.hpp`: public API
- `src/lexicon.cpp`: library implementation
- `examples/main.cpp`: example executable
- `CMakeLists.txt`: CMake build definition

## Public API

The main type is `lexicon::Lexicon`.

- `explicit Lexicon(CaseMode mode = CaseMode::Insensitive)`
  Creates a lexicon. Case mode is fixed for the object because the DFA is built
  from normalized keys.
- `buildFromSorted(const std::vector<std::string>& words)`
  Rebuilds the automaton from a lexicographically sorted collection of words.
  Duplicate words are ignored. Input must be sorted for the selected case mode.
  Unsorted input causes `std::invalid_argument`.
- `contains(const std::string& word) const`
  Returns `true` only for exact words stored in the lexicon.
- `find(const std::string& word) const`
  Returns an iterator to the stored word, or `end()` when it is absent.
- `count(const std::string& word) const`
  Returns `1` if the word exists, otherwise `0`.
- `begin()`, `end()`, `cbegin()`, `cend()`
  Iterate over stored words in lexicographical order. In case-insensitive mode,
  iteration returns normalized words.
- `empty() const`
  Checks whether any words were inserted.
- `size() const`
  Returns the number of distinct inserted words.
- `clear()`
  Removes all current data.

Diagnostic and visualization API:

- `stateCount() const`
  Returns the number of states stored after final minimization and compaction.
- `reachableStateCount() const`
  Returns the number of states reachable from the root.
- `transitionCount() const`
  Returns the number of outgoing transitions from reachable states.
- `preMinimizationStateCount() const`
  Returns the state count captured just before final minimization.
- `preMinimizationMemoryUsageEstimate() const`
  Estimates builder memory captured just before final minimization.
- `memoryUsageEstimate() const`
  Estimates memory used by the compacted reachable DFA.
- `exportToDot() const`
  Returns a Graphviz DOT representation of the reachable automaton.
- `exportToDotFile(const std::string& path) const`
  Writes the DOT representation to a file.

## Important Constraints

- Input passed to `buildFromSorted` must already be sorted lexicographically for
  the selected `CaseMode`.
- Default lookup is case-insensitive. Use `Lexicon(CaseMode::Sensitive)` for
  case-sensitive behavior.
- UTF-8 input is decoded to code points, so Polish letters such as `l` with
  stroke or `z` with dot are treated as single transition symbols.
- Prefixes are not matches unless they were inserted as complete words.
- Case folding is intentionally limited to ASCII and explicit Polish uppercase
  letters used by the project. Full Unicode case folding is out of scope.
- `memoryUsageEstimate()` and the set comparisons in the demo are estimates,
  not allocator-exact process memory measurements.

## Build

```bash
cmake -S . -B build
cmake --build build
```

The commands produce:

- `liblexicon.a`: static library
- `lexicon_demo`: example executable

## Example

```cpp
#include <lexicon/lexicon.hpp>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

int main() {
    std::vector<std::string> words = {
        "Apple",
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

    std::cout << std::boolalpha;
    std::cout << dictionary.contains("car") << '\n';
    std::cout << dictionary.contains("cow") << '\n';

    for (const auto& word : dictionary) {
        std::cout << word << '\n';
    }

    if (auto it = dictionary.find("APPLE"); it != dictionary.end()) {
        std::cout << "found: " << *it << '\n';
    }
}
```

The default lexicon is case-insensitive, so the example stores and iterates over
normalized words. Construct `lexicon::Lexicon dictionary(lexicon::CaseMode::Sensitive);`
to keep case distinctions.

## Demo Program

```bash
./build/lexicon_demo
./build/lexicon_demo -cs
./build/lexicon_demo -f words.txt
./build/lexicon_demo -dot
./build/lexicon_demo -dot graphs/example.dot
```

- `-cs` enables case-sensitive mode.
- `-f WORDS_FILE` loads words from a file.
- `-dot [DOT_FILE]` exports Graphviz DOT. Without a path, the demo writes to
  `graphs/YYYYMMDD_HHMMSS.dot`.

## How Construction Works

The implementation follows the classic incremental minimization approach for
sorted input:

1. Compare the current word with the previous word.
2. Preserve the common prefix and minimize the unchecked suffix.
3. Append states for the remaining suffix of the new word.
4. Reuse equivalent states through a signature registry.

This keeps exact lookups fast while allowing many words to share structure.
