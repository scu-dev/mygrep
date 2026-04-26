#pragma once
#include <iostream>
#include <string>
#include <CLI/CLI.hpp>

namespace MyGrep {
    using std::string, std::cout, std::cerr, std::endl, CLI::App, CLI::ExistingFile, CLI::ParseError, CLI::CallForHelp, CLI::CallForAllHelp, CLI::CallForVersion, CLI::AppFormatMode;

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
        app.allow_windows_style_options(false);
        app.allow_config_extras(false);
        app.set_help_all_flag("");
        app.set_version_flag("--version", "0.0.1", "Display semantic version and exit.");
        app.set_help_flag("-h,--help", "Print help message and exit.");
        CommandLineOptions options;
        app.add_option("regex", options.pattern, "regular expression")->required();
        app.add_option("file", options.filePath, "file path")->required()->check(ExistingFile);
        string visualizationModeStr;
        app.add_option("-v,--visualize", visualizationModeStr, "visualize regex automaton")->option_text("{NFA,DFA,MINDFA}");
        if (visualizationModeStr == "NFA") options.visualizationMode = VisualizationMode::NFA;
        else if (visualizationModeStr == "DFA") options.visualizationMode = VisualizationMode::DFA;
        else if (visualizationModeStr == "MINDFA") options.visualizationMode = VisualizationMode::MinDFA;
        try { app.parse(argc, argv); }
        catch (const CallForHelp&) {
            cout << app.help("", AppFormatMode::All) << endl;
            exit(0);
        }
        catch (const CallForVersion&) {
            cout << "0.0.1" << endl;
            exit(0);
        }
        catch (const ParseError& e) {
            cerr << "Error occured during argument parsing: (" << e.get_exit_code() << ") " << e.get_name() << " " << e.what() << endl;
            exit(1);
        }
        return options;
    }
}