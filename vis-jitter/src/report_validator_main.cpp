/**
 * report_validator_main.cpp
 *
 * VIS Report Validator CLI entry point.
 *
 * License: MIT
 */

#include "../include/report_validator.hpp"

#include <cstdio>

static void print_usage(const char* argv0) {
    std::fprintf(stderr,
                 "Usage: %s <report.json>\n"
                 "\n"
                 "Validates basic VIS JSON report metadata:\n"
                 "  - known VIS report root object\n"
                 "  - schema_version\n"
                 "  - generator\n"
                 "  - policy bundle evidence semantics\n",
                 argv0);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 2;
    }

    vis_report_validation_result_t result;
    std::string error;
    const bool ok = vis_report_validate_file(argv[1], &result, &error);

    if (!error.empty()) {
        std::fprintf(stderr, "[vis-report-validate] ERROR: %s\n",
                     error.c_str());
        return 1;
    }

    std::printf("VIS Report Validator\n");
    std::printf("Report: %s\n", argv[1]);
    std::printf("Status: %s\n", ok ? "VALID" : "INVALID");
    std::printf("Type: %s\n",
                result.report_type.empty()
                    ? "unknown"
                    : result.report_type.c_str());
    std::printf("Schema: %s\n",
                result.schema_version.empty()
                    ? "unknown"
                    : result.schema_version.c_str());
    std::printf("Generator: %s\n",
                result.generator.empty()
                    ? "unknown"
                    : result.generator.c_str());
    if (!result.evidence_level.empty()) {
        std::printf("Evidence level: %s\n", result.evidence_level.c_str());
        const std::string evidence_meaning =
            result.report_type == "vis_probe_report"
                ? vis_probe_evidence_level_semantics(result.evidence_level)
                : vis_policy_evidence_level_semantics(result.evidence_level);
        std::printf("Evidence meaning: %s\n", evidence_meaning.c_str());
    }
    if (!result.control_level.empty()) {
        std::printf("Control level: %s\n", result.control_level.c_str());
    }

    for (const auto& warning : result.warnings) {
        std::printf("Warning: %s\n", warning.c_str());
    }
    for (const auto& validation_error : result.errors) {
        std::printf("Error: %s\n", validation_error.c_str());
    }

    return ok ? 0 : 1;
}
