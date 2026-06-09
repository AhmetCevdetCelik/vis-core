/**
 * report_redactor_main.cpp
 *
 * VIS Report Redactor CLI entry point.
 *
 * License: MIT
 */

#include "../include/report_redactor.hpp"

#include <cstdio>
#include <string>

static void print_usage(const char* argv0) {
    std::fprintf(stderr,
                 "Usage: %s --input <report.json> --output <redacted.json>\n"
                 "\n"
                 "Redacts share-sensitive VIS JSON report values:\n"
                 "  - hostnames and usernames\n"
                 "  - path-like fields\n"
                 "  - workload command strings\n"
                 "  - raw metric/content fields\n",
                 argv0);
}

int main(int argc, char** argv) {
    std::string input_path;
    std::string output_path;

    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--input") {
            if (i + 1 >= argc) {
                std::fprintf(stderr,
                             "[vis-report-redact] ERROR: missing --input path\n");
                return 2;
            }
            input_path = argv[++i];
        } else if (arg == "--output") {
            if (i + 1 >= argc) {
                std::fprintf(stderr,
                             "[vis-report-redact] ERROR: missing --output path\n");
                return 2;
            }
            output_path = argv[++i];
        } else {
            std::fprintf(stderr,
                         "[vis-report-redact] ERROR: unknown argument: %s\n",
                         arg.c_str());
            return 2;
        }
    }

    if (input_path.empty() || output_path.empty()) {
        print_usage(argv[0]);
        return 2;
    }

    vis_report_redaction_result_t result;
    std::string error;
    if (!vis_report_redact_file(input_path.c_str(),
                                output_path.c_str(),
                                &result,
                                &error)) {
        std::fprintf(stderr, "[vis-report-redact] ERROR: %s\n",
                     error.c_str());
        return 1;
    }

    std::printf("VIS Report Redactor\n");
    std::printf("Input: %s\n", input_path.c_str());
    std::printf("Output: %s\n", output_path.c_str());
    std::printf("Replacements: %u\n", result.replacements);
    if (!result.redacted_fields.empty()) {
        std::printf("Fields:");
        for (const auto& field : result.redacted_fields) {
            std::printf(" %s", field.c_str());
        }
        std::printf("\n");
    }

    return 0;
}
