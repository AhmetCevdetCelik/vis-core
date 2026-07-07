# VIS Core Examples

These examples show public VIS Core evidence flows without overclaiming what
the evidence proves.

## CPU

See [cpu/README.md](cpu/README.md).

Use when the question is:

`Which CPUs look safe, noisy, excluded, or ready for deeper placement audit?`

Primary tool:

- `vis-doctor`

## Probe

See [probe/README.md](probe/README.md).

Use when the question is:

`What does VIS currently know about timing source evidence versus target RTOS contract evidence?`

Primary tool:

- `vis-probe`

## Report Packaging

See [../docs/report-package.md](../docs/report-package.md).

Use before sharing evidence externally:

- validate
- redact
- summarize
- bundle

## Probe Contracts

See [probe/README.md](probe/README.md).

Use when the question is:

`Is this report hosted runtime evidence, an RTOS target contract, or a future target-runtime evidence shape?`

Primary artifacts:

- `examples/probe/linux_x86_rich_evidence.json`
- `examples/probe/arm_generic_timer_evidence.json`
- `examples/probe/arinc653_contract_only.json`

## Claim Boundaries

Before sharing evidence externally, use:

- [Claim boundaries](../docs/claim-boundaries.md)
- [GIS probe approach](../docs/gis-vis-probe-approach.md)
- [ARINC/PSE53 PoC plan](../docs/arinc-pse53-poc-plan.md)

VIS Core examples are public evidence examples. Commercial pilot workflows,
generic audit orchestration, profile store drift, and inference advisor flows
belong to VIS Pro.
