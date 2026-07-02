# ARINC 653 and POSIX PSE53 PoC Plan

The table below describes the next probe contracts even before a production
backend exists. The point is to make open claims and closed claims explicit.

| Profile | Candidate APIs or surfaces | Fields VIS should fill | Claims that can open | Claims that stay closed |
|---|---|---|---|---|
| ARINC 653 | partition status and time services, process management services, health monitoring hooks, platform timer API | `target_profile_family`, `target_runtime_api_status`, scheduler model, partition model, selected timer, timer overhead, monotonicity, partition window metadata if exposed | target contract shape exists, partition and scheduler surface identified, timer source contract identified | no temporal isolation proof, no WCET proof, no partition behavior claim without target evidence |
| POSIX PSE53 | `clock_gettime`, thread scheduling APIs, CPU affinity if available, process and thread state APIs, vendor timer wrappers | `target_profile_family`, selected timer, timer evidence level, execution evidence level, scheduling surface, limitations | portable probe can state timer and execution surface and gather target-side timer evidence when APIs exist | no partition isolation claim, no certification claim, no stronger claim than exposed target APIs allow |

## Minimum ARINC 653 Contract

VIS should be able to state:

- which partition and timing service family is expected
- whether the backend is contract-only or active
- whether any partition window metadata was actually observed
- whether the timer path is target-runtime evidence or contract-only

## Minimum PSE53 Contract

VIS should be able to state:

- which portable timer was used
- whether thread scheduling state was visible
- whether the report is still host-like or actually target-runtime evidence
- which claims remain closed due to missing APIs

## Claim Rules

Open a claim only when the report has matching evidence:

- `backend_status=selected` for real collection
- `execution_evidence_level=rtos_execution_surface` for target runtime
  execution evidence
- `target_runtime_api_status=host_native` only when the requested contract
  actually matches the current runtime

Keep claims closed when any of these are true:

- `evidence_level=contract_only`
- `backend_status=recognized_api_missing`
- `timer_evidence_level=contract_only`
- `execution_evidence_level=contract_only`

This lets the report say "we know the contract" without pretending "we
measured the target."
