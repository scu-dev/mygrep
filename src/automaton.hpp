#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace MyGrep {
    typedef uint8_t u8;
    typedef uint64_t u64;
    using std::array, std::string, std::vector, std::numeric_limits, std::map, std::sort, std::binary_search, std::size_t;

    struct Automaton {
        vector<array<u64, 256>> transitions;
        vector<u8> accepting;
        u64 startState{0};
    };

    namespace detail {
        inline const char* error = "";
        inline constexpr u64 invalidState = numeric_limits<u64>::max();

        enum struct TokenKind {
            Literal, LeftParen, RightParen, Alternation, Star,
            Concat
        };

        struct Token {
            TokenKind kind{TokenKind::Literal};
            u8 value{0};
        };

        struct NFANode {
            array<vector<u64>, 256> transitions;
            vector<u64> epsilonTransitions;
        };

        struct NFA {
            vector<NFANode> nodes;
            array<bool, 256> alphabet{};
            u64 startState{invalidState};
            u64 acceptState{invalidState};
        };

        struct NFAFragment {
            u64 start{invalidState};
            u64 accept{invalidState};
        };

        [[nodiscard]] inline array<u64, 256> makeTransitionTable() noexcept {
            array<u64, 256> transitions;
            transitions.fill(invalidState);
            return transitions;
        }

        [[nodiscard]] inline bool traverse(const Automaton& automaton, u64& state, u8 c) noexcept {
            if (state >= automaton.transitions.size()) return false;
            const u64 nextState = automaton.transitions[state][c];
            if (nextState == invalidState) return false;
            state = nextState;
            return true;
        }

        [[nodiscard]] inline bool isAccepting(const Automaton& automaton, u64 state) noexcept {
            return state < automaton.accepting.size() && static_cast<bool>(automaton.accepting[state]);
        }

        [[nodiscard]] inline bool traverse(const NFANode& node, u8 c, vector<u64>& result) noexcept {
            const auto oldSize = result.size();
            result.insert(result.end(), node.transitions[c].begin(), node.transitions[c].end());
            return result.size() != oldSize;
        }

        [[nodiscard]] inline bool traverseEpsilon(const NFANode& node, vector<u64>& result) noexcept {
            const auto oldSize = result.size();
            result.insert(result.end(), node.epsilonTransitions.begin(), node.epsilonTransitions.end());
            return result.size() != oldSize;
        }

        [[nodiscard]] inline bool canEndExpression(TokenKind kind) noexcept {
            return kind == TokenKind::Literal || kind == TokenKind::RightParen || kind == TokenKind::Star;
        }

        [[nodiscard]] inline bool canStartExpression(TokenKind kind) noexcept {
            return kind == TokenKind::Literal || kind == TokenKind::LeftParen;
        }

        [[nodiscard]] inline bool isBinaryOperator(TokenKind kind) noexcept {
            return kind == TokenKind::Alternation || kind == TokenKind::Concat;
        }

        [[nodiscard]] inline int precedence(TokenKind kind) noexcept {
            if (kind == TokenKind::Concat) return 2;
            if (kind == TokenKind::Alternation) return 1;
            return 0;
        }

        inline void setError(const char* message) noexcept {
            error = message;
        }

        [[nodiscard]] inline bool tokenize(const string& pattern, vector<Token>& tokens) noexcept {
            tokens.clear();
            u64 openParens = 0;
            bool havePrevious = false;
            TokenKind previousKind = TokenKind::Alternation;

            for (u8 raw : pattern) {
                Token token{TokenKind::Literal, raw};
                if (raw == '(') token.kind = TokenKind::LeftParen;
                else if (raw == ')') token.kind = TokenKind::RightParen;
                else if (raw == '|') token.kind = TokenKind::Alternation;
                else if (raw == '*') token.kind = TokenKind::Star;

                if (token.kind == TokenKind::RightParen) {
                    if (openParens == 0) {
                        setError("unmatched ')'");
                        return false;
                    }
                    if (!havePrevious || previousKind == TokenKind::LeftParen) {
                        setError("empty group is not allowed");
                        return false;
                    }
                    if (previousKind == TokenKind::Alternation) {
                        setError("alternation missing right operand");
                        return false;
                    }
                    --openParens;
                } else if (token.kind == TokenKind::Alternation) {
                    if (!havePrevious || previousKind == TokenKind::LeftParen || previousKind == TokenKind::Alternation) {
                        setError("alternation missing left operand");
                        return false;
                    }
                } else if (token.kind == TokenKind::Star) {
                    if (!havePrevious || !canEndExpression(previousKind)) {
                        setError("'*' missing operand");
                        return false;
                    }
                }

                if (havePrevious && canEndExpression(previousKind) && canStartExpression(token.kind)) {
                    tokens.push_back({TokenKind::Concat, 0});
                }

                if (token.kind == TokenKind::LeftParen) openParens++;
                tokens.push_back(token);
                previousKind = token.kind;
                havePrevious = true;
            }

            if (openParens != 0) {
                setError("unmatched '('");
                return false;
            }
            if (havePrevious && previousKind == TokenKind::Alternation) {
                setError("alternation missing right operand");
                return false;
            }
            return true;
        }

        [[nodiscard]] inline bool toPostfix(const vector<Token>& tokens, vector<Token>& postfix) noexcept {
            postfix.clear();
            vector<Token> operators;

            for (const Token& token : tokens) {
                if (token.kind == TokenKind::Literal || token.kind == TokenKind::Star) {
                    postfix.push_back(token);
                } else if (token.kind == TokenKind::LeftParen) {
                    operators.push_back(token);
                } else if (token.kind == TokenKind::RightParen) {
                    while (!operators.empty() && operators.back().kind != TokenKind::LeftParen) {
                        postfix.push_back(operators.back());
                        operators.pop_back();
                    }
                    if (operators.empty()) {
                        setError("unmatched ')'");
                        return false;
                    }
                    operators.pop_back();
                } else if (isBinaryOperator(token.kind)) {
                    while (!operators.empty() && operators.back().kind != TokenKind::LeftParen &&
                           precedence(operators.back().kind) >= precedence(token.kind)) {
                        postfix.push_back(operators.back());
                        operators.pop_back();
                    }
                    operators.push_back(token);
                }
            }

            while (!operators.empty()) {
                if (operators.back().kind == TokenKind::LeftParen) {
                    setError("unmatched '('");
                    return false;
                }
                postfix.push_back(operators.back());
                operators.pop_back();
            }
            return true;
        }

        [[nodiscard]] inline u64 addNFANode(NFA& nfa) noexcept {
            nfa.nodes.emplace_back();
            return static_cast<u64>(nfa.nodes.size() - 1);
        }

        [[nodiscard]] inline bool failBuildNFA(NFA& nfa, const char* message) noexcept {
            nfa = NFA{};
            setError(message);
            return false;
        }

        [[nodiscard]] inline bool buildNFAFromPostfix(const vector<Token>& postfix, NFA& result) noexcept {
            result = NFA{};
            vector<NFAFragment> fragments;

            for (const Token& token : postfix) {
                if (token.kind == TokenKind::Literal) {
                    const u64 start = addNFANode(result);
                    const u64 accept = addNFANode(result);
                    result.nodes[start].transitions[token.value].push_back(accept);
                    result.alphabet[token.value] = true;
                    fragments.push_back({start, accept});
                } else if (token.kind == TokenKind::Star) {
                    if (fragments.empty()) {
                        return failBuildNFA(result, "'*' missing operand");
                    }
                    const NFAFragment child = fragments.back();
                    fragments.pop_back();
                    const u64 start = addNFANode(result);
                    const u64 accept = addNFANode(result);
                    result.nodes[start].epsilonTransitions.push_back(child.start);
                    result.nodes[start].epsilonTransitions.push_back(accept);
                    result.nodes[child.accept].epsilonTransitions.push_back(child.start);
                    result.nodes[child.accept].epsilonTransitions.push_back(accept);
                    fragments.push_back({start, accept});
                } else if (token.kind == TokenKind::Concat) {
                    if (fragments.size() < 2) {
                        return failBuildNFA(result, "concatenation missing operand");
                    }
                    const NFAFragment right = fragments.back();
                    fragments.pop_back();
                    const NFAFragment left = fragments.back();
                    fragments.pop_back();
                    result.nodes[left.accept].epsilonTransitions.push_back(right.start);
                    fragments.push_back({left.start, right.accept});
                } else if (token.kind == TokenKind::Alternation) {
                    if (fragments.size() < 2) {
                        return failBuildNFA(result, "alternation missing operand");
                    }
                    const NFAFragment right = fragments.back();
                    fragments.pop_back();
                    const NFAFragment left = fragments.back();
                    fragments.pop_back();
                    const u64 start = addNFANode(result);
                    const u64 accept = addNFANode(result);
                    result.nodes[start].epsilonTransitions.push_back(left.start);
                    result.nodes[start].epsilonTransitions.push_back(right.start);
                    result.nodes[left.accept].epsilonTransitions.push_back(accept);
                    result.nodes[right.accept].epsilonTransitions.push_back(accept);
                    fragments.push_back({start, accept});
                }
            }

            if (fragments.size() != 1) {
                return failBuildNFA(result, "invalid regular expression");
            }
            result.startState = fragments.back().start;
            result.acceptState = fragments.back().accept;
            return true;
        }

        [[nodiscard]] inline bool buildNFAFromPattern(const string& pattern, NFA& result) noexcept {
            result = NFA{};
            error = "";

            vector<Token> tokens;
            if (!tokenize(pattern, tokens)) return false;
            if (tokens.empty()) {
                const u64 state = addNFANode(result);
                result.startState = state;
                result.acceptState = state;
                return true;
            }

            vector<Token> postfix;
            if (!toPostfix(tokens, postfix)) return false;
            return buildNFAFromPostfix(postfix, result);
        }

        [[nodiscard]] inline vector<u64> epsilonClosure(const vector<NFANode>& nodes, const vector<u64>& states) noexcept {
            vector<u8> visited(nodes.size(), 0);
            vector<u64> closure;
            vector<u64> stack;

            for (u64 state : states) {
                if (state >= nodes.size() || static_cast<bool>(visited[state])) continue;
                visited[state] = 1;
                closure.push_back(state);
                stack.push_back(state);
            }

            while (!stack.empty()) {
                const u64 state = stack.back();
                stack.pop_back();
                vector<u64> nextStates;
                static_cast<void>(traverseEpsilon(nodes[state], nextStates));
                for (u64 nextState : nextStates) {
                    if (nextState >= nodes.size() || static_cast<bool>(visited[nextState])) continue;
                    visited[nextState] = 1;
                    closure.push_back(nextState);
                    stack.push_back(nextState);
                }
            }

            sort(closure.begin(), closure.end());
            return closure;
        }

        [[nodiscard]] inline vector<u64> moveOnCharacter(const vector<NFANode>& nodes, const vector<u64>& states, u8 c) noexcept {
            vector<u64> moved;
            for (u64 state : states) {
                if (state >= nodes.size()) continue;
                static_cast<void>(traverse(nodes[state], c, moved));
            }
            return moved;
        }

        [[nodiscard]] inline bool containsState(const vector<u64>& states, u64 state) noexcept {
            return binary_search(states.begin(), states.end(), state);
        }

        [[nodiscard]] inline bool buildDFA(const NFA& nfa, Automaton& result) noexcept {
            result = Automaton{};
            error = "";
            if (nfa.startState >= nfa.nodes.size() || nfa.acceptState >= nfa.nodes.size()) {
                setError("invalid NFA");
                return false;
            }

            vector<u64> startStates{nfa.startState};
            vector<u64> startClosure = epsilonClosure(nfa.nodes, startStates);
            if (startClosure.empty()) {
                setError("NFA start state is unreachable");
                return false;
            }

            map<vector<u64>, u64> dfaStateByNFAStates;
            vector<vector<u64>> pendingStates;
            dfaStateByNFAStates.emplace(startClosure, 0);
            pendingStates.push_back(startClosure);
            result.transitions.push_back(makeTransitionTable());
            result.accepting.push_back(static_cast<u8>(containsState(startClosure, nfa.acceptState)));

            for (size_t pendingIndex = 0; pendingIndex < pendingStates.size(); pendingIndex++) {
                const vector<u64> currentStates = pendingStates[pendingIndex];
                const auto currentIt = dfaStateByNFAStates.find(currentStates);
                if (currentIt == dfaStateByNFAStates.end()) {
                    setError("internal DFA state lookup failed");
                    return false;
                }
                const u64 currentDFAState = currentIt->second;

                for (size_t symbol = 0; symbol < nfa.alphabet.size(); symbol++) {
                    if (!nfa.alphabet[symbol]) continue;
                    vector<u64> movedStates = moveOnCharacter(nfa.nodes, currentStates, static_cast<u8>(symbol));
                    if (movedStates.empty()) continue;
                    vector<u64> targetStates = epsilonClosure(nfa.nodes, movedStates);
                    if (targetStates.empty()) continue;

                    u64 targetDFAState = invalidState;
                    const auto targetIt = dfaStateByNFAStates.find(targetStates);
                    if (targetIt == dfaStateByNFAStates.end()) {
                        targetDFAState = static_cast<u64>(result.transitions.size());
                        dfaStateByNFAStates.emplace(targetStates, targetDFAState);
                        pendingStates.push_back(targetStates);
                        result.transitions.push_back(makeTransitionTable());
                        result.accepting.push_back(static_cast<u8>(containsState(targetStates, nfa.acceptState)));
                    } else {
                        targetDFAState = targetIt->second;
                    }
                    result.transitions[currentDFAState][symbol] = targetDFAState;
                }
            }

            result.startState = 0;
            return true;
        }
    }

    [[nodiscard]] inline bool buildAutomaton(const string& pattern, Automaton& result) noexcept {
        result = Automaton{};
        detail::error = "";

        detail::NFA nfa;
        if (!detail::buildNFAFromPattern(pattern, nfa)) return false;
        if (!detail::buildDFA(nfa, result)) return false;
        return true;
    }

    [[nodiscard]] inline bool match(const string& line, const Automaton& automaton, bool wholeLine = false) noexcept {
        if (automaton.startState >= automaton.transitions.size()) return false;

        if (wholeLine) {
            u64 state = automaton.startState;
            for (size_t index = 0; index < line.size(); index++) {
                if (!detail::traverse(automaton, state, static_cast<u8>(line[index]))) return false;
            }
            return detail::isAccepting(automaton, state);
        }

        for (size_t start = 0; start <= line.size(); start++) {
            u64 state = automaton.startState;
            if (detail::isAccepting(automaton, state)) return true;

            for (size_t index = start; index < line.size(); index++) {
                if (!detail::traverse(automaton, state, static_cast<u8>(line[index]))) break;
                if (detail::isAccepting(automaton, state)) return true;
            }
        }
        return false;
    }
}