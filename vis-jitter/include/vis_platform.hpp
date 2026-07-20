#pragma once

/**
 * vis_platform.hpp
 *
 * Portable VIS Core platform profile primitives.
 *
 * This layer does not make a platform deterministic. It records which
 * architecture, OS surface, and time-source capabilities VIS can see so later
 * measurement reports can state their evidence level honestly.
 *
 * License: MIT
 */

#include <cstdint>

#define VIS_PLATFORM_PROFILE_VERSION "0.1"

struct vis_time_source_candidate_t {
    char name[64];
    bool available;
    bool monotonic;
    double read_overhead_ns;
    char evidence_level[32];
    char reason[160];
};

struct vis_platform_profile_t {
    char profile_version[8];
    char arch[32];
    char os_family[32];
    char environment[48];
    uint32_t abi_bits;
    char kernel_release[96];
    char platform_fingerprint[32];

    char selected_time_source[64];
    char time_source_evidence_level[32];
    double time_source_read_overhead_ns;
    bool time_source_monotonic;

    char affinity_control[48];
    char timer_access_model[48];
    char scheduler_model[48];
    char partitioning_hint[48];
    char posix_profile[48];
    char arinc653_surface[48];
    char autosar_adaptive_surface[48];
    char hypervisor_surface[48];
    char runtime_isolation_hint[64];
    char interrupt_evidence[48];
    char thermal_evidence[48];
    char memory_policy[48];
    char privileged_counters[48];
    char claim_level[48];
    char limitations[256];

    uint32_t candidate_count;
    vis_time_source_candidate_t candidates[4];
};

/** Target-supplied platform detection boundary. */
struct vis_platform_adapter_t {
    void* context;
    int (*detect_profile)(void* context, vis_platform_profile_t* profile);
};

/**
 * Detect the current platform and select the best available low-level timing
 * source known to this VIS build.
 *
 * The function uses compile-time architecture identity first, then performs
 * whitelist-only capability validation. It never probes arbitrary registers.
 */
int vis_platform_detect_profile(vis_platform_profile_t* profile);

/** Detect through an explicitly supplied hosted or target adapter. */
int vis_platform_detect_profile_with_adapter(
    const vis_platform_adapter_t* adapter,
    vis_platform_profile_t* profile);

/** Default adapter linked by the selected platform build. */
const vis_platform_adapter_t* vis_platform_default_adapter();
