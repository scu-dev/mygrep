#pragma once
#include <string>
#include <CLI/CLI.hpp>

namespace MyGrep {
    using std::string, CLI::App, CLI::ExistingFile;

    enum struct VisualizationMode {
        None, NFA, DFA, MinDFA
    };

    struct CommandLineOptions {
        string pattern;
        string filePath;
        VisualizationMode visualizationMode{VisualizationMode::None};
    };

    [[nodiscard]] inline CommandLineOptions parseArguments(int argc, char** argv) noexcept {
        App app{"grep clone"};
        CommandLineOptions options;
        app.add_option("regex", options.pattern, "regular expression")->required();
        app.add_option("file", options.filePath, "file path")->required()->check(ExistingFile);
        string visualizationModeStr;
        app.add_option("-v,--visualize", visualizationModeStr, "visualize regex automaton")->option_text("{NFA,DFA,MINDFA}");
        if (visualizationModeStr == "NFA") options.visualizationMode = VisualizationMode::NFA;
        else if (visualizationModeStr == "DFA") options.visualizationMode = VisualizationMode::DFA;
        else if (visualizationModeStr == "MINDFA") options.visualizationMode = VisualizationMode::MinDFA;
        return options;
    }
}