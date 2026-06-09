# Community Testing

VIS needs measurements from many Linux/x86_64 machines. The goal is not to
prove that every system improves. The goal is to collect reproducible evidence:
CPU jitter, SMI contamination, and host diagnosis evidence.

## What to test

Useful machines include:

- laptops on AC power and battery
- desktops and workstations
- servers
- hybrid P-core/E-core CPUs
- robotics or drone companion computers
- low-latency, audio, trading, inference, and gaming systems

## Safety notes

VIS Core commands do not make persistent system changes. The commands
below build the tools, run rootless tests, load the Linux `msr` module, and run
a root-required CPU scan. They do not change BIOS, governor, IRQ, isolation, or
memory settings.

Before sharing reports, remove hostnames, usernames, private paths, serial
numbers, customer names, or any other identifier you do not want public.

## Prerequisites

Ubuntu/Debian example:

```bash
sudo apt install build-essential libnuma-dev
```

The current prototype targets Linux/x86_64 with MSR access. Windows, macOS, and
non-x86 systems are not supported yet.

## Quick community test

Run from a fresh clone:

```bash
git clone https://github.com/AhmetCevdetCelik/vis-core.git
cd vis-core/vis-jitter

make
make test
sudo modprobe msr

sudo ./vis-doctor --scan --duration 30 --threshold 100 \
  --output doctor.json \
  --llm doctor.md \
  --cpu-policy-output vis-cpu-policy.json

./vis-report-validate doctor.json
./vis-report-bundle \
  --json doctor.json \
  --llm doctor.md \
  --command "vis-doctor community scan" \
  --output-dir doctor-bundle
```

If the 30 second all-core scan is too long, use `--duration 10` and mention that
the report is a short scan. Longer scans give better evidence for intermittent
SMI or scheduler noise.

## What to submit

Open a `Machine Report` issue and include:

- CPU model
- distro and kernel
- laptop/desktop/server/companion-computer
- AC power or battery
- whether the machine was idle or under load
- exact commands you ran
- `doctor.md`
- optional `doctor.json`, `vis-cpu-policy.json`, and the `doctor-bundle`

Markdown reports are easiest for humans and AI assistants to review. JSON
reports are better for later aggregation.

## What VIS can learn from reports

Community reports can help answer:

- How often do SMI-contaminated windows appear?
- Which CPUs are clean candidates on different machines?
- Do hybrid CPUs show clear throughput classes?
- How often do governor, SMT, or isolation warnings appear?
- Do Doctor policies look stable across repeated scans?
- Which hosts need stronger private placement/audit testing?

Do not interpret one result as a universal rule. VIS is evidence-first: repeat,
compare, and report the conditions.
