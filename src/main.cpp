#include <iostream>
#include <fstream>
#include <string>

#include "automaton.hpp"
#include "parseArguments.hpp"

using std::string, std::ifstream, std::getline, std::cout, std::cerr, std::endl;

int main(int argc, char** argv) {
    const auto options = MyGrep::parseArguments(argc, argv);
    MyGrep::Automaton automaton;
    if (!MyGrep::buildAutomaton(options.pattern, automaton)) {
        cout << "Failed to build automaton from pattern: " << options.pattern << endl;
        return 1;
    }
    ifstream file(options.filePath);
    if (!file) {
        cerr << "Failed to open file: " << options.filePath << endl;
        return 1;
    }
    string line;
    while (getline(file, line)) if (MyGrep::match(line, automaton, options.matchWholeLine)) cout << line << '\n';
    return 0;
}