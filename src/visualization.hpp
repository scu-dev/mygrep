#pragma once
#include <array>
#include <fstream>
#include <string>

#include "automaton.hpp"

namespace MyGrep {
    typedef uint8_t u8;
    typedef uint64_t u64;
    using std::array, std::ofstream, std::string;

    namespace detail {
        inline constexpr array<u8, 16> hexDigits{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

        struct DotEdge {
            u64 source{0};
            u64 target{0};
            u8 label{0};
            u8 epsilon{0};
        };

        inline void writeDotLabel(ofstream& file, const DotEdge& edge) noexcept {
            if (static_cast<bool>(edge.epsilon)) {
                file << "ɛ";
                return;
            }
            switch (edge.label) {
                case '\\': file << "\\\\"; break;
                case '"': file << "\\\""; break;
                case '\n': file << "\\n"; break;
                case '\r': file << "\\r"; break;
                case '\t': file << "\\t"; break;
                default:
                    if (edge.label >= 32 && edge.label <= 126) file << static_cast<char>(edge.label);
                    else file << "0x" << hexDigits[edge.label >> 4] << hexDigits[edge.label & 0x0F];
                    break;
            }
        }

        inline void writeDotGraph(const string& graphName, const string& outFile, u64 stateCount, u64 startState, const vector<u8>& accepting, const vector<DotEdge>& edges) noexcept {
            if (startState >= stateCount) return;
            ofstream file(outFile);
            if (!file) return;
            file << "digraph " << graphName << " {\n    rankdir=LR;\n    node [shape = circle];\n    start [shape = point];\n    start -> " << startState + 1 << ";\n";
            for (u64 state = 0; state < stateCount; state++) {
                if (state < accepting.size() && static_cast<bool>(accepting[state])) file << "    " << state + 1 << " [shape = doublecircle];\n";
            }
            for (const DotEdge& edge : edges) {
                if (edge.source >= stateCount || edge.target >= stateCount) continue;
                file << "    " << edge.source + 1 << " -> " << edge.target + 1 << " [label = \"";
                writeDotLabel(file, edge);
                file << "\"];\n";
            }
            file << "}\n";
        }

    }

    inline void generateNFAGraph(const detail::NFA& nfa, const string& outFile) noexcept {
        const u64 stateCount = nfa.nodes.size();
        vector<u8> accepting(stateCount, 0);
        if (nfa.acceptState < stateCount) accepting[nfa.acceptState] = 1;
        vector<detail::DotEdge> edges;
        for (u64 state = 0; state < stateCount; state++) {
            for (u64 symbol = 0; symbol < nfa.nodes[state].transitions.size(); symbol++) {
                for (u64 target : nfa.nodes[state].transitions[symbol]) {
                    if (target >= stateCount) continue;
                    edges.emplace_back(state, target, static_cast<u8>(symbol), 0);
                }
            }
            for (u64 target : nfa.nodes[state].epsilonTransitions) {
                if (target >= stateCount) continue;
                edges.emplace_back(state, target, 0, 1);
            }
        }
        detail::writeDotGraph("NFA", outFile, stateCount, nfa.startState, accepting, edges);
    }

    inline void generateDFAGraph(const Automaton& automaton, const string& outFile) noexcept {
        const u64 stateCount = automaton.transitions.size();
        vector<detail::DotEdge> edges;
        for (u64 state = 0; state < stateCount; state++) for (u64 symbol = 0; symbol < automaton.transitions[state].size(); symbol++) {
            const u64 target = automaton.transitions[state][symbol];
            if (target >= stateCount) continue;
            edges.emplace_back(state, target, static_cast<u8>(symbol), 0);
        }
        detail::writeDotGraph("DFA", outFile, stateCount, automaton.startState, automaton.accepting, edges);
    }

    inline void generateMinDFAGraph(const Automaton& automaton, const string& outFile) noexcept {
        const u64 originalStateCount = automaton.transitions.size();
        if (automaton.startState >= originalStateCount) return;
        vector<u8> reachable(originalStateCount, 0);
        vector<u64> reachableStates;
        reachable[automaton.startState] = 1;
        reachableStates.push_back(automaton.startState);
        for (u64 index = 0; index < reachableStates.size(); index++) {
            const u64 state = reachableStates[index];
            for (u64 symbol = 0; symbol < automaton.transitions[state].size(); symbol++) {
                const u64 nextState = automaton.transitions[state][symbol];
                if (nextState >= originalStateCount || static_cast<bool>(reachable[nextState])) continue;
                reachable[nextState] = 1;
                reachableStates.push_back(nextState);
            }
        }
        array<u8, 256> alphabet{};
        for (u64 state : reachableStates) for (u64 symbol = 0; symbol < automaton.transitions[state].size(); symbol++) {
            const u64 nextState = automaton.transitions[state][symbol];
            if (nextState < originalStateCount) alphabet[symbol] = 1;
        }
        vector<u64> compactIndexByOriginal(originalStateCount, detail::invalidState);
        for (u64 index = 0; index < reachableStates.size(); index++) {
            compactIndexByOriginal[reachableStates[index]] = index;
        }
        const u64 stateCount = reachableStates.size();
        vector<array<u64, 256>> transitions(stateCount);
        vector<u8> accepting(stateCount, 0);
        for (u64 index = 0; index < stateCount; index++) {
            transitions[index].fill(detail::invalidState);
            const u64 originalState = reachableStates[index];
            accepting[index] = static_cast<u8>(originalState < automaton.accepting.size() && static_cast<bool>(automaton.accepting[originalState]));
            for (u64 symbol = 0; symbol < automaton.transitions[originalState].size(); symbol++) {
                const u64 nextState = automaton.transitions[originalState][symbol];
                if (nextState >= originalStateCount) continue;
                transitions[index][symbol] = compactIndexByOriginal[nextState];
            }
        }
        vector<u64> stateClass(stateCount, detail::invalidState);
        u64 classCount = 0;
        for (u64 state = 0; state < stateCount; state++) {
            const u64 nextClass = static_cast<bool>(accepting[state]) ? 1 : 0;
            stateClass[state] = nextClass;
            if (nextClass + 1 > classCount) classCount = nextClass + 1;
        }
        struct StateSignature {
            u8 accepting{0};
            array<u64, 256> targets{};
        };
        bool changed = true;
        while (changed) {
            changed = false;
            vector<StateSignature> signatures;
            vector<u64> nextStateClass(stateCount, detail::invalidState);
            for (u64 state = 0; state < stateCount; state++) {
                StateSignature signature;
                signature.accepting = accepting[state];
                signature.targets.fill(detail::invalidState);
                for (u64 symbol = 0; symbol < alphabet.size(); symbol++) {
                    if (!alphabet[symbol]) continue;
                    const u64 nextState = transitions[state][symbol];
                    if (nextState < stateCount) signature.targets[symbol] = stateClass[nextState];
                }
                u64 signatureIndex = detail::invalidState;
                for (u64 index = 0; index < signatures.size(); index++) {
                    if (signatures[index].accepting != signature.accepting) continue;
                    if (signatures[index].targets != signature.targets) continue;
                    signatureIndex = index;
                    break;
                }
                if (signatureIndex == detail::invalidState) {
                    signatureIndex = signatures.size();
                    signatures.push_back(signature);
                }
                nextStateClass[state] = signatureIndex;
                if (stateClass[state] != signatureIndex) changed = true;
            }
            stateClass = nextStateClass;
            classCount = signatures.size();
        }
        const u64 startClass = stateClass[compactIndexByOriginal[automaton.startState]];
        vector<u64> remappedClass(classCount, detail::invalidState);
        u64 remappedClassCount = 0;
        remappedClass[startClass] = remappedClassCount++;
        for (u64 oldClass = 0; oldClass < classCount; oldClass++) {
            if (remappedClass[oldClass] != detail::invalidState) continue;
            remappedClass[oldClass] = remappedClassCount++;
        }
        vector<array<u64, 256>> minTransitions(classCount);
        vector<u8> minAccepting(classCount, 0);
        for (u64 state = 0; state < stateCount; state++) {
            const u64 sourceClass = remappedClass[stateClass[state]];
            minTransitions[sourceClass].fill(detail::invalidState);
        }
        for (u64 state = 0; state < stateCount; state++) {
            const u64 sourceClass = remappedClass[stateClass[state]];
            if (static_cast<bool>(accepting[state])) minAccepting[sourceClass] = 1;
            for (u64 symbol = 0; symbol < alphabet.size(); symbol++) {
                if (!alphabet[symbol]) continue;
                const u64 nextState = transitions[state][symbol];
                if (nextState >= stateCount) continue;
                minTransitions[sourceClass][symbol] = remappedClass[stateClass[nextState]];
            }
        }
        vector<detail::DotEdge> edges;
        for (u64 state = 0; state < classCount; state++) for (u64 symbol = 0; symbol < alphabet.size(); symbol++) {
            if (!alphabet[symbol]) continue;
            const u64 nextState = minTransitions[state][symbol];
            if (nextState >= classCount) continue;
            edges.emplace_back(state, nextState, static_cast<u8>(symbol), 0);
        }
        detail::writeDotGraph("MinDFA", outFile, classCount, 0, minAccepting, edges);
    }
}