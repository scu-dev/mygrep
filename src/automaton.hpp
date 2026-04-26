#pragma once
#include <string>

namespace MyGrep {
    using std::string;

    namespace detail { inline string error; }

    [[nodiscard]] inline string getLastParserError() noexcept { return detail::error; }

    struct Automaton {

    };

    [[nodiscard]] inline bool buildAutomaton(const string& pattern, Automaton& result) noexcept {

    }
}