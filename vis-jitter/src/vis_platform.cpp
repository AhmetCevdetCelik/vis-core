/**
 * vis_platform.cpp
 *
 * Target-neutral platform adapter dispatch.
 *
 * License: MIT
 */

#include "../include/vis_platform.hpp"

int vis_platform_detect_profile_with_adapter(
    const vis_platform_adapter_t* adapter,
    vis_platform_profile_t* profile) {
    if (adapter == nullptr || adapter->detect_profile == nullptr ||
        profile == nullptr) {
        return -1;
    }
    return adapter->detect_profile(adapter->context, profile);
}

int vis_platform_detect_profile(vis_platform_profile_t* profile) {
    return vis_platform_detect_profile_with_adapter(
        vis_platform_default_adapter(), profile);
}
