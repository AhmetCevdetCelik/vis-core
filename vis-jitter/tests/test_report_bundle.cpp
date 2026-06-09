/**
 * test_report_bundle.cpp
 *
 * Rootless tests for VIS report bundle creation.
 *
 * License: MIT
 */

#include "../include/report_bundle.hpp"

#include <cstdio>
#include <fstream>
#include <string>

static bool contains(const std::string& text, const char* needle) {
    return text.find(needle) != std::string::npos;
}

static std::string read_file(const char* path) {
    std::ifstream in(path);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

int main() {
    const char* json_path = "/tmp/vis_report_bundle_input.json";
    const char* md_path = "/tmp/vis_report_bundle_input.md";
    const char* out_dir = "/tmp/vis_report_bundle_test";

    {
        std::ofstream out(json_path);
        out << "{\n"
            << "  \"vis_mem_probe_report\": {\n"
            << "    \"schema_version\": \"0.1\",\n"
            << "    \"generator\": \"vis-mem-probe 0.1.0\"\n"
            << "  }\n"
            << "}\n";
    }
    {
        std::ofstream out(md_path);
        out << "# VIS Mem Probe AI Context\n";
    }

    vis_report_bundle_config_t config;
    config.json_path = json_path;
    config.markdown_path = md_path;
    config.command = "./vis-mem-probe --size-mb 1";
    config.output_dir = out_dir;

    vis_report_bundle_result_t result;
    if (!vis_report_bundle_write(config, &result) ||
        !result.created ||
        result.report_type != "vis_mem_probe_report" ||
        result.schema_version != "0.1" ||
        result.generator != "vis-mem-probe 0.1.0") {
        std::fprintf(stderr, "[test] bundle creation failed: %s\n",
                     result.error.c_str());
        return 1;
    }

    const std::string manifest = read_file(result.manifest_path.c_str());
    const std::string readme = read_file(result.readme_path.c_str());
    const std::string bundled_json = read_file(result.report_json_path.c_str());
    const std::string bundled_md =
        read_file(result.report_markdown_path.c_str());

    if (!contains(manifest, "\"vis_report_bundle\"") ||
        !contains(manifest, "\"report_type\": \"vis_mem_probe_report\"") ||
        !contains(manifest, "\"command\": \"./vis-mem-probe --size-mb 1\"") ||
        !contains(manifest, "\"environment_snapshot\"") ||
        !contains(readme, "VIS Report Bundle") ||
        !contains(bundled_json, "\"vis_mem_probe_report\"") ||
        !contains(bundled_md, "VIS Mem Probe AI Context")) {
        std::fprintf(stderr, "[test] bundle files are incomplete.\n");
        return 1;
    }

    {
        std::ofstream out(json_path);
        out << "{\n"
            << "  \"vis_run_attestation\": {\n"
            << "    \"schema_version\": \"0.1\",\n"
            << "    \"generator\": \"vis-run 0.1.0\",\n"
            << "    \"vis_cpu_policy_bundle\": {\n"
            << "      \"schema_version\": \"0.1\",\n"
            << "      \"generator\": \"vis-doctor 0.1.0\",\n"
            << "      \"evidence_level\": \"advisory\"\n"
            << "    }\n"
            << "  }\n"
            << "}\n";
    }
    config.markdown_path.clear();
    config.command = "./vis-run -- /bin/true";
    if (!vis_report_bundle_write(config, &result) ||
        result.report_type != "vis_run_attestation") {
        std::fprintf(stderr,
                     "[test] vis-run attestation bundle failed: %s\n",
                     result.error.c_str());
        return 1;
    }

    {
        std::ofstream out(json_path);
        out << "{\n"
            << "  \"vis_mem_run_attestation\": {\n"
            << "    \"schema_version\": \"0.1\",\n"
            << "    \"generator\": \"vis-mem-run 0.1.0\",\n"
            << "    \"vis_mem_policy_bundle\": {\n"
            << "      \"schema_version\": \"0.1\",\n"
            << "      \"generator\": \"vis-mem-run 0.1.0\",\n"
            << "      \"evidence_level\": \"attested\"\n"
            << "    }\n"
            << "  }\n"
            << "}\n";
    }
    config.command = "./vis-mem-run -- /bin/true";
    if (!vis_report_bundle_write(config, &result) ||
        result.report_type != "vis_mem_run_attestation") {
        std::fprintf(stderr,
                     "[test] vis-mem-run attestation bundle failed: %s\n",
                     result.error.c_str());
        return 1;
    }

    vis_report_bundle_config_t bad;
    bad.json_path = "/tmp/does-not-exist-vis-report.json";
    bad.output_dir = out_dir;
    if (vis_report_bundle_write(bad, &result) || result.error.empty()) {
        std::fprintf(stderr, "[test] invalid bundle input was accepted.\n");
        return 1;
    }

    std::printf("[test] PASS: VIS Report Bundle works.\n");
    return 0;
}
