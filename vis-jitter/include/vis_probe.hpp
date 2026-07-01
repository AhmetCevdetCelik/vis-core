#pragma once

/**
 * vis_probe.hpp
 *
 * Portable VIS probe API for claim-safe platform and timing-source evidence.
 *
 * License: MIT
 */

#include "report_schema.hpp"
#include "vis_platform.hpp"

#include <cstdint>

#define VIS_PROBE_VERSION "0.1.0"

enum class vis_probe_status_t : int {
    VIS_PROBE_OK = 0,
    VIS_PROBE_ERR_INVALID_ARG = -1,
    VIS_PROBE_ERR_BACKEND_UNAVAILABLE = -2,
    VIS_PROBE_ERR_NO_TIME_SOURCE = -3,
};

enum class vis_probe_backend_hint_t : int {
    AUTO = 0,
    POSIX_GENERIC = 1,
    LINUX_X86_RDTSCP_MSR = 2,
    ARM_GENERIC_TIMER = 3,
    ARINC653_PARTITION_PROBE = 4,
    POSIX_PSE53_PROBE = 5,
    AUTOSAR_ADAPTIVE_PROBE = 6,
    HYPERVISOR_PARTITION_PROBE = 7,
};

struct vis_execution_profile_t {
    char execution_environment[48];
    char partition_model[32];
    char scheduler_surface[48];
    char scheduling_scope[48];
    char affinity_surface[48];
    char thread_model[48];
    char posix_surface[48];
    char arinc653_surface[48];
    char autosar_adaptive_surface[48];
    char hypervisor_surface[48];
    char runtime_isolation_model[64];
    char portability_tier[48];
    char interrupt_visibility[48];
    char memory_visibility[48];
};

struct vis_probe_result_t {
    char selected_backend[48];
    char backend_status[48];
    char backend_status_reason[160];
    char selected_time_source[64];
    bool time_source_monotonic;
    double time_source_read_overhead_ns;
    char privilege_requirement[48];
    char evidence_level[48];
    char timer_evidence_level[48];
    char execution_evidence_level[48];
    char unsupported_reason[160];
    char limitations[256];
};

struct vis_target_contract_t {
    char target_profile_family[48];
    char target_timer_model[48];
    char target_scheduler_model[48];
    char target_partition_model[48];
    char target_privilege_model[48];
    char target_runtime_api_status[48];
    char target_arinc653_surface[48];
    char target_posix_pse53_surface[48];
    char target_autosar_adaptive_surface[48];
    char target_hypervisor_surface[48];
    char target_limitations[256];
};

struct vis_probe_report_t {
    char schema_version[8];
    char report_id[40];
    char generated_at[32];
    char generator[64];
    vis_platform_profile_t platform_profile;
    vis_target_contract_t target_contract;
    vis_execution_profile_t execution_profile;
    vis_probe_result_t probe_result;
};

struct vis_probe_config_t {
    vis_probe_backend_hint_t backend_hint;
};

vis_probe_status_t vis_probe_run(const vis_probe_config_t* config,
                                 vis_probe_report_t* report);
char* vis_probe_report_to_json(const vis_probe_report_t* report);
void vis_probe_report_print_summary(const vis_probe_report_t* report);
