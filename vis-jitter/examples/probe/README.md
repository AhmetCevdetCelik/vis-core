# VIS Probe Examples

These examples show three distinct evidence shapes:

- `linux_x86_rich_evidence.json`: real hosted Linux/x86 runtime evidence shape
- `arm_generic_timer_evidence.json`: ARM generic timer evidence shape
- `arinc653_contract_only.json`: target contract without target measurement

The important point is not only the timer name. The important point is whether
the report is:

- hosted runtime evidence
- target contract only
- future-looking target-runtime evidence shape

Use `vis-report-validate` before sharing:

```bash
cd vis-jitter
make vis-report-validate
./vis-report-validate examples/probe/linux_x86_rich_evidence.json
./vis-report-validate examples/probe/arm_generic_timer_evidence.json
./vis-report-validate examples/probe/arinc653_contract_only.json
```
