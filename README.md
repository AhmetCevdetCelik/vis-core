# VIS Core

VIS Core is the public open-core evidence toolkit for VIS.

It focuses on the parts that are useful for trust, inspection, and community
testing:

- CPU jitter measurement with SMI-aware evidence
- host/runtime diagnosis through VIS Doctor
- structured JSON report metadata
- report validation, redaction, summary, and bundle tooling
- claim boundaries and community testing docs

VIS Core is **not** a magic accelerator. It does not claim to make every
program faster. Its purpose is narrower:

> Critical software should not run on hardware blindly. It should run with
> measured, explainable, and repeatable runtime evidence.

## What Is Included

The implementation lives in `vis-jitter/`.

Public tools:

- `vis-jitter`: SMI-aware CPU jitter measurement
- `vis-doctor`: host inspection and CPU evidence diagnosis
- `vis-report-validate`: VIS report schema/metadata validation
- `vis-report-redact`: share-safe redaction for report JSON
- `vis-report-summary`: short executive summary generation
- `vis-report-bundle`: bundle JSON, Markdown, command provenance, and README

Public docs:

- [Claim boundaries](vis-jitter/docs/claim-boundaries.md)
- [Report package](vis-jitter/docs/report-package.md)
- [Doctor output](vis-jitter/docs/doctor-output.md)
- [Community testing](docs/community-testing.md)
- [Sharing guide](docs/share.md)

## What Is Not Included

Commercial/private VIS workflow code is not part of VIS Core:

- one-command generic audit orchestration
- profile store and drift workflows
- advanced inference/profile advisor workflows
- advanced gate policies
- customer pilot playbooks and case-study packages
- future dashboard/API/agent work

Those belong to the private VIS Pro line.

## Build

```bash
cd vis-jitter
make
make test
make lint
make docs-check
```

Optional root evidence:

```bash
sudo modprobe msr
sudo ./vis-doctor --scan --duration 30 --threshold 100 \
  --output doctor.json \
  --llm doctor.md \
  --cpu-policy-output vis-cpu-policy.json
```

## Quick Smoke

```bash
cd vis-jitter
make vis-doctor vis-report-validate vis-report-bundle

./vis-doctor \
  --inspect \
  --output doctor-inspect.json \
  --llm doctor-inspect.md \
  --cpu-policy-output vis-cpu-policy.json

./vis-report-validate doctor-inspect.json

./vis-report-bundle \
  --json doctor-inspect.json \
  --llm doctor-inspect.md \
  --command "vis-doctor --inspect" \
  --output-dir doctor-bundle
```

## License

VIS Core is released under the MIT License. See [LICENSE](LICENSE).

## Security And Sharing

VIS reports can contain hostnames, paths, workload commands, and environment
details. Before sharing reports:

```bash
./vis-report-redact --input report.json --output report.redacted.json
./vis-report-summary --json report.redacted.json --output summary.md
```

Then inspect the redacted report manually.
