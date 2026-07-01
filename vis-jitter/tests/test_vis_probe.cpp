/**
 * test_vis_probe.cpp
 *
 * Rootless smoke test for portable VIS probe backends.
 *
 * License: MIT
 */

#include "../include/vis_probe.hpp"

#include <cstdlib>
#include <cstdio>
#include <cstring>

static bool empty(const char* value) {
    return value == nullptr || value[0] == '\0';
}

int main() {
    if (vis_probe_run(nullptr, nullptr) !=
        vis_probe_status_t::VIS_PROBE_ERR_INVALID_ARG) {
        std::printf("[test] FAILED: null report should be invalid arg.\n");
        return 1;
    }

    vis_probe_report_t auto_report;
    vis_probe_status_t status = vis_probe_run(nullptr, &auto_report);
    if (status != vis_probe_status_t::VIS_PROBE_OK) {
        std::printf("[test] FAILED: auto probe run failed.\n");
        return 1;
    }
    if (empty(auto_report.platform_profile.arch) ||
        empty(auto_report.execution_profile.execution_environment) ||
        empty(auto_report.probe_result.selected_backend) ||
        empty(auto_report.probe_result.evidence_level) ||
        empty(auto_report.probe_result.backend_status) ||
        empty(auto_report.probe_result.timer_evidence_level) ||
        empty(auto_report.probe_result.execution_evidence_level)) {
        std::printf("[test] FAILED: auto probe report is incomplete.\n");
        return 1;
    }
    if (empty(auto_report.execution_profile.posix_surface) ||
        empty(auto_report.execution_profile.arinc653_surface) ||
        empty(auto_report.execution_profile.hypervisor_surface) ||
        empty(auto_report.execution_profile.runtime_isolation_model)) {
        std::printf("[test] FAILED: execution surfaces are incomplete.\n");
        return 1;
    }

    vis_probe_config_t posix_config{vis_probe_backend_hint_t::POSIX_GENERIC};
    vis_probe_report_t posix_report;
    status = vis_probe_run(&posix_config, &posix_report);
    if (status != vis_probe_status_t::VIS_PROBE_OK) {
        std::printf("[test] FAILED: posix_generic backend is unavailable.\n");
        return 1;
    }
    if (std::strcmp(posix_report.probe_result.selected_backend,
                    "posix_generic") != 0 ||
        std::strcmp(posix_report.probe_result.evidence_level,
                    "portable_user_space") != 0 ||
        std::strcmp(posix_report.probe_result.backend_status,
                    "selected") != 0 ||
        std::strcmp(posix_report.platform_profile.selected_time_source,
                    "posix_clock_monotonic") != 0) {
        std::printf("[test] FAILED: posix backend fields are wrong.\n");
        return 1;
    }
    if (std::strcmp(posix_report.execution_profile.portability_tier,
                    "portable_probe_foundation") != 0) {
        std::printf("[test] FAILED: portability tier is wrong.\n");
        return 1;
    }

#if defined(__x86_64__) || defined(__i386__)
    vis_probe_config_t x86_config{
        vis_probe_backend_hint_t::LINUX_X86_RDTSCP_MSR};
    vis_probe_report_t x86_report;
    status = vis_probe_run(&x86_config, &x86_report);
    if (status != vis_probe_status_t::VIS_PROBE_OK ||
        std::strcmp(x86_report.probe_result.selected_backend,
                    "linux_x86_rdtscp_msr") != 0) {
        std::printf("[test] FAILED: x86 backend should work on x86.\n");
        return 1;
    }
#elif defined(__aarch64__)
    vis_probe_config_t arm_config{
        vis_probe_backend_hint_t::ARM_GENERIC_TIMER};
    vis_probe_report_t arm_report;
    status = vis_probe_run(&arm_config, &arm_report);
    if (status != vis_probe_status_t::VIS_PROBE_OK ||
        std::strcmp(arm_report.probe_result.selected_backend,
                    "arm_generic_timer") != 0 ||
        std::strcmp(arm_report.probe_result.evidence_level,
                    "arm_generic_timer_evidence") != 0) {
        std::printf("[test] FAILED: ARM backend should work on AArch64.\n");
        return 1;
    }
#endif

    vis_probe_config_t arinc_config{
        vis_probe_backend_hint_t::ARINC653_PARTITION_PROBE};
    vis_probe_report_t arinc_report;
    status = vis_probe_run(&arinc_config, &arinc_report);
    if (status != vis_probe_status_t::VIS_PROBE_OK ||
        std::strcmp(arinc_report.probe_result.selected_backend,
                    "arinc653_partition_probe") != 0 ||
        std::strcmp(arinc_report.probe_result.evidence_level,
                    "contract_only") != 0 ||
        std::strcmp(arinc_report.probe_result.backend_status,
                    "recognized_api_missing") != 0 ||
        std::strstr(arinc_report.probe_result.unsupported_reason,
                    "arinc653_partition_services") == nullptr) {
        std::printf("[test] FAILED: ARINC stub backend contract is wrong.\n");
        return 1;
    }

    char* json = vis_probe_report_to_json(&posix_report);
    if (json == nullptr || std::strstr(json, "\"vis_probe_report\"") == nullptr ||
        std::strstr(json, "\"evidence_level\": \"portable_user_space\"") ==
            nullptr ||
        std::strstr(json, "\"backend_status\": \"selected\"") ==
            nullptr ||
        std::strstr(json, "\"arinc653_surface\"") == nullptr ||
        std::strstr(json, "\"hypervisor_surface\"") == nullptr ||
        std::strstr(json, "\"execution_evidence_level\"") == nullptr ||
        std::strstr(json, "\"portability_tier\"") ==
            nullptr) {
        std::printf("[test] FAILED: probe JSON serialization failed.\n");
        std::free(json);
        return 1;
    }
    std::free(json);

    std::printf("[test] PASS: VIS Probe works.\n");
    return 0;
}
