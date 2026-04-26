#include <iostream>

#include "automaton.hpp"
#include "parseArguments.hpp"

int main(int argc, char** argv) {
    const auto options = MyGrep::parseArguments(argc, argv);
    MyGrep::Automaton automaton;
    if (!MyGrep::buildAutomaton(options.pattern, automaton)) {
        std::cout << "Failed to build automaton from pattern: " << options.pattern << ", error: " << MyGrep::getLastParserError() << std::endl;
    }
    return 0;
}