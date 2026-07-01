#pragma once

/**
 * report_validator.hpp
 *
 * Lightweight VIS JSON report validator.
 *
 * License: MIT
 */

#include <string>
#include <vector>

struct vis_report_validation_result_t {
    bool valid = false;
    std::string report_type;
    std::string schema_version;
    std::string generator;
    std::string evidence_level;
    std::string timer_evidence_level;
    std::string execution_evidence_level;
    std::string backend_status;
    std::string control_level;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

bool vis_report_validate_json(const std::string& json,
                              vis_report_validation_result_t* result);
bool vis_report_validate_file(const char* path,
                              vis_report_validation_result_t* result,
                              std::string* error);
bool vis_policy_evidence_level_is_valid(const std::string& level);
std::string vis_policy_evidence_level_semantics(const std::string& level);
bool vis_probe_evidence_level_is_valid(const std::string& level);
std::string vis_probe_evidence_level_semantics(const std::string& level);
