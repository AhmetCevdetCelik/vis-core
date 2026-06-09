#pragma once

/**
 * evidence.hpp
 *
 * Shared VIS evidence completeness model.
 *
 * License: MIT
 */

#include <cstdint>
#include <string>
#include <vector>

struct vis_evidence_completeness_t {
    std::string level;
    uint32_t score = 0;
    std::vector<std::string> signals;
    std::vector<std::string> limitations;
};

static inline std::string vis_evidence_level_from_score(uint32_t score) {
    if (score >= 80) return "strong";
    if (score >= 50) return "partial";
    if (score > 0) return "limited";
    return "unavailable";
}
