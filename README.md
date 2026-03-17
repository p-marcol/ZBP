# ZBP Lexicon

`ZBP` is a small C++20 library for building a lexicon backed by a minimal
deterministic finite automaton (DFA). It is designed for exact whole-word
membership checks with compact shared suffixes.

## What The Library Provides

- Incremental construction of a minimal DFA from a sorted word list
- Exact lookup through `lexicon::Lexicon::contains`
- Simple metrics for inserted word count and internal state count
- A small example program in `examples/main.cpp`

## Project Layout

- `include/lexicon/lexicon.hpp`: public API
- `src/lexicon.cpp`: library implementation
- `examples/main.cpp`: example executable
- `CMakeLists.txt`: CMake build definition

## Public API

The main type is `lexicon::Lexicon`.

- `buildFromSorted(const std::vector<std::string>& words)`
  Rebuilds the automaton from a lexicographically sorted collection of words.
  Duplicate words are ignored. Unsorted input causes `std::invalid_argument`.
- `contains(std::string word) const`
  Returns `true` only for exact words stored in the lexicon.
- `empty() const`
  Checks whether any words were inserted.
- `size() const`
  Returns the number of distinct inserted words.
- `stateCount() const`
  Returns the number of states currently stored by the builder.
- `clear()`
  Removes all current data.

## Important Constraints

- Input passed to `buildFromSorted` must already be sorted lexicographically.
- Lookups are case-sensitive.
- Prefixes are not matches unless they were inserted as complete words.
- `stateCount()` is a diagnostic metric for internal storage, not a formal
  promise about the compacted canonical automaton size.

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
}
```

## How Construction Works

The implementation follows the classic incremental minimization approach for
sorted input:

1. Compare the current word with the previous word.
2. Preserve the common prefix and minimize the unchecked suffix.
3. Append states for the remaining suffix of the new word.
4. Reuse equivalent states through a signature registry.

This keeps exact lookups fast while allowing many words to share structure.
