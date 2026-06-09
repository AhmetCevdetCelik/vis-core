/**
 * report_redactor.cpp
 *
 * Lightweight VIS JSON report redaction implementation.
 *
 * License: MIT
 */

#include "../include/report_redactor.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

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

static std::string lowercase_ascii(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char c : value) {
        out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

static bool is_json_key(const std::string& json, size_t string_end_quote) {
    size_t pos = string_end_quote + 1;
    while (pos < json.size() &&
           std::isspace(static_cast<unsigned char>(json[pos]))) {
        pos++;
    }
    return pos < json.size() && json[pos] == ':';
}

static bool read_json_string(const std::string& json,
                             size_t start_quote,
                             std::string* value,
                             size_t* end_quote) {
    if (start_quote >= json.size() || json[start_quote] != '"') return false;
    std::string out;
    bool escaped = false;
    for (size_t i = start_quote + 1; i < json.size(); i++) {
        const char c = json[i];
        if (escaped) {
            switch (c) {
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                default: out.push_back(c); break;
            }
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '"') {
            if (value) *value = out;
            if (end_quote) *end_quote = i;
            return true;
        }
        out.push_back(c);
    }
    return false;
}

static void remember_field(vis_report_redaction_result_t* result,
                           const std::string& field) {
    if (result == nullptr || field.empty()) return;
    for (const auto& existing : result->redacted_fields) {
        if (existing == field) return;
    }
    result->redacted_fields.push_back(field);
}

static bool key_is_path_like(const std::string& key) {
    return key == "path" ||
           (key.size() >= 5 && key.substr(key.size() - 5) == "_path") ||
           key.find("path") != std::string::npos;
}

static bool key_is_sensitive(const std::string& key) {
    return key == "hostname" ||
           key == "username" ||
           key == "user" ||
           key == "metric_file_content" ||
           key == "content" ||
           key == "workload" ||
           key == "workload_argv" ||
           key == "command" ||
           key == "argv" ||
           key == "env" ||
           key == "environment_variables" ||
           key_is_path_like(key);
}

static std::string redact_home_paths(const std::string& value,
                                     bool* changed) {
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size();) {
        if (value.compare(i, 6, "/home/") == 0) {
            size_t end = i + 6;
            while (end < value.size() &&
                   value[end] != '/' &&
                   !std::isspace(static_cast<unsigned char>(value[end])) &&
                   value[end] != '"' &&
                   value[end] != '\'') {
                end++;
            }
            out += "/home/<redacted-user>";
            i = end;
            if (changed) *changed = true;
            continue;
        }
        out.push_back(value[i++]);
    }
    return out;
}

static std::string redacted_value_for_key(const std::string& key,
                                          const std::string& value,
                                          bool* changed) {
    const std::string lower_key = lowercase_ascii(key);
    if (changed) *changed = false;

    if (lower_key == "hostname") {
        if (changed && value != "<redacted-hostname>") *changed = true;
        return "<redacted-hostname>";
    }
    if (lower_key == "username" || lower_key == "user") {
        if (changed && value != "<redacted-user>") *changed = true;
        return "<redacted-user>";
    }
    if (key_is_path_like(lower_key)) {
        if (changed && value != "<redacted-path>") *changed = true;
        return "<redacted-path>";
    }
    if (lower_key == "metric_file_content" || lower_key == "content") {
        if (changed && value != "<redacted-content>") *changed = true;
        return "<redacted-content>";
    }
    if (lower_key == "workload" ||
        lower_key == "workload_argv" ||
        lower_key == "command" ||
        lower_key == "argv") {
        if (changed && value != "<redacted-command>") *changed = true;
        return "<redacted-command>";
    }
    if (lower_key == "env" || lower_key == "environment_variables") {
        if (changed && value != "<redacted-env>") *changed = true;
        return "<redacted-env>";
    }

    return redact_home_paths(value, changed);
}

bool vis_report_redact_json(const std::string& json,
                            vis_report_redaction_result_t* result,
                            std::string* error) {
    if (result == nullptr) return false;
    *result = {};
    if (json.empty()) {
        if (error) *error = "input JSON is empty";
        return false;
    }

    std::string current_key;
    std::ostringstream out;
    for (size_t i = 0; i < json.size();) {
        if (json[i] != '"') {
            out << json[i++];
            continue;
        }

        std::string value;
        size_t end_quote = 0;
        if (!read_json_string(json, i, &value, &end_quote)) {
            if (error) *error = "invalid JSON string";
            return false;
        }

        if (is_json_key(json, end_quote)) {
            current_key = value;
            out << "\"" << json_escape(value) << "\"";
        } else {
            bool changed = false;
            std::string redacted = redacted_value_for_key(
                current_key,
                value,
                &changed
            );
            if (changed || key_is_sensitive(lowercase_ascii(current_key))) {
                result->replacements++;
                remember_field(result, current_key);
            }
            out << "\"" << json_escape(redacted) << "\"";
        }

        i = end_quote + 1;
    }

    result->redacted_json = out.str();
    return true;
}

bool vis_report_redact_file(const char* input_path,
                            const char* output_path,
                            vis_report_redaction_result_t* result,
                            std::string* error) {
    if (input_path == nullptr || input_path[0] == '\0') {
        if (error) *error = "missing input path";
        return false;
    }
    if (output_path == nullptr || output_path[0] == '\0') {
        if (error) *error = "missing output path";
        return false;
    }

    std::ifstream input(input_path);
    if (!input) {
        if (error) *error = "failed to open input report";
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (!input.good() && !input.eof()) {
        if (error) *error = "failed to read input report";
        return false;
    }

    vis_report_redaction_result_t local;
    if (!vis_report_redact_json(buffer.str(), &local, error)) {
        return false;
    }

    std::ofstream output(output_path);
    if (!output) {
        if (error) *error = "failed to open output report";
        return false;
    }
    output << local.redacted_json;
    if (!output) {
        if (error) *error = "failed to write output report";
        return false;
    }

    if (result) *result = local;
    return true;
}
