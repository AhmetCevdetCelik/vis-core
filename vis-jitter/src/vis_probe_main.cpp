/**
 * vis_probe_main.cpp
 *
 * vis-probe CLI entry point.
 *
 * License: MIT
 */

#include "../include/vis_probe.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static void print_usage(const char* argv0) {
    std::fprintf(stderr,
                 "Usage: %s [OPTIONS]\n"
                 "\n"
                 "Options:\n"
                 "  --backend <auto|posix_generic|linux_x86_rdtscp_msr|arm_generic_timer|\n"
                 "             arinc653_partition_probe|posix_pse53_probe|\n"
                 "             autosar_adaptive_probe|hypervisor_partition_probe>\n"
                 "                         Probe backend selection (default: auto)\n"
                 "  --output  <file.json>  Save JSON report to file\n"
                 "  --help                 Show this message\n"
                 "\n"
                 "Example:\n"
                 "  ./vis-probe --backend auto --output probe.json\n",
                 argv0);
}

static bool parse_backend(const char* text, vis_probe_backend_hint_t* out) {
    if (text == nullptr || out == nullptr) return false;
    if (std::strcmp(text, "auto") == 0) {
        *out = vis_probe_backend_hint_t::AUTO;
        return true;
    }
    if (std::strcmp(text, "posix_generic") == 0) {
        *out = vis_probe_backend_hint_t::POSIX_GENERIC;
        return true;
    }
    if (std::strcmp(text, "linux_x86_rdtscp_msr") == 0) {
        *out = vis_probe_backend_hint_t::LINUX_X86_RDTSCP_MSR;
        return true;
    }
    if (std::strcmp(text, "arm_generic_timer") == 0) {
        *out = vis_probe_backend_hint_t::ARM_GENERIC_TIMER;
        return true;
    }
    if (std::strcmp(text, "arinc653_partition_probe") == 0) {
        *out = vis_probe_backend_hint_t::ARINC653_PARTITION_PROBE;
        return true;
    }
    if (std::strcmp(text, "posix_pse53_probe") == 0) {
        *out = vis_probe_backend_hint_t::POSIX_PSE53_PROBE;
        return true;
    }
    if (std::strcmp(text, "autosar_adaptive_probe") == 0) {
        *out = vis_probe_backend_hint_t::AUTOSAR_ADAPTIVE_PROBE;
        return true;
    }
    if (std::strcmp(text, "hypervisor_partition_probe") == 0) {
        *out = vis_probe_backend_hint_t::HYPERVISOR_PARTITION_PROBE;
        return true;
    }
    return false;
}

int main(int argc, char* argv[]) {
    vis_probe_config_t config{vis_probe_backend_hint_t::AUTO};
    const char* output_path = nullptr;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (std::strcmp(argv[i], "--backend") == 0 && i + 1 < argc) {
            if (!parse_backend(argv[++i], &config.backend_hint)) {
                std::fprintf(stderr, "[vis-probe] Invalid --backend value.\n");
                print_usage(argv[0]);
                return 1;
            }
        } else if (std::strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output_path = argv[++i];
        } else {
            std::fprintf(stderr, "[vis-probe] Unknown argument: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    vis_probe_report_t report;
    vis_probe_status_t status = vis_probe_run(&config, &report);
    if (status != vis_probe_status_t::VIS_PROBE_OK) {
        std::fprintf(stderr,
                     "[vis-probe] ERROR: probe failed (code %d).\n",
                     static_cast<int>(status));
        return 1;
    }

    vis_probe_report_print_summary(&report);

    if (output_path != nullptr) {
        char* json = vis_probe_report_to_json(&report);
        if (json == nullptr) {
            std::fprintf(stderr,
                         "[vis-probe] ERROR: JSON serialization failed.\n");
            return 1;
        }

        FILE* f = std::fopen(output_path, "w");
        if (f == nullptr) {
            std::fprintf(stderr,
                         "[vis-probe] ERROR: Cannot open output file: %s\n",
                         output_path);
            std::free(json);
            return 1;
        }

        std::fputs(json, f);
        std::fclose(f);
        std::free(json);
        std::printf("[vis-probe] Report saved to: %s\n", output_path);
    }

    return 0;
}
