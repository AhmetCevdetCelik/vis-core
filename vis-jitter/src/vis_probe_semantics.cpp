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
           status == "partial_evidence" ||
           status == "collection_failed" ||
           status == "permission_denied" ||
           status == "recognized_api_missing" ||
           status == "not_supported_on_build";
}

bool vis_probe_timer_evidence_level_is_valid(const std::string& level) {
    return level == "portable" ||
           level == "architecture_counter" ||
           level == "target_timer" ||
           level == "contract_only";
}

bool vis_probe_execution_evidence_level_is_valid(const std::string& level) {
    return level == "portable_user_space" ||
           level == "contract_only" ||
           level == "rtos_execution_surface" ||
           level == "partial_rtos_execution_surface" ||
           level == "hypervisor_partition_hint";
}

bool vis_probe_target_runtime_api_status_is_valid(const std::string& status) {
    return status == "host_native" ||
           status == "recognized_api_missing" ||
           status == "permission_denied" ||
           status == "available_collection_failed" ||
           status == "collection_not_reached" ||
           status == "not_supported_on_build";
}

bool vis_probe_surface_state_is_valid(const std::string& state) {
    return state == "not_claimed" ||
           state == "surface_visible" ||
           state == "contract_recognized_api_missing" ||
           state == "host_posix_like" ||
           state == "host_linux_runtime";
}

bool vis_probe_hosted_evidence_state_is_valid(const std::string& state) {
    return state == "observed_host_runtime" ||
           state == "not_observed";
}

bool vis_probe_target_claim_state_is_valid(const std::string& state) {
    return state == "host_only" ||
           state == "contract_only" ||
           state == "target_attested";
}

bool vis_probe_support_state_is_valid(const std::string& state) {
    return state == "supporting_only" ||
           state == "target_attested" ||
           state == "not_supported";
}

bool vis_probe_direct_claim_state_is_valid(const std::string& state) {
    return state == "not_proven" ||
           state == "target_specific_proof_required";
}

std::string vis_probe_backend_status_semantics(const std::string& status) {
    if (status == "selected") {
        return "Backend executed on this host and produced runtime evidence.";
    }
    if (status == "partial_evidence") {
        return "Backend collected target evidence, but one or more requested "
               "runtime surfaces were incomplete.";
    }
    if (status == "collection_failed") {
        return "Backend callbacks were present, but evidence collection "
               "failed.";
    }
    if (status == "permission_denied") {
        return "Backend collection was denied by the target privilege model.";
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
    if (level == "partial_rtos_execution_surface") {
        return "Only part of the target scheduler, partition, or runtime "
               "surface was observed.";
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
    if (status == "permission_denied") {
        return "The target runtime API exists, but the query was denied.";
    }
    if (status == "available_collection_failed") {
        return "The target runtime API exists, but returned invalid or "
               "incomplete data.";
    }
    if (status == "collection_not_reached") {
        return "An earlier target-service failure prevented the runtime API "
               "query.";
    }
    if (status == "not_supported_on_build") {
        return "The target contract is not supported by the current build.";
    }
    return "Unknown target runtime API status.";
}

std::string vis_probe_hosted_evidence_state_semantics(
    const std::string& state
) {
    if (state == "observed_host_runtime") {
        return "The report contains evidence gathered from the current hosted runtime.";
    }
    if (state == "not_observed") {
        return "The report does not claim hosted runtime observation.";
    }
    return "Unknown hosted evidence state.";
}

std::string vis_probe_target_claim_state_semantics(const std::string& state) {
    if (state == "host_only") {
        return "Evidence is limited to a hosted runtime and does not open a target claim.";
    }
    if (state == "contract_only") {
        return "A target contract is modeled, but the target claim remains closed.";
    }
    if (state == "target_attested") {
        return "A target-specific backend has attested this claim.";
    }
    return "Unknown target claim state.";
}

std::string vis_probe_support_state_semantics(const std::string& state) {
    if (state == "supporting_only") {
        return "The report may support analysis, but it is not direct proof.";
    }
    if (state == "target_attested") {
        return "The report includes target-specific evidence for this analysis surface.";
    }
    if (state == "not_supported") {
        return "The report does not support this analysis surface.";
    }
    return "Unknown support state.";
}

std::string vis_probe_direct_claim_state_semantics(
    const std::string& state
) {
    if (state == "not_proven") {
        return "The report does not directly prove the corresponding safety claim.";
    }
    if (state == "target_specific_proof_required") {
        return "Direct proof requires target-specific evidence beyond this report.";
    }
    return "Unknown direct claim state.";
}
