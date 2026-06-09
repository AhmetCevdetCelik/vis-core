#pragma once

/**
 * report_summary.hpp
 *
 * Human-readable VIS executive summary generator.
 *
 * License: MIT
 */

#include <string>

#define VIS_REPORT_SUMMARY_VERSION "0.1.0"

struct vis_report_summary_config_t {
    std::string json_path;
    std::string output_path;
};

struct vis_report_summary_result_t {
    bool written = false;
    std::string report_type;
    std::string schema_version;
    std::string generator;
    std::string output_path;
    std::string error;
};

bool vis_report_summary_generate(const std::string& json,
                                 std::string* markdown,
                                 vis_report_summary_result_t* result);
bool vis_report_summary_write(const vis_report_summary_config_t& config,
                              vis_report_summary_result_t* result);
