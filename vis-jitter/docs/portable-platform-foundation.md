# VIS Core Portable Platform Foundation

VIS Core started as a Linux/x86_64 jitter probe. That path is still the richest
evidence path because it can combine RDTSCP/TSC timing, Linux affinity, NUMA,
and privileged MSR-backed SMI evidence. The portable platform foundation keeps
that path intact while separating the parts that must vary by architecture or
runtime environment.

## Goal

The goal is not to claim that VIS now supports every architecture or RTOS. The
goal is to make every report explicit about:

- which architecture and OS/runtime surface produced the measurement,
- which time source was selected,
- which candidate time sources were considered,
- which platform capabilities are visible,
- what evidence level and limitations apply.

This is the first step toward a smaller, lower-intrusion VIS probe that can be
adapted to ARM, PowerPC, RISC-V, POSIX-like RTOS profiles, and partitioned
execution environments without carrying Linux/x86 assumptions into the core.

## Current Scope

The current foundation adds:

- `vis_platform_profile_t` for architecture, OS, ABI, kernel/runtime, selected
  time source, capability, and limitation evidence.
- A whitelist-only time-source candidate model.
- x86 RDTSCP capability validation when the build target is x86.
- POSIX `clock_gettime(CLOCK_MONOTONIC)` fallback evidence.
- A platform profile block in `vis-jitter` JSON and terminal reports.

The existing Linux/x86 measurement behavior remains the primary full-evidence
path. The platform profile does not replace SMI/MSR measurement; it describes
whether that kind of rich evidence is available.

## Claim Boundaries

Safe claim:

> VIS records platform identity and timing-source capability evidence, then
> states the evidence level used by a measurement report.

Unsafe claims:

- VIS supports certified RTOS deployments.
- VIS proves WCET.
- VIS proves temporal isolation.
- VIS makes ARM, PowerPC, or RTOS systems deterministic.
- A POSIX fallback timer has the same evidence strength as RDTSCP/TSC plus MSR.

## RTOS Mapping

For RTOS or safety-critical environments, VIS should not blindly port Linux
collectors such as `/proc`, `/sys`, NUMA, or MSR access. The portable probe
should instead map to the APIs and policies exposed by the target environment.

Relevant profiles to study:

- **ARINC 653**: partition scheduling, partition time windows, health monitoring,
  and temporal isolation evidence.
- **POSIX PSE53**: process/thread services, timers, clocks, scheduling, and
  portable APIs suitable for a low-intrusion probe.
- **AUTOSAR Adaptive**: POSIX-like application processes, service-oriented
  execution, execution management, and timing evidence for automotive systems.

VIS can support WCET and temporal isolation activities by producing measurement
evidence, jitter histograms, platform capability records, and limitations. It is
not a replacement for safety analysis, certification evidence, or formal WCET
proof.

## Future Backends

Candidate future backends:

- ARM generic timer (`CNTVCT_EL0`) with guarded user-space validation.
- PowerPC time-base counter with target-specific validation.
- RISC-V `time`/`cycle` counters when the kernel allows user access.
- Vendor RTOS timer APIs for safety profiles where exception-based probing is
  not acceptable.

Each backend should provide:

- availability,
- monotonicity,
- read overhead,
- privilege requirement,
- evidence level,
- limitations.
