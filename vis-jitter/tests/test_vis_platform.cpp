/**
 * test_vis_platform.cpp
 *
 * Rootless smoke test for VIS Core platform profile detection.
 *
 * License: MIT
 */

#include "../include/vis_platform.hpp"

#include <cstdio>
#include <cstring>

static bool empty(const char* value) {
    return value == nullptr || value[0] == '\0';
}

int main() {
    vis_platform_profile_t profile;
    if (vis_platform_detect_profile(&profile) != 0) {
        std::printf("[test] FAILED: platform profile detection failed.\n");
        return 1;
    }

    if (std::strcmp(profile.profile_version,
                    VIS_PLATFORM_PROFILE_VERSION) != 0) {
        std::printf("[test] FAILED: unexpected profile version.\n");
        return 1;
    }
    if (empty(profile.arch) || empty(profile.os_family) ||
        empty(profile.environment) || empty(profile.platform_fingerprint)) {
        std::printf("[test] FAILED: platform identity fields are incomplete.\n");
        return 1;
    }
    if (profile.abi_bits != 32 && profile.abi_bits != 64) {
        std::printf("[test] FAILED: ABI bits should be 32 or 64.\n");
        return 1;
    }
    if (empty(profile.selected_time_source) ||
        empty(profile.time_source_evidence_level) ||
        empty(profile.claim_level)) {
        std::printf("[test] FAILED: selected time source fields are incomplete.\n");
        return 1;
    }
    if (profile.candidate_count == 0 || profile.candidate_count > 4) {
        std::printf("[test] FAILED: time source candidate count is invalid.\n");
        return 1;
    }

    bool has_available_candidate = false;
    for (uint32_t i = 0; i < profile.candidate_count; i++) {
        const vis_time_source_candidate_t& c = profile.candidates[i];
        if (empty(c.name) || empty(c.evidence_level) || empty(c.reason)) {
            std::printf("[test] FAILED: candidate %u is incomplete.\n", i);
            return 1;
        }
        if (c.available) {
            has_available_candidate = true;
        }
    }
    if (!has_available_candidate) {
        std::printf("[test] FAILED: no usable timing candidate was found.\n");
        return 1;
    }

    std::printf("[test] PASS: VIS Platform profile works.\n");
    std::printf("[test] arch=%s os=%s time_source=%s claim=%s\n",
                profile.arch,
                profile.os_family,
                profile.selected_time_source,
                profile.claim_level);
    return 0;
}
