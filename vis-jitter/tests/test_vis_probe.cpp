/**
 * test_vis_probe.cpp
 *
 * Rootless smoke test for portable VIS probe backends.
 *
 * License: MIT
 */

#include "../include/vis_probe.hpp"

#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

static bool empty(const char* value) {
    return value == nullptr || value[0] == '\0';
}

static vis_probe_status_t run_test_backend(
    const vis_probe_services_t* services, vis_probe_report_t* report) {
    if (services == nullptr || services->target_context == nullptr ||
        *static_cast<const int*>(services->target_context) != 42) {
        return vis_probe_status_t::VIS_PROBE_ERR_INVALID_ARG;
    }
    std::snprintf(report->probe_result.selected_backend,
                  sizeof(report->probe_result.selected_backend),
                  "test_target");
    return vis_probe_status_t::VIS_PROBE_OK;
}

static vis_probe_status_t run_auto_test_backend(
    const vis_probe_services_t*, vis_probe_report_t* report) {
    std::snprintf(report->probe_result.selected_backend,
                  sizeof(report->probe_result.selected_backend),
                  "auto_test_target");
    return vis_probe_status_t::VIS_PROBE_OK;
}

static vis_probe_status_t run_dirty_unavailable_backend(
    const vis_probe_services_t*, vis_probe_report_t* report) {
    std::snprintf(report->generator, sizeof(report->generator),
                  "contaminated-generator");
    return vis_probe_status_t::VIS_PROBE_ERR_BACKEND_UNAVAILABLE;
}

static vis_probe_status_t run_fatal_auto_backend(
    const vis_probe_services_t* services, vis_probe_report_t*) {
    if (services == nullptr || services->target_context == nullptr) {
        return vis_probe_status_t::VIS_PROBE_ERR_BACKEND_UNAVAILABLE;
    }
    return *static_cast<const int*>(services->target_context) == 1
        ? vis_probe_status_t::VIS_PROBE_ERR_INVALID_ARG
        : vis_probe_status_t::VIS_PROBE_ERR_NO_TIME_SOURCE;
}

static int detect_posix_only_platform(void*, vis_platform_profile_t* profile) {
    std::memset(profile, 0, sizeof(*profile));
    std::snprintf(profile->profile_version, sizeof(profile->profile_version),
                  "%s", VIS_PLATFORM_PROFILE_VERSION);
    std::snprintf(profile->arch, sizeof(profile->arch), "test");
    std::snprintf(profile->os_family, sizeof(profile->os_family), "posix");
    profile->candidate_count = 1;
    vis_time_source_candidate_t* candidate = &profile->candidates[0];
    std::snprintf(candidate->name, sizeof(candidate->name),
                  "posix_clock_monotonic");
    candidate->available = true;
    candidate->monotonic = true;
    std::snprintf(candidate->evidence_level,
                  sizeof(candidate->evidence_level), "portable");
    return 0;
}

int main() {
    const vis_probe_backend_hint_t test_hint =
        static_cast<vis_probe_backend_hint_t>(100);
    const vis_probe_backend_descriptor_t test_backend{
        test_hint, "test_target", false, run_test_backend};
    if (!vis_probe_register_backend(&test_backend) ||
        std::strcmp(vis_probe_backend_name(test_hint), "test_target") != 0) {
        std::printf("[test] FAILED: backend registry rejected target adapter.\n");
        return 1;
    }

    char transient_name[] = "transient_target";
    const vis_probe_backend_hint_t transient_hint =
        static_cast<vis_probe_backend_hint_t>(101);
    const vis_probe_backend_descriptor_t transient_backend{
        transient_hint, transient_name, false, run_test_backend};
    if (!vis_probe_register_backend(&transient_backend)) {
        std::printf("[test] FAILED: transient backend registration failed.\n");
        return 1;
    }
    std::memset(transient_name, 'x', sizeof(transient_name) - 1);
    if (std::strcmp(vis_probe_backend_name(transient_hint),
                    "transient_target") != 0) {
        std::printf("[test] FAILED: backend registry did not own its name.\n");
        return 1;
    }
    vis_probe_backend_hint_t parsed_hint = vis_probe_backend_hint_t::AUTO;
    if (!vis_probe_backend_parse("test_target", &parsed_hint) ||
        parsed_hint != test_hint) {
        std::printf("[test] FAILED: backend registry did not drive parsing.\n");
        return 1;
    }
    const int target_context = 42;
    const vis_probe_services_t test_services{
        vis_platform_default_adapter(),
        const_cast<int*>(&target_context),
        nullptr, nullptr, nullptr, nullptr, nullptr};
    const vis_probe_config_t test_config{test_hint, &test_services};

    std::atomic<bool> registry_failed{false};
    std::vector<std::thread> registry_readers;
    for (int i = 0; i < 4; i++) {
        registry_readers.emplace_back([&]() {
            for (int j = 0; j < 1000; j++) {
                vis_probe_backend_hint_t parsed = vis_probe_backend_hint_t::AUTO;
                if (!vis_probe_backend_parse("test_target", &parsed) ||
                    parsed != test_hint) {
                    registry_failed.store(true, std::memory_order_relaxed);
                }
                uint32_t count = 0;
                const vis_probe_backend_descriptor_t* backends =
                    vis_probe_backend_registry(&count);
                for (uint32_t k = 0; k < count; k++) {
                    if (backends[k].name == nullptr ||
                        backends[k].name[0] == '\0' ||
                        backends[k].run == nullptr) {
                        registry_failed.store(true,
                                              std::memory_order_relaxed);
                    }
                }
            }
        });
    }
    std::vector<std::thread> registry_writers;
    for (int i = 0; i < 4; i++) {
        registry_writers.emplace_back([&, i]() {
            char name[32];
            std::snprintf(name, sizeof(name), "concurrent_target_%d", i);
            const vis_probe_backend_descriptor_t backend{
                static_cast<vis_probe_backend_hint_t>(110 + i),
                name, false, run_test_backend};
            if (!vis_probe_register_backend(&backend)) {
                registry_failed.store(true, std::memory_order_relaxed);
            }
        });
    }
    for (std::thread& reader : registry_readers) reader.join();
    for (std::thread& writer : registry_writers) writer.join();
    for (int i = 0; i < 4; i++) {
        char expected_name[32];
        std::snprintf(expected_name, sizeof(expected_name),
                      "concurrent_target_%d", i);
        if (std::strcmp(vis_probe_backend_name(
                            static_cast<vis_probe_backend_hint_t>(110 + i)),
                        expected_name) != 0) {
            registry_failed.store(true, std::memory_order_relaxed);
        }
    }
    if (registry_failed.load(std::memory_order_relaxed)) {
        std::printf("[test] FAILED: concurrent backend registry access failed.\n");
        return 1;
    }
    vis_probe_report_t test_report;
    if (vis_probe_run(&test_config, &test_report) !=
            vis_probe_status_t::VIS_PROBE_OK ||
        std::strcmp(test_report.probe_result.selected_backend,
                    "test_target") != 0) {
        std::printf("[test] FAILED: backend services were not injected.\n");
        return 1;
    }

    if (vis_probe_run(nullptr, nullptr) !=
        vis_probe_status_t::VIS_PROBE_ERR_INVALID_ARG) {
        std::printf("[test] FAILED: null report should be invalid arg.\n");
        return 1;
    }

    vis_probe_report_t auto_report;
    vis_probe_status_t status = vis_probe_run(nullptr, &auto_report);
    if (status != vis_probe_status_t::VIS_PROBE_OK) {
        std::printf("[test] FAILED: auto probe run failed.\n");
        return 1;
    }
    if (empty(auto_report.platform_profile.arch) ||
        empty(auto_report.execution_profile.execution_environment) ||
        empty(auto_report.probe_result.selected_backend) ||
        empty(auto_report.probe_result.evidence_level) ||
        empty(auto_report.probe_result.backend_status) ||
        empty(auto_report.probe_result.timer_evidence_level) ||
        empty(auto_report.probe_result.execution_evidence_level)) {
        std::printf("[test] FAILED: auto probe report is incomplete.\n");
        return 1;
    }
    if (empty(auto_report.execution_profile.posix_surface) ||
        empty(auto_report.execution_profile.arinc653_surface) ||
        empty(auto_report.execution_profile.hypervisor_surface) ||
        empty(auto_report.execution_profile.runtime_isolation_model)) {
        std::printf("[test] FAILED: execution surfaces are incomplete.\n");
        return 1;
    }
    if (empty(auto_report.target_contract.target_profile_family) ||
        empty(auto_report.target_contract.target_runtime_api_status) ||
        empty(auto_report.target_contract.target_timer_model)) {
        std::printf("[test] FAILED: target contract is incomplete.\n");
        return 1;
    }
    if (empty(auto_report.claim_gates.hosted_evidence_state) ||
        empty(auto_report.claim_gates.target_timer_claim_state) ||
        empty(auto_report.claim_gates.target_execution_claim_state) ||
        empty(auto_report.claim_gates.temporal_isolation_state) ||
        empty(auto_report.claim_gates.wcet_state) ||
        empty(auto_report.claim_gates.direct_claim_state)) {
        std::printf("[test] FAILED: claim gates are incomplete.\n");
        return 1;
    }

    vis_probe_config_t posix_config{vis_probe_backend_hint_t::POSIX_GENERIC};
    vis_probe_report_t posix_report;
    status = vis_probe_run(&posix_config, &posix_report);
    if (status != vis_probe_status_t::VIS_PROBE_OK) {
        std::printf("[test] FAILED: posix_generic backend is unavailable.\n");
        return 1;
    }
    if (std::strcmp(posix_report.probe_result.selected_backend,
                    "posix_generic") != 0 ||
        std::strcmp(posix_report.probe_result.evidence_level,
                    "portable_user_space") != 0 ||
        std::strcmp(posix_report.probe_result.backend_status,
                    "selected") != 0 ||
        std::strcmp(posix_report.platform_profile.selected_time_source,
                    "posix_clock_monotonic") != 0 ||
        posix_report.probe_result.timer_frequency_hz != 0 ||
        std::strcmp(posix_report.probe_result.timer_unit, "ns") != 0 ||
        posix_report.probe_result.timer_counter_width_bits != 0 ||
        posix_report.probe_result.timer_wraps ||
        std::strcmp(posix_report.probe_result.timer_metadata_status, "normalized_api_unit") != 0 ||
        std::strcmp(posix_report.schema_version, VIS_REPORT_SCHEMA_VERSION) != 0) {
        std::printf("[test] FAILED: posix backend fields are wrong.\n");
        return 1;
    }
    if (std::strcmp(posix_report.execution_profile.portability_tier,
                    "portable_probe_foundation") != 0) {
        std::printf("[test] FAILED: portability tier is wrong.\n");
        return 1;
    }
    if (std::strcmp(posix_report.target_contract.target_profile_family,
                    "hosted_posix") != 0 ||
        std::strcmp(posix_report.target_contract.target_runtime_api_status,
                    "host_native") != 0) {
        std::printf("[test] FAILED: hosted target contract is wrong.\n");
        return 1;
    }
    if (std::strcmp(posix_report.claim_gates.target_timer_claim_state,
                    "host_only") != 0 ||
        std::strcmp(posix_report.claim_gates.target_execution_claim_state,
                    "host_only") != 0 ||
        std::strcmp(posix_report.claim_gates.temporal_isolation_state,
                    "supporting_only") != 0) {
        std::printf("[test] FAILED: hosted claim gates are wrong.\n");
        return 1;
    }

#if defined(__x86_64__) || defined(__i386__)
    vis_probe_config_t x86_config{
        vis_probe_backend_hint_t::LINUX_X86_RDTSCP_MSR};
    vis_probe_report_t x86_report;
    status = vis_probe_run(&x86_config, &x86_report);
    const bool hosted_linux = std::strcmp(x86_report.platform_profile.os_family, "linux") == 0;if(hosted_linux) {
    bool rdtscp_usable = false;
    for (uint32_t i = 0; i < x86_report.platform_profile.candidate_count; i++) {
        const vis_time_source_candidate_t& candidate =
            x86_report.platform_profile.candidates[i];
        if (std::strcmp(candidate.name, "x86_rdtscp") == 0) {
            rdtscp_usable = candidate.available && candidate.monotonic;
        }
    }
    if (rdtscp_usable &&
        (status != vis_probe_status_t::VIS_PROBE_OK ||
         std::strcmp(x86_report.probe_result.selected_backend,
                     "linux_x86_rdtscp_msr") != 0 ||
             x86_report.probe_result.timer_frequency_hz != 0 ||
             std::strcmp(x86_report.probe_result.timer_metadata_status,
                         "frequency_not_collected") != 0)) {
        std::printf("[test] FAILED: usable RDTSCP backend was not selected.\n");
        return 1;
    }
    if (!rdtscp_usable &&
        (status != vis_probe_status_t::VIS_PROBE_ERR_BACKEND_UNAVAILABLE ||
         std::strcmp(auto_report.probe_result.selected_backend,
                     "posix_generic") != 0)) {
        std::printf("[test] FAILED: unavailable RDTSCP should fall back to POSIX.\n");
        return 1;
    }
    }else {
    if (status != vis_probe_status_t::VIS_PROBE_ERR_BACKEND_UNAVAILABLE ||
        std::strcmp(auto_report.probe_result.selected_backend,
                    "posix_generic") != 0 ||
        std::strcmp(auto_report.probe_result.evidence_level, "portable_user_space") != 0 ||
            std::strcmp(auto_report.platform_profile.claim_level, "linux_x86_rich_evidence") == 0) {
        std::printf("[test] FAILED: non-Linux x86 should use the POSIX backend.\n");
        return 1;
    }
    }
#elif defined(__aarch64__)
    vis_probe_config_t arm_config{
        vis_probe_backend_hint_t::ARM_GENERIC_TIMER};
    vis_probe_report_t arm_report;
    status = vis_probe_run(&arm_config, &arm_report);
    if (status == vis_probe_status_t::VIS_PROBE_OK &&
        (std::strcmp(arm_report.probe_result.selected_backend,
                     "arm_generic_timer") != 0 ||
         std::strcmp(arm_report.probe_result.evidence_level,
                     "arm_generic_timer_evidence") != 0 ||
         arm_report.probe_result.timer_frequency_hz != 0 ||
         std::strcmp(arm_report.probe_result.timer_metadata_status, "frequency_not_collected") !=
             0)) {
        std::printf("[test] FAILED: available ARM backend fields are wrong.\n");
        return 1;
    }
    if (status != vis_probe_status_t::VIS_PROBE_OK &&
        status != vis_probe_status_t::VIS_PROBE_ERR_BACKEND_UNAVAILABLE) {
        std::printf("[test] FAILED: inaccessible ARM timer should be unavailable.\n");
        return 1;
    }
    if (status == vis_probe_status_t::VIS_PROBE_ERR_BACKEND_UNAVAILABLE &&
        std::strcmp(auto_report.probe_result.selected_backend,
                    "posix_generic") != 0) {
        std::printf("[test] FAILED: inaccessible ARM timer should fall back to POSIX.\n");
        return 1;
    }
#endif

    const vis_platform_adapter_t posix_only_adapter{
        nullptr, detect_posix_only_platform};
    const int fatal_backend_context = 1;
    const vis_probe_services_t posix_only_services{
        &posix_only_adapter, const_cast<int*>(&fatal_backend_context),
        nullptr, nullptr,
        nullptr, nullptr, nullptr};
    const vis_probe_config_t posix_only_auto_config{
        vis_probe_backend_hint_t::AUTO, &posix_only_services};

    const vis_probe_backend_hint_t dirty_unavailable_hint =
        static_cast<vis_probe_backend_hint_t>(103);
    const vis_probe_backend_descriptor_t dirty_unavailable_backend{
        dirty_unavailable_hint, "dirty_unavailable", true,
        run_dirty_unavailable_backend};
    if (!vis_probe_register_backend(&dirty_unavailable_backend)) {
        std::printf("[test] FAILED: unavailable backend registration failed.\n");
        return 1;
    }
    vis_probe_report_t isolated_auto_report;
    status = vis_probe_run(&posix_only_auto_config, &isolated_auto_report);
    if (status != vis_probe_status_t::VIS_PROBE_OK ||
        std::strcmp(isolated_auto_report.probe_result.selected_backend,
                    "posix_generic") != 0 ||
        std::strcmp(isolated_auto_report.generator,
                    "vis-probe " VIS_PROBE_VERSION) != 0) {
        std::printf("[test] FAILED: unavailable backend contaminated AUTO "
                    "fallback.\n");
        return 1;
    }

    const vis_probe_backend_hint_t fatal_auto_hint =
        static_cast<vis_probe_backend_hint_t>(104);
    const vis_probe_backend_descriptor_t fatal_auto_backend{
        fatal_auto_hint, "fatal_auto", true, run_fatal_auto_backend};
    if (!vis_probe_register_backend(&fatal_auto_backend)) {
        std::printf("[test] FAILED: fatal backend registration failed.\n");
        return 1;
    }
    vis_probe_report_t fatal_auto_report;
    status = vis_probe_run(&posix_only_auto_config, &fatal_auto_report);
    if (status != vis_probe_status_t::VIS_PROBE_ERR_INVALID_ARG ||
        std::strcmp(fatal_auto_report.probe_result.selected_backend,
                    "auto") != 0) {
        std::printf("[test] FAILED: fatal AUTO backend error was masked.\n");
        return 1;
    }
    const int no_time_source_context = 2;
    const vis_probe_services_t no_time_source_services{
        &posix_only_adapter, const_cast<int*>(&no_time_source_context),
        nullptr, nullptr, nullptr, nullptr, nullptr};
    const vis_probe_config_t no_time_source_config{
        vis_probe_backend_hint_t::AUTO, &no_time_source_services};
    status = vis_probe_run(&no_time_source_config, &fatal_auto_report);
    if (status != vis_probe_status_t::VIS_PROBE_ERR_NO_TIME_SOURCE) {
        std::printf("[test] FAILED: AUTO time-source error was masked.\n");
        return 1;
    }

    const vis_probe_backend_hint_t auto_test_hint =
        static_cast<vis_probe_backend_hint_t>(102);
    const vis_probe_backend_descriptor_t auto_test_backend{
        auto_test_hint, "auto_test_target", true, run_auto_test_backend};
    if (!vis_probe_register_backend(&auto_test_backend)) {
        std::printf("[test] FAILED: automatic backend registration failed.\n");
        return 1;
    }
    uint32_t backend_count = 0;
    const vis_probe_backend_descriptor_t* backends =
        vis_probe_backend_registry(&backend_count);
    uint32_t auto_test_index = backend_count;
    uint32_t posix_index = backend_count;
    for (uint32_t i = 0; i < backend_count; i++) {
        if (backends[i].hint == auto_test_hint) auto_test_index = i;
        if (backends[i].hint == vis_probe_backend_hint_t::POSIX_GENERIC) {
            posix_index = i;
        }
    }
    if (auto_test_index >= posix_index) {
        std::printf("[test] FAILED: registered automatic backend follows "
                    "POSIX fallback.\n");
        return 1;
    }
    if (std::strcmp(auto_report.probe_result.selected_backend,
                    "posix_generic") == 0) {
        vis_probe_report_t registered_auto_report;
        status = vis_probe_run(nullptr, &registered_auto_report);
        if (status != vis_probe_status_t::VIS_PROBE_OK ||
            std::strcmp(registered_auto_report.probe_result.selected_backend,
                        "auto_test_target") != 0) {
            std::printf("[test] FAILED: registered automatic backend was not "
                        "selected before POSIX fallback.\n");
            return 1;
        }
    }

    vis_probe_config_t arinc_config{
        vis_probe_backend_hint_t::ARINC653_PARTITION_PROBE};
    vis_probe_report_t arinc_report;
    status = vis_probe_run(&arinc_config, &arinc_report);
    if (status != vis_probe_status_t::VIS_PROBE_OK ||
        std::strcmp(arinc_report.probe_result.selected_backend,
                    "arinc653_partition_probe") != 0 ||
        std::strcmp(arinc_report.probe_result.evidence_level,
                    "contract_only") != 0 ||
        std::strcmp(arinc_report.probe_result.backend_status,
                    "recognized_api_missing") != 0 ||
        std::strcmp(arinc_report.target_contract.target_profile_family,
                    "arinc653") != 0 ||
        std::strcmp(arinc_report.target_contract.target_runtime_api_status,
                    "recognized_api_missing") != 0 ||
        arinc_report.probe_result.timer_frequency_hz != 0 ||
        arinc_report.probe_result.timer_unit[0] != '\0' ||
        std::strcmp(arinc_report.probe_result.timer_metadata_status, "not_collected") != 0 ||
        std::strstr(arinc_report.probe_result.unsupported_reason,
                    "arinc653_partition_services") == nullptr) {
        std::printf("[test] FAILED: ARINC stub backend contract is wrong.\n");
        return 1;
    }
    if (std::strcmp(arinc_report.claim_gates.target_timer_claim_state,
                    "contract_only") != 0 ||
        std::strcmp(arinc_report.claim_gates.target_execution_claim_state,
                    "contract_only") != 0) {
        std::printf("[test] FAILED: ARINC claim gates are wrong.\n");
        return 1;
    }

    char* json = vis_probe_report_to_json(&posix_report);
    if (json == nullptr || std::strstr(json, "\"vis_probe_report\"") == nullptr ||
        std::strstr(json, "\"evidence_level\": \"portable_user_space\"") ==
            nullptr ||
        std::strstr(json, "\"backend_status\": \"selected\"") ==
            nullptr ||
        std::strstr(json, "\"timer_unit\": \"ns\"") == nullptr ||
        std::strstr(json, "\"target_contract\"") == nullptr ||
        std::strstr(json, "\"claim_gates\"") == nullptr ||
        std::strstr(json, "\"target_profile_family\": \"hosted_posix\"") ==
            nullptr ||
        std::strstr(json, "\"target_timer_claim_state\": \"host_only\"") ==
            nullptr ||
        std::strstr(json, "\"arinc653_surface\"") == nullptr ||
        std::strstr(json, "\"hypervisor_surface\"") == nullptr ||
        std::strstr(json, "\"execution_evidence_level\"") == nullptr ||
        std::strstr(json, "\"portability_tier\"") ==
            nullptr) {
        std::printf("[test] FAILED: probe JSON serialization failed.\n");
        std::free(json);
        return 1;
    }
    std::free(json);

    std::printf("[test] PASS: VIS Probe works.\n");
    return 0;
}
