/**
 * test_report_json.cpp
 *
 * Regression test for JSON report serialization.
 *
 * Run:
 *   ./test_report_json
 *
 * License: MIT
 */

#include "../include/report.hpp"
#include "../include/report_schema.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static bool contains(const char* text, const char* needle) {
    return std::strstr(text, needle) != nullptr;
}

static bool has_trailing_comma_before_close(const char* json) {
    for (const char* p = json; *p != '\0'; p++) {
        if (*p != ',') {
            continue;
        }

        const char* q = p + 1;
        while (*q == ' ' || *q == '\n' || *q == '\r' || *q == '\t') {
            q++;
        }

        if (*q == '}' || *q == ']') {
            return true;
        }
    }

    return false;
}

static void populate_report(vis_report_t* report) {
    std::memset(report, 0, sizeof(vis_report_t));

    std::strncpy(report->schema_version, VIS_CORE_REPORT_SCHEMA_VERSION,
                 sizeof(report->schema_version) - 1);
    std::strncpy(report->report_id,
                 "00000000-0000-4000-8000-000000000000",
                 sizeof(report->report_id) - 1);
    std::strncpy(report->generated_at, "2026-05-07T00:00:00Z",
                 sizeof(report->generated_at) - 1);
    std::strncpy(report->generator, "vis-jitter test",
                 sizeof(report->generator) - 1);

    std::strncpy(report->platform.profile_version,
                 VIS_PLATFORM_PROFILE_VERSION,
                 sizeof(report->platform.profile_version) - 1);
    std::strncpy(report->platform.arch, "x86_64",
                 sizeof(report->platform.arch) - 1);
    std::strncpy(report->platform.os_family, "linux",
                 sizeof(report->platform.os_family) - 1);
    std::strncpy(report->platform.environment, "linux_user_space",
                 sizeof(report->platform.environment) - 1);
    report->platform.abi_bits = 64;
    std::strncpy(report->platform.kernel_release, "test-kernel",
                 sizeof(report->platform.kernel_release) - 1);
    std::strncpy(report->platform.platform_fingerprint, "abc123",
                 sizeof(report->platform.platform_fingerprint) - 1);
    std::strncpy(report->platform.selected_time_source, "x86_rdtscp",
                 sizeof(report->platform.selected_time_source) - 1);
    std::strncpy(report->platform.time_source_evidence_level,
                 "architecture_counter",
                 sizeof(report->platform.time_source_evidence_level) - 1);
    report->platform.time_source_read_overhead_ns = 12.5;
    report->platform.time_source_monotonic = true;
    std::strncpy(report->platform.affinity_control,
                 "pthread_affinity_available",
                 sizeof(report->platform.affinity_control) - 1);
    std::strncpy(report->platform.interrupt_evidence,
                 "linux_proc_interrupts_optional",
                 sizeof(report->platform.interrupt_evidence) - 1);
    std::strncpy(report->platform.thermal_evidence, "linux_sysfs_optional",
                 sizeof(report->platform.thermal_evidence) - 1);
    std::strncpy(report->platform.memory_policy, "linux_numa_optional",
                 sizeof(report->platform.memory_policy) - 1);
    std::strncpy(report->platform.privileged_counters,
                 "msr_requires_root_or_cap_sys_rawio",
                 sizeof(report->platform.privileged_counters) - 1);
    std::strncpy(report->platform.claim_level, "linux_x86_rich_evidence",
                 sizeof(report->platform.claim_level) - 1);
    std::strncpy(report->platform.limitations, "test limitation",
                 sizeof(report->platform.limitations) - 1);
    report->platform.candidate_count = 1;
    std::strncpy(report->platform.candidates[0].name, "x86_rdtscp",
                 sizeof(report->platform.candidates[0].name) - 1);
    report->platform.candidates[0].available = true;
    report->platform.candidates[0].monotonic = true;
    report->platform.candidates[0].read_overhead_ns = 12.5;
    std::strncpy(report->platform.candidates[0].evidence_level,
                 "architecture_counter",
                 sizeof(report->platform.candidates[0].evidence_level) - 1);
    std::strncpy(report->platform.candidates[0].reason,
                 "test candidate",
                 sizeof(report->platform.candidates[0].reason) - 1);

    report->detected.cpu_core = 2;
    report->detected.frequency_ghz = 4.800;
    report->detected.numa_node = 0;
    report->detected.smt_active = false;
    report->detected.tsc_invariant = true;
    report->detected.rdtscp_supported = true;

    std::strncpy(report->asserted.p_state, "P0\"locked\\test",
                 sizeof(report->asserted.p_state) - 1);
    std::strncpy(report->asserted.c_states_disabled, "C1E\nC3\tC6",
                 sizeof(report->asserted.c_states_disabled) - 1);
    report->asserted.hugepages_1gb = true;
    std::strncpy(report->asserted.egress_memory, "UC",
                 sizeof(report->asserted.egress_memory) - 1);
    std::strncpy(report->asserted.rx_buffer_memory, "WC",
                 sizeof(report->asserted.rx_buffer_memory) - 1);
    report->asserted.ddio_enabled = false;

    report->smi_audit.msr_start = 100;
    report->smi_audit.msr_end = 104;
    report->smi_audit.msr_delta = 4;
    report->smi_audit.smi_events_detected = 4;
    report->smi_audit.events_detected = 2;
    report->smi_audit.contaminated_windows = 2;
    report->smi_audit.samples_rejected = 2'000'000;
    std::strncpy(report->smi_audit.rejection_policy, "full_window",
                 sizeof(report->smi_audit.rejection_policy) - 1);

    report->results.samples_accepted = 10'000'000;
    report->results.core_migration_rejected = 3;
    report->results.latency_ns.min_ns = 0.0;
    report->results.latency_ns.p50_ns = 5.0;
    report->results.latency_ns.p99_ns = 15.0;
    report->results.latency_ns.p99_9_ns = 25.0;
    report->results.latency_ns.p99_99_ns = 35.0;
    report->results.latency_ns.max_ns = 45.0;
    report->results.latency_ns.overflow_count = 0;
    report->results.latency_ns.max_saturated = false;
    report->results.determinism_pass = true;
    report->results.threshold_ns = 100.0;
}

int main() {
    if (vis_report_to_json(nullptr) != nullptr) {
        std::printf("[test] FAILED: null report should return nullptr.\n");
        return 1;
    }

    vis_report_t report;
    populate_report(&report);

    char* json = vis_report_to_json(&report);
    if (json == nullptr) {
        std::printf("[test] FAILED: serializer returned nullptr.\n");
        return 1;
    }

    const char* required_fields[] = {
        "\"schema_version\": \"" VIS_CORE_REPORT_SCHEMA_VERSION "\"",
        "\"report_id\": \"00000000-0000-4000-8000-000000000000\"",
        "\"platform_profile\"",
        "\"arch\": \"x86_64\"",
        "\"selected_time_source\": \"x86_rdtscp\"",
        "\"time_source_evidence_level\": \"architecture_counter\"",
        "\"claim_level\": \"linux_x86_rich_evidence\"",
        "\"time_source_candidates\"",
        "\"cpu_core\": 2",
        "\"frequency_ghz\": 4.800",
        "\"msr_delta\": 4",
        "\"smi_events_detected\": 4",
        "\"events_detected\": 2",
        "\"events_detected_note\": \"deprecated alias for contaminated_windows\"",
        "\"contaminated_windows\": 2",
        "\"samples_rejected\": 2000000",
        "\"samples_accepted\": 10000000",
        "\"p99\": 15.0",
        "\"overflow_count\": 0",
        "\"max_saturated\": false",
        "\"determinism_verdict\": \"PASS\"",
        "\"threshold_ns\": 100.0",
        "\"p_state\": \"P0\\\"locked\\\\test\"",
        "\"c_states_disabled\": \"C1E\\nC3\\tC6\""
    };

    for (size_t i = 0; i < sizeof(required_fields) / sizeof(required_fields[0]); i++) {
        if (!contains(json, required_fields[i])) {
            std::printf("[test] FAILED: missing JSON field: %s\n",
                        required_fields[i]);
            std::free(json);
            return 1;
        }
    }

    if (has_trailing_comma_before_close(json)) {
        std::printf("[test] FAILED: JSON has a trailing comma before a close token.\n");
        std::free(json);
        return 1;
    }

    const char* output_path = "/tmp/vis_report_json_test.json";
    FILE* output = std::fopen(output_path, "w");
    if (output == nullptr) {
        std::printf("[test] FAILED: cannot open %s for writing.\n", output_path);
        std::free(json);
        return 1;
    }

    std::fputs(json, output);
    std::fclose(output);
    std::free(json);

    std::printf("[test] PASS: JSON report serialization is structurally valid.\n");
    std::printf("[test] Wrote parser fixture to %s\n", output_path);
    return 0;
}
