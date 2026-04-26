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
    using std::array, std::string, std::vector;

    using u64 = std::uint64_t;

    namespace detail {
        inline string error;
        inline constexpr u64 invalidState = std::numeric_limits<u64>::max();
    }

    [[nodiscard]] inline string getLastParserError() noexcept { return detail::error; }

    struct AutomatonNode {
        array<u64, 256> transitions;
        bool accepting{false};

        AutomatonNode() noexcept {
            transitions.fill(detail::invalidState);
        }
    };

    struct Automaton {
        vector<AutomatonNode> nodes;
        u64 state{0};
        u64 startState{0};

        inline void reset() noexcept {
            state = startState;
        }

        [[nodiscard]] inline bool isAccepting() const noexcept {
            return state < nodes.size() && nodes[state].accepting;
        }

        [[nodiscard]] inline bool traverse(char c) noexcept {
            if (state >= nodes.size()) return false;
            const u64 nextState = nodes[state].transitions[static_cast<unsigned char>(c)];
            if (nextState == detail::invalidState) return false;
            state = nextState;
            return true;
        }
    };

    namespace detail {
        enum struct TokenKind {
            Literal,
            LeftParen,
            RightParen,
            Alternation,
            Star,
            Concat
        };

        struct Token {
            TokenKind kind{TokenKind::Literal};
            unsigned char value{0};
        };

        struct NfaNode {
            array<vector<u64>, 256> transitions;
            vector<u64> epsilonTransitions;

            [[nodiscard]] inline bool traverse(unsigned char c, vector<u64>& result) const {
                const auto oldSize = result.size();
                result.insert(result.end(), transitions[c].begin(), transitions[c].end());
                return result.size() != oldSize;
            }

            [[nodiscard]] inline bool traverseEpsilon(vector<u64>& result) const {
                const auto oldSize = result.size();
                result.insert(result.end(), epsilonTransitions.begin(), epsilonTransitions.end());
                return result.size() != oldSize;
            }
        };

        struct NfaFragment {
            u64 start{invalidState};
            u64 accept{invalidState};
        };

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

        inline void setError(const string& message) {
            error = message;
        }

        [[nodiscard]] inline bool tokenize(const string& pattern, vector<Token>& tokens) {
            tokens.clear();
            u64 openParens = 0;
            bool havePrevious = false;
            TokenKind previousKind = TokenKind::Alternation;

            for (char raw : pattern) {
                Token token{TokenKind::Literal, static_cast<unsigned char>(raw)};
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

                if (token.kind == TokenKind::LeftParen) ++openParens;
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

        [[nodiscard]] inline bool toPostfix(const vector<Token>& tokens, vector<Token>& postfix) {
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

        [[nodiscard]] inline u64 addNfaNode(vector<NfaNode>& nodes) {
            nodes.emplace_back();
            return static_cast<u64>(nodes.size() - 1);
        }

        [[nodiscard]] inline bool buildNfa(const vector<Token>& postfix, vector<NfaNode>& nodes, NfaFragment& result,
                                           array<bool, 256>& alphabet) {
            nodes.clear();
            alphabet.fill(false);
            vector<NfaFragment> fragments;

            for (const Token& token : postfix) {
                if (token.kind == TokenKind::Literal) {
                    const u64 start = addNfaNode(nodes);
                    const u64 accept = addNfaNode(nodes);
                    nodes[start].transitions[token.value].push_back(accept);
                    alphabet[token.value] = true;
                    fragments.push_back({start, accept});
                } else if (token.kind == TokenKind::Star) {
                    if (fragments.empty()) {
                        setError("'*' missing operand");
                        return false;
                    }
                    const NfaFragment child = fragments.back();
                    fragments.pop_back();
                    const u64 start = addNfaNode(nodes);
                    const u64 accept = addNfaNode(nodes);
                    nodes[start].epsilonTransitions.push_back(child.start);
                    nodes[start].epsilonTransitions.push_back(accept);
                    nodes[child.accept].epsilonTransitions.push_back(child.start);
                    nodes[child.accept].epsilonTransitions.push_back(accept);
                    fragments.push_back({start, accept});
                } else if (token.kind == TokenKind::Concat) {
                    if (fragments.size() < 2) {
                        setError("concatenation missing operand");
                        return false;
                    }
                    const NfaFragment right = fragments.back();
                    fragments.pop_back();
                    const NfaFragment left = fragments.back();
                    fragments.pop_back();
                    nodes[left.accept].epsilonTransitions.push_back(right.start);
                    fragments.push_back({left.start, right.accept});
                } else if (token.kind == TokenKind::Alternation) {
                    if (fragments.size() < 2) {
                        setError("alternation missing operand");
                        return false;
                    }
                    const NfaFragment right = fragments.back();
                    fragments.pop_back();
                    const NfaFragment left = fragments.back();
                    fragments.pop_back();
                    const u64 start = addNfaNode(nodes);
                    const u64 accept = addNfaNode(nodes);
                    nodes[start].epsilonTransitions.push_back(left.start);
                    nodes[start].epsilonTransitions.push_back(right.start);
                    nodes[left.accept].epsilonTransitions.push_back(accept);
                    nodes[right.accept].epsilonTransitions.push_back(accept);
                    fragments.push_back({start, accept});
                }
            }

            if (fragments.size() != 1) {
                setError("invalid regular expression");
                return false;
            }
            result = fragments.back();
            return true;
        }

        [[nodiscard]] inline vector<u64> epsilonClosure(const vector<NfaNode>& nodes, const vector<u64>& states) {
            vector<bool> visited(nodes.size(), false);
            vector<u64> closure;
            vector<u64> stack;

            for (u64 state : states) {
                if (state >= nodes.size() || visited[state]) continue;
                visited[state] = true;
                closure.push_back(state);
                stack.push_back(state);
            }

            while (!stack.empty()) {
                const u64 state = stack.back();
                stack.pop_back();
                vector<u64> nextStates;
                (void)nodes[state].traverseEpsilon(nextStates);
                for (u64 nextState : nextStates) {
                    if (nextState >= nodes.size() || visited[nextState]) continue;
                    visited[nextState] = true;
                    closure.push_back(nextState);
                    stack.push_back(nextState);
                }
            }

            std::sort(closure.begin(), closure.end());
            return closure;
        }

        [[nodiscard]] inline vector<u64> moveOnCharacter(const vector<NfaNode>& nodes, const vector<u64>& states,
                                                         unsigned char c) {
            vector<u64> moved;
            for (u64 state : states) {
                if (state >= nodes.size()) continue;
                (void)nodes[state].traverse(c, moved);
            }
            return moved;
        }

        [[nodiscard]] inline bool containsState(const vector<u64>& states, u64 state) noexcept {
            return std::binary_search(states.begin(), states.end(), state);
        }

        [[nodiscard]] inline Automaton buildEmptyAutomaton() {
            Automaton automaton;
            automaton.nodes.emplace_back();
            automaton.nodes[0].accepting = true;
            return automaton;
        }

        [[nodiscard]] inline bool buildDfa(const vector<NfaNode>& nfaNodes, u64 nfaStart, u64 nfaAccept,
                                           const array<bool, 256>& alphabet, Automaton& result) {
            result = Automaton{};
            vector<u64> startStates{nfaStart};
            vector<u64> startClosure = epsilonClosure(nfaNodes, startStates);
            if (startClosure.empty()) {
                setError("NFA start state is unreachable");
                return false;
            }

            std::map<vector<u64>, u64> dfaStateByNfaStates;
            vector<vector<u64>> pendingStates;
            dfaStateByNfaStates.emplace(startClosure, 0);
            pendingStates.push_back(startClosure);
            result.nodes.emplace_back();
            result.nodes[0].accepting = containsState(startClosure, nfaAccept);

            for (std::size_t pendingIndex = 0; pendingIndex < pendingStates.size(); ++pendingIndex) {
                const vector<u64> currentStates = pendingStates[pendingIndex];
                const auto currentIt = dfaStateByNfaStates.find(currentStates);
                if (currentIt == dfaStateByNfaStates.end()) {
                    setError("internal DFA state lookup failed");
                    return false;
                }
                const u64 currentDfaState = currentIt->second;

                for (std::size_t symbol = 0; symbol < alphabet.size(); ++symbol) {
                    if (!alphabet[symbol]) continue;
                    vector<u64> movedStates = moveOnCharacter(nfaNodes, currentStates, static_cast<unsigned char>(symbol));
                    if (movedStates.empty()) continue;
                    vector<u64> targetStates = epsilonClosure(nfaNodes, movedStates);
                    if (targetStates.empty()) continue;

                    u64 targetDfaState = invalidState;
                    const auto targetIt = dfaStateByNfaStates.find(targetStates);
                    if (targetIt == dfaStateByNfaStates.end()) {
                        targetDfaState = static_cast<u64>(result.nodes.size());
                        dfaStateByNfaStates.emplace(targetStates, targetDfaState);
                        pendingStates.push_back(targetStates);
                        result.nodes.emplace_back();
                        result.nodes.back().accepting = containsState(targetStates, nfaAccept);
                    } else {
                        targetDfaState = targetIt->second;
                    }
                    result.nodes[currentDfaState].transitions[symbol] = targetDfaState;
                }
            }

            result.startState = 0;
            result.state = result.startState;
            return true;
        }
    }

    [[nodiscard]] inline bool buildAutomaton(const string& pattern, Automaton& result) noexcept {
        result = Automaton{};
        detail::error.clear();

        try {
            vector<detail::Token> tokens;
            if (!detail::tokenize(pattern, tokens)) return false;
            if (tokens.empty()) {
                result = detail::buildEmptyAutomaton();
                return true;
            }

            vector<detail::Token> postfix;
            if (!detail::toPostfix(tokens, postfix)) return false;

            vector<detail::NfaNode> nfaNodes;
            detail::NfaFragment nfaFragment;
            array<bool, 256> alphabet;
            if (!detail::buildNfa(postfix, nfaNodes, nfaFragment, alphabet)) return false;
            if (!detail::buildDfa(nfaNodes, nfaFragment.start, nfaFragment.accept, alphabet, result)) return false;
            return true;
        } catch (...) {
            result = Automaton{};
            detail::setError("failed to build automaton");
            return false;
        }
    }
}