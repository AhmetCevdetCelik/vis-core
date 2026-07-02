# arinc653_contract_only

This example shows the intended contract-only RTOS behavior.

- Host timing evidence is real and hosted: `architecture_counter`
- Target contract is ARINC 653
- Target runtime API is missing on the hosted machine
- No ARINC timing, partition isolation, WCET, or temporal isolation claim is
  opened

This is the core "supporting evidence" story for GIS-style review: VIS can
model the target backend honestly before it can actually attest it.

The explicit claim gates keep target timer and execution at `contract_only`
and require target-specific proof before any direct claim opens.
