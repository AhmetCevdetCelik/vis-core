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

#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <vector>

class json_syntax_parser_t {
public:
    static constexpr size_t kMaxNestingDepth = 128;

    explicit json_syntax_parser_t(const std::string& json) : json_(json) {}

    bool parse() {
        skip_whitespace();
        if (!parse_value(0)) return false;
        skip_whitespace();
        return pos_ == json_.size();
    }

    bool parse_string_document(std::string* value) {
        skip_whitespace();
        if (!parse_string(value)) return false;
        skip_whitespace();
        return pos_ == json_.size();
    }

private:
    void skip_whitespace() {
        while (pos_ < json_.size() &&
               (json_[pos_] == ' ' || json_[pos_] == '\t' ||
                json_[pos_] == '\r' || json_[pos_] == '\n')) {
            pos_++;
        }
    }

    bool consume(char expected) {
        if (pos_ >= json_.size() || json_[pos_] != expected) return false;
        pos_++;
        return true;
    }

    bool parse_value(size_t depth) {
        skip_whitespace();
        if (pos_ >= json_.size() || depth > kMaxNestingDepth) return false;
        switch (json_[pos_]) {
            case '{': return parse_object(depth);
            case '[': return parse_array(depth);
            case '"': return parse_string();
            case 't': return parse_literal("true");
            case 'f': return parse_literal("false");
            case 'n': return parse_literal("null");
            default: return parse_number();
        }
    }

    bool parse_object(size_t depth) {
        if (!consume('{')) return false;
        skip_whitespace();
        if (consume('}')) return true;
        std::set<std::string> member_names;
        while (true) {
            std::string member_name;
            if (!parse_string(&member_name) ||
                !member_names.insert(member_name).second) {
                return false;
            }
            skip_whitespace();
            if (!consume(':') || !parse_value(depth + 1)) return false;
            skip_whitespace();
            if (consume('}')) return true;
            if (!consume(',')) return false;
            skip_whitespace();
        }
    }

    bool parse_array(size_t depth) {
        if (!consume('[')) return false;
        skip_whitespace();
        if (consume(']')) return true;
        while (true) {
            if (!parse_value(depth + 1)) return false;
            skip_whitespace();
            if (consume(']')) return true;
            if (!consume(',')) return false;
            skip_whitespace();
        }
    }

    static void append_utf8(unsigned int code_point, std::string* value) {
        if (code_point <= 0x7f) {
            value->push_back(static_cast<char>(code_point));
        } else if (code_point <= 0x7ff) {
            value->push_back(static_cast<char>(0xc0 | (code_point >> 6)));
            value->push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
        } else if (code_point <= 0xffff) {
            value->push_back(static_cast<char>(0xe0 | (code_point >> 12)));
            value->push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
            value->push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
        } else {
            value->push_back(static_cast<char>(0xf0 | (code_point >> 18)));
            value->push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3f)));
            value->push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
            value->push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
        }
    }

    bool parse_hex_escape(unsigned int* value) {
        unsigned int result = 0;
        for (int i = 0; i < 4; i++) {
            if (pos_ >= json_.size() ||
                !std::isxdigit(static_cast<unsigned char>(json_[pos_]))) {
                return false;
            }
            const char c = json_[pos_++];
            result = (result << 4) |
                     static_cast<unsigned int>(
                         c >= '0' && c <= '9' ? c - '0' :
                         c >= 'a' && c <= 'f' ? c - 'a' + 10 : c - 'A' + 10);
        }
        *value = result;
        return true;
    }

    bool parse_utf8_sequence(unsigned char first, std::string* value) {
        size_t continuation_count = 0;
        unsigned int code_point = 0;
        unsigned int minimum = 0;
        if (first >= 0xc2 && first <= 0xdf) {
            continuation_count = 1;
            code_point = first & 0x1f;
            minimum = 0x80;
        } else if (first >= 0xe0 && first <= 0xef) {
            continuation_count = 2;
            code_point = first & 0x0f;
            minimum = 0x800;
        } else if (first >= 0xf0 && first <= 0xf4) {
            continuation_count = 3;
            code_point = first & 0x07;
            minimum = 0x10000;
        } else {
            return false;
        }

        if (pos_ + continuation_count > json_.size()) return false;
        value->push_back(static_cast<char>(first));
        for (size_t i = 0; i < continuation_count; i++) {
            const unsigned char continuation =
                static_cast<unsigned char>(json_[pos_++]);
            if ((continuation & 0xc0) != 0x80) return false;
            code_point = (code_point << 6) | (continuation & 0x3f);
            value->push_back(static_cast<char>(continuation));
        }

        return code_point >= minimum && code_point <= 0x10ffff &&
               !(code_point >= 0xd800 && code_point <= 0xdfff);
    }

    bool parse_string(std::string* value = nullptr) {
        if (!consume('"')) return false;
        std::string decoded;
        while (pos_ < json_.size()) {
            const unsigned char c =
                static_cast<unsigned char>(json_[pos_++]);
            if (c == '"') {
                if (value != nullptr) *value = decoded;
                return true;
            }
            if (c < 0x20) return false;
            if (c != '\\') {
                if (c < 0x80) {
                    decoded.push_back(static_cast<char>(c));
                } else if (!parse_utf8_sequence(c, &decoded)) {
                    return false;
                }
                continue;
            }
            if (pos_ >= json_.size()) return false;
            const char escaped = json_[pos_++];
            if (escaped == 'u') {
                unsigned int code_point = 0;
                if (!parse_hex_escape(&code_point)) return false;
                if (code_point >= 0xd800 && code_point <= 0xdbff) {
                    if (pos_ + 2 > json_.size() || json_[pos_] != '\\' ||
                        json_[pos_ + 1] != 'u') {
                        return false;
                    }
                    pos_ += 2;
                    unsigned int low = 0;
                    if (!parse_hex_escape(&low) || low < 0xdc00 ||
                        low > 0xdfff) {
                        return false;
                    }
                    code_point = 0x10000 + ((code_point - 0xd800) << 10) +
                                 (low - 0xdc00);
                } else if (code_point >= 0xdc00 && code_point <= 0xdfff) {
                    return false;
                }
                append_utf8(code_point, &decoded);
            } else if (escaped != '"' && escaped != '\\' && escaped != '/' &&
                       escaped != 'b' && escaped != 'f' && escaped != 'n' &&
                       escaped != 'r' && escaped != 't') {
                return false;
            } else {
                static const char escaped_values[] = {
                    '"', '\\', '/', '\b', '\f', '\n', '\r', '\t'};
                static const char escaped_names[] = {
                    '"', '\\', '/', 'b', 'f', 'n', 'r', 't'};
                for (size_t i = 0; i < sizeof(escaped_names); i++) {
                    if (escaped == escaped_names[i]) {
                        decoded.push_back(escaped_values[i]);
                        break;
                    }
                }
            }
        }
        return false;
    }

    bool parse_number() {
        const size_t begin = pos_;
        if (pos_ < json_.size() && json_[pos_] == '-') pos_++;
        if (pos_ >= json_.size()) return false;
        if (json_[pos_] == '0') {
            pos_++;
        } else if (json_[pos_] >= '1' && json_[pos_] <= '9') {
            while (pos_ < json_.size() && std::isdigit(
                       static_cast<unsigned char>(json_[pos_]))) {
                pos_++;
            }
        } else {
            return false;
        }
        if (pos_ < json_.size() && json_[pos_] == '.') {
            pos_++;
            const size_t fraction = pos_;
            while (pos_ < json_.size() && std::isdigit(
                       static_cast<unsigned char>(json_[pos_]))) {
                pos_++;
            }
            if (pos_ == fraction) return false;
        }
        if (pos_ < json_.size() &&
            (json_[pos_] == 'e' || json_[pos_] == 'E')) {
            pos_++;
            if (pos_ < json_.size() &&
                (json_[pos_] == '+' || json_[pos_] == '-')) {
                pos_++;
            }
            const size_t exponent = pos_;
            while (pos_ < json_.size() && std::isdigit(
                       static_cast<unsigned char>(json_[pos_]))) {
                pos_++;
            }
            if (pos_ == exponent) return false;
        }
        return pos_ > begin;
    }

    bool parse_literal(const char* literal) {
        const size_t length = std::char_traits<char>::length(literal);
        if (json_.compare(pos_, length, literal) != 0) return false;
        pos_ += length;
        return true;
    }

    const std::string& json_;
    size_t pos_ = 0;
};

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

    bool escaped = false;
    for (size_t i = quote + 1; i < json.size(); i++) {
        const char c = json[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '"') {
            const std::string encoded = json.substr(quote, i - quote + 1);
            return json_syntax_parser_t(encoded).parse_string_document(value);
        }
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

enum class json_value_type_t {
    string,
    object,
    other,
};

struct json_member_t {
    json_value_type_t type = json_value_type_t::other;
    size_t value_begin = 0;
    size_t value_end = 0;
};

static bool find_matching_json_delimiter(const std::string& json,
                                         size_t open,
                                         char opening,
                                         char closing,
                                         size_t* close_out) {
    bool in_string = false;
    bool escaped = false;
    int depth = 0;
    for (size_t i = open; i < json.size(); i++) {
        const char c = json[i];
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
        } else if (c == opening) {
            depth++;
        } else if (c == closing && --depth == 0) {
            if (close_out != nullptr) *close_out = i;
            return true;
        }
    }
    return false;
}

static bool find_direct_json_member(const std::string& json,
                                    size_t object_begin,
                                    size_t object_end,
                                    const std::string& field,
                                    json_member_t* member) {
    size_t i = object_begin + 1;
    while (i < object_end) {
        i = json.find_first_not_of(" \t\r\n,", i);
        if (i == std::string::npos || i >= object_end || json[i] != '"') {
            return false;
        }

        size_t key_end = i + 1;
        bool escaped = false;
        for (; key_end < object_end; key_end++) {
            const char c = json[key_end];
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                break;
            }
        }
        if (key_end >= object_end) return false;
        const std::string encoded_key =
            json.substr(i, key_end - i + 1);
        std::string key;
        if (!json_syntax_parser_t(encoded_key).parse_string_document(&key)) {
            return false;
        }

        size_t colon = json.find_first_not_of(" \t\r\n", key_end + 1);
        if (colon == std::string::npos || colon >= object_end ||
            json[colon] != ':') {
            return false;
        }
        size_t value = json.find_first_not_of(" \t\r\n", colon + 1);
        if (value == std::string::npos || value >= object_end) return false;

        json_member_t current;
        current.value_begin = value;
        if (json[value] == '"') {
            current.type = json_value_type_t::string;
            size_t end = value + 1;
            bool value_escaped = false;
            for (; end < object_end; end++) {
                const char c = json[end];
                if (value_escaped) {
                    value_escaped = false;
                } else if (c == '\\') {
                    value_escaped = true;
                } else if (c == '"') {
                    break;
                }
            }
            if (end >= object_end) return false;
            current.value_end = end;
        } else if (json[value] == '{') {
            current.type = json_value_type_t::object;
            if (!find_matching_json_delimiter(json, value, '{', '}',
                                              &current.value_end) ||
                current.value_end > object_end) {
                return false;
            }
        } else if (json[value] == '[') {
            if (!find_matching_json_delimiter(json, value, '[', ']',
                                              &current.value_end) ||
                current.value_end > object_end) {
                return false;
            }
        } else {
            current.value_end = json.find_first_of(",}", value);
            if (current.value_end == std::string::npos ||
                current.value_end > object_end) {
                return false;
            }
            current.value_end--;
        }

        if (key == field) {
            if (member != nullptr) *member = current;
            return true;
        }
        i = current.value_end + 1;
    }
    return false;
}

static bool extract_direct_json_string(const std::string& json,
                                       size_t object_begin,
                                       size_t object_end,
                                       const std::string& field,
                                       std::string* value) {
    json_member_t member;
    if (!find_direct_json_member(json, object_begin, object_end, field,
                                 &member) ||
        member.type != json_value_type_t::string) {
        return false;
    }
    if (value != nullptr) {
        const std::string encoded =
            json.substr(member.value_begin,
                        member.value_end - member.value_begin + 1);
        if (!json_syntax_parser_t(encoded).parse_string_document(value)) {
            return false;
        }
    }
    return true;
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
                bool key_escaped = false;
                size_t end = i + 1;
                for (; end < json.size(); end++) {
                    const char kc = json[end];
                    if (key_escaped) {
                        key_escaped = false;
                        continue;
                    }
                    if (kc == '\\') {
                        key_escaped = true;
                        continue;
                    }
                    if (kc == '"') break;
                }
                if (end >= json.size()) break;

                size_t after = json.find_first_not_of(" \t\r\n", end + 1);
                if (after != std::string::npos && json[after] == ':') {
                    const std::string encoded_key =
                        json.substr(i, end - i + 1);
                    std::string key;
                    if (json_syntax_parser_t(encoded_key)
                            .parse_string_document(&key)) {
                        top_level_keys.push_back(key);
                    }
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
    size_t document_begin = json.find_first_not_of(" \t\r\n");
    size_t document_end = 0;
    json_member_t report;
    if (document_begin == std::string::npos || json[document_begin] != '{' ||
        !find_matching_json_delimiter(json, document_begin, '{', '}',
                                      &document_end) ||
        !find_direct_json_member(json, document_begin, document_end,
                                 "vis_probe_report", &report) ||
        report.type != json_value_type_t::object) {
        result->errors.push_back("vis_probe_report must be an object");
        return;
    }

    struct required_section_t {
        const char* name;
        const char* const* fields;
        size_t field_count;
    };
    static const char* kPlatformFields[] = {"selected_time_source"};
    static const char* kExecutionFields[] = {"execution_environment"};
    static const char* kProbeResultFields[] = {
        "selected_backend", "backend_status", "timer_evidence_level",
        "execution_evidence_level",
    };
    static const char* kTargetFields[] = {
        "target_profile_family", "target_runtime_api_status",
    };
    static const required_section_t kSections[] = {
        {"platform_profile", kPlatformFields, 1},
        {"execution_profile", kExecutionFields, 1},
        {"probe_result", kProbeResultFields, 4},
        {"target_contract", kTargetFields, 2},
    };

    json_member_t sections[4];
    for (size_t section_index = 0; section_index < 4; section_index++) {
        const required_section_t& required = kSections[section_index];
        if (!find_direct_json_member(json, report.value_begin,
                                     report.value_end, required.name,
                                     &sections[section_index]) ||
            sections[section_index].type != json_value_type_t::object) {
            result->errors.push_back(
                std::string("required section must be an object: ") +
                required.name);
            continue;
        }
        for (size_t field_index = 0; field_index < required.field_count;
             field_index++) {
            if (!extract_direct_json_string(
                    json, sections[section_index].value_begin,
                    sections[section_index].value_end,
                    required.fields[field_index], nullptr)) {
                result->errors.push_back(
                    std::string("missing or non-string required field: ") +
                    required.name + "." + required.fields[field_index]);
            }
        }
    }

    if (!extract_direct_json_string(json, report.value_begin, report.value_end,
                                    "evidence_level",
                                    &result->evidence_level)) {
        result->errors.push_back("missing required field: evidence_level");
        return;
    }
    extract_direct_json_string(json, sections[2].value_begin,
                               sections[2].value_end, "backend_status",
                               &result->backend_status);
    extract_direct_json_string(json, sections[2].value_begin,
                               sections[2].value_end, "timer_evidence_level",
                               &result->timer_evidence_level);
    extract_direct_json_string(json, sections[2].value_begin,
                               sections[2].value_end,
                               "execution_evidence_level",
                               &result->execution_evidence_level);
    extract_direct_json_string(json, sections[3].value_begin,
                               sections[3].value_end, "target_profile_family",
                               &result->target_profile_family);
    extract_direct_json_string(json, sections[3].value_begin,
                               sections[3].value_end,
                               "target_runtime_api_status",
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
    extract_direct_json_string(json, sections[0].value_begin,
                               sections[0].value_end, "selected_time_source",
                               &selected_time_source);
    if (result->evidence_level == "linux_x86_rich_evidence" &&
        selected_time_source != "x86_rdtscp") {
        result->errors.push_back(
            "linux_x86_rich_evidence must rely on x86_rdtscp");
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

    if (!json_syntax_parser_t(json).parse()) {
        result->errors.push_back("input is not valid JSON");
        return false;
    }

    result->report_type = detect_report_type(json);
    if (result->report_type.empty()) {
        result->errors.push_back("no known VIS report root object found");
    } else if (result->report_type == "ambiguous") {
        result->errors.push_back("multiple VIS report root objects found");
    }

    size_t document_begin = json.find_first_not_of(" \t\r\n");
    size_t document_end = 0;
    json_member_t report;
    const bool report_object_found =
        !result->report_type.empty() && result->report_type != "ambiguous" &&
        document_begin != std::string::npos && json[document_begin] == '{' &&
        find_matching_json_delimiter(json, document_begin, '{', '}',
                                     &document_end) &&
        find_direct_json_member(json, document_begin, document_end,
                                result->report_type, &report) &&
        report.type == json_value_type_t::object;

    if (!report_object_found ||
        !extract_direct_json_string(json, report.value_begin, report.value_end,
                                    "schema_version",
                                    &result->schema_version)) {
        result->errors.push_back("missing string field: schema_version");
    }

    if (!report_object_found ||
        !extract_direct_json_string(json, report.value_begin, report.value_end,
                                    "generator", &result->generator)) {
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
