/**
 * vis_platform.cpp
 *
 * Portable VIS Core platform profile detection.
 *
 * License: MIT
 */

#include "../include/vis_platform.hpp"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <cerrno>

#include <unistd.h>
#include <sys/utsname.h>

#if defined(__aarch64__)
#include <sys/wait.h>
#endif

#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif

static void copy_string(char* dst, size_t dst_size, const char* value) {
    if (dst == nullptr || dst_size == 0) {
        return;
    }
    dst[0] = '\0';
    if (value == nullptr) {
        return;
    }
    size_t i = 0;
    for (; i + 1 < dst_size && value[i] != '\0'; i++) {
        dst[i] = value[i];
    }
    dst[i] = '\0';
}

static const char* detect_arch() {
#if defined(__x86_64__)
    return "x86_64";
#elif defined(__i386__)
    return "x86";
#elif defined(__aarch64__)
    return "aarch64";
#elif defined(__arm__)
    return "arm";
#elif defined(__powerpc64__) || defined(__ppc64__)
    return "powerpc64";
#elif defined(__powerpc__) || defined(__ppc__)
    return "powerpc";
#elif defined(__riscv) && (__riscv_xlen == 64)
    return "riscv64";
#elif defined(__riscv)
    return "riscv";
#else
    return "unknown";
#endif
}

static const char* detect_os_family() {
#if defined(__ANDROID__)
    return "android";
#elif defined(__linux__)
    return "linux";
#elif defined(__unix__)
    return "posix";
#else
    return "unknown";
#endif
}

static const char* detect_environment() {
#if defined(__ANDROID__)
    return "android";
#elif defined(__linux__)
    if (access("/.dockerenv", F_OK) == 0) {
        return "linux_container";
    }
    return "linux_user_space";
#elif defined(__unix__)
    return "posix_user_space";
#else
    return "unknown";
#endif
}

static const char* detect_hypervisor_surface() {
#if defined(__linux__)
    // An empty /sys/hypervisor directory is present on some bare-metal hosts.
    // Require a concrete interface entry before claiming hypervisor evidence.
    if (access("/sys/hypervisor/type", F_OK) == 0 ||
        access("/sys/hypervisor/properties", F_OK) == 0 ||
        access("/sys/hypervisor/version", F_OK) == 0) {
        return "linux_sysfs_hypervisor_present";
    }
    return "not_detected_rootless";
#else
    return "vendor_probe_required";
#endif
}

static uint64_t fnv1a64(const char* text, uint64_t hash) {
    if (text == nullptr) {
        return hash;
    }
    while (*text != '\0') {
        hash ^= static_cast<unsigned char>(*text);
        hash *= 1099511628211ull;
        text++;
    }
    return hash;
}

static void fill_platform_fingerprint(vis_platform_profile_t* profile) {
    uint64_t hash = 1469598103934665603ull;
    hash = fnv1a64(profile->arch, hash);
    hash = fnv1a64(profile->os_family, hash);
    hash = fnv1a64(profile->environment, hash);
    hash = fnv1a64(profile->kernel_release, hash);

    char abi[16];
    std::snprintf(abi, sizeof(abi), "%u", profile->abi_bits);
    hash = fnv1a64(abi, hash);

    std::snprintf(profile->platform_fingerprint,
                  sizeof(profile->platform_fingerprint),
                  "%016llx",
                  static_cast<unsigned long long>(hash));
}

static double monotonic_seconds(const struct timespec& ts) {
    return static_cast<double>(ts.tv_sec) +
           static_cast<double>(ts.tv_nsec) / 1e9;
}

static double measure_read_overhead_ns(void (*read_fn)()) {
    struct timespec start;
    struct timespec end;
    constexpr uint32_t iterations = 10000;

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (uint32_t i = 0; i < iterations; i++) {
        read_fn();
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed = monotonic_seconds(end) - monotonic_seconds(start);
    if (elapsed <= 0.0) {
        return 0.0;
    }
    return (elapsed * 1e9) / static_cast<double>(iterations);
}

static void posix_clock_read_once() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
}

static bool posix_clock_monotonic() {
    struct timespec a;
    struct timespec b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    clock_gettime(CLOCK_MONOTONIC, &b);
    if (b.tv_sec > a.tv_sec) {
        return true;
    }
    return b.tv_sec == a.tv_sec && b.tv_nsec >= a.tv_nsec;
}

#if defined(__x86_64__) || defined(__i386__)
static bool x86_rdtscp_supported() {
    uint32_t eax = 0;
    uint32_t ebx = 0;
    uint32_t ecx = 0;
    uint32_t edx = 0;
    if (!__get_cpuid(0x80000001, &eax, &ebx, &ecx, &edx)) {
        return false;
    }
    return ((edx >> 27) & 1u) != 0;
}

static bool x86_invariant_tsc_supported() {
    uint32_t eax = 0;
    uint32_t ebx = 0;
    uint32_t ecx = 0;
    uint32_t edx = 0;
    if (!__get_cpuid(0x80000007, &eax, &ebx, &ecx, &edx)) {
        return false;
    }
    return ((edx >> 8) & 1u) != 0;
}

static inline uint64_t x86_rdtscp_read(uint32_t* aux) {
    uint32_t lo = 0;
    uint32_t hi = 0;
    uint32_t core = 0;
    __asm__ volatile (
        "rdtscp"
        : "=a"(lo), "=d"(hi), "=c"(core)
        :
        : "memory"
    );
    if (aux != nullptr) {
        *aux = core;
    }
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

static void x86_rdtscp_read_once() {
    uint32_t aux = 0;
    (void)x86_rdtscp_read(&aux);
}

static bool x86_rdtscp_stable_on_current_cpu() {
    uint32_t initial_aux = 0;
    uint64_t previous = x86_rdtscp_read(&initial_aux);
    constexpr uint32_t validation_reads = 1024;

    for (uint32_t i = 0; i < validation_reads; i++) {
        uint32_t aux = 0;
        uint64_t current = x86_rdtscp_read(&aux);
        if (aux != initial_aux || current < previous) {
            return false;
        }
        previous = current;
    }
    return true;
}
#endif

#if defined(__aarch64__)
static inline uint64_t arm_cntvct_read() {
    uint64_t value = 0;
    __asm__ volatile("isb" ::: "memory");
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(value));
    return value;
}

static void arm_cntvct_read_once() {
    (void)arm_cntvct_read();
}

static bool arm_cntvct_monotonic() {
    uint64_t a = arm_cntvct_read();
    uint64_t b = arm_cntvct_read();
    return b >= a;
}

static bool arm_cntvct_accessible() {
    // Some kernels and hypervisors trap CNTVCT_EL0 reads. Probe in a child so
    // SIGILL cannot terminate the portable probe itself.
    pid_t child = fork();
    if (child < 0) {
        return false;
    }
    if (child == 0) {
        (void)arm_cntvct_read();
        _exit(0);
    }

    int status = 0;
    pid_t waited = 0;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);

    return waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
#endif

static vis_time_source_candidate_t* next_candidate(
    vis_platform_profile_t* profile
) {
    if (profile->candidate_count >= 4) {
        return nullptr;
    }
    return &profile->candidates[profile->candidate_count++];
}

static void add_posix_candidate(vis_platform_profile_t* profile) {
    vis_time_source_candidate_t* candidate = next_candidate(profile);
    if (candidate == nullptr) {
        return;
    }

    copy_string(candidate->name, sizeof(candidate->name),
                "posix_clock_monotonic");
    candidate->available = true;
    candidate->monotonic = posix_clock_monotonic();
    candidate->read_overhead_ns = measure_read_overhead_ns(posix_clock_read_once);
    copy_string(candidate->evidence_level, sizeof(candidate->evidence_level),
                "portable");
    copy_string(candidate->reason, sizeof(candidate->reason),
                "Portable POSIX fallback timer available in user space.");
}

static void add_arch_counter_candidate(vis_platform_profile_t* profile) {
    vis_time_source_candidate_t* candidate = next_candidate(profile);
    if (candidate == nullptr) {
        return;
    }

#if defined(__x86_64__) || defined(__i386__)
    copy_string(candidate->name, sizeof(candidate->name), "x86_rdtscp");
    const bool instruction_available = x86_rdtscp_supported();
    const bool invariant_tsc = x86_invariant_tsc_supported();
    const bool stable_read = instruction_available && invariant_tsc &&
                             x86_rdtscp_stable_on_current_cpu();
    candidate->available = stable_read;
    if (candidate->available) {
        candidate->monotonic = true;
        candidate->read_overhead_ns =
            measure_read_overhead_ns(x86_rdtscp_read_once);
        copy_string(candidate->evidence_level, sizeof(candidate->evidence_level),
                    "architecture_counter");
        copy_string(candidate->reason, sizeof(candidate->reason),
                    "CPUID reports RDTSCP and invariant TSC support; repeated "
                    "reads were monotonic without CPU migration.");
    } else {
        candidate->monotonic = false;
        candidate->read_overhead_ns = 0.0;
        copy_string(candidate->evidence_level, sizeof(candidate->evidence_level),
                    "unavailable");
        if (!instruction_available) {
            copy_string(candidate->reason, sizeof(candidate->reason),
                        "CPUID does not report RDTSCP support.");
        } else if (!invariant_tsc) {
            copy_string(candidate->reason, sizeof(candidate->reason),
                        "CPUID does not report an invariant TSC.");
        } else {
            copy_string(candidate->reason, sizeof(candidate->reason),
                        "RDTSCP validation observed CPU migration or a "
                        "non-monotonic counter.");
        }
    }
#elif defined(__aarch64__)
    copy_string(candidate->name, sizeof(candidate->name), "arm_cntvct_el0");
    candidate->available = arm_cntvct_accessible();
    if (candidate->available) {
        candidate->monotonic = arm_cntvct_monotonic();
        candidate->read_overhead_ns =
            measure_read_overhead_ns(arm_cntvct_read_once);
        copy_string(candidate->evidence_level, sizeof(candidate->evidence_level),
                    "architecture_counter");
        copy_string(candidate->reason, sizeof(candidate->reason),
                    "AArch64 generic timer is readable from EL0.");
    } else {
        candidate->monotonic = false;
        candidate->read_overhead_ns = 0.0;
        copy_string(candidate->evidence_level, sizeof(candidate->evidence_level),
                    "unavailable");
        copy_string(candidate->reason, sizeof(candidate->reason),
                    "CNTVCT_EL0 access is disabled or could not be probed safely.");
    }
#elif defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc__) || defined(__ppc64__)
    copy_string(candidate->name, sizeof(candidate->name), "powerpc_time_base");
    candidate->available = false;
    candidate->monotonic = false;
    candidate->read_overhead_ns = 0.0;
    copy_string(candidate->evidence_level, sizeof(candidate->evidence_level),
                "not_probed");
    copy_string(candidate->reason, sizeof(candidate->reason),
                "PowerPC time-base backend requires target-specific validation.");
#elif defined(__riscv)
    copy_string(candidate->name, sizeof(candidate->name), "riscv_time_or_cycle");
    candidate->available = false;
    candidate->monotonic = false;
    candidate->read_overhead_ns = 0.0;
    copy_string(candidate->evidence_level, sizeof(candidate->evidence_level),
                "not_probed");
    copy_string(candidate->reason, sizeof(candidate->reason),
                "RISC-V counters may be disabled by kernel counter policy.");
#else
    copy_string(candidate->name, sizeof(candidate->name), "arch_counter");
    candidate->available = false;
    candidate->monotonic = false;
    candidate->read_overhead_ns = 0.0;
    copy_string(candidate->evidence_level, sizeof(candidate->evidence_level),
                "unavailable");
    copy_string(candidate->reason, sizeof(candidate->reason),
                "No architecture-specific counter backend is known to this build.");
#endif
}

static void select_time_source(vis_platform_profile_t* profile) {
    add_arch_counter_candidate(profile);
    add_posix_candidate(profile);

    const vis_time_source_candidate_t* selected = nullptr;
    for (uint32_t i = 0; i < profile->candidate_count; i++) {
        const vis_time_source_candidate_t* candidate = &profile->candidates[i];
        if (candidate->available && candidate->monotonic) {
            selected = candidate;
            break;
        }
    }

    if (selected == nullptr && profile->candidate_count > 0) {
        selected = &profile->candidates[profile->candidate_count - 1];
    }

    if (selected != nullptr) {
        copy_string(profile->selected_time_source,
                    sizeof(profile->selected_time_source),
                    selected->name);
        copy_string(profile->time_source_evidence_level,
                    sizeof(profile->time_source_evidence_level),
                    selected->evidence_level);
        profile->time_source_read_overhead_ns = selected->read_overhead_ns;
        profile->time_source_monotonic = selected->monotonic;
    }
}

static int hosted_detect_profile(void*, vis_platform_profile_t* profile) {
    if (profile == nullptr) {
        return -1;
    }

    std::memset(profile, 0, sizeof(vis_platform_profile_t));

    copy_string(profile->profile_version, sizeof(profile->profile_version),
                VIS_PLATFORM_PROFILE_VERSION);
    copy_string(profile->arch, sizeof(profile->arch), detect_arch());
    copy_string(profile->os_family, sizeof(profile->os_family),
                detect_os_family());
    copy_string(profile->environment, sizeof(profile->environment),
                detect_environment());
    profile->abi_bits = static_cast<uint32_t>(sizeof(void*) * 8);

    struct utsname uts;
    if (uname(&uts) == 0) {
        copy_string(profile->kernel_release, sizeof(profile->kernel_release),
                    uts.release);
    } else {
        copy_string(profile->kernel_release, sizeof(profile->kernel_release),
                    "unknown");
    }

    select_time_source(profile);
    copy_string(profile->hypervisor_surface,
                sizeof(profile->hypervisor_surface),
                detect_hypervisor_surface());

#if defined(__linux__)
    copy_string(profile->affinity_control, sizeof(profile->affinity_control),
                "pthread_affinity_available");
    copy_string(profile->scheduler_model, sizeof(profile->scheduler_model),
                "linux_sched_attr_or_pthreads");
    copy_string(profile->partitioning_hint, sizeof(profile->partitioning_hint),
                "process_or_container_isolation");
    copy_string(profile->posix_profile, sizeof(profile->posix_profile),
                "posix_like_not_pse53_claimed");
    copy_string(profile->arinc653_surface, sizeof(profile->arinc653_surface),
                "not_present_in_hosted_linux_build");
    copy_string(profile->autosar_adaptive_surface,
                sizeof(profile->autosar_adaptive_surface),
                "posix_like_user_space_candidate");
    copy_string(profile->runtime_isolation_hint,
                sizeof(profile->runtime_isolation_hint),
                "shared_host_scheduler_runtime");
    copy_string(profile->interrupt_evidence, sizeof(profile->interrupt_evidence),
                "linux_proc_interrupts_optional");
    copy_string(profile->thermal_evidence, sizeof(profile->thermal_evidence),
                "linux_sysfs_optional");
    copy_string(profile->memory_policy, sizeof(profile->memory_policy),
                "linux_numa_optional");
#else
    copy_string(profile->affinity_control, sizeof(profile->affinity_control),
                "unknown_or_vendor_api");
    copy_string(profile->scheduler_model, sizeof(profile->scheduler_model),
                "vendor_scheduler_api");
    copy_string(profile->partitioning_hint, sizeof(profile->partitioning_hint),
                "vendor_partition_model");
    copy_string(profile->posix_profile, sizeof(profile->posix_profile),
                "vendor_posix_profile");
    copy_string(profile->arinc653_surface, sizeof(profile->arinc653_surface),
                "vendor_probe_required");
    copy_string(profile->autosar_adaptive_surface,
                sizeof(profile->autosar_adaptive_surface),
                "vendor_probe_required");
    copy_string(profile->runtime_isolation_hint,
                sizeof(profile->runtime_isolation_hint),
                "runtime_isolation_unknown");
    copy_string(profile->interrupt_evidence, sizeof(profile->interrupt_evidence),
                "unavailable");
    copy_string(profile->thermal_evidence, sizeof(profile->thermal_evidence),
                "unavailable");
    copy_string(profile->memory_policy, sizeof(profile->memory_policy),
                "unavailable");
#endif

    if (std::strcmp(profile->selected_time_source, "x86_rdtscp") == 0 ||
        std::strcmp(profile->selected_time_source, "arm_cntvct_el0") == 0) {
        copy_string(profile->timer_access_model,
                    sizeof(profile->timer_access_model),
                    "user_space_arch_counter");
    } else {
        copy_string(profile->timer_access_model,
                    sizeof(profile->timer_access_model),
                    "posix_clock_api");
    }

#if defined(__x86_64__) || defined(__i386__)
    copy_string(profile->privileged_counters,
                sizeof(profile->privileged_counters),
                "msr_requires_root_or_cap_sys_rawio");
#elif defined(__aarch64__)
    copy_string(profile->privileged_counters,
                sizeof(profile->privileged_counters),
                "el0_counter_no_privileged_attestation");
#else
    copy_string(profile->privileged_counters,
                sizeof(profile->privileged_counters),
                "platform_specific");
#endif

    if (std::strcmp(profile->os_family, "linux") == 0 &&
        std::strcmp(profile->selected_time_source, "x86_rdtscp") == 0) {
        copy_string(profile->claim_level, sizeof(profile->claim_level),
                    "linux_x86_rich_evidence");
        copy_string(profile->limitations, sizeof(profile->limitations),
                    "Portable profile records capability evidence; SMI/MSR evidence remains Linux x86 specific.");
    } else if (std::strcmp(profile->selected_time_source,
                           "arm_cntvct_el0") == 0) {
        copy_string(profile->claim_level, sizeof(profile->claim_level),
                    "arm_generic_timer_evidence");
        copy_string(profile->limitations, sizeof(profile->limitations),
                    "Portable profile records ARM generic timer evidence only; RTOS partitioning, interrupt isolation, and hypervisor claims still require target-specific backends.");
    } else {
        copy_string(profile->claim_level, sizeof(profile->claim_level),
                    "portable_user_space");
        copy_string(profile->limitations, sizeof(profile->limitations),
                    "Portable profile does not imply RTOS certification or hardware determinism.");
    }

    fill_platform_fingerprint(profile);
    return 0;
}

const vis_platform_adapter_t* vis_platform_default_adapter() {
    static const vis_platform_adapter_t adapter{nullptr,
                                                hosted_detect_profile};
    return &adapter;
}
