# VIS Claim Boundaries

VIS reports are useful because they separate evidence from interpretation. This
page defines what can and cannot be claimed from current VIS reports.

## Evidence Levels

`advisory`:

- Evidence exists, but runtime placement or memory ownership was not fully
  verified.
- Use as candidate selection evidence.

`attested`:

- Runtime behavior was verified from sources such as `/proc/<pid>/status`,
  `/proc/<pid>/task/<tid>/status`, or child `smaps`.
- Use as stronger workload-specific evidence.

`production_controlled`:

- Placement was applied with containment such as cgroup/cpuset, membership was
  checked, escape detection ran, and cleanup was recorded.
- Use only when the report explicitly allows the control claim.

## CPU Claims

Allowed:

- `VIS observed CPU jitter/noise evidence on this host.`
- `VIS Doctor recommended CPU candidates from scan evidence.`
- `VIS Run applied CPU placement to this workload.`
- `VIS Run attested process/thread CPU allowance for this workload.`
- `This workload escaped the requested CPU set.`

Not allowed unless the report says so:

- `VIS production-controlled workload placement.`
- `VIS isolated all system noise.`
- `VIS CPU placement improves all workloads.`

Required before saying `VIS controls workload placement`:

- CPU policy application
- `/proc/<pid>/status` CPU allowance evidence
- `/proc/<pid>/task/<tid>/status` CPU allowance evidence
- cgroup membership evidence when cgroup mode is used
- no escape detected
- cleanup result when cgroup mode is used
- `control_claim_allowed: true` or equivalent report language

## Platform Claims

Allowed:

- `VIS recorded architecture, OS/runtime, ABI, and timing-source capability evidence.`
- `VIS selected a timing source and reported its evidence level.`
- `VIS used a POSIX fallback timing source when architecture-specific evidence was unavailable.`

Not allowed:

- `VIS now supports certified RTOS deployments.`
- `VIS proves WCET or temporal isolation.`
- `A portable POSIX timer has the same evidence strength as x86 RDTSCP/TSC plus MSR-backed SMI evidence.`
- `VIS makes ARM, PowerPC, RISC-V, or RTOS workloads deterministic.`
- `ARINC 653 surface recognized` means `ARINC 653 attested.`
- `POSIX-like execution evidence` means `PSE53 compliance proven.`
- `Hypervisor surface visible` means `partition isolation proven.`

Required before saying `VIS produced rich platform-specific timing evidence`:

- the report includes `platform_profile`
- the selected time source is available and monotonic
- the report lists the timing-source evidence level
- limitations do not downgrade the claim to portable or advisory evidence

Required before saying `VIS observed RTOS execution evidence`:

- the backend status is `selected`
- execution evidence is stronger than `contract_only`
- the report states which partition/scheduler surface was actually visible
- limitations do not say that target APIs were missing

## Memory Claims

Allowed:

- `VIS-Mem prepared a VIS-owned memory envelope.`
- `VIS-Mem inspected child status/maps/smaps evidence.`
- `VIS-Mem observed child hugepage or locked backing.`
- `VIS-Mem reported memory policy as false, partial, or unverified.`

Not allowed today:

- `VIS-Mem redirected the child allocator.`
- `VIS-Mem proved model mmap, heap, or KV-cache placement.`
- `VIS-Mem fully controlled application-owned memory.`

Required before saying `VIS-Mem policy was applied to AI inference`:

- child memory mappings inspected
- `smaps` backing available
- requested memory policy recorded
- backing evidence matches the requested policy
- report distinguishes parent-envelope evidence from child allocator evidence

Current VIS-Mem reports may show partial child backing observation. That is not
the same as full policy attribution.

## Inference Claims

Allowed:

- `VIS-Infer compared profiles on this model, host, backend, and benchmark.`
- `VIS-Infer observed profile X as the best candidate in this run.`
- `VIS-Infer recommended or rejected candidates based on measured metrics.`
- `This result is limited to the tested workload and machine.`

Not allowed:

- `VIS speeds up all AI inference.`
- `CPU affinity is always better than the scheduler.`
- `Memory policy improved inference` when the report says memory policy is
  advisory or not verified on child allocations.

Required before saying `meaningful speedup on selected workload`:

- representative workload
- repeated measured runs
- warmup excluded from decision
- confidence not low
- primary metric improves above the configured threshold
- guardrail metrics do not regress meaningfully
- report includes limitations and rejected candidates

## Negative Results

Negative or inconclusive results are valid VIS output.

Examples:

- TinyLlama produces no significant improvement.
- CPU affinity combo fails to beat thread-only.
- Memory parent envelope produces only advisory/noisy change.
- Environment is unstable, so recommendation is downgraded.

These outcomes improve trust because VIS is an evidence layer, not a forced
speedup generator.
