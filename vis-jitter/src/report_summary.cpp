/**
 * report_summary.cpp
 *
 * Human-readable VIS executive summary generator implementation.
 *
 * License: MIT
 */

#include "../include/report_summary.hpp"

#include "../include/report_validator.hpp"

#include <fstream>
#include <sstream>
#include <string>

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

static std::string field_or_unknown(const std::string& json,
                                    const std::string& field) {
    std::string value;
    if (extract_json_string_field(json, field, &value) && !value.empty()) {
        return value;
    }
    return "unknown";
}

static bool json_array_field_has_entries(const std::string& json,
                                         const std::string& field) {
    const std::string needle = "\"" + field + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return false;

    size_t colon = json.find(':', pos + needle.size());
    if (colon == std::string::npos) return false;

    size_t open = json.find('[', colon + 1);
    if (open == std::string::npos) return false;

    for (size_t i = open + 1; i < json.size(); i++) {
        const char c = json[i];
        if (c == ']') return false;
        if (c != ' ' && c != '\n' && c != '\r' && c != '\t') return true;
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
    if (path.empty()) {
        if (error) *error = "missing output path";
        return false;
    }

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

static std::string describe_report_type(const std::string& type) {
    if (type == "vis_report") return "Core CPU jitter report";
    if (type == "vis_probe_report") return "Portable VIS probe report";
    if (type == "vis_doctor_report") return "VIS Doctor system diagnosis";
    if (type == "vis_run_attestation") return "VIS CPU runtime attestation";
    if (type == "vis_compare_report") return "VIS CPU profile comparison";
    if (type == "vis_mem_probe_report") return "VIS-Mem probe report";
    if (type == "vis_mem_compare_report") return "VIS-Mem profile comparison";
    if (type == "vis_mem_run_attestation") {
        return "VIS-Mem runtime attestation";
    }
    if (type == "vis_mem_compare_run_report") {
        return "VIS-Mem workload profile comparison";
    }
    return "VIS report";
}

static void append_metadata(std::ostringstream& out,
                            const vis_report_validation_result_t& validation,
                            const std::string& json) {
    out << "## Report\n";
    out << "- Type: " << describe_report_type(validation.report_type)
        << " (`" << validation.report_type << "`)\n";
    out << "- Schema: " << validation.schema_version << "\n";
    out << "- Generator: " << validation.generator << "\n";

    const std::string evidence_level =
        field_or_unknown(json, "evidence_level");
    if (evidence_level != "unknown") {
        out << "- Evidence level: " << evidence_level << "\n";
    }
    const std::string timer_evidence_level =
        field_or_unknown(json, "timer_evidence_level");
    if (timer_evidence_level != "unknown") {
        out << "- Timer evidence: " << timer_evidence_level << "\n";
    }
    const std::string execution_evidence_level =
        field_or_unknown(json, "execution_evidence_level");
    if (execution_evidence_level != "unknown") {
        out << "- Execution evidence: " << execution_evidence_level << "\n";
    }
    const std::string target_profile_family =
        field_or_unknown(json, "target_profile_family");
    if (target_profile_family != "unknown") {
        out << "- Target contract: " << target_profile_family << "\n";
    }
    const std::string hosted_evidence_state =
        field_or_unknown(json, "hosted_evidence_state");
    if (hosted_evidence_state != "unknown") {
        out << "- Hosted evidence gate: " << hosted_evidence_state << "\n";
    }
    const std::string target_timer_claim_state =
        field_or_unknown(json, "target_timer_claim_state");
    if (target_timer_claim_state != "unknown") {
        out << "- Target timer gate: " << target_timer_claim_state << "\n";
    }
    const std::string target_execution_claim_state =
        field_or_unknown(json, "target_execution_claim_state");
    if (target_execution_claim_state != "unknown") {
        out << "- Target execution gate: " << target_execution_claim_state
            << "\n";
    }

    const std::string confidence_level =
        field_or_unknown(json, "confidence_level");
    if (confidence_level != "unknown") {
        out << "- Confidence: " << confidence_level << "\n";
    }
    out << "\n";
}

static void append_what_changed(std::ostringstream& out,
                                const vis_report_validation_result_t& v,
                                const std::string& json) {
    out << "## What Changed\n";

    if (v.report_type == "vis_doctor_report") {
        out << "- VIS Doctor captured machine/runtime evidence and produced "
            << "advisory findings for the current environment.\n";
        if (json_array_field_has_entries(json, "scan") ||
            json_array_field_has_entries(json, "scan_results")) {
            out << "- The report includes scan evidence that can support CPU "
                << "candidate ranking.\n";
        } else {
            out << "- This appears to be inspect-oriented evidence; CPU "
                << "candidate ranking may require a scan run.\n";
        }
        return;
    }

    if (v.report_type == "vis_probe_report") {
        const std::string backend =
            field_or_unknown(json, "selected_backend");
        const std::string backend_status =
            field_or_unknown(json, "backend_status");
        const std::string source =
            field_or_unknown(json, "selected_time_source");
        const std::string environment =
            field_or_unknown(json, "execution_environment");
        const std::string target_api_status =
            field_or_unknown(json, "target_runtime_api_status");
        const std::string gate_reason =
            field_or_unknown(json, "gate_reason");
        out << "- VIS Probe captured portable timing-source and execution "
            << "surface evidence without assuming Linux/x86 jitter access.\n";
        if (backend != "unknown") {
            out << "- Selected backend: `" << backend << "`.\n";
        }
        if (backend_status != "unknown") {
            out << "- Backend status: `" << backend_status << "`.\n";
        }
        if (source != "unknown") {
            out << "- Selected time source: `" << source << "`.\n";
        }
        if (environment != "unknown") {
            out << "- Execution environment: `" << environment << "`.\n";
        }
        if (target_api_status != "unknown") {
            out << "- Target runtime API status: `" << target_api_status
                << "`.\n";
        }
        if (gate_reason != "unknown") {
            out << "- Claim gate reason: " << gate_reason << "\n";
        }
        return;
    }

    if (v.report_type == "vis_mem_compare_run_report") {
        const std::string profile =
            field_or_unknown(json, "recommended_first_profile");
        const std::string reason = field_or_unknown(json, "recommendation_reason");
        const std::string metric_key = field_or_unknown(json, "metric_key");
        out << "- VIS ran the same workload under multiple memory policy "
            << "profiles and compared the observed runtime evidence.\n";
        if (profile != "unknown") {
            out << "- Recommended first profile: `" << profile << "`.\n";
        }
        if (metric_key != "unknown") {
            out << "- Workload metric key used for ranking: `" << metric_key
                << "`.\n";
        }
        if (reason != "unknown") {
            out << "- Recommendation reason: " << reason << "\n";
        }
        return;
    }

    if (v.report_type == "vis_mem_compare_report") {
        const std::string profile =
            field_or_unknown(json, "recommended_first_profile");
        out << "- VIS-Mem compared probe-local memory policy profiles.\n";
        if (profile != "unknown") {
            out << "- Recommended first probe profile: `" << profile << "`.\n";
        }
        return;
    }

    if (v.report_type == "vis_mem_probe_report") {
        const std::string verdict = field_or_unknown(json, "verdict");
        out << "- VIS-Mem measured page touch behavior, page faults, and "
            << "optional memory policy evidence for one probe profile.\n";
        if (verdict != "unknown") {
            out << "- Probe verdict: `" << verdict << "`.\n";
        }
        return;
    }

    if (v.report_type == "vis_mem_run_attestation") {
        const std::string verdict = field_or_unknown(json, "verdict");
        out << "- VIS-Mem prepared a workload-scoped memory policy envelope "
            << "and kept it alive while the workload ran.\n";
        if (verdict != "unknown") {
            out << "- Runtime verdict: `" << verdict << "`.\n";
        }
        return;
    }

    if (v.report_type == "vis_compare_report") {
        out << "- VIS compared CPU runtime profiles for the same workload and "
            << "captured side-by-side evidence.\n";
        return;
    }

    if (v.report_type == "vis_run_attestation") {
        const std::string verdict = field_or_unknown(json, "verdict");
        out << "- VIS applied a temporary CPU runtime policy and emitted an "
            << "attestation for the workload run.\n";
        if (verdict != "unknown") {
            out << "- Runtime verdict: `" << verdict << "`.\n";
        }
        return;
    }

    out << "- VIS captured runtime evidence for later review.\n";
}

static void append_not_proven(std::ostringstream& out,
                              const vis_report_validation_result_t& v) {
    out << "\n## What This Does Not Prove\n";
    out << "- This summary is advisory evidence, not a certification or "
        << "production guarantee.\n";
    out << "- One report does not prove long-term stability across kernel, "
        << "BIOS, power, thermal, or workload changes.\n";

    if (v.report_type == "vis_mem_compare_report") {
        out << "- Probe-local memory results do not prove the same improvement "
            << "inside the real workload allocator.\n";
    } else if (v.report_type == "vis_probe_report") {
        out << "- Probe evidence does not prove WCET, temporal isolation, or "
            << "certified RTOS behavior.\n";
    } else if (v.report_type == "vis_mem_compare_run_report") {
        out << "- Sequential workload comparisons can be affected by ordering, "
            << "cache warmth, and background activity.\n";
    } else if (v.report_type == "vis_mem_run_attestation") {
        out << "- The memory envelope does not redirect the target "
            << "application's allocator by itself.\n";
    } else if (v.report_type == "vis_doctor_report") {
        out << "- Inspection evidence alone does not prove that a selected CPU "
            << "is best for a real workload.\n";
    } else {
        out << "- Reported evidence should be rechecked with repeated runs "
            << "before it drives a critical policy.\n";
    }
}

static void append_next_step(std::ostringstream& out,
                             const vis_report_validation_result_t& v) {
    out << "\n## Recommended Next Step\n";

    if (v.report_type == "vis_doctor_report") {
        out << "- Run or repeat a workload-specific scan, then validate the "
            << "recommended CPU policy with a workload audit/comparison "
            << "workflow before making a control claim.\n";
    } else if (v.report_type == "vis_probe_report") {
        out << "- Add or implement a platform-specific backend for the target "
            << "execution environment before making stronger timing claims.\n";
    } else if (v.report_type == "vis_mem_probe_report") {
        out << "- Compare memory profiles, then test a real workload with "
            << "child memory evidence before promoting a policy.\n";
    } else if (v.report_type == "vis_mem_compare_report") {
        out << "- Use a real workload metric and child memory evidence before "
            << "promoting any memory profile.\n";
    } else if (v.report_type == "vis_mem_run_attestation") {
        out << "- Compare the same workload against default behavior and at "
            << "least one alternate memory profile.\n";
    } else if (v.report_type == "vis_mem_compare_run_report") {
        out << "- Repeat the comparison under controlled conditions and treat "
            << "the winning profile as a candidate, not a permanent rule.\n";
    } else if (v.report_type == "vis_run_attestation") {
        out << "- Compare the same workload against default behavior before "
            << "treating the applied CPU policy as useful.\n";
    } else if (v.report_type == "vis_compare_report") {
        out << "- Repeat the comparison and consider adding a baseline/current "
            << "gate once the metric is stable.\n";
    } else {
        out << "- Create a Doctor or compare report so the evidence can be "
            << "reviewed against an explicit policy decision.\n";
    }
    out << "\n";
}

bool vis_report_summary_generate(const std::string& json,
                                 std::string* markdown,
                                 vis_report_summary_result_t* result) {
    if (markdown == nullptr || result == nullptr) return false;
    *result = {};
    markdown->clear();

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

    std::ostringstream out;
    out << "# VIS Executive Summary\n\n";
    append_metadata(out, validation, json);
    append_what_changed(out, validation, json);
    append_not_proven(out, validation);
    append_next_step(out, validation);

    *markdown = out.str();
    return true;
}

bool vis_report_summary_write(const vis_report_summary_config_t& config,
                              vis_report_summary_result_t* result) {
    if (result == nullptr) return false;
    *result = {};
    result->output_path = config.output_path;

    std::string json;
    if (!read_file(config.json_path, &json, &result->error)) {
        return false;
    }

    std::string markdown;
    if (!vis_report_summary_generate(json, &markdown, result)) {
        return false;
    }

    result->output_path = config.output_path;
    if (!write_file(config.output_path, markdown, &result->error)) {
        return false;
    }

    result->written = true;
    return true;
}
