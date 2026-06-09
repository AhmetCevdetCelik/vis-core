/**
 * report_summary_main.cpp
 *
 * CLI for VIS executive summary generation.
 *
 * License: MIT
 */

#include "../include/report_summary.hpp"

#include <cstdio>
#include <cstring>

static void print_usage(const char* argv0) {
    std::fprintf(stderr,
                 "Usage: %s --json report.json --output summary.md\n",
                 argv0);
}

int main(int argc, char** argv) {
    vis_report_summary_config_t config;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--json") == 0) {
            if (++i >= argc) {
                std::fprintf(stderr,
                             "[vis-report-summary] ERROR: missing --json path\n");
                return 1;
            }
            config.json_path = argv[i];
        } else if (std::strcmp(argv[i], "--output") == 0) {
            if (++i >= argc) {
                std::fprintf(stderr,
                             "[vis-report-summary] ERROR: missing --output path\n");
                return 1;
            }
            config.output_path = argv[i];
        } else if (std::strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            std::fprintf(stderr,
                         "[vis-report-summary] ERROR: unknown argument: %s\n",
                         argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (config.json_path.empty()) {
        std::fprintf(stderr,
                     "[vis-report-summary] ERROR: missing --json path\n");
        print_usage(argv[0]);
        return 1;
    }
    if (config.output_path.empty()) {
        std::fprintf(stderr,
                     "[vis-report-summary] ERROR: missing --output path\n");
        print_usage(argv[0]);
        return 1;
    }

    vis_report_summary_result_t result;
    if (!vis_report_summary_write(config, &result)) {
        std::fprintf(stderr, "[vis-report-summary] ERROR: %s\n",
                     result.error.c_str());
        return 1;
    }

    std::printf("VIS Report Summary\n");
    std::printf("Report type: %s\n", result.report_type.c_str());
    std::printf("Schema: %s\n", result.schema_version.c_str());
    std::printf("Generator: %s\n", result.generator.c_str());
    std::printf("[vis-report-summary] Markdown saved to: %s\n",
                result.output_path.c_str());
    return 0;
}
