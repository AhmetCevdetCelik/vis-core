/**
 * test_report_validator.cpp
 *
 * Rootless tests for VIS report metadata validation.
 *
 * License: MIT
 */

#include "../include/report_validator.hpp"
#include "../include/vis_probe_semantics.hpp"

#include <cstdio>
#include <fstream>
#include <string>

int main() {
    const std::string valid =
        "{\n"
        "  \"vis_mem_probe_report\": {\n"
        "    \"schema_version\": \"0.1\",\n"
        "    \"generator\": \"vis-mem-probe 0.1.0\"\n"
        "  }\n"
        "}\n";

    vis_report_validation_result_t result;
    if (!vis_report_validate_json(valid, &result) ||
        !result.valid ||
        result.report_type != "vis_mem_probe_report" ||
        result.schema_version != "0.1" ||
        result.generator != "vis-mem-probe 0.1.0") {
        std::fprintf(stderr, "[test] valid report was rejected\n");
        return 1;
    }

    std::string valid_utf8 =
        "{\"vis_mem_probe_report\":{\"schema_version\":\"0.1\","
        "\"generator\":\"caf";
    valid_utf8.append("\xc3\xa9 \xf0\x9f\x98\x80", 7);
    valid_utf8 += "\"}}";
    if (!vis_report_validate_json(valid_utf8, &result)) {
        std::fprintf(stderr, "[test] valid UTF-8 was rejected\n");
        return 1;
    }

    const std::string invalid_utf8_sequences[] = {
        std::string("\xff", 1),
        std::string("\x80", 1),
        std::string("\xc0\xaf", 2),
        std::string("\xe2\x28\xa1", 3),
        std::string("\xed\xa0\x80", 3),
        std::string("\xf4\x90\x80\x80", 4),
    };
    for (const std::string& sequence : invalid_utf8_sequences) {
        std::string invalid_utf8 =
            "{\"vis_mem_probe_report\":{\"schema_version\":\"0.1\","
            "\"generator\":\"bad";
        invalid_utf8 += sequence;
        invalid_utf8 += "\"}}";
        if (vis_report_validate_json(invalid_utf8, &result) ||
            result.errors.empty()) {
            std::fprintf(stderr, "[test] invalid UTF-8 was accepted\n");
            return 1;
        }
    }

    const std::string escaped_member_names =
        "{\"vis\\u005fmem_probe_report\":{"
        "\"schema\\u005fversion\":\"0.1\","
        "\"generator\":\"vis-mem-probe 0.1.0\"}}";
    if (!vis_report_validate_json(escaped_member_names, &result) ||
        result.report_type != "vis_mem_probe_report" ||
        result.schema_version != "0.1") {
        std::fprintf(stderr, "[test] escaped member names were rejected\n");
        return 1;
    }

    const std::string old_core =
        "{\n"
        "  \"vis_report\": {\n"
        "    \"schema_version\": \"1.0\",\n"
        "    \"generator\": \"vis-jitter 1.0.0\"\n"
        "  }\n"
        "}\n";
    if (!vis_report_validate_json(old_core, &result) ||
        result.report_type != "vis_report" ||
        result.schema_version != "1.0") {
        std::fprintf(stderr, "[test] core report schema was rejected\n");
        return 1;
    }

    const std::string infer =
        "{\n"
        "  \"vis_infer_llama_report\": {\n"
        "    \"schema_version\": \"0.1\",\n"
        "    \"generator\": \"vis-infer-llama 0.1.0\"\n"
        "  }\n"
        "}\n";
    if (!vis_report_validate_json(infer, &result) ||
        result.report_type != "vis_infer_llama_report") {
        std::fprintf(stderr, "[test] VIS-Infer report was rejected\n");
        return 1;
    }

    const std::string probe =
        "{\n"
        "  \"vis_probe_report\": {\n"
        "    \"schema_version\": \"0.1\",\n"
        "    \"generator\": \"vis-probe 0.1.0\",\n"
        "    \"platform_profile\": {\n"
        "      \"selected_time_source\": \"posix_clock_monotonic\"\n"
        "    },\n"
        "    \"target_contract\": {\n"
        "      \"target_profile_family\": \"hosted_posix\",\n"
        "      \"target_runtime_api_status\": \"host_native\"\n"
        "    },\n"
        "    \"claim_gates\": {\n"
        "      \"hosted_evidence_state\": \"observed_host_runtime\",\n"
        "      \"target_timer_claim_state\": \"host_only\",\n"
        "      \"target_execution_claim_state\": \"host_only\",\n"
        "      \"temporal_isolation_state\": \"supporting_only\",\n"
        "      \"wcet_state\": \"supporting_only\",\n"
        "      \"direct_claim_state\": \"target_specific_proof_required\"\n"
        "    },\n"
        "    \"execution_profile\": {\n"
        "      \"execution_environment\": \"posix_user_space\"\n"
        "    },\n"
        "    \"probe_result\": {\n"
        "      \"selected_backend\": \"posix_generic\",\n"
        "      \"backend_status\": \"selected\",\n"
        "      \"timer_evidence_level\": \"portable\",\n"
        "      \"execution_evidence_level\": \"portable_user_space\"\n"
        "    },\n"
        "    \"evidence_level\": \"portable_user_space\"\n"
        "  }\n"
        "}\n";
    if (!vis_report_validate_json(probe, &result) ||
        result.report_type != "vis_probe_report" ||
        result.evidence_level != "portable_user_space") {
        std::fprintf(stderr, "[test] VIS Probe report was rejected\n");
        return 1;
    }

    // Schema 0.1 remains valid even for legacy target-timer shaped reports.

    std::string legacy_target_timer = probe;
    const std::string portable_timer = "\"timer_evidence_level\": \"portable\"";
    const size_t timer_level_position = legacy_target_timer.find(portable_timer);
    if (timer_level_position == std::string::npos) {
        std::fprintf(stderr, "[test] target timer fixture was not found\n");
        return 1;
    }
    legacy_target_timer.replace(timer_level_position, portable_timer.size(),
                                "\"timer_evidence_level\": \"target_timer\"");
    if (!vis_report_validate_json(legacy_target_timer, &result) || !result.errors.empty()) {
        std::fprintf(stderr, "[test] legacy 0.1 target timer report was rejected\n");
        return 1;
    }

    std::string incomplete_target_timer = legacy_target_timer;
    const size_t schema_position = incomplete_target_timer.find("\"schema_version\": \"0.1\"");
    const size_t backend_position =
        incomplete_target_timer.find("\"selected_backend\": \"posix_generic\"");
    if (schema_position == std::string::npos || backend_position == std::string::npos) {
        std::fprintf(stderr, "[test] schema 0.2 fixture was not found\n");
        return 1;
    }
    incomplete_target_timer.replace(schema_position,
                                    std::string("\"schema_version\": \"0.1\"").size(),
                                    "\"schema_version\": \"0.2\"");
    incomplete_target_timer.replace(backend_position,
                                    std::string("\"selected_backend\": \"posix_generic\"").size(),
                                    "\"selected_backend\": \"target_services_probe\"");
    if (vis_report_validate_json(incomplete_target_timer, &result) || result.errors.empty()) {
        std::fprintf(stderr, "[test] schema 0.2 target report without metadata was "
                             "accepted\n");
        return 1;
    }

    std::string complete_target_timer = incomplete_target_timer;
    const std::string execution_level = "\"execution_evidence_level\": \"portable_user_space\"";
    const size_t execution_level_position = complete_target_timer.find(execution_level);
    if (execution_level_position == std::string::npos) {
        std::fprintf(stderr, "[test] target capability fixture was not found\n");
        return 1;
    }
    complete_target_timer.insert(execution_level_position,
                                 "\"timer_frequency_hz\": 1000000,\n"
                                 "      \"timer_unit\": \"ticks\",\n"
                                 "      \"timer_counter_width_bits\": 32,\n"
                                 "      \"timer_wraps\": true,\n"
                                 "      \"timer_metadata_status\": \"reported_by_target\",\n"
                                 "      \"required_capabilities\": 31,\n"
                                 "      \"available_capabilities\": 31,\n"
                                 "      ");
    if (!vis_report_validate_json(complete_target_timer, &result) || !result.errors.empty()) {
        std::fprintf(stderr, "[test] complete target timer report was rejected\n");
        return 1;
    }

    std::string invalid_metadata_status = complete_target_timer;
    const std::string reported_status = "\"timer_metadata_status\": \"reported_by_target\"";
    const size_t metadata_status_position = invalid_metadata_status.find(reported_status);
    if (metadata_status_position == std::string::npos) {
        std::fprintf(stderr, "[test] timer metadata status fixture missing\n");
        return 1;
    }
    invalid_metadata_status.replace(metadata_status_position, reported_status.size(),
                                    "\"timer_metadata_status\": \"unknown_contract_value\"");
    if (vis_report_validate_json(invalid_metadata_status, &result) || result.errors.empty()) {
        std::fprintf(stderr, "[test] invalid timer metadata status was accepted\n");
        return 1;
    }

    std::string unknown_probe_schema = probe;
    unknown_probe_schema.replace(unknown_probe_schema.find("\"schema_version\": \"0.1\""),
                                 std::string("\"schema_version\": \"0.1\"").size(),
                                 "\"schema_version\": \"9.9\"");
    if (vis_report_validate_json(unknown_probe_schema, &result) || result.errors.empty() ||
        result.errors[0].find("9.9") == std::string::npos) {
        std::fprintf(stderr, "[test] unknown probe schema did not produce a clear "
                             "error\n");
        return 1;
    }

    std::string escaped_probe = probe;
    const size_t escaped_level = escaped_probe.rfind("portable_user_space");
    if (escaped_level == std::string::npos) {
        std::fprintf(stderr, "[test] escaped probe fixture was not found\n");
        return 1;
    }
    escaped_probe.replace(escaped_level,
                          std::string("portable_user_space").size(),
                          "portable_user_\\u0073pace");
    if (!vis_report_validate_json(escaped_probe, &result) ||
        result.evidence_level != "portable_user_space") {
        std::fprintf(stderr, "[test] escaped probe enum was rejected\n");
        return 1;
    }

    std::string contradictory_x86_probe = probe;
    const size_t portable_level =
        contradictory_x86_probe.rfind("portable_user_space");
    if (portable_level == std::string::npos) {
        std::fprintf(stderr, "[test] x86 probe fixture was not found\n");
        return 1;
    }
    contradictory_x86_probe.replace(
        portable_level, std::string("portable_user_space").size(),
        "linux_x86_rich_evidence");
    const size_t posix_source =
        contradictory_x86_probe.find("posix_clock_monotonic");
    if (posix_source == std::string::npos) {
        std::fprintf(stderr, "[test] timer fixture was not found\n");
        return 1;
    }
    contradictory_x86_probe.replace(
        posix_source, std::string("posix_clock_monotonic").size(),
        "arm_cntvct_el0");
    if (vis_report_validate_json(contradictory_x86_probe, &result) ||
        result.errors.empty()) {
        std::fprintf(stderr,
                     "[test] contradictory x86 rich evidence was accepted\n");
        return 1;
    }

    std::string deeply_nested =
        "{\"vis_mem_probe_report\":{\"schema_version\":\"0.1\","
        "\"generator\":\"test\",\"nested\":";
    for (size_t i = 0; i <= 128; i++) deeply_nested += '[';
    deeply_nested += "null";
    for (size_t i = 0; i <= 128; i++) deeply_nested += ']';
    deeply_nested += "}}";
    if (vis_report_validate_json(deeply_nested, &result) ||
        result.errors.empty()) {
        std::fprintf(stderr, "[test] excessive JSON nesting was accepted\n");
        return 1;
    }

    std::string missing_member_comma = probe;
    const std::string comma_before_evidence =
        "    },\n    \"evidence_level\"";
    const size_t comma_position =
        missing_member_comma.find(comma_before_evidence);
    if (comma_position == std::string::npos) {
        std::fprintf(stderr, "[test] malformed probe fixture was not found\n");
        return 1;
    }
    missing_member_comma.erase(comma_position + 5, 1);
    if (vis_report_validate_json(missing_member_comma, &result) ||
        result.errors.empty()) {
        std::fprintf(stderr, "[test] probe with missing comma was accepted\n");
        return 1;
    }

    const std::string trailing_content = probe + "garbage";
    if (vis_report_validate_json(trailing_content, &result) ||
        result.errors.empty()) {
        std::fprintf(stderr, "[test] probe with trailing content was accepted\n");
        return 1;
    }

    const std::string duplicate_schema =
        "{\"vis_mem_probe_report\":{"
        "\"schema_version\":\"0.1\","
        "\"schema_version\":\"9.9\","
        "\"generator\":\"vis-mem-probe 0.1.0\"}}";
    if (vis_report_validate_json(duplicate_schema, &result) ||
        result.errors.empty()) {
        std::fprintf(stderr, "[test] duplicate schema member was accepted\n");
        return 1;
    }

    const std::string escaped_duplicate_member =
        "{\"vis_mem_probe_report\":{"
        "\"schema_version\":\"0.1\","
        "\"schema\\u005fversion\":\"9.9\","
        "\"generator\":\"vis-mem-probe 0.1.0\"}}";
    if (vis_report_validate_json(escaped_duplicate_member, &result) ||
        result.errors.empty()) {
        std::fprintf(stderr,
                     "[test] escaped duplicate member was accepted\n");
        return 1;
    }

    const std::string structurally_invalid_probe =
        "{\n"
        "  \"vis_probe_report\": {\n"
        "    \"schema_version\": \"0.1\",\n"
        "    \"generator\": \"vis-probe 0.1.0\",\n"
        "    \"platform_profile\": null,\n"
        "    \"execution_profile\": null,\n"
        "    \"probe_result\": null,\n"
        "    \"target_contract\": null,\n"
        "    \"evidence_level\": \"portable_user_space\",\n"
        "    \"misplaced\": {\n"
        "      \"selected_time_source\": \"posix_clock_monotonic\",\n"
        "      \"execution_environment\": \"posix_user_space\",\n"
        "      \"selected_backend\": \"posix_generic\",\n"
        "      \"backend_status\": \"selected\",\n"
        "      \"timer_evidence_level\": \"portable\",\n"
        "      \"execution_evidence_level\": \"portable_user_space\",\n"
        "      \"target_profile_family\": \"hosted_posix\",\n"
        "      \"target_runtime_api_status\": \"host_native\"\n"
        "    }\n"
        "  }\n"
        "}\n";
    if (vis_report_validate_json(structurally_invalid_probe, &result) ||
        result.errors.empty()) {
        std::fprintf(stderr,
                     "[test] structurally invalid probe was accepted\n");
        return 1;
    }

    const std::string probe_with_nested_metadata =
        "{\n"
        "  \"vis_probe_report\": {\n"
        "    \"metadata\": {\n"
        "      \"schema_version\": \"0.1\",\n"
        "      \"generator\": \"vis-probe 0.1.0\"\n"
        "    }\n"
        "  }\n"
        "}\n";
    if (vis_report_validate_json(probe_with_nested_metadata, &result) ||
        result.errors.empty()) {
        std::fprintf(stderr,
                     "[test] probe with nested metadata was accepted\n");
        return 1;
    }

    const std::string incomplete_probe =
        "{\n"
        "  \"vis_probe_report\": {\n"
        "    \"schema_version\": \"0.1\",\n"
        "    \"generator\": \"vis-probe 0.1.0\",\n"
        "    \"platform_profile\": {\"selected_time_source\": \"posix_clock_monotonic\"},\n"
        "    \"execution_profile\": {\"execution_environment\": \"posix_user_space\"},\n"
        "    \"probe_result\": {\"selected_backend\": \"posix_generic\"},\n"
        "    \"target_contract\": {\"target_profile_family\": \"hosted_posix\"},\n"
        "    \"evidence_level\": \"portable_user_space\"\n"
        "  }\n"
        "}\n";
    if (vis_report_validate_json(incomplete_probe, &result) ||
        result.errors.empty()) {
        std::fprintf(stderr, "[test] incomplete probe was accepted\n");
        return 1;
    }

    const std::string run_with_nested_policy =
        "{\n"
        "  \"vis_run_attestation\": {\n"
        "    \"schema_version\": \"0.1\",\n"
        "    \"generator\": \"vis-run 0.1.0\",\n"
        "    \"vis_cpu_policy_bundle\": {\n"
        "      \"schema_version\": \"0.1\",\n"
        "      \"generator\": \"vis-doctor 0.1.0\",\n"
        "      \"evidence_level\": \"advisory\"\n"
        "    }\n"
        "  }\n"
        "}\n";
    if (!vis_report_validate_json(run_with_nested_policy, &result) ||
        result.report_type != "vis_run_attestation") {
        std::fprintf(stderr,
                     "[test] nested policy bundle was counted as a root\n");
        return 1;
    }

    const std::string cpu_policy =
        "{\n"
        "  \"vis_cpu_policy_bundle\": {\n"
        "    \"schema_version\": \"0.1\",\n"
        "    \"generator\": \"vis-doctor 0.1.0\",\n"
        "    \"evidence_level\": \"advisory\",\n"
        "    \"control_level\": \"advisory\",\n"
        "    \"production_controlled\": false,\n"
        "    \"attested\": false,\n"
        "    \"confidence\": \"medium\",\n"
        "    \"online_cpus\": [0, 1],\n"
        "    \"recommended_thread_counts\": [1, 2],\n"
        "    \"recommended_cpu_sets\": [\n"
        "      {\"name\": \"primary\", \"cpus\": [0, 1]}\n"
        "    ],\n"
        "    \"excluded_cpus\": [],\n"
        "    \"cpu_topology\": []\n"
        "  }\n"
        "}\n";
    if (!vis_report_validate_json(cpu_policy, &result) ||
        result.report_type != "vis_cpu_policy_bundle" ||
        result.evidence_level != "advisory" ||
        result.control_level != "advisory" ||
        result.warnings.empty()) {
        std::fprintf(stderr, "[test] CPU policy bundle was rejected\n");
        return 1;
    }

    if (!vis_policy_evidence_level_is_valid("advisory") ||
        !vis_policy_evidence_level_is_valid("attested") ||
        !vis_policy_evidence_level_is_valid("production_controlled") ||
        vis_policy_evidence_level_is_valid("strong") ||
        !vis_probe_evidence_level_is_valid("portable_user_space") ||
        !vis_probe_evidence_level_is_valid("linux_x86_rich_evidence") ||
        !vis_probe_evidence_level_is_valid("arm_generic_timer_evidence") ||
        !vis_probe_evidence_level_is_valid("contract_only") ||
        !
        vis_probe_evidence_level_is_valid("partial_target_evidence") ||
        vis_probe_evidence_level_is_valid("portable") ||
        vis_policy_evidence_level_semantics("attested").find("/proc") ==
            std::string::npos ||
        vis_probe_evidence_level_semantics("portable_user_space").find(
            "portable user-space") == std::string::npos ||
        vis_probe_evidence_level_semantics("linux_x86_rich_evidence").find(
            "RDTSCP") == std::string::npos ||
        vis_probe_evidence_level_semantics("arm_generic_timer_evidence").find(
            "ARM generic timer") == std::string::npos ||
        vis_probe_evidence_level_semantics("contract_only").find(
            "target backend contract") == std::string::npos ||
        vis_probe_evidence_level_semantics(
            "partial_target_evidence").find("incomplete") ==
            std::string::npos ||
        !vis_probe_hosted_evidence_state_is_valid("observed_host_runtime") ||
        vis_probe_hosted_evidence_state_is_valid("hosted") ||
        !vis_probe_target_claim_state_is_valid("host_only") ||
        !vis_probe_support_state_is_valid("supporting_only") ||
        !vis_probe_direct_claim_state_is_valid(
            "target_specific_proof_required")) {
        std::fprintf(stderr, "[test] policy evidence semantics are wrong\n");
        return 1;
    }

    const std::string contract_only_probe =
        "{\n"
        "  \"vis_probe_report\": {\n"
        "    \"schema_version\": \"0.1\",\n"
        "    \"generator\": \"vis-probe 0.1.0\",\n"
        "    \"platform_profile\": {\n"
        "      \"selected_time_source\": \"x86_rdtscp\"\n"
        "    },\n"
        "    \"target_contract\": {\n"
        "      \"target_profile_family\": \"arinc653\",\n"
        "      \"target_runtime_api_status\": \"recognized_api_missing\"\n"
        "    },\n"
        "    \"claim_gates\": {\n"
        "      \"hosted_evidence_state\": \"observed_host_runtime\",\n"
        "      \"target_timer_claim_state\": \"contract_only\",\n"
        "      \"target_execution_claim_state\": \"contract_only\",\n"
        "      \"temporal_isolation_state\": \"supporting_only\",\n"
        "      \"wcet_state\": \"supporting_only\",\n"
        "      \"direct_claim_state\": \"target_specific_proof_required\"\n"
        "    },\n"
        "    \"execution_profile\": {\n"
        "      \"execution_environment\": \"linux_user_space\"\n"
        "    },\n"
        "    \"probe_result\": {\n"
        "      \"selected_backend\": \"arinc653_partition_probe\",\n"
        "      \"backend_status\": \"recognized_api_missing\",\n"
        "      \"timer_evidence_level\": \"contract_only\",\n"
        "      \"execution_evidence_level\": \"contract_only\"\n"
        "    },\n"
        "    \"evidence_level\": \"contract_only\"\n"
        "  }\n"
        "}\n";
    if (!vis_report_validate_json(contract_only_probe, &result) ||
        result.evidence_level != "contract_only" ||
        result.backend_status != "recognized_api_missing" ||
        result.target_runtime_api_status != "recognized_api_missing") {
        std::fprintf(stderr, "[test] contract-only probe was rejected\n");
        return 1;
    }

    const std::string invalid_target_status_probe =
        "{\n"
        "  \"vis_probe_report\": {\n"
        "    \"schema_version\": \"0.1\",\n"
        "    \"generator\": \"vis-probe 0.1.0\",\n"
        "    \"platform_profile\": {\n"
        "      \"selected_time_source\": \"x86_rdtscp\"\n"
        "    },\n"
        "    \"target_contract\": {\n"
        "      \"target_profile_family\": \"arinc653\",\n"
        "      \"target_runtime_api_status\": \"host_native\"\n"
        "    },\n"
        "    \"claim_gates\": {\n"
        "      \"hosted_evidence_state\": \"observed_host_runtime\",\n"
        "      \"target_timer_claim_state\": \"host_only\",\n"
        "      \"target_execution_claim_state\": \"contract_only\",\n"
        "      \"temporal_isolation_state\": \"supporting_only\",\n"
        "      \"wcet_state\": \"supporting_only\",\n"
        "      \"direct_claim_state\": \"target_specific_proof_required\"\n"
        "    },\n"
        "    \"execution_profile\": {\n"
        "      \"execution_environment\": \"linux_user_space\"\n"
        "    },\n"
        "    \"probe_result\": {\n"
        "      \"selected_backend\": \"arinc653_partition_probe\",\n"
        "      \"backend_status\": \"recognized_api_missing\",\n"
        "      \"timer_evidence_level\": \"contract_only\",\n"
        "      \"execution_evidence_level\": \"contract_only\"\n"
        "    },\n"
        "    \"evidence_level\": \"contract_only\"\n"
        "  }\n"
        "}\n";
    if (vis_report_validate_json(invalid_target_status_probe, &result) ||
        result.errors.empty()) {
        std::fprintf(stderr, "[test] invalid target runtime status was accepted\n");
        return 1;
    }

    const std::string invalid_host_gate_probe =
        "{\n"
        "  \"vis_probe_report\": {\n"
        "    \"schema_version\": \"0.1\",\n"
        "    \"generator\": \"vis-probe 0.1.0\",\n"
        "    \"platform_profile\": {\n"
        "      \"selected_time_source\": \"posix_clock_monotonic\"\n"
        "    },\n"
        "    \"target_contract\": {\n"
        "      \"target_profile_family\": \"hosted_posix\",\n"
        "      \"target_runtime_api_status\": \"host_native\"\n"
        "    },\n"
        "    \"claim_gates\": {\n"
        "      \"hosted_evidence_state\": \"not_observed\",\n"
        "      \"target_timer_claim_state\": \"host_only\",\n"
        "      \"target_execution_claim_state\": \"host_only\",\n"
        "      \"temporal_isolation_state\": \"supporting_only\",\n"
        "      \"wcet_state\": \"supporting_only\",\n"
        "      \"direct_claim_state\": \"target_specific_proof_required\"\n"
        "    },\n"
        "    \"execution_profile\": {\n"
        "      \"execution_environment\": \"posix_user_space\"\n"
        "    },\n"
        "    \"probe_result\": {\n"
        "      \"selected_backend\": \"posix_generic\",\n"
        "      \"backend_status\": \"selected\",\n"
        "      \"timer_evidence_level\": \"portable\",\n"
        "      \"execution_evidence_level\": \"portable_user_space\"\n"
        "    },\n"
        "    \"evidence_level\": \"portable_user_space\"\n"
        "  }\n"
        "}\n";
    if (vis_report_validate_json(invalid_host_gate_probe, &result) ||
        result.errors.empty()) {
        std::fprintf(stderr, "[test] invalid hosted evidence gate was accepted\n");
        return 1;
    }

    const std::string mem_policy =
        "{\n"
        "  \"vis_mem_policy_bundle\": {\n"
        "    \"schema_version\": \"0.1\",\n"
        "    \"generator\": \"vis-mem 0.1.0\",\n"
        "    \"evidence_level\": \"attested\",\n"
        "    \"control_level\": \"attested\",\n"
        "    \"production_controlled\": false,\n"
        "    \"attested\": true,\n"
        "    \"confidence\": \"medium\",\n"
        "    \"verification_confidence\": \"medium\",\n"
        "    \"recommended_memory_policies\": [\"pretouch\"],\n"
        "    \"unsupported_policies\": []\n"
        "  }\n"
        "}\n";
    if (!vis_report_validate_json(mem_policy, &result) ||
        result.report_type != "vis_mem_policy_bundle" ||
        result.evidence_level != "attested") {
        std::fprintf(stderr, "[test] Mem policy bundle was rejected\n");
        return 1;
    }

    const std::string production_policy =
        "{\n"
        "  \"vis_cpu_policy_bundle\": {\n"
        "    \"schema_version\": \"0.1\",\n"
        "    \"generator\": \"vis-cpu 0.1.0\",\n"
        "    \"evidence_level\": \"production_controlled\",\n"
        "    \"control_level\": \"production_controlled\",\n"
        "    \"production_controlled\": true,\n"
        "    \"attested\": true,\n"
        "    \"confidence\": \"high\",\n"
        "    \"online_cpus\": [0, 1],\n"
        "    \"recommended_thread_counts\": [2],\n"
        "    \"recommended_cpu_sets\": [\n"
        "      {\"name\": \"primary\", \"cpus\": [0, 1]}\n"
        "    ],\n"
        "    \"excluded_cpus\": [],\n"
        "    \"cpu_topology\": []\n"
        "  }\n"
        "}\n";
    if (!vis_report_validate_json(production_policy, &result) ||
        result.evidence_level != "production_controlled") {
        std::fprintf(stderr,
                     "[test] production-controlled policy was rejected\n");
        return 1;
    }

    const std::string invalid_level =
        "{\n"
        "  \"vis_cpu_policy_bundle\": {\n"
        "    \"schema_version\": \"0.1\",\n"
        "    \"generator\": \"vis-doctor 0.1.0\",\n"
        "    \"evidence_level\": \"strong\",\n"
        "    \"control_level\": \"advisory\",\n"
        "    \"confidence\": \"medium\",\n"
        "    \"online_cpus\": [0],\n"
        "    \"recommended_thread_counts\": [1],\n"
        "    \"recommended_cpu_sets\": [],\n"
        "    \"excluded_cpus\": [],\n"
        "    \"cpu_topology\": []\n"
        "  }\n"
        "}\n";
    if (vis_report_validate_json(invalid_level, &result) ||
        result.errors.empty()) {
        std::fprintf(stderr, "[test] invalid policy level was accepted\n");
        return 1;
    }

    const std::string missing_policy_fields =
        "{\n"
        "  \"vis_cpu_policy_bundle\": {\n"
        "    \"schema_version\": \"0.1\",\n"
        "    \"generator\": \"vis-doctor 0.1.0\",\n"
        "    \"evidence_level\": \"advisory\",\n"
        "    \"control_level\": \"advisory\",\n"
        "    \"confidence\": \"medium\"\n"
        "  }\n"
        "}\n";
    if (vis_report_validate_json(missing_policy_fields, &result) ||
        result.errors.empty()) {
        std::fprintf(stderr,
                     "[test] incomplete policy bundle was accepted\n");
        return 1;
    }

    const std::string overclaiming_policy =
        "{\n"
        "  \"vis_cpu_policy_bundle\": {\n"
        "    \"schema_version\": \"0.1\",\n"
        "    \"generator\": \"vis-doctor 0.1.0\",\n"
        "    \"evidence_level\": \"advisory\",\n"
        "    \"control_level\": \"advisory\",\n"
        "    \"production_controlled\": true,\n"
        "    \"attested\": false,\n"
        "    \"confidence\": \"medium\",\n"
        "    \"online_cpus\": [0],\n"
        "    \"recommended_thread_counts\": [1],\n"
        "    \"recommended_cpu_sets\": [],\n"
        "    \"excluded_cpus\": [],\n"
        "    \"cpu_topology\": []\n"
        "  }\n"
        "}\n";
    if (vis_report_validate_json(overclaiming_policy, &result) ||
        result.errors.empty()) {
        std::fprintf(stderr,
                     "[test] overclaiming advisory policy was accepted\n");
        return 1;
    }

    const std::string missing_schema =
        "{\n"
        "  \"vis_mem_probe_report\": {\n"
        "    \"generator\": \"vis-mem-probe 0.1.0\"\n"
        "  }\n"
        "}\n";
    if (vis_report_validate_json(missing_schema, &result) ||
        result.errors.empty()) {
        std::fprintf(stderr, "[test] missing schema was accepted\n");
        return 1;
    }

    const std::string wrong_schema =
        "{\n"
        "  \"vis_mem_probe_report\": {\n"
        "    \"schema_version\": \"9.9\",\n"
        "    \"generator\": \"vis-mem-probe 0.1.0\"\n"
        "  }\n"
        "}\n";
    if (vis_report_validate_json(wrong_schema, &result) ||
        result.errors.empty()) {
        std::fprintf(stderr, "[test] unsupported schema was accepted\n");
        return 1;
    }

    const char* path = "/tmp/vis_report_validator_test.json";
    {
        std::ofstream out(path);
        out << valid;
    }

    std::string error;
    if (!vis_report_validate_file(path, &result, &error) ||
        !error.empty() ||
        !result.valid) {
        std::fprintf(stderr, "[test] valid report file was rejected: %s\n",
                     error.c_str());
        return 1;
    }

    std::printf("[test] PASS: VIS Report Validator works.\n");
    return 0;
}
