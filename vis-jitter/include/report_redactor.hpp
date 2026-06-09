#pragma once

/**
 * report_redactor.hpp
 *
 * Lightweight VIS JSON report redaction.
 *
 * License: MIT
 */

#include <cstdint>
#include <string>
#include <vector>

struct vis_report_redaction_result_t {
    std::string redacted_json;
    uint32_t replacements = 0;
    std::vector<std::string> redacted_fields;
};

bool vis_report_redact_json(const std::string& json,
                            vis_report_redaction_result_t* result,
                            std::string* error);
bool vis_report_redact_file(const char* input_path,
                            const char* output_path,
                            vis_report_redaction_result_t* result,
                            std::string* error);
