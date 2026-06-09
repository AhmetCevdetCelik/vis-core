/**
 * test_report_redactor.cpp
 *
 * Rootless tests for VIS report redaction.
 *
 * License: MIT
 */

#include "../include/report_redactor.hpp"

#include <cstdio>
#include <fstream>
#include <string>

static bool contains(const std::string& text, const char* needle) {
    return text.find(needle) != std::string::npos;
}

int main() {
    const std::string report =
        "{\n"
        "  \"vis_mem_run_attestation\": {\n"
        "    \"schema_version\": \"0.1\",\n"
        "    \"generator\": \"vis-mem-run 0.1.0\",\n"
        "    \"machine\": { \"hostname\": \"ahmet-laptop\" },\n"
        "    \"metric_file\": {\n"
        "      \"path\": \"/home/ahmet/private/metric.json\",\n"
        "      \"content\": \"{\\\"token\\\":\\\"secret\\\"}\"\n"
        "    },\n"
        "    \"workload\": {\n"
        "      \"argv\": \"python /home/ahmet/private/run.py\"\n"
        "    },\n"
        "    \"note\": \"safe text under /home/ahmet/project\"\n"
        "  }\n"
        "}\n";

    vis_report_redaction_result_t result;
    std::string error;
    if (!vis_report_redact_json(report, &result, &error) ||
        result.redacted_json.empty() ||
        result.replacements < 4) {
        std::fprintf(stderr, "[test] redaction failed: %s\n",
                     error.c_str());
        return 1;
    }
    if (contains(result.redacted_json, "ahmet-laptop") ||
        contains(result.redacted_json, "/home/ahmet") ||
        contains(result.redacted_json, "secret") ||
        contains(result.redacted_json, "run.py")) {
        std::fprintf(stderr,
                     "[test] sensitive values survived redaction:\n%s\n",
                     result.redacted_json.c_str());
        return 1;
    }
    if (!contains(result.redacted_json, "<redacted-hostname>") ||
        !contains(result.redacted_json, "<redacted-path>") ||
        !contains(result.redacted_json, "<redacted-content>") ||
        !contains(result.redacted_json, "<redacted-command>")) {
        std::fprintf(stderr, "[test] redaction markers are missing.\n");
        return 1;
    }

    const char* input_path = "/tmp/vis_report_redactor_input.json";
    const char* output_path = "/tmp/vis_report_redactor_output.json";
    {
        std::ofstream out(input_path);
        out << report;
    }
    if (!vis_report_redact_file(input_path,
                                output_path,
                                &result,
                                &error)) {
        std::fprintf(stderr, "[test] file redaction failed: %s\n",
                     error.c_str());
        return 1;
    }
    std::ifstream in(output_path);
    std::string output((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
    if (!contains(output, "<redacted-hostname>") ||
        contains(output, "ahmet-laptop")) {
        std::fprintf(stderr, "[test] redacted output file is invalid.\n");
        return 1;
    }

    std::printf("[test] PASS: VIS Report Redactor works.\n");
    return 0;
}
