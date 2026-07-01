/**
 * vis_probe.cpp
 *
 * Portable VIS probe implementation.
 *
 * License: MIT
 */

#include "../include/vis_probe.hpp"

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include <unistd.h>
#ifdef __linux__
#include <sys/random.h>
#endif

static void copy_string(char* dst, size_t dst_size, const char* value) {
    if (dst == nullptr || dst_size == 0) return;
    dst[0] = '\0';
    if (value == nullptr) return;
    size_t i = 0;
    for (; i + 1 < dst_size && value[i] != '\0'; i++) {
        dst[i] = value[i];
    }
    dst[i] = '\0';
}

static std::string json_escape(const char* value) {
    std::string out;
    if (value == nullptr) return out;

    for (const unsigned char* p =
             reinterpret_cast<const unsigned char*>(value);
         *p != '\0'; p++) {
        switch (*p) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (*p < 0x20) {
                    char escaped[7];
                    std::snprintf(escaped, sizeof(escaped), "\\u%04x", *p);
                    out += escaped;
                } else {
                    out += static_cast<char>(*p);
                }
                break;
        }
    }

    return out;
}

template <typename... Args>
static void append_format(std::string* out, const char* format, Args... args) {
    int needed = std::snprintf(nullptr, 0, format, args...);
    if (needed <= 0) return;

    std::vector<char> buffer(static_cast<size_t>(needed) + 1);
    std::snprintf(buffer.data(), buffer.size(), format, args...);
    out->append(buffer.data(), static_cast<size_t>(needed));
}

static bool fill_random_bytes(uint8_t* bytes, size_t count) {
    if (bytes == nullptr) return false;

#ifdef __linux__
    size_t offset = 0;
    while (offset < count) {
        ssize_t n = getrandom(bytes + offset, count - offset, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;
        offset += static_cast<size_t>(n);
    }
    if (offset == count) return true;
#endif

    static std::atomic<uint64_t> fallback_counter{0};
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t counter = fallback_counter.fetch_add(1, std::memory_order_relaxed);
    uint64_t pid_bits = static_cast<uint64_t>(getpid()) << 32;
    uint64_t seed_a =
        (static_cast<uint64_t>(ts.tv_sec) << 32) ^
        static_cast<uint64_t>(ts.tv_nsec);
    uint64_t seed_b = pid_bits ^ counter;

    size_t copied = 0;
    while (copied < count) {
        const uint64_t value = copied < sizeof(seed_a) ? seed_a : seed_b;
        const size_t remaining = count - copied;
        const size_t chunk = remaining < sizeof(value) ? remaining : sizeof(value);
        std::memcpy(bytes + copied, &value, chunk);
        copied += chunk;
    }
    return true;
}

static void generate_uuid(char* buf, size_t buf_size) {
    uint8_t bytes[16] = {};
    fill_random_bytes(bytes, sizeof(bytes));

    bytes[6] = static_cast<uint8_t>((bytes[6] & 0x0f) | 0x40);
    bytes[8] = static_cast<uint8_t>((bytes[8] & 0x3f) | 0x80);

    uint32_t time_low =
        (static_cast<uint32_t>(bytes[0]) << 24) |
        (static_cast<uint32_t>(bytes[1]) << 16) |
        (static_cast<uint32_t>(bytes[2]) << 8) |
        static_cast<uint32_t>(bytes[3]);
    uint16_t time_mid =
        static_cast<uint16_t>((bytes[4] << 8) | bytes[5]);
    uint16_t time_hi =
        static_cast<uint16_t>((bytes[6] << 8) | bytes[7]);
    uint16_t clock_seq =
        static_cast<uint16_t>((bytes[8] << 8) | bytes[9]);
    uint64_t node =
        (static_cast<uint64_t>(bytes[10]) << 40) |
        (static_cast<uint64_t>(bytes[11]) << 32) |
        (static_cast<uint64_t>(bytes[12]) << 24) |
        (static_cast<uint64_t>(bytes[13]) << 16) |
        (static_cast<uint64_t>(bytes[14]) << 8) |
        static_cast<uint64_t>(bytes[15]);

    std::snprintf(buf, buf_size, "%08x-%04x-%04x-%04x-%012llx",
                  time_low, time_mid, time_hi, clock_seq,
                  static_cast<unsigned long long>(node));
}

static void generate_timestamp(char* buf, size_t buf_size) {
    time_t now = time(nullptr);
    struct tm* utc = gmtime(&now);
    if (utc == nullptr) {
        std::snprintf(buf, buf_size, "unknown");
        return;
    }
    strftime(buf, buf_size, "%Y-%m-%dT%H:%M:%SZ", utc);
}

static const char* probe_backend_name(vis_probe_backend_hint_t hint) {
    switch (hint) {
        case vis_probe_backend_hint_t::POSIX_GENERIC:
            return "posix_generic";
        case vis_probe_backend_hint_t::LINUX_X86_RDTSCP_MSR:
            return "linux_x86_rdtscp_msr";
        case vis_probe_backend_hint_t::ARM_GENERIC_TIMER:
            return "arm_generic_timer";
        case vis_probe_backend_hint_t::ARINC653_PARTITION_PROBE:
            return "arinc653_partition_probe";
        case vis_probe_backend_hint_t::POSIX_PSE53_PROBE:
            return "posix_pse53_probe";
        case vis_probe_backend_hint_t::AUTOSAR_ADAPTIVE_PROBE:
            return "autosar_adaptive_probe";
        case vis_probe_backend_hint_t::HYPERVISOR_PARTITION_PROBE:
            return "hypervisor_partition_probe";
        case vis_probe_backend_hint_t::AUTO:
        default:
            return "auto";
    }
}

static void fill_probe_result(vis_probe_result_t* result,
                              const char* selected_backend,
                              const char* backend_status,
                              const char* backend_status_reason,
                              const char* privilege_requirement,
                              const char* evidence_level,
                              const char* timer_evidence_level,
                              const char* execution_evidence_level,
                              const char* unsupported_reason,
                              const char* limitations,
                              const vis_platform_profile_t* platform) {
    if (result == nullptr || platform == nullptr) return;

    copy_string(result->selected_backend, sizeof(result->selected_backend),
                selected_backend);
    copy_string(result->backend_status, sizeof(result->backend_status),
                backend_status);
    copy_string(result->backend_status_reason,
                sizeof(result->backend_status_reason),
                backend_status_reason);
    copy_string(result->selected_time_source,
                sizeof(result->selected_time_source),
                platform->selected_time_source);
    result->time_source_monotonic = platform->time_source_monotonic;
    result->time_source_read_overhead_ns =
        platform->time_source_read_overhead_ns;
    copy_string(result->privilege_requirement,
                sizeof(result->privilege_requirement),
                privilege_requirement);
    copy_string(result->evidence_level, sizeof(result->evidence_level),
                evidence_level);
    copy_string(result->timer_evidence_level,
                sizeof(result->timer_evidence_level),
                timer_evidence_level);
    copy_string(result->execution_evidence_level,
                sizeof(result->execution_evidence_level),
                execution_evidence_level);
    copy_string(result->unsupported_reason,
                sizeof(result->unsupported_reason),
                unsupported_reason);
    copy_string(result->limitations, sizeof(result->limitations),
                limitations);
}

static void fill_target_contract(vis_target_contract_t* contract,
                                 const char* profile_family,
                                 const char* timer_model,
                                 const char* scheduler_model,
                                 const char* partition_model,
                                 const char* privilege_model,
                                 const char* runtime_api_status,
                                 const char* arinc653_surface,
                                 const char* posix_pse53_surface,
                                 const char* autosar_adaptive_surface,
                                 const char* hypervisor_surface,
                                 const char* limitations) {
    if (contract == nullptr) return;

    copy_string(contract->target_profile_family,
                sizeof(contract->target_profile_family),
                profile_family);
    copy_string(contract->target_timer_model,
                sizeof(contract->target_timer_model),
                timer_model);
    copy_string(contract->target_scheduler_model,
                sizeof(contract->target_scheduler_model),
                scheduler_model);
    copy_string(contract->target_partition_model,
                sizeof(contract->target_partition_model),
                partition_model);
    copy_string(contract->target_privilege_model,
                sizeof(contract->target_privilege_model),
                privilege_model);
    copy_string(contract->target_runtime_api_status,
                sizeof(contract->target_runtime_api_status),
                runtime_api_status);
    copy_string(contract->target_arinc653_surface,
                sizeof(contract->target_arinc653_surface),
                arinc653_surface);
    copy_string(contract->target_posix_pse53_surface,
                sizeof(contract->target_posix_pse53_surface),
                posix_pse53_surface);
    copy_string(contract->target_autosar_adaptive_surface,
                sizeof(contract->target_autosar_adaptive_surface),
                autosar_adaptive_surface);
    copy_string(contract->target_hypervisor_surface,
                sizeof(contract->target_hypervisor_surface),
                hypervisor_surface);
    copy_string(contract->target_limitations,
                sizeof(contract->target_limitations),
                limitations);
}

static bool select_platform_candidate(vis_platform_profile_t* profile,
                                      const char* candidate_name,
                                      const char* evidence_level_override,
                                      const char* claim_level,
                                      const char* limitations,
                                      const char* privileged_counters) {
    if (profile == nullptr || candidate_name == nullptr) return false;

    for (uint32_t i = 0; i < profile->candidate_count && i < 4; i++) {
        const vis_time_source_candidate_t* candidate = &profile->candidates[i];
        if (std::strcmp(candidate->name, candidate_name) != 0) continue;

        copy_string(profile->selected_time_source,
                    sizeof(profile->selected_time_source),
                    candidate->name);
        copy_string(profile->time_source_evidence_level,
                    sizeof(profile->time_source_evidence_level),
                    evidence_level_override != nullptr
                        ? evidence_level_override
                        : candidate->evidence_level);
        profile->time_source_read_overhead_ns = candidate->read_overhead_ns;
        profile->time_source_monotonic = candidate->monotonic;
        if (claim_level != nullptr) {
            copy_string(profile->claim_level, sizeof(profile->claim_level),
                        claim_level);
        }
        if (limitations != nullptr) {
            copy_string(profile->limitations, sizeof(profile->limitations),
                        limitations);
        }
        if (privileged_counters != nullptr) {
            copy_string(profile->privileged_counters,
                        sizeof(profile->privileged_counters),
                        privileged_counters);
        }
        return true;
    }
    return false;
}

static void fill_common_execution_profile(vis_execution_profile_t* execution,
                                          const vis_platform_profile_t* platform) {
    if (execution == nullptr || platform == nullptr) return;

    copy_string(execution->thread_model, sizeof(execution->thread_model),
                "single_process_threads");
    copy_string(execution->interrupt_visibility,
                sizeof(execution->interrupt_visibility),
                platform->interrupt_evidence);
    copy_string(execution->memory_visibility,
                sizeof(execution->memory_visibility),
                platform->memory_policy);

    if (std::strcmp(platform->environment, "linux_container") == 0) {
        copy_string(execution->execution_environment,
                    sizeof(execution->execution_environment),
                    "linux_container");
    } else if (std::strcmp(platform->os_family, "linux") == 0) {
        copy_string(execution->execution_environment,
                    sizeof(execution->execution_environment),
                    "linux_user_space");
    } else if (std::strcmp(platform->os_family, "posix") == 0 ||
               std::strcmp(platform->os_family, "android") == 0) {
        copy_string(execution->execution_environment,
                    sizeof(execution->execution_environment),
                    "posix_user_space");
    } else {
        copy_string(execution->execution_environment,
                    sizeof(execution->execution_environment),
                    "unknown");
    }

    copy_string(execution->partition_model, sizeof(execution->partition_model),
                platform->partitioning_hint);
    copy_string(execution->scheduler_surface,
                sizeof(execution->scheduler_surface), platform->scheduler_model);
    copy_string(execution->scheduling_scope,
                sizeof(execution->scheduling_scope),
                "process_local_user_space");
    copy_string(execution->affinity_surface,
                sizeof(execution->affinity_surface),
                platform->affinity_control);
    copy_string(execution->posix_surface,
                sizeof(execution->posix_surface),
                platform->posix_profile);
    copy_string(execution->arinc653_surface,
                sizeof(execution->arinc653_surface),
                platform->arinc653_surface);
    copy_string(execution->autosar_adaptive_surface,
                sizeof(execution->autosar_adaptive_surface),
                platform->autosar_adaptive_surface);
    copy_string(execution->hypervisor_surface,
                sizeof(execution->hypervisor_surface),
                platform->hypervisor_surface);
    copy_string(execution->runtime_isolation_model,
                sizeof(execution->runtime_isolation_model),
                platform->runtime_isolation_hint);
    copy_string(execution->portability_tier,
                sizeof(execution->portability_tier),
                "portable_probe_foundation");
}

static void fill_host_native_target_contract(
    vis_target_contract_t* contract,
    const vis_platform_profile_t* platform,
    const char* profile_family
) {
    if (contract == nullptr || platform == nullptr) return;

    fill_target_contract(
        contract,
        profile_family,
        platform->selected_time_source,
        platform->scheduler_model,
        platform->partitioning_hint,
        platform->privileged_counters,
        "host_native",
        "not_claimed",
        std::strcmp(platform->os_family, "linux") == 0 ||
                std::strcmp(platform->os_family, "posix") == 0 ||
                std::strcmp(platform->os_family, "android") == 0
            ? "host_posix_like"
            : "not_claimed",
        "not_claimed",
        "not_claimed",
        "Target contract matches the current hosted runtime.");
}

static vis_probe_status_t run_posix_generic_backend(vis_probe_report_t* report) {
    if (report == nullptr) return vis_probe_status_t::VIS_PROBE_ERR_INVALID_ARG;
    if (vis_platform_detect_profile(&report->platform_profile) < 0) {
        return vis_probe_status_t::VIS_PROBE_ERR_INVALID_ARG;
    }

    if (!select_platform_candidate(
            &report->platform_profile,
            "posix_clock_monotonic",
            "portable",
            "portable_user_space",
            "Portable probe records user-space timing evidence only; partitioned RTOS and architecture-specific counters require dedicated backends.",
            "platform_specific")) {
        return vis_probe_status_t::VIS_PROBE_ERR_NO_TIME_SOURCE;
    }

    fill_common_execution_profile(&report->execution_profile,
                                  &report->platform_profile);
    fill_host_native_target_contract(&report->target_contract,
                                     &report->platform_profile,
                                     "hosted_posix");
    fill_probe_result(&report->probe_result,
                      "posix_generic",
                      "selected",
                      "Portable POSIX timer path is active on this host.",
                      "none",
                      "portable_user_space",
                      report->platform_profile.time_source_evidence_level,
                      "portable_user_space",
                      "none",
                      report->platform_profile.limitations,
                      &report->platform_profile);
    return vis_probe_status_t::VIS_PROBE_OK;
}

static vis_probe_status_t run_arm_generic_timer_backend(
    vis_probe_report_t* report
) {
    if (report == nullptr) return vis_probe_status_t::VIS_PROBE_ERR_INVALID_ARG;
    if (vis_platform_detect_profile(&report->platform_profile) < 0) {
        return vis_probe_status_t::VIS_PROBE_ERR_INVALID_ARG;
    }

#if defined(__aarch64__)
    if (!select_platform_candidate(
            &report->platform_profile,
            "arm_cntvct_el0",
            "architecture_counter",
            "arm_generic_timer_evidence",
            "Portable probe records ARM generic timer evidence only; RTOS interrupt models and partition timing still require target-specific probe backends.",
            "el0_counter_no_privileged_attestation")) {
        return vis_probe_status_t::VIS_PROBE_ERR_BACKEND_UNAVAILABLE;
    }

    fill_common_execution_profile(&report->execution_profile,
                                  &report->platform_profile);
    fill_host_native_target_contract(&report->target_contract,
                                     &report->platform_profile,
                                     "hosted_aarch64");
    fill_probe_result(&report->probe_result,
                      "arm_generic_timer",
                      "selected",
                      "AArch64 EL0 generic timer path is active on this host.",
                      "none_for_el0_timer_read",
                      "arm_generic_timer_evidence",
                      report->platform_profile.time_source_evidence_level,
                      "portable_user_space",
                      "none",
                      report->platform_profile.limitations,
                      &report->platform_profile);
    return vis_probe_status_t::VIS_PROBE_OK;
#else
    (void)report;
    return vis_probe_status_t::VIS_PROBE_ERR_BACKEND_UNAVAILABLE;
#endif
}

static vis_probe_status_t run_linux_x86_backend(vis_probe_report_t* report) {
    if (report == nullptr) return vis_probe_status_t::VIS_PROBE_ERR_INVALID_ARG;
    if (vis_platform_detect_profile(&report->platform_profile) < 0) {
        return vis_probe_status_t::VIS_PROBE_ERR_INVALID_ARG;
    }

#if defined(__x86_64__) || defined(__i386__)
    if (!select_platform_candidate(
            &report->platform_profile,
            "x86_rdtscp",
            "architecture_counter",
            "linux_x86_rich_evidence",
            "Portable probe records timing-source evidence only; SMI-backed jitter measurement remains a vis-jitter workflow.",
            "msr_requires_root_or_cap_sys_rawio")) {
        return vis_probe_status_t::VIS_PROBE_ERR_BACKEND_UNAVAILABLE;
    }

    fill_common_execution_profile(&report->execution_profile,
                                  &report->platform_profile);
    fill_host_native_target_contract(&report->target_contract,
                                     &report->platform_profile,
                                     "hosted_linux_x86");
    fill_probe_result(&report->probe_result,
                      "linux_x86_rdtscp_msr",
                      "selected",
                      "Linux/x86 RDTSCP timing path is active on this host.",
                      "root_or_cap_sys_rawio_for_msr",
                      "linux_x86_rich_evidence",
                      report->platform_profile.time_source_evidence_level,
                      "portable_user_space",
                      "none",
                      report->platform_profile.limitations,
                      &report->platform_profile);
    return vis_probe_status_t::VIS_PROBE_OK;
#else
    (void)report;
    return vis_probe_status_t::VIS_PROBE_ERR_BACKEND_UNAVAILABLE;
#endif
}

static void override_execution_surface(vis_execution_profile_t* execution,
                                       const char* partition_model,
                                       const char* scheduler_surface,
                                       const char* posix_surface,
                                       const char* arinc653_surface,
                                       const char* autosar_surface,
                                       const char* hypervisor_surface,
                                       const char* runtime_isolation_model) {
    if (execution == nullptr) return;

    if (partition_model != nullptr) {
        copy_string(execution->partition_model,
                    sizeof(execution->partition_model),
                    partition_model);
    }
    if (scheduler_surface != nullptr) {
        copy_string(execution->scheduler_surface,
                    sizeof(execution->scheduler_surface),
                    scheduler_surface);
    }
    if (posix_surface != nullptr) {
        copy_string(execution->posix_surface,
                    sizeof(execution->posix_surface),
                    posix_surface);
    }
    if (arinc653_surface != nullptr) {
        copy_string(execution->arinc653_surface,
                    sizeof(execution->arinc653_surface),
                    arinc653_surface);
    }
    if (autosar_surface != nullptr) {
        copy_string(execution->autosar_adaptive_surface,
                    sizeof(execution->autosar_adaptive_surface),
                    autosar_surface);
    }
    if (hypervisor_surface != nullptr) {
        copy_string(execution->hypervisor_surface,
                    sizeof(execution->hypervisor_surface),
                    hypervisor_surface);
    }
    if (runtime_isolation_model != nullptr) {
        copy_string(execution->runtime_isolation_model,
                    sizeof(execution->runtime_isolation_model),
                    runtime_isolation_model);
    }
}

static vis_probe_status_t emit_contract_only_stub(
    vis_probe_report_t* report,
    const char* backend_name,
    const char* target_profile_family,
    const char* target_timer_model,
    const char* backend_status_reason,
    const char* unsupported_reason,
    const char* partition_model,
    const char* scheduler_surface,
    const char* posix_surface,
    const char* arinc653_surface,
    const char* autosar_surface,
    const char* hypervisor_surface,
    const char* runtime_isolation_model
) {
    if (report == nullptr) return vis_probe_status_t::VIS_PROBE_ERR_INVALID_ARG;
    if (vis_platform_detect_profile(&report->platform_profile) < 0) {
        return vis_probe_status_t::VIS_PROBE_ERR_INVALID_ARG;
    }

    fill_common_execution_profile(&report->execution_profile,
                                  &report->platform_profile);
    override_execution_surface(&report->execution_profile,
                               partition_model,
                               scheduler_surface,
                               posix_surface,
                               arinc653_surface,
                               autosar_surface,
                               hypervisor_surface,
                               runtime_isolation_model);
    copy_string(report->execution_profile.portability_tier,
                sizeof(report->execution_profile.portability_tier),
                "rtos_contract_stub");
    fill_target_contract(&report->target_contract,
                         target_profile_family,
                         target_timer_model,
                         scheduler_surface,
                         partition_model,
                         "target_vendor_api_required",
                         "recognized_api_missing",
                         arinc653_surface,
                         posix_surface,
                         autosar_surface,
                         hypervisor_surface,
                         "Target contract is modeled, but the required RTOS "
                         "or hypervisor API is unavailable on this hosted "
                         "runtime.");

    fill_probe_result(&report->probe_result,
                      backend_name,
                      "recognized_api_missing",
                      backend_status_reason,
                      "target_vendor_api_required",
                      "contract_only",
                      report->platform_profile.time_source_evidence_level,
                      "contract_only",
                      unsupported_reason,
                      "Requested RTOS-oriented backend was recognized, but "
                      "this hosted build does not expose the target runtime "
                      "API needed for partition/scheduler attestation.",
                      &report->platform_profile);
    copy_string(report->platform_profile.claim_level,
                sizeof(report->platform_profile.claim_level),
                "contract_only");
    copy_string(report->platform_profile.limitations,
                sizeof(report->platform_profile.limitations),
                report->probe_result.limitations);
    return vis_probe_status_t::VIS_PROBE_OK;
}

static vis_probe_status_t run_arinc653_partition_backend(
    vis_probe_report_t* report
) {
    return emit_contract_only_stub(
        report,
        "arinc653_partition_probe",
        "arinc653",
        "arinc653_partition_clock",
        "ARINC 653 partition services are not available on this hosted Linux "
        "build.",
        "target_runtime_api_missing: arinc653_partition_services",
        "arinc653_partition_contract",
        "partition_schedule_contract_only",
        "not_claimed",
        "contract_recognized_api_missing",
        "not_claimed",
        "not_claimed",
        "partition_isolation_not_attested");
}

static vis_probe_status_t run_posix_pse53_backend(vis_probe_report_t* report) {
    return emit_contract_only_stub(
        report,
        "posix_pse53_probe",
        "posix_pse53",
        "pse53_clock_api",
        "POSIX PSE53 scheduling and timer APIs are not exposed as a distinct "
        "target profile on this hosted build.",
        "target_runtime_api_missing: posix_pse53_profile",
        "process_contract_only",
        "pse53_contract_only",
        "pse53_contract_recognized_api_missing",
        "not_claimed",
        "not_claimed",
        "not_claimed",
        "rtos_runtime_isolation_not_attested");
}

static vis_probe_status_t run_autosar_adaptive_backend(
    vis_probe_report_t* report
) {
    return emit_contract_only_stub(
        report,
        "autosar_adaptive_probe",
        "autosar_adaptive",
        "adaptive_posix_clock",
        "AUTOSAR Adaptive execution management services are not available on "
        "this hosted build.",
        "target_runtime_api_missing: autosar_adaptive_execution_management",
        "adaptive_process_contract",
        "execution_management_contract_only",
        "posix_like_contract_only",
        "not_claimed",
        "contract_recognized_api_missing",
        "not_claimed",
        "adaptive_runtime_isolation_not_attested");
}

static vis_probe_status_t run_hypervisor_partition_backend(
    vis_probe_report_t* report
) {
    return emit_contract_only_stub(
        report,
        "hypervisor_partition_probe",
        "hypervisor_partition",
        "partition_timer_api",
        "Partition scheduler or virtual-machine control APIs are not exposed "
        "to this hosted user-space build.",
        "target_runtime_api_missing: hypervisor_partition_control",
        "virtual_partition_contract",
        "hypervisor_schedule_contract_only",
        "not_claimed",
        "not_claimed",
        "not_claimed",
        "contract_recognized_api_missing",
        "hypervisor_partition_isolation_not_attested");
}

struct vis_probe_backend_descriptor_t {
    vis_probe_backend_hint_t hint;
    bool auto_candidate;
    vis_probe_status_t (*run)(vis_probe_report_t* report);
};

vis_probe_status_t vis_probe_run(const vis_probe_config_t* config,
                                 vis_probe_report_t* report) {
    if (report == nullptr) {
        return vis_probe_status_t::VIS_PROBE_ERR_INVALID_ARG;
    }

    const vis_probe_config_t default_config{vis_probe_backend_hint_t::AUTO};
    const vis_probe_config_t* active = config != nullptr ? config : &default_config;
    std::memset(report, 0, sizeof(vis_probe_report_t));
    copy_string(report->schema_version, sizeof(report->schema_version),
                VIS_REPORT_SCHEMA_VERSION);
    copy_string(report->generator, sizeof(report->generator),
                "vis-probe " VIS_PROBE_VERSION);
    generate_uuid(report->report_id, sizeof(report->report_id));
    generate_timestamp(report->generated_at, sizeof(report->generated_at));

    const vis_probe_backend_descriptor_t descriptors[] = {
        {vis_probe_backend_hint_t::LINUX_X86_RDTSCP_MSR, true, run_linux_x86_backend},
        {vis_probe_backend_hint_t::ARM_GENERIC_TIMER, true, run_arm_generic_timer_backend},
        {vis_probe_backend_hint_t::POSIX_GENERIC, true, run_posix_generic_backend},
        {vis_probe_backend_hint_t::ARINC653_PARTITION_PROBE, false, run_arinc653_partition_backend},
        {vis_probe_backend_hint_t::POSIX_PSE53_PROBE, false, run_posix_pse53_backend},
        {vis_probe_backend_hint_t::AUTOSAR_ADAPTIVE_PROBE, false, run_autosar_adaptive_backend},
        {vis_probe_backend_hint_t::HYPERVISOR_PARTITION_PROBE, false, run_hypervisor_partition_backend},
    };

    auto run_backend = [&](vis_probe_backend_hint_t hint) {
        for (const vis_probe_backend_descriptor_t& descriptor : descriptors) {
            if (descriptor.hint == hint) {
                return descriptor.run(report);
            }
        }
        return vis_probe_status_t::VIS_PROBE_ERR_BACKEND_UNAVAILABLE;
    };

    vis_probe_status_t status =
        vis_probe_status_t::VIS_PROBE_ERR_BACKEND_UNAVAILABLE;
    if (active->backend_hint == vis_probe_backend_hint_t::AUTO) {
        for (const vis_probe_backend_descriptor_t& descriptor : descriptors) {
            if (!descriptor.auto_candidate) continue;
            status = descriptor.run(report);
            if (status == vis_probe_status_t::VIS_PROBE_OK) {
                break;
            }
        }
    } else {
        status = run_backend(active->backend_hint);
    }

    if (status != vis_probe_status_t::VIS_PROBE_OK) {
        copy_string(report->probe_result.selected_backend,
                    sizeof(report->probe_result.selected_backend),
                    probe_backend_name(active->backend_hint));
    }
    return status;
}

char* vis_probe_report_to_json(const vis_probe_report_t* report) {
    if (report == nullptr) return nullptr;

    const vis_platform_profile_t* p = &report->platform_profile;
    const vis_target_contract_t* t = &report->target_contract;
    const vis_execution_profile_t* e = &report->execution_profile;
    const vis_probe_result_t* r = &report->probe_result;

    std::string schema_version = json_escape(report->schema_version);
    std::string report_id = json_escape(report->report_id);
    std::string generated_at = json_escape(report->generated_at);
    std::string generator = json_escape(report->generator);
    std::string profile_version = json_escape(p->profile_version);
    std::string arch = json_escape(p->arch);
    std::string os_family = json_escape(p->os_family);
    std::string environment = json_escape(p->environment);
    std::string kernel_release = json_escape(p->kernel_release);
    std::string platform_fingerprint = json_escape(p->platform_fingerprint);
    std::string selected_time_source = json_escape(p->selected_time_source);
    std::string time_source_evidence_level =
        json_escape(p->time_source_evidence_level);
    std::string affinity_control = json_escape(p->affinity_control);
    std::string timer_access_model = json_escape(p->timer_access_model);
    std::string scheduler_model = json_escape(p->scheduler_model);
    std::string partitioning_hint = json_escape(p->partitioning_hint);
    std::string posix_profile = json_escape(p->posix_profile);
    std::string arinc653_surface = json_escape(p->arinc653_surface);
    std::string autosar_adaptive_surface =
        json_escape(p->autosar_adaptive_surface);
    std::string hypervisor_surface = json_escape(p->hypervisor_surface);
    std::string runtime_isolation_hint =
        json_escape(p->runtime_isolation_hint);
    std::string interrupt_evidence = json_escape(p->interrupt_evidence);
    std::string thermal_evidence = json_escape(p->thermal_evidence);
    std::string memory_policy = json_escape(p->memory_policy);
    std::string privileged_counters = json_escape(p->privileged_counters);
    std::string claim_level = json_escape(p->claim_level);
    std::string platform_limitations = json_escape(p->limitations);
    std::string target_profile_family = json_escape(t->target_profile_family);
    std::string target_timer_model = json_escape(t->target_timer_model);
    std::string target_scheduler_model =
        json_escape(t->target_scheduler_model);
    std::string target_partition_model =
        json_escape(t->target_partition_model);
    std::string target_privilege_model =
        json_escape(t->target_privilege_model);
    std::string target_runtime_api_status =
        json_escape(t->target_runtime_api_status);
    std::string target_arinc653_surface =
        json_escape(t->target_arinc653_surface);
    std::string target_posix_pse53_surface =
        json_escape(t->target_posix_pse53_surface);
    std::string target_autosar_adaptive_surface =
        json_escape(t->target_autosar_adaptive_surface);
    std::string target_hypervisor_surface =
        json_escape(t->target_hypervisor_surface);
    std::string target_limitations = json_escape(t->target_limitations);
    std::string execution_environment =
        json_escape(e->execution_environment);
    std::string partition_model = json_escape(e->partition_model);
    std::string scheduler_surface = json_escape(e->scheduler_surface);
    std::string scheduling_scope = json_escape(e->scheduling_scope);
    std::string affinity_surface = json_escape(e->affinity_surface);
    std::string thread_model = json_escape(e->thread_model);
    std::string posix_surface = json_escape(e->posix_surface);
    std::string execution_arinc653_surface = json_escape(e->arinc653_surface);
    std::string execution_autosar_surface =
        json_escape(e->autosar_adaptive_surface);
    std::string execution_hypervisor_surface =
        json_escape(e->hypervisor_surface);
    std::string runtime_isolation_model =
        json_escape(e->runtime_isolation_model);
    std::string portability_tier = json_escape(e->portability_tier);
    std::string interrupt_visibility = json_escape(e->interrupt_visibility);
    std::string memory_visibility = json_escape(e->memory_visibility);
    std::string backend = json_escape(r->selected_backend);
    std::string backend_status = json_escape(r->backend_status);
    std::string backend_status_reason = json_escape(r->backend_status_reason);
    std::string probe_selected_time_source =
        json_escape(r->selected_time_source);
    std::string privilege_requirement = json_escape(r->privilege_requirement);
    std::string evidence_level = json_escape(r->evidence_level);
    std::string timer_evidence_level = json_escape(r->timer_evidence_level);
    std::string execution_evidence_level =
        json_escape(r->execution_evidence_level);
    std::string unsupported_reason = json_escape(r->unsupported_reason);
    std::string probe_limitations = json_escape(r->limitations);

    std::string candidate_json;
    for (uint32_t i = 0; i < p->candidate_count && i < 4; i++) {
        const vis_time_source_candidate_t* c = &p->candidates[i];
        std::string name = json_escape(c->name);
        std::string candidate_level = json_escape(c->evidence_level);
        std::string reason = json_escape(c->reason);
        append_format(&candidate_json,
                      "%s"
                      "          {\"name\": \"%s\", \"available\": %s, "
                      "\"monotonic\": %s, \"read_overhead_ns\": %.1f, "
                      "\"evidence_level\": \"%s\", \"reason\": \"%s\"}",
                      i == 0 ? "" : ",\n",
                      name.c_str(),
                      c->available ? "true" : "false",
                      c->monotonic ? "true" : "false",
                      c->read_overhead_ns,
                      candidate_level.c_str(),
                      reason.c_str());
    }

    std::string json;
    append_format(
        &json,
        "{\n"
        "  \"vis_probe_report\": {\n"
        "    \"schema_version\": \"%s\",\n"
        "    \"report_id\": \"%s\",\n"
        "    \"generated_at\": \"%s\",\n"
        "    \"generator\": \"%s\",\n"
        "    \"evidence_level\": \"%s\",\n"
        "    \"limitations\": \"%s\",\n"
        "    \"platform_profile\": {\n"
        "      \"profile_version\": \"%s\",\n"
        "      \"arch\": \"%s\",\n"
        "      \"os_family\": \"%s\",\n"
        "      \"environment\": \"%s\",\n"
        "      \"abi_bits\": %u,\n"
        "      \"kernel_release\": \"%s\",\n"
        "      \"platform_fingerprint\": \"%s\",\n"
        "      \"selected_time_source\": \"%s\",\n"
        "      \"time_source_evidence_level\": \"%s\",\n"
        "      \"time_source_read_overhead_ns\": %.1f,\n"
        "      \"time_source_monotonic\": %s,\n"
        "      \"capabilities\": {\n"
        "        \"affinity_control\": \"%s\",\n"
        "        \"timer_access_model\": \"%s\",\n"
        "        \"scheduler_model\": \"%s\",\n"
        "        \"partitioning_hint\": \"%s\",\n"
        "        \"posix_profile\": \"%s\",\n"
        "        \"arinc653_surface\": \"%s\",\n"
        "        \"autosar_adaptive_surface\": \"%s\",\n"
        "        \"hypervisor_surface\": \"%s\",\n"
        "        \"runtime_isolation_hint\": \"%s\",\n"
        "        \"interrupt_evidence\": \"%s\",\n"
        "        \"thermal_evidence\": \"%s\",\n"
        "        \"memory_policy\": \"%s\",\n"
        "        \"privileged_counters\": \"%s\"\n"
        "      },\n"
        "      \"claim_level\": \"%s\",\n"
        "      \"limitations\": \"%s\",\n"
        "      \"time_source_candidates\": [\n"
        "%s\n"
        "      ]\n"
        "    },\n"
        "    \"target_contract\": {\n"
        "      \"target_profile_family\": \"%s\",\n"
        "      \"target_timer_model\": \"%s\",\n"
        "      \"target_scheduler_model\": \"%s\",\n"
        "      \"target_partition_model\": \"%s\",\n"
        "      \"target_privilege_model\": \"%s\",\n"
        "      \"target_runtime_api_status\": \"%s\",\n"
        "      \"target_arinc653_surface\": \"%s\",\n"
        "      \"target_posix_pse53_surface\": \"%s\",\n"
        "      \"target_autosar_adaptive_surface\": \"%s\",\n"
        "      \"target_hypervisor_surface\": \"%s\",\n"
        "      \"target_limitations\": \"%s\"\n"
        "    },\n"
        "    \"execution_profile\": {\n"
        "      \"execution_environment\": \"%s\",\n"
        "      \"partition_model\": \"%s\",\n"
        "      \"scheduler_surface\": \"%s\",\n"
        "      \"scheduling_scope\": \"%s\",\n"
        "      \"affinity_surface\": \"%s\",\n"
        "      \"thread_model\": \"%s\",\n"
        "      \"posix_surface\": \"%s\",\n"
        "      \"arinc653_surface\": \"%s\",\n"
        "      \"autosar_adaptive_surface\": \"%s\",\n"
        "      \"hypervisor_surface\": \"%s\",\n"
        "      \"runtime_isolation_model\": \"%s\",\n"
        "      \"portability_tier\": \"%s\",\n"
        "      \"interrupt_visibility\": \"%s\",\n"
        "      \"memory_visibility\": \"%s\"\n"
        "    },\n"
        "    \"probe_result\": {\n"
        "      \"selected_backend\": \"%s\",\n"
        "      \"backend_status\": \"%s\",\n"
        "      \"backend_status_reason\": \"%s\",\n"
        "      \"selected_time_source\": \"%s\",\n"
        "      \"time_source_monotonic\": %s,\n"
        "      \"time_source_read_overhead_ns\": %.1f,\n"
        "      \"privilege_requirement\": \"%s\",\n"
        "      \"timer_evidence_level\": \"%s\",\n"
        "      \"execution_evidence_level\": \"%s\",\n"
        "      \"unsupported_reason\": \"%s\"\n"
        "    }\n"
        "  }\n"
        "}\n",
        schema_version.c_str(),
        report_id.c_str(),
        generated_at.c_str(),
        generator.c_str(),
        evidence_level.c_str(),
        probe_limitations.c_str(),
        profile_version.c_str(),
        arch.c_str(),
        os_family.c_str(),
        environment.c_str(),
        p->abi_bits,
        kernel_release.c_str(),
        platform_fingerprint.c_str(),
        selected_time_source.c_str(),
        time_source_evidence_level.c_str(),
        p->time_source_read_overhead_ns,
        p->time_source_monotonic ? "true" : "false",
        affinity_control.c_str(),
        timer_access_model.c_str(),
        scheduler_model.c_str(),
        partitioning_hint.c_str(),
        posix_profile.c_str(),
        arinc653_surface.c_str(),
        autosar_adaptive_surface.c_str(),
        hypervisor_surface.c_str(),
        runtime_isolation_hint.c_str(),
        interrupt_evidence.c_str(),
        thermal_evidence.c_str(),
        memory_policy.c_str(),
        privileged_counters.c_str(),
        claim_level.c_str(),
        platform_limitations.c_str(),
        candidate_json.c_str(),
        target_profile_family.c_str(),
        target_timer_model.c_str(),
        target_scheduler_model.c_str(),
        target_partition_model.c_str(),
        target_privilege_model.c_str(),
        target_runtime_api_status.c_str(),
        target_arinc653_surface.c_str(),
        target_posix_pse53_surface.c_str(),
        target_autosar_adaptive_surface.c_str(),
        target_hypervisor_surface.c_str(),
        target_limitations.c_str(),
        execution_environment.c_str(),
        partition_model.c_str(),
        scheduler_surface.c_str(),
        scheduling_scope.c_str(),
        affinity_surface.c_str(),
        thread_model.c_str(),
        posix_surface.c_str(),
        execution_arinc653_surface.c_str(),
        execution_autosar_surface.c_str(),
        execution_hypervisor_surface.c_str(),
        runtime_isolation_model.c_str(),
        portability_tier.c_str(),
        interrupt_visibility.c_str(),
        memory_visibility.c_str(),
        backend.c_str(),
        backend_status.c_str(),
        backend_status_reason.c_str(),
        probe_selected_time_source.c_str(),
        r->time_source_monotonic ? "true" : "false",
        r->time_source_read_overhead_ns,
        privilege_requirement.c_str(),
        timer_evidence_level.c_str(),
        execution_evidence_level.c_str(),
        unsupported_reason.c_str());

    char* out = static_cast<char*>(std::malloc(json.size() + 1));
    if (out == nullptr) return nullptr;
    std::memcpy(out, json.c_str(), json.size() + 1);
    return out;
}

void vis_probe_report_print_summary(const vis_probe_report_t* report) {
    if (report == nullptr) return;

    const vis_platform_profile_t* p = &report->platform_profile;
    const vis_target_contract_t* t = &report->target_contract;
    const vis_execution_profile_t* e = &report->execution_profile;
    const vis_probe_result_t* r = &report->probe_result;

    std::printf("\n");
    std::printf("========================================\n");
    std::printf(" vis-probe report\n");
    std::printf("========================================\n");
    std::printf(" Generated : %s\n", report->generated_at);
    std::printf(" Report ID : %s\n", report->report_id);
    std::printf("----------------------------------------\n");
    std::printf(" Platform profile\n");
    std::printf("   Arch        : %s\n", p->arch);
    std::printf("   OS/env      : %s / %s\n", p->os_family, p->environment);
    std::printf("   ABI bits    : %u\n", p->abi_bits);
    std::printf("   Backend     : %s\n", r->selected_backend);
    std::printf("   Status      : %s\n", r->backend_status);
    std::printf("   Time source : %s (%s)\n",
                p->selected_time_source, p->time_source_evidence_level);
    std::printf("   Claim level : %s\n", r->evidence_level);
    std::printf("----------------------------------------\n");
    std::printf(" Target contract\n");
    std::printf("   Family      : %s\n", t->target_profile_family);
    std::printf("   Timer model : %s\n", t->target_timer_model);
    std::printf("   Runtime API : %s\n", t->target_runtime_api_status);
    std::printf("   Partition   : %s\n", t->target_partition_model);
    std::printf("----------------------------------------\n");
    std::printf(" Execution profile\n");
    std::printf("   Environment : %s\n", e->execution_environment);
    std::printf("   Partition   : %s\n", e->partition_model);
    std::printf("   Scheduler   : %s\n", e->scheduler_surface);
    std::printf("   POSIX surf. : %s\n", e->posix_surface);
    std::printf("   ARINC surf. : %s\n", e->arinc653_surface);
    std::printf("   Hypervisor  : %s\n", e->hypervisor_surface);
    std::printf("   Affinity    : %s\n", e->affinity_surface);
    std::printf("----------------------------------------\n");
    std::printf(" Probe result\n");
    std::printf("   Timer evid. : %s\n", r->timer_evidence_level);
    std::printf("   Exec evid.  : %s\n", r->execution_evidence_level);
    std::printf("   Monotonic   : %s\n", r->time_source_monotonic ? "yes" : "no");
    std::printf("   Overhead    : %.1f ns\n", r->time_source_read_overhead_ns);
    std::printf("   Privilege   : %s\n", r->privilege_requirement);
    std::printf("   Reason      : %s\n", r->backend_status_reason);
    if (std::strcmp(r->unsupported_reason, "none") != 0 &&
        r->unsupported_reason[0] != '\0') {
        std::printf("   Unsupported : %s\n", r->unsupported_reason);
    }
    std::printf("   Limitations : %s\n", r->limitations);
    std::printf("========================================\n\n");
}
