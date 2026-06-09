# VIS-CPU Evidence Examples

VIS-CPU separates CPU recommendation evidence from workload placement control
evidence.

## CPU Policy Bundle

Use VIS Doctor to inspect the host and export an advisory CPU policy bundle:

```bash
./vis-doctor \
  --scan \
  --duration 60 \
  --threshold 100 \
  --output out/cpu/doctor.json \
  --llm out/cpu/doctor.md \
  --cpu-policy-output out/cpu/vis-cpu-policy.json
```

The CPU policy bundle includes:

- CPU stability profile
- CPU grades: `good`, `avoid`, or `unknown`
- recommended CPU sets
- rejected CPU reasons
- placement audit requirements

Correct interpretation:

`VIS-CPU recommends CPU candidates from scan evidence.`

Incorrect interpretation:

`The workload was production-controlled by this bundle alone.`

## Placement Control Boundary

The public CPU policy bundle is advisory evidence. It can say which CPU sets
look promising or risky, but it does not prove that a workload actually stayed
on those CPUs.

Placement control claims require a separate workload audit layer that can
report:

- placement method: cgroup/cpuset or affinity fallback
- requested CPUs
- process/thread status attestation
- cgroup membership attestation when cgroup mode is used
- escape detection
- cleanup result
- whether a production control claim is allowed

Correct interpretation:

`VIS Core recommends CPU candidates. Workload placement control requires
runtime placement attestation.`
