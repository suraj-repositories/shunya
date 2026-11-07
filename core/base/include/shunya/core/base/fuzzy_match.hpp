#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <cctype>

namespace shunya::core::base {

struct FuzzyMatchResult {
    bool matches{false};
    int score{0};
    std::vector<size_t> matched_indices;
};

class FuzzyMatcher {
public:
    [[nodiscard]] static FuzzyMatchResult match(std::string_view pattern, std::string_view text) noexcept {
        FuzzyMatchResult result;
        if (pattern.empty()) {
            result.matches = true;
            result.score = 0;
            return result;
        }
        if (text.empty() || pattern.size() > text.size()) {
            return result;
        }

        size_t p_idx = 0;
        size_t t_idx = 0;
        int score = 0;
        int consecutive_bonus = 0;
        bool is_word_start = true;

        while (p_idx < pattern.size() && t_idx < text.size()) {
            char p = static_cast<char>(std::tolower(static_cast<unsigned char>(pattern[p_idx])));
            char t = static_cast<char>(std::tolower(static_cast<unsigned char>(text[t_idx])));

            if (p == t) {
                result.matched_indices.push_back(t_idx);
                score += 10;

                if (t_idx == 0) {
                    score += 25; // Prefix match bonus
                } else if (is_word_start) {
                    score += 15; // Word boundary match bonus
                }

                if (consecutive_bonus > 0) {
                    score += (consecutive_bonus * 5); // Consecutive match bonus
                }
                ++consecutive_bonus;
                ++p_idx;
            } else {
                consecutive_bonus = 0;
            }

            is_word_start = (text[t_idx] == ' ' || text[t_idx] == '_' || text[t_idx] == '-' || text[t_idx] == '.');
            ++t_idx;
        }

        if (p_idx == pattern.size()) {
            result.matches = true;
            int length_penalty = static_cast<int>(text.size() - pattern.size());
            result.score = std::max(1, score - length_penalty);
        }

        return result;
    }
};

} // namespace shunya::core::base
