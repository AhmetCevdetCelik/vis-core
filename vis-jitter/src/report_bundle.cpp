/**
 * report_bundle.cpp
 *
 * VIS report bundle writer implementation.
 *
 * License: MIT
 */

#include "../include/report_bundle.hpp"

#include "../include/report_validator.hpp"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

static std::string json_escape(const std::string& value) {
    std::string out;
    for (unsigned char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += static_cast<char>(c); break;
        }
    }
    return out;
}

static bool ensure_dir(const std::string& path, std::string* error) {
    if (path.empty()) {
        if (error) *error = "missing output directory";
        return false;
    }
    struct stat st {};
    if (stat(path.c_str(), &st) == 0) {
        if (S_ISDIR(st.st_mode)) return true;
        if (error) *error = "output path exists and is not a directory";
        return false;
    }
    if (mkdir(path.c_str(), 0755) == 0) return true;
    if (error) {
        *error = std::string("failed to create output directory: ") +
                 strerror(errno);
    }
    return false;
}

static bool read_file(const std::string& path,
                      std::string* content,
                      std::string* error) {
    if (content == nullptr) return false;
    if (path.empty()) {
        if (error) *error = "missing input path";
        return false;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) *error = "failed to open input file: " + path;
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    if (!in.good() && !in.eof()) {
        if (error) *error = "failed to read input file: " + path;
        return false;
    }
    *content = buffer.str();
    return true;
}

static bool write_file(const std::string& path,
                       const std::string& content,
                       std::string* error) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        if (error) *error = "failed to open output file: " + path;
        return false;
    }
    out << content;
    if (!out) {
        if (error) *error = "failed to write output file: " + path;
        return false;
    }
    return true;
}

static std::string path_join(const std::string& dir,
                             const std::string& name) {
    if (dir.empty() || dir.back() == '/') return dir + name;
    return dir + "/" + name;
}

static std::string runtime_snapshot_json() {
    struct utsname uts {};
    std::string kernel = "unknown";
    if (uname(&uts) == 0) {
        kernel = std::string(uts.sysname) + " " + uts.release;
    }

    std::ostringstream out;
    out << "{\n";
    out << "      \"kernel\": \"" << json_escape(kernel) << "\",\n";
    out << "      \"uid\": " << static_cast<unsigned long long>(getuid())
        << ",\n";
    out << "      \"euid\": " << static_cast<unsigned long long>(geteuid())
        << ",\n";
    out << "      \"pid\": " << static_cast<unsigned long long>(getpid())
        << "\n";
    out << "    }";
    return out.str();
}

static std::string build_manifest(
    const vis_report_bundle_config_t& config,
    const vis_report_bundle_result_t& result
) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"vis_report_bundle\": {\n";
    out << "    \"version\": \"" << VIS_REPORT_BUNDLE_VERSION << "\",\n";
    out << "    \"report_type\": \"" << json_escape(result.report_type)
        << "\",\n";
    out << "    \"schema_version\": \"" << json_escape(result.schema_version)
        << "\",\n";
    out << "    \"generator\": \"" << json_escape(result.generator)
        << "\",\n";
    out << "    \"files\": {\n";
    out << "      \"json\": \"report.json\",\n";
    out << "      \"markdown\": "
        << (config.markdown_path.empty() ? "null" : "\"report.md\"")
        << ",\n";
    out << "      \"manifest\": \"manifest.json\",\n";
    out << "      \"readme\": \"README.md\"\n";
    out << "    },\n";
    out << "    \"source\": {\n";
    out << "      \"json_path\": \"" << json_escape(config.json_path)
        << "\",\n";
    out << "      \"markdown_path\": \""
        << json_escape(config.markdown_path) << "\",\n";
    out << "      \"command\": \"" << json_escape(config.command) << "\"\n";
    out << "    },\n";
    out << "    \"environment_snapshot\": ";
    out << runtime_snapshot_json() << ",\n";
    out << "    \"limitations\": [\n";
    out << "      \"Bundle metadata is a packaging snapshot, not a new measurement.\",\n";
    out << "      \"Review or redact reports before sharing outside the trusted team.\"\n";
    out << "    ]\n";
    out << "  }\n";
    out << "}\n";
    return out.str();
}

static std::string build_readme(
    const vis_report_bundle_config_t& config,
    const vis_report_bundle_result_t& result
) {
    std::ostringstream out;
    out << "# VIS Report Bundle\n\n";
    out << "- Report type: " << result.report_type << "\n";
    out << "- Schema: " << result.schema_version << "\n";
    out << "- Generator: " << result.generator << "\n";
    if (!config.command.empty()) {
        out << "- Command: `" << config.command << "`\n";
    }
    out << "\n## Files\n";
    out << "- `report.json`: machine-readable VIS report\n";
    if (!config.markdown_path.empty()) {
        out << "- `report.md`: AI-readable/human-readable context\n";
    }
    out << "- `manifest.json`: bundle metadata and environment snapshot\n";
    out << "- `README.md`: this file\n";
    out << "\n## Sharing Note\n";
    out << "Review or redact reports before sharing outside the trusted team. "
        << "This bundle does not prove that redaction has been applied.\n";
    return out.str();
}

bool vis_report_bundle_write(const vis_report_bundle_config_t& config,
                             vis_report_bundle_result_t* result) {
    if (result == nullptr) return false;
    *result = {};
    result->output_dir = config.output_dir;

    if (config.json_path.empty()) {
        result->error = "missing JSON report path";
        return false;
    }
    if (!ensure_dir(config.output_dir, &result->error)) {
        return false;
    }

    std::string json;
    if (!read_file(config.json_path, &json, &result->error)) {
        return false;
    }

    vis_report_validation_result_t validation;
    if (!vis_report_validate_json(json, &validation)) {
        result->error = "input JSON report failed validation";
        if (!validation.errors.empty()) {
            result->error += ": " + validation.errors.front();
        }
        return false;
    }
    result->report_type = validation.report_type;
    result->schema_version = validation.schema_version;
    result->generator = validation.generator;

    result->report_json_path = path_join(config.output_dir, "report.json");
    result->report_markdown_path =
        config.markdown_path.empty()
            ? ""
            : path_join(config.output_dir, "report.md");
    result->manifest_path = path_join(config.output_dir, "manifest.json");
    result->readme_path = path_join(config.output_dir, "README.md");

    if (!write_file(result->report_json_path, json, &result->error)) {
        return false;
    }

    if (!config.markdown_path.empty()) {
        std::string markdown;
        if (!read_file(config.markdown_path, &markdown, &result->error)) {
            return false;
        }
        if (!write_file(result->report_markdown_path,
                        markdown,
                        &result->error)) {
            return false;
        }
    }

    if (!write_file(result->manifest_path,
                    build_manifest(config, *result),
                    &result->error)) {
        return false;
    }
    if (!write_file(result->readme_path,
                    build_readme(config, *result),
                    &result->error)) {
        return false;
    }

    result->created = true;
    return true;
}
