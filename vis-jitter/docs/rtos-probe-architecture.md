# VIS RTOS Probe Architecture

VIS Core now separates rich Linux/x86 jitter measurement from a smaller probe
architecture intended for RTOS-oriented adaptation work.

## Goal

The goal of `vis-probe` is to record claim-safe runtime evidence about:

- timing source access,
- execution model,
- scheduler surface,
- partition surface,
- hypervisor surface,
- interrupt and memory visibility,
- backend limitations.

It is not a goal of this layer to prove WCET, temporal isolation, or
certification readiness.

## Core Design

`vis-jitter` remains the richest Linux/x86 evidence path. `vis-probe` becomes
the minimal evidence collector that future RTOS targets can adapt without
dragging `/proc`, `/sys`, NUMA, or MSR assumptions into every backend.

The architecture is split into independent evidence axes:

- **host profile**: what the current host actually exposes today;
- **target contract**: what the requested RTOS or hypervisor backend expects to
  see before stronger claims become valid;
- **timer evidence**: which timing source exists, whether it is monotonic, what
  read overhead it shows, and what privilege is required;
- **execution evidence**: what scheduler, partition, or process model is
  visible to the probe;
- **surface evidence**: whether POSIX, ARINC 653, AUTOSAR Adaptive, or
  hypervisor-related APIs are recognized, present, or unavailable;
- **limitations**: what the report explicitly does not prove.

## Backend Contract

Every probe backend should conceptually answer the same questions:

1. Is this backend recognized by the build?
2. Is the target runtime API available here?
3. Which timing source can VIS honestly describe?
4. Which execution/partition surfaces can VIS see?
5. What claim level is safe?
6. What limitations must be emitted?

The current codebase now supports both active backends and contract-only stubs.

### Active backends

- `posix_generic`
- `linux_x86_rdtscp_msr`
- `arm_generic_timer`
- `target_services_probe`

The `linux_x86_rdtscp_msr` probe backend actively validates the RDTSCP timing
path. Its name preserves the relationship with the richer Linux/x86
`vis-jitter` workflow, but selecting it does not mean that `vis-probe` read an
MSR or collected SMI evidence. MSR/SMI evidence requires the separate
privileged `vis-jitter` workflow and must be present explicitly in its report.

`target_services_probe` is a vendor-neutral adapter backend. It consumes the
versioned `vis_probe_services_t` timer, scheduler, partition, privilege, and
runtime callbacks. Complete collection may emit `rtos_execution_surface`;
missing or denied execution services emit `partial_target_evidence`. Missing
timer callbacks or an explicit `UNAVAILABLE` timer API let AUTO fall back.
Once a target API exists, read failure, invalid timer metadata, non-monotonic
reads, and adapter-internal errors stop AUTO with
`VIS_PROBE_ERR_TARGET_SERVICE`; they are not hidden by hosted POSIX evidence.
The legacy `timer_now` hook remains source-compatible but is not sufficient
for this backend because it cannot report read failure, frequency, unit,
counter width, wrap behavior, or privilege requirements.

AUTO considers this backend only when the versioned service struct carries an
explicit non-`UNSPECIFIED` target profile. Merely injecting a custom hosted
platform adapter is not target intent. Explicit `TARGET_SERVICES_PROBE`
selection remains sufficient intent and preserves detailed failures.

The minimal capability model is a bit mask covering timer, scheduler,
partition, privilege, and runtime services. Timer, scheduler, privilege, and
runtime are the defaults for generic and PSE53-like profiles. ARINC653-like
profiles also default to requiring partition evidence. A nonzero
caller-supplied mask replaces the profile defaults and is the complete
capability requirement contract, but it must still include `TIMER` because
target-services reports are anchored by validated target timer metadata and
reads. Generic targets never acquire a partition requirement implicitly.

Permission denial on a required capability is a fatal target-service error.
Permission denial on an optional capability keeps the backend result
available, but lowers the overall claim to partial evidence. Missing optional
callbacks and explicit `UNAVAILABLE` responses remain
`not_required_for_profile`.

`VIS_PROBE_VERSION` identifies the probe implementation/API release; it does
not define the serialized report contract. Probe API 0.2 appends target
profile/capability fields and timer metadata to the existing structs.
Target-services reports use report schema `0.2`, while hosted and legacy
reports remain schema `0.1`. The validator supports both. Timer and capability
metadata is required only for schema 0.2 target-services reports.

`timer_metadata_status` keeps unknown data separate from invalid data.
Target adapters use `reported_by_target` only after metadata validation.
Contract-only or failed target collection uses `not_collected`. POSIX reports
use `normalized_api_unit`: `ns` describes the normalized API result, not a
1 GHz hardware counter, and physical width/wrap remain unknown. x86 TSC and
ARM generic timer reports keep frequency at zero with
`frequency_not_collected` unless a backend actually reads or validates that
frequency.

### Contract-only stubs

- `arinc653_partition_probe`
- `posix_pse53_probe`
- `autosar_adaptive_probe`
- `hypervisor_partition_probe`

Contract-only stubs are successful reports, not crashes. They emit:

- `backend_status = recognized_api_missing`
- `evidence_level = contract_only`
- explicit unsupported reasons
- target-specific limitations

This keeps the architecture inspectable on a hosted Linux machine while making
it obvious that target attestation still requires a real backend.

## Evidence Model

Current probe reports should be interpreted in three layers:

- `target_contract`: requested target family and runtime API status
- `evidence_level`: overall claim-safe summary
- `timer_evidence_level`: timing-source confidence
- `execution_evidence_level`: execution/partition surface confidence

Recommended meanings:

- `portable_user_space`: user-space timing and execution evidence only
- `linux_x86_rich_evidence`: Linux/x86 architecture counter path is active
- `arm_generic_timer_evidence`: ARM generic timer path is active
- `partial_target_evidence`: target timer evidence exists, but one or more
  services required by the selected target profile are incomplete
- `contract_only`: target backend contract is recognized, but target runtime API
  is unavailable
- `rtos_execution_surface`: target timer and profile-required execution
  services were collected; certification and isolation proof remain out of
  scope
- `hypervisor_partition_hint`: reserved for future hypervisor/partition surface
  evidence

Current contract states:

- `host_native`: the requested target matches the current hosted runtime
- `recognized_api_missing`: the target family is modeled, but target APIs are
  not present on this host
- `not_supported_on_build`: the build cannot support that target family

## Backend Expansion Rules

Future RTOS backends should follow these rules:

- never reuse Linux collector paths just because they exist in VIS Core;
- prefer target timer or scheduler APIs over inference from hosted Linux files;
- distinguish target execution surface evidence from timing-source evidence;
- emit limitation text before stronger claims;
- only claim partition or hypervisor isolation when target evidence actually
  exists.

## Intended TÜBİTAK Story

This architecture is designed to answer the specific concern that VIS should not
be blindly ported from Linux/x86 into RTOS products. Instead, VIS now exposes a
portable probe foundation with clear claim boundaries and explicit hooks for
ARINC 653, POSIX PSE53, AUTOSAR Adaptive, and hypervisor-oriented targets.
