/**
 * report_validator.cpp
 *
 * Lightweight VIS JSON report validator implementation.
 *
 * License: MIT
 */

#include "../include/report_validator.hpp"

#include "../include/report_schema.hpp"
#include "../include/vis_probe_semantics.hpp"

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

static bool extract_root_level_string_field(const std::string& json,
                                            const std::string& root,
                                            const std::string& field,
                                            std::string* value) {
    const std::string root_needle = "\"" + root + "\"";
    size_t root_pos = json.find(root_needle);
    if (root_pos == std::string::npos) return false;

    size_t root_colon = json.find(':', root_pos + root_needle.size());
    if (root_colon == std::string::npos) return false;
    size_t root_open = json.find('{', root_colon + 1);
    if (root_open == std::string::npos) return false;

    bool in_string = false;
    bool escaped = false;
    int depth = 1;
    for (size_t i = root_open + 1; i < json.size(); i++) {
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
                if (end >= json.size()) return false;
                size_t after = json.find_first_not_of(" \t\r\n", end + 1);
                if (after != std::string::npos && json[after] == ':' &&
                    key == field) {
                    size_t quote = json.find('"', after + 1);
                    if (quote == std::string::npos) return false;
                    std::string out;
                    bool value_escaped = false;
                    for (size_t v = quote + 1; v < json.size(); v++) {
                        const char vc = json[v];
                        if (value_escaped) {
                            out.push_back(vc);
                            value_escaped = false;
                            continue;
                        }
                        if (vc == '\\') {
                            value_escaped = true;
                            continue;
                        }
                        if (vc == '"') {
                            if (value != nullptr) *value = out;
                            return true;
                        }
                        out.push_back(vc);
                    }
                    return false;
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
            if (depth == 0) return false;
        }
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
        "vis_probe_report",
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

static void require_field(const std::string& json,
                          const std::string& field,
                          vis_report_validation_result_t* result);

static void validate_probe_report_semantics(
    const std::string& json,
    vis_report_validation_result_t* result
) {
    if (!json_has_field(json, "platform_profile")) {
        result->errors.push_back("missing required field: platform_profile");
    }
    if (!json_has_field(json, "execution_profile")) {
        result->errors.push_back("missing required field: execution_profile");
    }
    if (!json_has_field(json, "probe_result")) {
        result->errors.push_back("missing required field: probe_result");
    }
    if (!json_has_field(json, "target_contract")) {
        result->errors.push_back("missing required field: target_contract");
    }
    if (!extract_root_level_string_field(json, "vis_probe_report",
                                         "evidence_level",
                                         &result->evidence_level)) {
        result->errors.push_back("missing required field: evidence_level");
        return;
    }
    extract_json_string_field(json, "backend_status", &result->backend_status);
    extract_json_string_field(json, "timer_evidence_level",
                              &result->timer_evidence_level);
    extract_json_string_field(json, "execution_evidence_level",
                              &result->execution_evidence_level);
    extract_json_string_field(json, "target_profile_family",
                              &result->target_profile_family);
    extract_json_string_field(json, "target_runtime_api_status",
                              &result->target_runtime_api_status);

    if (!vis_probe_evidence_level_is_valid(result->evidence_level)) {
        result->errors.push_back("invalid probe evidence_level: " +
                                 result->evidence_level);
        return;
    }
    if (!result->backend_status.empty() &&
        !vis_probe_backend_status_is_valid(result->backend_status)) {
        result->errors.push_back("invalid probe backend_status: " +
                                 result->backend_status);
    }
    if (!result->timer_evidence_level.empty() &&
        !vis_probe_timer_evidence_level_is_valid(
            result->timer_evidence_level)) {
        result->errors.push_back("invalid timer_evidence_level: " +
                                 result->timer_evidence_level);
    }
    if (!result->execution_evidence_level.empty() &&
        !vis_probe_execution_evidence_level_is_valid(
            result->execution_evidence_level)) {
        result->errors.push_back("invalid execution_evidence_level: " +
                                 result->execution_evidence_level);
    }
    if (!result->target_runtime_api_status.empty() &&
        !vis_probe_target_runtime_api_status_is_valid(
            result->target_runtime_api_status)) {
        result->errors.push_back("invalid target_runtime_api_status: " +
                                 result->target_runtime_api_status);
    }

    std::string selected_time_source;
    extract_json_string_field(json, "selected_time_source",
                              &selected_time_source);
    if (result->evidence_level == "linux_x86_rich_evidence" &&
        selected_time_source == "posix_clock_monotonic") {
        result->errors.push_back(
            "linux_x86_rich_evidence cannot rely on posix_clock_monotonic");
    }
    if (result->evidence_level == "arm_generic_timer_evidence" &&
        selected_time_source != "arm_cntvct_el0") {
        result->errors.push_back(
            "arm_generic_timer_evidence must rely on arm_cntvct_el0");
    }
    if (result->evidence_level == "contract_only" &&
        result->backend_status == "selected") {
        result->errors.push_back(
            "contract_only probe evidence cannot claim backend_status selected");
    }
    if (result->target_runtime_api_status == "host_native" &&
        result->backend_status == "recognized_api_missing") {
        result->errors.push_back(
            "host_native target runtime cannot claim recognized_api_missing");
    }
    if (result->execution_evidence_level == "rtos_execution_surface" &&
        result->target_runtime_api_status != "host_native") {
        result->warnings.push_back(
            "rtos_execution_surface should usually require a host_native "
            "target runtime API status");
    }
    if ((result->evidence_level == "linux_x86_rich_evidence" ||
         result->evidence_level == "arm_generic_timer_evidence" ||
         result->evidence_level == "portable_user_space") &&
        !result->backend_status.empty() &&
        result->backend_status != "selected") {
        result->warnings.push_back(
            "runtime evidence level is strong, but backend_status is not "
            "selected");
    }
}

bool vis_policy_evidence_level_is_valid(const std::string& level) {
    return level == "advisory" ||
           level == "attested" ||
           level == "production_controlled";
}

bool vis_probe_evidence_level_is_valid(const std::string& level) {
    return level == "portable_user_space" ||
           level == "linux_x86_rich_evidence" ||
           level == "arm_generic_timer_evidence" ||
           level == "contract_only" ||
           level == "rtos_execution_surface" ||
           level == "hypervisor_partition_hint";
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

std::string vis_probe_evidence_level_semantics(const std::string& level) {
    if (level == "portable_user_space") {
        return "Probe used portable user-space timing and execution evidence "
               "without architecture-specific privileged counters.";
    }
    if (level == "linux_x86_rich_evidence") {
        return "Probe selected Linux/x86 timing evidence such as RDTSCP "
               "surfaces while keeping jitter attestation out of scope.";
    }
    if (level == "arm_generic_timer_evidence") {
        return "Probe selected ARM generic timer evidence while leaving RTOS "
               "partition scheduling and interrupt attestation out of scope.";
    }
    if (level == "contract_only") {
        return "Probe recognized a target backend contract, but the runtime "
               "API needed for target-specific attestation was unavailable.";
    }
    if (level == "rtos_execution_surface") {
        return "Probe recorded target execution-surface evidence, but "
               "timing/isolation claims still depend on the target backend.";
    }
    if (level == "hypervisor_partition_hint") {
        return "Probe observed hypervisor or partition-management surfaces, "
               "but did not prove partition isolation.";
    }
    return "Unknown probe evidence level.";
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
    } else if (result->errors.empty() &&
               result->report_type == "vis_probe_report") {
        validate_probe_report_semantics(json, result);
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
