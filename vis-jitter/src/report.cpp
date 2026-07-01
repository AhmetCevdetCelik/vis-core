/**
 * report.cpp
 *
 * Report generation for vis-jitter.
 * JSON serialization and terminal summary output.
 *
 * License: MIT
 */

#include "../include/report.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static std::string json_escape(const char* value) {
    std::string out;
    if (value == nullptr) {
        return out;
    }

    for (const unsigned char* p =
             reinterpret_cast<const unsigned char*>(value);
         *p != '\0'; p++) {
        switch (*p) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (*p < 0x20) {
                    char escaped[7];
                    snprintf(escaped, sizeof(escaped), "\\u%04x", *p);
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
    int needed = snprintf(nullptr, 0, format, args...);
    if (needed <= 0) {
        return;
    }

    std::vector<char> buffer(static_cast<size_t>(needed) + 1);
    snprintf(buffer.data(), buffer.size(), format, args...);
    out->append(buffer.data(), static_cast<size_t>(needed));
}

// ---------------------------------------------------------------------------
// Public API implementation
// ---------------------------------------------------------------------------

void vis_report_print_summary(const vis_report_t* report) {
    if (report == nullptr) return;

    const vis_results_t*   r = &report->results;
    const vis_smi_audit_t* s = &report->smi_audit;
    const vis_detected_t*  d = &report->detected;
    const vis_platform_profile_t* p = &report->platform;

    printf("\n");
    printf("========================================\n");
    printf(" vis-jitter report\n");
    printf("========================================\n");
    printf(" Generated : %s\n", report->generated_at);
    printf(" Report ID : %s\n", report->report_id);
    printf("----------------------------------------\n");
    printf(" Platform profile\n");
    printf("   Arch        : %s\n", p->arch);
    printf("   OS/env      : %s / %s\n", p->os_family, p->environment);
    printf("   ABI bits    : %u\n", p->abi_bits);
    printf("   Time source : %s (%s)\n",
           p->selected_time_source, p->time_source_evidence_level);
    printf("   Claim level : %s\n", p->claim_level);
    printf("----------------------------------------\n");
    printf(" System (detected)\n");
    printf("   Core        : %u\n",  d->cpu_core);
    printf("   TSC freq.   : %.3f GHz\n", d->frequency_ghz);
    printf("   NUMA node   : %u\n",  d->numa_node);
    printf("   SMT active  : %s\n",  d->smt_active        ? "yes" : "no");
    printf("   TSC invar.  : %s\n",  d->tsc_invariant      ? "yes" : "no");
    printf("   RDTSCP      : %s\n",  d->rdtscp_supported   ? "yes" : "no");
    printf("----------------------------------------\n");
    printf(" SMI audit\n");
    printf("   Policy      : %s\n",  s->rejection_policy);
    printf("   Windows     : %u contaminated\n", s->contaminated_windows);
    printf("   MSR delta   : %llu\n", (unsigned long long)s->msr_delta);
    printf("   SMI events  : %llu\n",
           (unsigned long long)s->smi_events_detected);
    printf("   Rejected    : %llu samples\n",
           (unsigned long long)s->samples_rejected);
    printf("----------------------------------------\n");
    printf(" Latency results\n");
    printf("   Accepted    : %llu samples\n",
           (unsigned long long)r->samples_accepted);
    printf("   Core migr.  : %llu rejected\n",
           (unsigned long long)r->core_migration_rejected);
    printf("   min         : %.1f ns\n", r->latency_ns.min_ns);
    printf("   p50         : %.1f ns\n", r->latency_ns.p50_ns);
    printf("   p99         : %.1f ns\n", r->latency_ns.p99_ns);
    printf("   p99.9       : %.1f ns\n", r->latency_ns.p99_9_ns);
    printf("   p99.99      : %.1f ns\n", r->latency_ns.p99_99_ns);
    printf("   max         : %.1f ns%s\n",
           r->latency_ns.max_ns,
           r->latency_ns.max_saturated ? " (saturated)" : "");
    printf("   overflow    : %llu samples\n",
           (unsigned long long)r->latency_ns.overflow_count);
    printf("----------------------------------------\n");
    // DESIGN CHOICE: Show all percentiles, even those that fail.
    // Transparency > looking good. If P99.9 fails, the user
    // deserves to know — not just a binary pass/fail.
    if (r->determinism_pass) {
        printf(" VERDICT: PASS — P99 %.1f ns <= threshold %.1f ns\n",
               r->latency_ns.p99_ns, r->threshold_ns);
    } else {
        printf(" VERDICT: FAIL — P99 %.1f ns > threshold %.1f ns\n",
               r->latency_ns.p99_ns, r->threshold_ns);
    }

    printf("========================================\n\n");
}

char* vis_report_to_json(const vis_report_t* report) {
    if (report == nullptr) return nullptr;

    const vis_results_t*   r = &report->results;
    const vis_smi_audit_t* s = &report->smi_audit;
    const vis_detected_t*  d = &report->detected;
    const vis_asserted_t*  a = &report->asserted;
    const vis_platform_profile_t* p = &report->platform;

    std::string schema_version = json_escape(report->schema_version);
    std::string report_id = json_escape(report->report_id);
    std::string generated_at = json_escape(report->generated_at);
    std::string generator = json_escape(report->generator);
    std::string p_state = json_escape(a->p_state);
    std::string c_states_disabled = json_escape(a->c_states_disabled);
    std::string egress_memory = json_escape(a->egress_memory);
    std::string rx_buffer_memory = json_escape(a->rx_buffer_memory);
    std::string rejection_policy = json_escape(s->rejection_policy);
    std::string platform_profile_version = json_escape(p->profile_version);
    std::string platform_arch = json_escape(p->arch);
    std::string platform_os_family = json_escape(p->os_family);
    std::string platform_environment = json_escape(p->environment);
    std::string platform_kernel_release = json_escape(p->kernel_release);
    std::string platform_fingerprint = json_escape(p->platform_fingerprint);
    std::string selected_time_source = json_escape(p->selected_time_source);
    std::string time_source_evidence_level =
        json_escape(p->time_source_evidence_level);
    std::string affinity_control = json_escape(p->affinity_control);
    std::string interrupt_evidence = json_escape(p->interrupt_evidence);
    std::string thermal_evidence = json_escape(p->thermal_evidence);
    std::string memory_policy = json_escape(p->memory_policy);
    std::string privileged_counters = json_escape(p->privileged_counters);
    std::string claim_level = json_escape(p->claim_level);
    std::string limitations = json_escape(p->limitations);

    std::string candidate_json;
    for (uint32_t i = 0; i < p->candidate_count && i < 4; i++) {
        const vis_time_source_candidate_t* c = &p->candidates[i];
        std::string name = json_escape(c->name);
        std::string evidence_level = json_escape(c->evidence_level);
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
            evidence_level.c_str(),
            reason.c_str());
    }

    std::string json;
    append_format(&json,
        "{\n"
        "  \"vis_report\": {\n"
        "    \"schema_version\": \"%s\",\n"
        "    \"report_id\": \"%s\",\n"
        "    \"generated_at\": \"%s\",\n"
        "    \"generator\": \"%s\",\n"
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
        "    \"system\": {\n"
        "      \"detected\": {\n"
        "        \"cpu_core\": %u,\n"
        "        \"frequency_ghz\": %.3f,\n"
        "        \"numa_node\": %u,\n"
        "        \"smt_active\": %s,\n"
        "        \"tsc_invariant\": %s,\n"
        "        \"rdtscp_supported\": %s\n"
        "      },\n"
        // This separation is VIS's core differentiator.
        // detected = measured and verifiable. asserted = user's claim.
        // A regulator reads this and knows exactly what's proven vs stated.
        "      \"asserted\": {\n"
        "        \"p_state\": \"%s\",\n"
        "        \"c_states_disabled\": \"%s\",\n"
        "        \"hugepages_1gb\": %s,\n"
        "        \"egress_memory\": \"%s\",\n"
        "        \"rx_buffer_memory\": \"%s\",\n"
        "        \"ddio_enabled\": %s\n"
        "      },\n"
        "      \"verification_note\": \"asserted fields are user-supplied; not verified by vis-jitter\"\n"
        "    },\n"
        "    \"smi_audit\": {\n"
        "      \"msr_start\": %llu,\n"
        "      \"msr_end\": %llu,\n"
        "      \"msr_delta\": %llu,\n"
        "      \"smi_events_detected\": %llu,\n"
        "      \"events_detected\": %u,\n"
        "      \"events_detected_note\": \"deprecated alias for contaminated_windows\",\n"
        "      \"contaminated_windows\": %u,\n"
        "      \"samples_rejected\": %llu,\n"
        "      \"rejection_policy\": \"%s\"\n"
        "    },\n"
        "    \"results\": {\n"
        "      \"samples_accepted\": %llu,\n"
        "      \"core_migration_rejected\": %llu,\n"
        "      \"latency_ns\": {\n"
        "        \"min\": %.1f,\n"
        "        \"p50\": %.1f,\n"
        "        \"p99\": %.1f,\n"
        "        \"p99_9\": %.1f,\n"
        "        \"p99_99\": %.1f,\n"
        "        \"max\": %.1f,\n"
        "        \"overflow_count\": %llu,\n"
        "        \"max_saturated\": %s\n"
        "      },\n"
        "      \"determinism_verdict\": \"%s\",\n"
        "      \"threshold_ns\": %.1f\n"
        "    }\n"
        "  }\n"
        "}\n",
        schema_version.c_str(),
        report_id.c_str(),
        generated_at.c_str(),
        generator.c_str(),
        platform_profile_version.c_str(),
        platform_arch.c_str(),
        platform_os_family.c_str(),
        platform_environment.c_str(),
        p->abi_bits,
        platform_kernel_release.c_str(),
        platform_fingerprint.c_str(),
        selected_time_source.c_str(),
        time_source_evidence_level.c_str(),
        p->time_source_read_overhead_ns,
        p->time_source_monotonic ? "true" : "false",
        affinity_control.c_str(),
        interrupt_evidence.c_str(),
        thermal_evidence.c_str(),
        memory_policy.c_str(),
        privileged_counters.c_str(),
        claim_level.c_str(),
        limitations.c_str(),
        candidate_json.c_str(),
        d->cpu_core,
        d->frequency_ghz,
        d->numa_node,
        d->smt_active       ? "true" : "false",
        d->tsc_invariant     ? "true" : "false",
        d->rdtscp_supported  ? "true" : "false",
        p_state.c_str(),
        c_states_disabled.c_str(),
        a->hugepages_1gb     ? "true" : "false",
        egress_memory.c_str(),
        rx_buffer_memory.c_str(),
        a->ddio_enabled      ? "true" : "false",
        (unsigned long long)s->msr_start,
        (unsigned long long)s->msr_end,
        (unsigned long long)s->msr_delta,
        (unsigned long long)s->smi_events_detected,
        s->events_detected,
        s->contaminated_windows,
        (unsigned long long)s->samples_rejected,
        rejection_policy.c_str(),
        (unsigned long long)r->samples_accepted,
        (unsigned long long)r->core_migration_rejected,
        r->latency_ns.min_ns,
        r->latency_ns.p50_ns,
        r->latency_ns.p99_ns,
        r->latency_ns.p99_9_ns,
        r->latency_ns.p99_99_ns,
        r->latency_ns.max_ns,
        (unsigned long long)r->latency_ns.overflow_count,
        r->latency_ns.max_saturated ? "true" : "false",
        r->determinism_pass ? "PASS" : "FAIL",
        r->threshold_ns
    );

    char* buf = static_cast<char*>(malloc(json.size() + 1));
    if (buf == nullptr) return nullptr;

    memcpy(buf, json.c_str(), json.size() + 1);
    return buf;
}
vis_report_t* vis_report_from_json(const char* /*json_path*/) {
    // V1 stub — JSON deserialization planned for V2
    fprintf(stderr, "[vis-jitter] vis_report_from_json: not implemented in V1\n");
    return nullptr;
}

char* vis_report_sign(const vis_report_t* /*report*/,
                      const char*         /*private_key_path*/) {
    // V1 stub — signed reports are planned for a later attestation layer.
    fprintf(stderr, "[vis-jitter] vis_report_sign: not available in open source V1\n");
    return nullptr;
}
