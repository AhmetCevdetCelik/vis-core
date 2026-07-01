# VIS RTOS Capability Matrix

This matrix defines what VIS expects to observe from target execution
environments before making stronger probe claims.

## Capability Table

| Target profile | Execution model | Timer / clock surface | Scheduler / partition surface | Interrupt visibility | Memory visibility | Hypervisor relation | Expected probe evidence | Current status |
|---|---|---|---|---|---|---|---|---|
| ARINC 653 | Partitioned processes within fixed windows | Vendor partition clock or certified timer API | Partition schedule, window identity, health monitoring | Vendor-specific, usually restricted | Vendor-specific or partition policy specific | May run under partitioning hypervisor | `contract_only` today, future `rtos_execution_surface` | Contract recognized, API unavailable on hosted Linux |
| POSIX PSE53 | Restricted POSIX process/thread model | POSIX clocks allowed by the profile | POSIX scheduler classes and affinity where exposed | Usually limited compared with Linux | Profile or vendor specific | Optional | `contract_only` today, future `rtos_execution_surface` | Contract recognized, API unavailable on hosted Linux |
| AUTOSAR Adaptive | POSIX-like adaptive processes | POSIX-like clocks plus execution management timing surfaces | Execution Management and adaptive runtime policies | Runtime specific | Memory ownership and policy are implementation dependent | May coexist with hypervisor layers | `contract_only` today, future `rtos_execution_surface` | Contract recognized, API unavailable on hosted Linux |
| Hypervisor partition target | Guest or partition runtime inside virtualized isolation | Guest timer or partition timer API | Partition scheduler or VM control interface | Often indirect or mediated | Often mediated by guest/host split | Primary surface | `contract_only` today, future `hypervisor_partition_hint` | Contract recognized, API unavailable on hosted Linux |

## Field Mapping

The probe report should map target evidence into these areas:

- `platform_profile.timer_access_model`
- `platform_profile.scheduler_model`
- `platform_profile.partitioning_hint`
- `platform_profile.posix_profile`
- `platform_profile.arinc653_surface`
- `platform_profile.autosar_adaptive_surface`
- `platform_profile.hypervisor_surface`
- `platform_profile.runtime_isolation_hint`
- `probe_result.timer_evidence_level`
- `probe_result.execution_evidence_level`
- `probe_result.backend_status`
- `probe_result.unsupported_reason`

## Safe Claim Rules

What VIS may safely say today:

- a target contract is modeled
- a backend is recognized by the probe
- the current host lacks the target runtime API
- timing-source evidence belongs to the current host, not the absent RTOS

What VIS must not say today:

- ARINC 653 timing is attested
- PSE53 compliance is proven
- AUTOSAR Adaptive execution behavior is measured
- hypervisor partition isolation is verified

## Upgrade Path

When a real target backend is added, it should move from `contract_only` to a
stronger execution evidence class only if it can populate at least:

- target timer identity
- target scheduler or partition identity
- target privilege/access model
- target-specific limitations
- execution surface evidence separated from timer evidence
