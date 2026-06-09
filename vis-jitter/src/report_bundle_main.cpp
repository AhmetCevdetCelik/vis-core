/**
 * report_bundle_main.cpp
 *
 * VIS Report Bundle CLI entry point.
 *
 * License: MIT
 */

#include "../include/report_bundle.hpp"

#include <cstdio>
#include <string>

static void print_usage(const char* argv0) {
    std::fprintf(stderr,
                 "Usage: %s --json <report.json> --output-dir <bundle-dir> [options]\n"
                 "\n"
                 "Options:\n"
                 "  --llm <report.md>       Include AI-readable Markdown context\n"
                 "  --command <command>     Record the command used to create the report\n",
                 argv0);
}

int main(int argc, char** argv) {
    vis_report_bundle_config_t config;

    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--json") {
            if (i + 1 >= argc) {
                std::fprintf(stderr,
                             "[vis-report-bundle] ERROR: missing --json path\n");
                return 2;
            }
            config.json_path = argv[++i];
        } else if (arg == "--llm") {
            if (i + 1 >= argc) {
                std::fprintf(stderr,
                             "[vis-report-bundle] ERROR: missing --llm path\n");
                return 2;
            }
            config.markdown_path = argv[++i];
        } else if (arg == "--command") {
            if (i + 1 >= argc) {
                std::fprintf(stderr,
                             "[vis-report-bundle] ERROR: missing --command value\n");
                return 2;
            }
            config.command = argv[++i];
        } else if (arg == "--output-dir") {
            if (i + 1 >= argc) {
                std::fprintf(stderr,
                             "[vis-report-bundle] ERROR: missing --output-dir path\n");
                return 2;
            }
            config.output_dir = argv[++i];
        } else {
            std::fprintf(stderr,
                         "[vis-report-bundle] ERROR: unknown argument: %s\n",
                         arg.c_str());
            return 2;
        }
    }

    if (config.json_path.empty() || config.output_dir.empty()) {
        print_usage(argv[0]);
        return 2;
    }

    vis_report_bundle_result_t result;
    if (!vis_report_bundle_write(config, &result)) {
        std::fprintf(stderr, "[vis-report-bundle] ERROR: %s\n",
                     result.error.c_str());
        return 1;
    }

    std::printf("VIS Report Bundle\n");
    std::printf("Output: %s\n", result.output_dir.c_str());
    std::printf("Type: %s\n", result.report_type.c_str());
    std::printf("Schema: %s\n", result.schema_version.c_str());
    std::printf("Generator: %s\n", result.generator.c_str());
    std::printf("Files:\n");
    std::printf("  %s\n", result.report_json_path.c_str());
    if (!result.report_markdown_path.empty()) {
        std::printf("  %s\n", result.report_markdown_path.c_str());
    }
    std::printf("  %s\n", result.manifest_path.c_str());
    std::printf("  %s\n", result.readme_path.c_str());
    return 0;
}
