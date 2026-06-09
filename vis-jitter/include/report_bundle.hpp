#pragma once

/**
 * report_bundle.hpp
 *
 * VIS report bundle writer.
 *
 * License: MIT
 */

#include <string>

#define VIS_REPORT_BUNDLE_VERSION "0.1.0"

struct vis_report_bundle_config_t {
    std::string json_path;
    std::string markdown_path;
    std::string command;
    std::string output_dir;
};

struct vis_report_bundle_result_t {
    bool created = false;
    std::string output_dir;
    std::string report_json_path;
    std::string report_markdown_path;
    std::string manifest_path;
    std::string readme_path;
    std::string report_type;
    std::string schema_version;
    std::string generator;
    std::string error;
};

bool vis_report_bundle_write(const vis_report_bundle_config_t& config,
                             vis_report_bundle_result_t* result);
