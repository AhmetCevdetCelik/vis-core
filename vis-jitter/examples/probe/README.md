# VIS Probe Examples

These examples show three distinct probe evidence modes:

- hosted Linux/x86 rich timing evidence
- synthetic ARM generic timer evidence
- ARINC 653 contract-only target evidence

The goal is not to prove RTOS behavior from a hosted machine. The goal is to
show how VIS separates:

- host-observed platform evidence
- target runtime contract expectations
- safe claim boundaries

## Files

- [linux_x86_rich_evidence.json](linux_x86_rich_evidence.json)
- [linux_x86_rich_evidence.md](linux_x86_rich_evidence.md)
- [arm_generic_timer_evidence.json](arm_generic_timer_evidence.json)
- [arm_generic_timer_evidence.md](arm_generic_timer_evidence.md)
- [arinc653_contract_only.json](arinc653_contract_only.json)
- [arinc653_contract_only.md](arinc653_contract_only.md)

## Interpretation

`linux_x86_rich_evidence`:

- strong host timing-source evidence on Linux/x86
- still not a WCET or temporal isolation proof

`arm_generic_timer_evidence`:

- architecture-counter evidence for an AArch64 path
- target RTOS attestation is still out of scope

`arinc653_contract_only`:

- target contract is modeled
- hosted machine does not expose target partition APIs
- no ARINC 653 timing or isolation claim is opened

## Regeneration

The checked-in JSON files are curated evidence snapshots. To capture a new
hosted report and generate the standard executive summary for comparison, run:

```bash
./vis-probe --backend auto --output /tmp/probe.json
./vis-report-summary --json /tmp/probe.json --output /tmp/probe.md
```

To capture a new contract-only report and generate its standard executive
summary for comparison, run:

```bash
./vis-probe --backend arinc653_partition_probe --output /tmp/probe.json
./vis-report-summary --json /tmp/probe.json --output /tmp/probe.md
```

`vis-report-summary` always writes a `# VIS Executive Summary` document. The
checked-in Markdown files are intentionally shorter, example-specific
annotations and are maintained manually rather than copied from that output.
When refreshing an example, inspect `/tmp/probe.md`, then update the matching
Markdown file by hand so its evidence level, target contract, unavailable
runtime APIs, and claim limitations agree with the refreshed JSON. The
synthetic ARM example must likewise be updated manually from its curated JSON;
it cannot be captured on a non-AArch64 host.
