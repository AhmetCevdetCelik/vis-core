# arm_generic_timer_evidence

This example is a validator-compatible synthetic report that represents the
intended AArch64 generic timer path.

- Host timing evidence is `arm_generic_timer_evidence`
- Target contract is still hosted, not RTOS-attested
- Execution evidence remains `portable_user_space`

It demonstrates how VIS should describe an ARM path without claiming RTOS
partition or temporal isolation evidence.
