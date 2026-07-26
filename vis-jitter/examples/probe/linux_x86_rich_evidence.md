# linux_x86_rich_evidence

This example shows the strongest current hosted evidence path in VIS Core.

- Host timing evidence is strong: `linux_x86_rich_evidence`
- Target contract is still a hosted Linux contract, not an RTOS contract
- Execution evidence remains `portable_user_space`

This is useful as support evidence for runtime characterization, but it does not
prove WCET or temporal isolation.

The explicit gates keep target timer and target execution at `host_only`; this
does not measure RTOS partition behavior, ARINC scheduling, or WCET proof.
