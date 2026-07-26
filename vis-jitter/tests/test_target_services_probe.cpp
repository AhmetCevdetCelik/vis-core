/**
 * test_target_services_probe.cpp
 *
 * Deterministic fake-target coverage for the generic target-services backend.
 *
 * License: MIT
 */

#include "../include/vis_probe.hpp"
#include "../include/report_validator.hpp"

#include <cstdlib>
#include <cstdio>
#include <cstring>

struct fake_target_t {
    int timer_query_status = static_cast<int>(vis_probe_service_status_t::OK);
    int timer_read_status = static_cast<int>(vis_probe_service_status_t::OK);
    int scheduler_status = static_cast<int>(vis_probe_service_status_t::OK);
    int partition_status = static_cast<int>(vis_probe_service_status_t::OK);
    int privilege_status = static_cast<int>(vis_probe_service_status_t::OK);
    int runtime_status = static_cast<int>(vis_probe_service_status_t::OK);
    bool timer_monotonic = true;
    uint32_t timer_width_bits = 32;
    uint64_t timer_values[2] = {100, 101};
    uint32_t timer_read_index = 0;
    const char* scheduler = "fixed_priority";
    const char* partition = "partition_a";
    const char* privilege = "unprivileged";
    const char* runtime = "fake_rtos_runtime";
    uint32_t timer_query_calls = 0;
    uint32_t timer_read_calls = 0;
    uint32_t scheduler_calls = 0;
    uint32_t partition_calls = 0;
    uint32_t privilege_calls = 0;
    uint32_t runtime_calls = 0;
};

static int fake_detect(void*, vis_platform_profile_t* profile) {
    std::memset(profile, 0, sizeof(*profile));
    std::snprintf(profile->profile_version, sizeof(profile->profile_version), "%s",
                  VIS_PLATFORM_PROFILE_VERSION);
    std::snprintf(profile->arch, sizeof(profile->arch), "fake_target");
    std::snprintf(profile->os_family, sizeof(profile->os_family), "rtos");
    std::snprintf(profile->environment, sizeof(profile->environment), "target_runtime");
    profile->abi_bits = 64;
    std::snprintf(profile->selected_time_source, sizeof(profile->selected_time_source),
                  "posix_clock_monotonic");
    std::snprintf(profile->time_source_evidence_level, sizeof(profile->time_source_evidence_level),
                  "portable");
    profile->time_source_monotonic = true;
    std::snprintf(profile->claim_level, sizeof(profile->claim_level), "portable_user_space");
    profile->candidate_count = 1;
    vis_time_source_candidate_t* candidate = &profile->candidates[0];
    std::snprintf(candidate->name, sizeof(candidate->name), "posix_clock_monotonic");
    candidate->available = true;
    candidate->monotonic = true;
    std::snprintf(candidate->evidence_level, sizeof(candidate->evidence_level), "portable");
    std::snprintf(candidate->reason, sizeof(candidate->reason), "Fake POSIX fallback.");
    return 0;
}

static int fake_query_timer(void* opaque, vis_probe_timer_info_t* info) {
    fake_target_t* target = static_cast<fake_target_t*>(opaque);
    target->timer_query_calls++;
    if (target->timer_query_status != static_cast<int>(vis_probe_service_status_t::OK)) {
        return target->timer_query_status;
    }
    std::memset(info, 0, sizeof(*info));
    std::snprintf(info->name, sizeof(info->name), "fake_counter");
    info->frequency_hz = 1000000;
    std::snprintf(info->unit, sizeof(info->unit), "ticks");
    info->counter_width_bits = target->timer_width_bits;
    info->monotonic = target->timer_monotonic;
    info->wraps = true;
    std::snprintf(info->privilege_requirement, sizeof(info->privilege_requirement), "unprivileged");
    return static_cast<int>(vis_probe_service_status_t::OK);
}

static int fake_timer_read(void* opaque, uint64_t* value) {
    fake_target_t* target = static_cast<fake_target_t*>(opaque);
    target->timer_read_calls++;
    if (target->timer_read_status != static_cast<int>(vis_probe_service_status_t::OK)) {
        return target->timer_read_status;
    }
    if (value == nullptr) {
        return static_cast<int>(vis_probe_service_status_t::INVALID_ARG);
    }
    const uint32_t index = target->timer_read_index < 2 ? target->timer_read_index++ : 1;
    *value = target->timer_values[index];
    return static_cast<int>(vis_probe_service_status_t::OK);
}

static int copy_query_value(const char* value, int status, char* output, uint32_t output_size) {
    if (status != static_cast<int>(vis_probe_service_status_t::OK)) {
        return status;
    }
    if (output == nullptr || value == nullptr) {
        return static_cast<int>(vis_probe_service_status_t::INVALID_ARG);
    }
    if (std::strlen(value) + 1 > output_size) {
        return static_cast<int>(vis_probe_service_status_t::BUFFER_TOO_SMALL);
    }
    std::snprintf(output, output_size, "%s", value);
    return static_cast<int>(vis_probe_service_status_t::OK);
}

static int fake_query_scheduler(void* opaque, char* output, uint32_t output_size) {
    fake_target_t* target = static_cast<fake_target_t*>(opaque);
    target->scheduler_calls++;
    return copy_query_value(target->scheduler, target->scheduler_status, output, output_size);
}

static int fake_query_partition(void* opaque, char* output, uint32_t output_size) {
    fake_target_t* target = static_cast<fake_target_t*>(opaque);
    target->partition_calls++;
    return copy_query_value(target->partition, target->partition_status, output, output_size);
}

static int fake_query_privilege(void* opaque, char* output, uint32_t output_size) {
    fake_target_t* target = static_cast<fake_target_t*>(opaque);
    target->privilege_calls++;
    return copy_query_value(target->privilege, target->privilege_status, output, output_size);
}

static int fake_query_runtime(void* opaque, char* output, uint32_t output_size) {
    fake_target_t* target = static_cast<fake_target_t*>(opaque);
    target->runtime_calls++;
    return copy_query_value(target->runtime, target->runtime_status, output, output_size);
}

static vis_probe_services_t make_services(fake_target_t* target, vis_platform_adapter_t* adapter) {
    vis_probe_services_t services{};
    services.platform = adapter;
    services.target_context = target;
    services.query_scheduler = fake_query_scheduler;
    services.query_partition = fake_query_partition;
    services.query_privilege = fake_query_privilege;
    services.query_runtime = fake_query_runtime;
    services.api_version = VIS_PROBE_SERVICES_API_VERSION;
    services.struct_size = sizeof(services);
    services.timer_read = fake_timer_read;
    services.query_timer = fake_query_timer;
    services.target_profile = vis_probe_target_profile_t::GENERIC;
    return services;
}

static bool contains(const char* text, const char* expected) {
    return text != nullptr && std::strstr(text, expected) != nullptr;
}

static int require(bool condition, const char* message) {
    if (condition)
        return 0;
    std::fprintf(stderr, "[test] FAILED: %s\n", message);
    return 1;
}

static bool serialized_report_is_valid(const vis_probe_report_t* report,
                                       const char* required_text) {
    char* json = vis_probe_report_to_json(report);
    if (json == nullptr)
        return false;
    const bool contains_required =
        required_text == nullptr || std::strstr(json, required_text) != nullptr;
    vis_report_validation_result_t validation;
    const bool valid = vis_report_validate_json(json, &validation) && validation.errors.empty();
    std::free(json);
    return contains_required && valid;
}

int main() {
    vis_platform_adapter_t adapter{nullptr, fake_detect};
    const vis_probe_config_t explicit_config{vis_probe_backend_hint_t::TARGET_SERVICES_PROBE,
                                             nullptr};

    // 1. All target services are available.
    fake_target_t complete;
    vis_probe_services_t complete_services = make_services(&complete, &adapter);
    vis_probe_config_t config = explicit_config;
    config.services = &complete_services;
    vis_probe_report_t report;
    vis_probe_status_t status = vis_probe_run(&config, &report);
    if (require(
            status == vis_probe_status_t::VIS_PROBE_OK &&
                std::strcmp(report.probe_result.backend_status, "selected") == 0 &&
                std::strcmp(report.probe_result.evidence_level, "rtos_execution_surface") == 0 &&
                std::strcmp(report.probe_result.execution_evidence_level,
                            "rtos_execution_surface") == 0 &&
                report.probe_result.timer_frequency_hz == 1000000 &&
                std::strcmp(report.probe_result.timer_unit, "ticks") == 0 &&
                report.probe_result.timer_counter_width_bits == 32 &&
                report.probe_result.timer_wraps && complete.timer_query_calls == 1 &&
                std::strcmp(report.schema_version, VIS_PROBE_REPORT_SCHEMA_VERSION) == 0 &&
                std::strcmp(report.probe_result.timer_metadata_status, "reported_by_target") == 0 &&
                complete.timer_read_calls == 2 && complete.scheduler_calls == 1 &&
                complete.partition_calls == 1 && complete.privilege_calls == 1 &&
                complete.runtime_calls == 1 &&
                serialized_report_is_valid(&report, "\"timer_frequency_hz\": 1000000"),
            "complete target services were not collected")) {
        return 1;
    }

    // 2. Required timer callback is missing.
    fake_target_t missing_timer;
    vis_probe_services_t missing_timer_services = make_services(&missing_timer, &adapter);
    missing_timer_services.timer_read = nullptr;
    config.services = &missing_timer_services;
    status = vis_probe_run(&config, &report);
    if (require(
            status == vis_probe_status_t::VIS_PROBE_ERR_BACKEND_UNAVAILABLE &&
                std::strcmp(report.probe_result.backend_status, "recognized_api_missing") == 0 &&
                contains(report.probe_result.unsupported_reason, "callback_missing: timer_read") &&
                serialized_report_is_valid(&report, "\"timer_metadata_status\": \"not_collected\""),
            "missing timer callback was not distinguished")) {
        return 1;
    }

    // 3. An explicitly unavailable timer API differs from a missing callback.
    fake_target_t unavailable_timer;
    unavailable_timer.timer_query_status =
        static_cast<int>(vis_probe_service_status_t::UNAVAILABLE);
    vis_probe_services_t unavailable_timer_services = make_services(&unavailable_timer, &adapter);
    config.services = &unavailable_timer_services;
    status = vis_probe_run(&config, &report);
    if (require(status == vis_probe_status_t::VIS_PROBE_ERR_BACKEND_UNAVAILABLE &&
                    std::strcmp(report.probe_result.backend_status, "collection_failed") == 0 &&
                    contains(report.probe_result.unsupported_reason,
                             "runtime_api_missing: query_timer"),
                "unavailable timer API was not distinguished")) {
        return 1;
    }

    // 4. Timer callback reports a read failure.
    fake_target_t failed_timer;
    failed_timer.timer_read_status = static_cast<int>(vis_probe_service_status_t::READ_FAILED);
    vis_probe_services_t failed_timer_services = make_services(&failed_timer, &adapter);
    config.services = &failed_timer_services;
    status = vis_probe_run(&config, &report);
    if (require(status == vis_probe_status_t::VIS_PROBE_ERR_TARGET_SERVICE &&
                    std::strcmp(report.probe_result.backend_status, "collection_failed") == 0 &&
                    std::strcmp(report.probe_result.selected_backend, "target_services_probe") ==
                        0 &&
                    std::strcmp(report.target_contract.target_runtime_api_status,
                                "collection_not_reached") == 0 &&
                    contains(report.probe_result.unsupported_reason, "read_failed: timer_read"),
                "timer read failure was not distinguished")) {
        return 1;
    }

    // 5. Timer metadata declares a non-monotonic source.
    fake_target_t non_monotonic;
    non_monotonic.timer_monotonic = false;
    vis_probe_services_t non_monotonic_services = make_services(&non_monotonic, &adapter);
    config.services = &non_monotonic_services;
    status = vis_probe_run(&config, &report);
    if (require(status == vis_probe_status_t::VIS_PROBE_ERR_TARGET_SERVICE &&
                    contains(report.probe_result.unsupported_reason, "non_monotonic") &&
                    std::strcmp(report.probe_result.evidence_level, "contract_only") == 0,
                "non-monotonic timer opened a target claim")) {
        return 1;
    }

    // 6. Scheduler exists but the partition API is unavailable.
    fake_target_t missing_partition;
    missing_partition.partition_status = static_cast<int>(vis_probe_service_status_t::UNAVAILABLE);
    vis_probe_services_t missing_partition_services = make_services(&missing_partition, &adapter);
    missing_partition_services.target_profile = vis_probe_target_profile_t::ARINC653;
    config.services = &missing_partition_services;
    status = vis_probe_run(&config, &report);
    if (require(status == vis_probe_status_t::VIS_PROBE_OK &&
                    std::strcmp(report.probe_result.backend_status, "partial_evidence") == 0 &&
                    std::strcmp(report.probe_result.execution_evidence_level,
                                "partial_rtos_execution_surface") == 0 &&
                    contains(report.probe_result.unsupported_reason,
                             "runtime_api_missing: query_partition") &&
                    serialized_report_is_valid(&report,
                                               "\"evidence_level\": \"partial_target_evidence\""),
                "missing partition API was not reported as partial evidence")) {
        return 1;
    }

    // 7. Privilege query is denied.
    fake_target_t denied_privilege;
    denied_privilege.privilege_status =
        static_cast<int>(vis_probe_service_status_t::PERMISSION_DENIED);
    vis_probe_services_t denied_privilege_services = make_services(&denied_privilege, &adapter);
    config.services = &denied_privilege_services;
    status = vis_probe_run(&config, &report);
    if (require(status == vis_probe_status_t::VIS_PROBE_ERR_TARGET_SERVICE &&
                    std::strcmp(report.probe_result.backend_status, "permission_denied") == 0 &&
                    contains(report.probe_result.unsupported_reason,
                             "permission_denied: query_privilege"),
                "required permission denial was not fatal")) {
        return 1;
    }

    // 8. A query output does not fit the backend's fixed buffer.
    fake_target_t oversized_output;
    oversized_output.scheduler =
        "scheduler_output_that_is_deliberately_longer_than_the_report_buffer";
    vis_probe_services_t oversized_output_services = make_services(&oversized_output, &adapter);
    config.services = &oversized_output_services;
    status = vis_probe_run(&config, &report);
    if (require(status == vis_probe_status_t::VIS_PROBE_OK &&
                    std::strcmp(report.probe_result.backend_status, "partial_evidence") == 0 &&
                    contains(report.probe_result.unsupported_reason,
                             "buffer_too_small: query_scheduler"),
                "short query buffer was not distinguished")) {
        return 1;
    }

    // 9. AUTO falls back only when the target timer API is unavailable.
    fake_target_t auto_unavailable;
    auto_unavailable.timer_query_status = static_cast<int>(vis_probe_service_status_t::UNAVAILABLE);
    vis_probe_services_t auto_unavailable_services = make_services(&auto_unavailable, &adapter);
    const vis_probe_config_t auto_config{vis_probe_backend_hint_t::AUTO,
                                         &auto_unavailable_services};
    status = vis_probe_run(&auto_config, &report);
    if (require(status == vis_probe_status_t::VIS_PROBE_OK &&
                    std::strcmp(report.probe_result.selected_backend, "posix_generic") == 0 &&
                    std::strcmp(report.probe_result.evidence_level, "portable_user_space") == 0,
                "AUTO did not fall back after an unavailable target API")) {
        return 1;
    }

    // 10. Failed target candidate data does not leak into POSIX fallback.
    if (require(std::strcmp(report.generator, "vis-probe " VIS_PROBE_VERSION) == 0 &&
                    std::strcmp(report.schema_version, VIS_REPORT_SCHEMA_VERSION) == 0 &&
                    std::strcmp(report.target_contract.target_profile_family, "hosted_posix") ==
                        0 &&
                    std::strcmp(report.probe_result.unsupported_reason, "none") == 0 &&
                    std::strcmp(report.platform_profile.claim_level, "portable_user_space") == 0,
                "failed target candidate contaminated the fallback report")) {
        return 1;
    }

    // 11. A real target read failure is fatal in AUTO and remains visible.
    fake_target_t auto_failure;
    auto_failure.timer_read_status = static_cast<int>(vis_probe_service_status_t::READ_FAILED);
    vis_probe_services_t auto_failure_services = make_services(&auto_failure, &adapter);
    const vis_probe_config_t auto_failure_config{vis_probe_backend_hint_t::AUTO,
                                                 &auto_failure_services};
    status = vis_probe_run(&auto_failure_config, &report);
    if (require(status == vis_probe_status_t::VIS_PROBE_ERR_TARGET_SERVICE &&
                    std::strcmp(report.probe_result.selected_backend, "target_services_probe") ==
                        0 &&
                    contains(report.probe_result.unsupported_reason, "read_failed: timer_read"),
                "AUTO hid a target timer read failure behind POSIX")) {
        return 1;
    }

    // 12. Invalid target timer metadata is also fatal in AUTO.
    fake_target_t invalid_metadata;
    invalid_metadata.timer_width_bits = 0;
    vis_probe_services_t invalid_metadata_services = make_services(&invalid_metadata, &adapter);
    const vis_probe_config_t invalid_metadata_auto{vis_probe_backend_hint_t::AUTO,
                                                   &invalid_metadata_services};
    status = vis_probe_run(&invalid_metadata_auto, &report);
    if (require(status == vis_probe_status_t::VIS_PROBE_ERR_TARGET_SERVICE &&
                    std::strcmp(report.probe_result.selected_backend, "target_services_probe") ==
                        0 &&
                    contains(report.probe_result.unsupported_reason, "invalid_timer_metadata"),
                "AUTO hid invalid target timer metadata behind POSIX")) {
        return 1;
    }

    // 13. Partial target data never opens the strong target claim.
    config.services = &missing_partition_services;
    status = vis_probe_run(&config, &report);
    if (require(status == vis_probe_status_t::VIS_PROBE_OK &&
                    std::strcmp(report.probe_result.backend_status, "selected") != 0 &&
                    std::strcmp(report.probe_result.evidence_level, "rtos_execution_surface") !=
                        0 &&
                    std::strcmp(report.platform_profile.claim_level, "rtos_execution_surface") != 0,
                "partial target data opened a strong claim")) {
        return 1;
    }

    // 14. Partition is not required for a PSE53-like target.
    fake_target_t pse53;
    pse53.partition_status = static_cast<int>(vis_probe_service_status_t::UNAVAILABLE);
    vis_probe_services_t pse53_services = make_services(&pse53, &adapter);
    pse53_services.target_profile = vis_probe_target_profile_t::POSIX_PSE53;
    config.services = &pse53_services;
    status = vis_probe_run(&config, &report);
    if (require(status == vis_probe_status_t::VIS_PROBE_OK &&
                    std::strcmp(report.probe_result.backend_status, "selected") == 0 &&
                    std::strcmp(report.target_contract.target_profile_family, "posix_pse53") == 0 &&
                    std::strcmp(report.target_contract.target_partition_model,
                                "not_required_for_profile") == 0 &&
                    !contains(report.probe_result.unsupported_reason, "query_partition") &&
                    (report.probe_result.required_capabilities & VIS_PROBE_TARGET_CAP_PARTITION) ==
                        0,
                "PSE53-like target incorrectly required partition evidence")) {
        return 1;
    }

    // 15. Partition remains required for an ARINC653-like target.
    fake_target_t arinc653;
    arinc653.partition_status = static_cast<int>(vis_probe_service_status_t::UNAVAILABLE);
    vis_probe_services_t arinc653_services = make_services(&arinc653, &adapter);
    arinc653_services.target_profile = vis_probe_target_profile_t::ARINC653;
    config.services = &arinc653_services;
    status = vis_probe_run(&config, &report);
    if (require(status == vis_probe_status_t::VIS_PROBE_OK &&
                    std::strcmp(report.probe_result.backend_status, "partial_evidence") == 0 &&
                    std::strcmp(report.target_contract.target_profile_family, "arinc653") == 0 &&
                    contains(report.probe_result.unsupported_reason,
                             "runtime_api_missing: query_partition") &&
                    (report.probe_result.required_capabilities & VIS_PROBE_TARGET_CAP_PARTITION) !=
                        0,
                "ARINC653-like target did not require partition evidence")) {
        return 1;
    }

    // 16. Generic defaults do not require partition evidence.
    fake_target_t generic_default;
    generic_default.partition_status = static_cast<int>(vis_probe_service_status_t::UNAVAILABLE);
    vis_probe_services_t generic_default_services = make_services(&generic_default, &adapter);
    config.services = &generic_default_services;
    status = vis_probe_run(&config, &report);
    if (require(status == vis_probe_status_t::VIS_PROBE_OK &&
                    std::strcmp(report.probe_result.backend_status, "selected") == 0 &&
                    (report.probe_result.required_capabilities & VIS_PROBE_TARGET_CAP_PARTITION) ==
                        0 &&
                    std::strcmp(report.target_contract.target_partition_model,
                                "not_required_for_profile") == 0,
                "generic target incorrectly required partition evidence")) {
        return 1;
    }

    // 17. An explicit mask replaces profile defaults.
    fake_target_t explicit_mask;
    explicit_mask.partition_status = static_cast<int>(vis_probe_service_status_t::UNAVAILABLE);
    explicit_mask.privilege_status =
        static_cast<int>(vis_probe_service_status_t::PERMISSION_DENIED);
    vis_probe_services_t explicit_mask_services = make_services(&explicit_mask, &adapter);
    const uint32_t explicit_requirements =
        VIS_PROBE_TARGET_CAP_TIMER | VIS_PROBE_TARGET_CAP_SCHEDULER | VIS_PROBE_TARGET_CAP_RUNTIME;
    explicit_mask_services.required_capabilities = explicit_requirements;
    config.services = &explicit_mask_services;
    status = vis_probe_run(&config, &report);
    if (require(status == vis_probe_status_t::VIS_PROBE_OK &&
                    report.probe_result.required_capabilities == explicit_requirements &&
                    std::strcmp(report.probe_result.backend_status, "partial_evidence") == 0 &&
                    contains(report.probe_result.unsupported_reason,
                             "permission_denied: query_privilege") &&
                    !contains(report.probe_result.unsupported_reason, "query_partition"),
                "explicit capability mask precedence is wrong")) {
        return 1;
    }

    // 18. A hosted custom platform adapter alone is not target intent.
    fake_target_t hosted_adapter_only;
    hosted_adapter_only.timer_read_status =
        static_cast<int>(vis_probe_service_status_t::READ_FAILED);
    vis_probe_services_t hosted_services = make_services(&hosted_adapter_only, &adapter);
    hosted_services.target_profile = vis_probe_target_profile_t::UNSPECIFIED;
    const vis_probe_config_t hosted_auto{vis_probe_backend_hint_t::AUTO, &hosted_services};
    status = vis_probe_run(&hosted_auto, &report);
    if (require(status == vis_probe_status_t::VIS_PROBE_OK &&
                    std::strcmp(report.probe_result.selected_backend, "posix_generic") == 0 &&
                    hosted_adapter_only.timer_query_calls == 0 &&
                    hosted_adapter_only.timer_read_calls == 0,
                "custom hosted adapter accidentally activated target AUTO")) {
        return 1;
    }

    std::printf("[test] PASS: target-services probe works.\n");
    return 0;
}
