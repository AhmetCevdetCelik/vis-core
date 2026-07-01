#pragma once

/**
 * vis_probe_semantics.hpp
 *
 * Shared state/value validation helpers for VIS probe reports.
 *
 * License: MIT
 */

#include <string>

bool vis_probe_backend_status_is_valid(const std::string& status);
bool vis_probe_timer_evidence_level_is_valid(const std::string& level);
bool vis_probe_execution_evidence_level_is_valid(const std::string& level);
bool vis_probe_target_runtime_api_status_is_valid(const std::string& status);
bool vis_probe_surface_state_is_valid(const std::string& state);

std::string vis_probe_backend_status_semantics(const std::string& status);
std::string vis_probe_execution_evidence_level_semantics(
    const std::string& level);
std::string vis_probe_target_runtime_api_status_semantics(
    const std::string& status);
