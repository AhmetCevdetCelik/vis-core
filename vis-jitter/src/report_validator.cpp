/**
 * report_validator.cpp
 *
 * Lightweight VIS JSON report validator implementation.
 *
 * License: MIT
 */

#include "../include/report_validator.hpp"

#include "../include/report_schema.hpp"

#include <fstream>
#include <sstream>
#include <vector>

static bool extract_json_string_field(const std::string& json,
                                      const std::string& field,
                                      std::string* value) {
    const std::string needle = "\"" + field + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return false;

    size_t colon = json.find(':', pos + needle.size());
    if (colon == std::string::npos) return false;

    size_t quote = json.find('"', colon + 1);
    if (quote == std::string::npos) return false;

    std::string out;
    bool escaped = false;
    for (size_t i = quote + 1; i < json.size(); i++) {
        const char c = json[i];
        if (escaped) {
            out.push_back(c);
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '"') {
            if (value) *value = out;
            return true;
        }
        out.push_back(c);
    }
    return false;
}

static bool find_json_field_colon(const std::string& json,
                                  const std::string& field,
                                  size_t* colon_out) {
    const std::string needle = "\"" + field + "\"";
    size_t pos = 0;
    while ((pos = json.find(needle, pos)) != std::string::npos) {
        size_t after = pos + needle.size();
        size_t colon = json.find_first_not_of(" \t\r\n", after);
        if (colon != std::string::npos && json[colon] == ':') {
            if (colon_out) *colon_out = colon;
            return true;
        }
        pos = after;
    }
    return false;
}

static bool json_bool_field_is_true(const std::string& json,
                                    const std::string& field) {
    size_t colon = 0;
    if (!find_json_field_colon(json, field, &colon)) return false;

    size_t value = json.find_first_not_of(" \t\r\n", colon + 1);
    if (value == std::string::npos) return false;
    return json.compare(value, 4, "true") == 0;
}

static bool json_has_field(const std::string& json,
                           const std::string& field) {
    return find_json_field_colon(json, field, nullptr);
}

static std::string detect_report_type(const std::string& json) {
    static const char* kTypes[] = {
        "vis_report",
        "vis_doctor_report",
        "vis_run_attestation",
        "vis_compare_report",
        "vis_mem_probe_report",
        "vis_mem_compare_report",
        "vis_mem_run_attestation",
        "vis_mem_compare_run_report",
        "vis_infer_llama_report",
        "vis_cpu_policy_bundle",
        "vis_mem_policy_bundle",
    };

    std::vector<std::string> top_level_keys;
    bool in_string = false;
    bool escaped = false;
    int depth = 0;

    for (size_t i = 0; i < json.size(); i++) {
        const char c = json[i];
        if (in_string) {
            if (escaped) {
                escaped = false;
                continue;
            }
            if (c == '\\') {
                escaped = true;
                continue;
            }
            if (c == '"') {
                in_string = false;
            }
            continue;
        }

        if (c == '"') {
            if (depth == 1) {
                std::string key;
                bool key_escaped = false;
                size_t end = i + 1;
                for (; end < json.size(); end++) {
                    const char kc = json[end];
                    if (key_escaped) {
                        key.push_back(kc);
                        key_escaped = false;
                        continue;
                    }
                    if (kc == '\\') {
                        key_escaped = true;
                        continue;
                    }
                    if (kc == '"') break;
                    key.push_back(kc);
                }
                if (end >= json.size()) break;

                size_t after = json.find_first_not_of(" \t\r\n", end + 1);
                if (after != std::string::npos && json[after] == ':') {
                    top_level_keys.push_back(key);
                }
                i = end;
                continue;
            }
            in_string = true;
            continue;
        }

        if (c == '{' || c == '[') {
            depth++;
        } else if (c == '}' || c == ']') {
            depth--;
            if (depth < 0) depth = 0;
        }
    }

    std::string detected;
    for (const char* type : kTypes) {
        for (const std::string& key : top_level_keys) {
            if (key != type) continue;
            if (!detected.empty()) return "ambiguous";
            detected = type;
        }
    }
    return detected;
}

static bool valid_schema_for_type(const std::string& type,
                                  const std::string& schema_version) {
    if (type == "vis_report") {
        return schema_version == VIS_CORE_REPORT_SCHEMA_VERSION;
    }
    return schema_version == VIS_REPORT_SCHEMA_VERSION;
}

static bool is_policy_bundle_type(const std::string& type) {
    return type == "vis_cpu_policy_bundle" ||
           type == "vis_mem_policy_bundle";
}

bool vis_policy_evidence_level_is_valid(const std::string& level) {
    return level == "advisory" ||
           level == "attested" ||
           level == "production_controlled";
}

std::string vis_policy_evidence_level_semantics(const std::string& level) {
    if (level == "advisory") {
        return "Evidence is available, but runtime placement or memory policy "
               "application was not attested.";
    }
    if (level == "attested") {
        return "Policy application was verified with runtime evidence such as "
               "/proc status or smaps, but production containment is not "
               "claimed.";
    }
    if (level == "production_controlled") {
        return "Policy was applied through production-style containment such "
               "as cgroup/cpuset with cleanup and escape detection.";
    }
    return "Unknown policy evidence level.";
}

static void require_field(const std::string& json,
                          const std::string& field,
                          vis_report_validation_result_t* result) {
    if (!json_has_field(json, field)) {
        result->errors.push_back("missing required field: " + field);
    }
}

static void validate_policy_bundle_semantics(
    const std::string& json,
    vis_report_validation_result_t* result
) {
    require_field(json, "schema_version", result);
    require_field(json, "generator", result);
    require_field(json, "evidence_level", result);
    require_field(json, "confidence", result);

    if (result->report_type == "vis_cpu_policy_bundle") {
        require_field(json, "control_level", result);
        require_field(json, "online_cpus", result);
        require_field(json, "recommended_thread_counts", result);
        require_field(json, "recommended_cpu_sets", result);
        require_field(json, "excluded_cpus", result);
        require_field(json, "cpu_topology", result);
    } else if (result->report_type == "vis_mem_policy_bundle") {
        require_field(json, "control_level", result);
        require_field(json, "recommended_memory_policies", result);
        require_field(json, "unsupported_policies", result);
        require_field(json, "verification_confidence", result);
    }

    if (!extract_json_string_field(json,
                                   "evidence_level",
                                   &result->evidence_level)) {
        return;
    }
    if (!vis_policy_evidence_level_is_valid(result->evidence_level)) {
        result->errors.push_back(
            "invalid policy evidence_level: " + result->evidence_level);
        return;
    }

    extract_json_string_field(json, "control_level", &result->control_level);
    if (!result->control_level.empty() &&
        !vis_policy_evidence_level_is_valid(result->control_level)) {
        result->errors.push_back(
            "invalid policy control_level: " + result->control_level);
    }

    const bool attested = json_bool_field_is_true(json, "attested");
    const bool production_controlled =
        json_bool_field_is_true(json, "production_controlled");

    if (result->evidence_level == "advisory") {
        if (attested || production_controlled) {
            result->errors.push_back(
                "advisory policy bundles must not claim attested or "
                "production_controlled runtime evidence");
        }
        result->warnings.push_back(
            "policy evidence is advisory; use as candidate selection evidence "
            "only");
    } else if (result->evidence_level == "attested") {
        if (!attested) {
            result->errors.push_back(
                "attested policy bundles must set attested: true");
        }
        if (production_controlled) {
            result->warnings.push_back(
                "production_controlled is true while evidence_level is only "
                "attested");
        }
    } else if (result->evidence_level == "production_controlled") {
        if (!attested || !production_controlled) {
            result->errors.push_back(
                "production_controlled policy bundles must set attested: true "
                "and production_controlled: true");
        }
    }
}

bool vis_report_validate_json(const std::string& json,
                              vis_report_validation_result_t* result) {
    if (result == nullptr) return false;
    *result = {};

    if (json.empty()) {
        result->errors.push_back("input JSON is empty");
        return false;
    }

    result->report_type = detect_report_type(json);
    if (result->report_type.empty()) {
        result->errors.push_back("no known VIS report root object found");
    } else if (result->report_type == "ambiguous") {
        result->errors.push_back("multiple VIS report root objects found");
    }

    if (!extract_json_string_field(json,
                                   "schema_version",
                                   &result->schema_version)) {
        result->errors.push_back("missing string field: schema_version");
    }

    if (!extract_json_string_field(json, "generator", &result->generator)) {
        result->errors.push_back("missing string field: generator");
    }

    if (result->errors.empty() &&
        !valid_schema_for_type(result->report_type,
                               result->schema_version)) {
        result->errors.push_back(
            "unsupported schema_version for report type");
    }

    if (result->errors.empty() && is_policy_bundle_type(result->report_type)) {
        validate_policy_bundle_semantics(json, result);
    }

    if (result->errors.empty() && result->generator.empty()) {
        result->warnings.push_back("generator field is empty");
    }

    result->valid = result->errors.empty();
    return result->valid;
}

bool vis_report_validate_file(const char* path,
                              vis_report_validation_result_t* result,
                              std::string* error) {
    if (path == nullptr || path[0] == '\0') {
        if (error) *error = "missing report path";
        return false;
    }

    std::ifstream input(path);
    if (!input) {
        if (error) *error = "failed to open report file";
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (!input.good() && !input.eof()) {
        if (error) *error = "failed to read report file";
        return false;
    }

    return vis_report_validate_json(buffer.str(), result);
}
