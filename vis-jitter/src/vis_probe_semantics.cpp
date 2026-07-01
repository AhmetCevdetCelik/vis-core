/**
 * vis_probe_semantics.cpp
 *
 * Shared state/value validation helpers for VIS probe reports.
 *
 * License: MIT
 */

#include "../include/vis_probe_semantics.hpp"

bool vis_probe_backend_status_is_valid(const std::string& status) {
    return status == "selected" ||
           status == "recognized_api_missing" ||
           status == "not_supported_on_build";
}

bool vis_probe_timer_evidence_level_is_valid(const std::string& level) {
    return level == "portable" ||
           level == "architecture_counter" ||
           level == "contract_only";
}

bool vis_probe_execution_evidence_level_is_valid(const std::string& level) {
    return level == "portable_user_space" ||
           level == "contract_only" ||
           level == "rtos_execution_surface" ||
           level == "hypervisor_partition_hint";
}

bool vis_probe_target_runtime_api_status_is_valid(const std::string& status) {
    return status == "host_native" ||
           status == "recognized_api_missing" ||
           status == "not_supported_on_build";
}

bool vis_probe_surface_state_is_valid(const std::string& state) {
    return state == "not_claimed" ||
           state == "surface_visible" ||
           state == "contract_recognized_api_missing" ||
           state == "host_posix_like" ||
           state == "host_linux_runtime";
}

std::string vis_probe_backend_status_semantics(const std::string& status) {
    if (status == "selected") {
        return "Backend executed on this host and produced runtime evidence.";
    }
    if (status == "recognized_api_missing") {
        return "Backend contract is recognized, but the target runtime API is "
               "not available on this host.";
    }
    if (status == "not_supported_on_build") {
        return "Backend is known by VIS, but this build target cannot support "
               "it.";
    }
    return "Unknown probe backend status.";
}

std::string vis_probe_execution_evidence_level_semantics(
    const std::string& level
) {
    if (level == "portable_user_space") {
        return "Execution evidence comes from hosted user-space surfaces only.";
    }
    if (level == "contract_only") {
        return "Execution surface is modeled as a target contract, but not "
               "attested on the current host.";
    }
    if (level == "rtos_execution_surface") {
        return "Execution surface evidence was observed from a target-specific "
               "RTOS backend.";
    }
    if (level == "hypervisor_partition_hint") {
        return "Execution evidence reflects partition or hypervisor surface "
               "visibility only.";
    }
    return "Unknown execution evidence level.";
}

std::string vis_probe_target_runtime_api_status_semantics(
    const std::string& status
) {
    if (status == "host_native") {
        return "The requested target contract matches the current hosted "
               "runtime.";
    }
    if (status == "recognized_api_missing") {
        return "The target contract is recognized, but target runtime APIs are "
               "missing on this host.";
    }
    if (status == "not_supported_on_build") {
        return "The target contract is not supported by the current build.";
    }
    return "Unknown target runtime API status.";
}
