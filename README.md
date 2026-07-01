# VIS Core

[![CI](https://github.com/AhmetCevdetCelik/vis-core/actions/workflows/ci.yml/badge.svg)](https://github.com/AhmetCevdetCelik/vis-core/actions/workflows/ci.yml)
![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20x86__64-blue.svg)
![Status](https://img.shields.io/badge/status-public%20core-lightgrey.svg)

VIS Core is the public open-core evidence toolkit for VIS.

It focuses on the parts that are useful for trust, inspection, and community
testing:

- CPU jitter measurement with SMI-aware evidence
- host/runtime diagnosis through VIS Doctor
- structured JSON report metadata
- report validation, redaction, summary, and bundle tooling
- claim boundaries and community testing docs
- portable platform profiling foundations for future non-x86 evidence paths

VIS Core is **not** a magic accelerator. It does not claim to make every
program faster. Its purpose is narrower:

> Critical software should not run on hardware blindly. It should run with
> measured, explainable, and repeatable runtime evidence.

## Try This First

```bash
git clone https://github.com/AhmetCevdetCelik/vis-core.git
cd vis-core
sudo apt install build-essential libnuma-dev

cd vis-jitter
make
make test

./vis-doctor \
  --inspect \
  --output doctor-inspect.json \
  --llm doctor-inspect.md \
  --cpu-policy-output vis-cpu-policy.json

./vis-report-validate doctor-inspect.json
```

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
- [Portable platform foundation](vis-jitter/docs/portable-platform-foundation.md)
- [Report package](vis-jitter/docs/report-package.md)
- [Doctor output](vis-jitter/docs/doctor-output.md)
- [Community testing](docs/community-testing.md)
- [Sharing guide](docs/share.md)

## VIS Core vs VIS Pro

| VIS Core | VIS Pro |
|---|---|
| `vis-jitter` CPU jitter measurement | `vis-audit` one-command workload audit |
| `vis-doctor` host diagnostics | Profile store and drift workflows |
| Report validation/redaction/bundling | CPU/Mem workload policy comparison |
| Public claim-boundary docs | Inference advisor and pilot reports |
| Community scan and sharing docs | Customer pilot playbooks and case studies |
| MIT open-source core | Private commercial suite |

Commercial/private VIS workflow code is not part of VIS Core. That includes
generic audit orchestration, profile stores, advanced inference/profile advisor
workflows, advanced gate policies, customer pilot packages, and future
dashboard/API/agent work.

Those belong to the private VIS Pro line.

## Prerequisites

Ubuntu/Debian:

```bash
sudo apt update
sudo apt install build-essential libnuma-dev
```

Optional developer tools:

```bash
sudo apt install clang-format cppcheck
```

## Build And Test

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

## Report Bundle Smoke

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
