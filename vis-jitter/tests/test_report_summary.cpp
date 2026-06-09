/**
 * test_report_summary.cpp
 *
 * Rootless tests for VIS report executive summary generation.
 *
 * License: MIT
 */

#include "../include/report_summary.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

static std::string read_file(const char* path) {
    std::ifstream in(path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

int main() {
    const std::string json =
        "{\n"
        "  \"vis_mem_compare_run_report\": {\n"
        "    \"schema_version\": \"0.1\",\n"
        "    \"generator\": \"vis-mem-compare-run\",\n"
        "    \"evidence_level\": \"strong\",\n"
        "    \"metric_key\": \"duration_ns\",\n"
        "    \"recommended_first_profile\": \"pretouch-hugepage\",\n"
        "    \"recommendation_reason\": \"Selected by parsed metric.\",\n"
        "    \"runs\": []\n"
        "  }\n"
        "}\n";

    std::string markdown;
    vis_report_summary_result_t result;
    if (!vis_report_summary_generate(json, &markdown, &result)) {
        std::cerr << "[test] FAIL: summary generation failed: "
                  << result.error << "\n";
        return 1;
    }

    if (!contains(markdown, "# VIS Executive Summary") ||
        !contains(markdown, "## What Changed") ||
        !contains(markdown, "## What This Does Not Prove") ||
        !contains(markdown, "## Recommended Next Step") ||
        !contains(markdown, "pretouch-hugepage") ||
        !contains(markdown, "duration_ns")) {
        std::cerr << "[test] FAIL: generated summary is missing expected "
                  << "sections or evidence.\n";
        return 1;
    }

    const char* input_path = "/tmp/vis_report_summary_input.json";
    const char* output_path = "/tmp/vis_report_summary_output.md";
    {
        std::ofstream out(input_path);
        out << json;
    }

    vis_report_summary_config_t config;
    config.json_path = input_path;
    config.output_path = output_path;

    if (!vis_report_summary_write(config, &result) || !result.written) {
        std::cerr << "[test] FAIL: summary write failed: "
                  << result.error << "\n";
        return 1;
    }

    const std::string saved = read_file(output_path);
    if (!contains(saved, "VIS-Mem workload profile comparison") ||
        !contains(saved, "advisory evidence")) {
        std::cerr << "[test] FAIL: saved summary missing expected content.\n";
        return 1;
    }

    vis_report_summary_result_t bad_result;
    std::string bad_markdown;
    if (vis_report_summary_generate("{}", &bad_markdown, &bad_result) ||
        bad_result.error.empty()) {
        std::cerr << "[test] FAIL: invalid report was accepted.\n";
        return 1;
    }

    std::cout << "[test] PASS: VIS Report Summary works.\n";
    return 0;
}
