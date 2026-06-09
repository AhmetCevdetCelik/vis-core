# VIS Report Package

A VIS pilot should produce a small evidence package that can be reviewed by a
technical buyer, an engineer, or an AI assistant without exposing raw local
details unnecessarily.

## Package Contents

Recommended files:

- `report.redacted.json`: share-safe machine-readable evidence
- `report.md`: AI-readable technical context, if produced by the tool
- `summary.md`: short executive summary
- `manifest.json`: bundle metadata and command provenance
- `README.md`: generated bundle notes

Optional supporting files:

- CPU policy bundle
- memory policy bundle
- workload metric file
- gate report
- before/after pair of baseline/current reports

## Validate

```bash
./vis-report-validate report.json
```

Validation checks known report roots, schema metadata, generator metadata, and
policy bundle evidence semantics.

Do not ship invalid reports unless the limitation is intentionally documented.

## Redact

```bash
./vis-report-redact \
  --input report.json \
  --output report.redacted.json
```

The redactor masks common share-sensitive values:

- hostnames and usernames
- path-like fields
- workload commands
- raw metric/content fields

Always inspect the redacted file before sharing.

## Summarize

```bash
./vis-report-summary \
  --json report.redacted.json \
  --output summary.md
```

The summary should answer:

- what changed
- what did not prove
- what the recommended next step is

## Bundle

```bash
./vis-report-bundle \
  --json report.redacted.json \
  --llm report.md \
  --command "exact command or short command label" \
  --output-dir report-bundle
```

Bundle output is the preferred artifact for a pilot.

## Buyer-Friendly Interpretation

Good package language:

`VIS produced workload-specific evidence and a recommendation with limitations.`

Bad package language:

`VIS guarantees this workload is faster everywhere.`

## Minimum Pilot Bundle Checklist

- report validates
- sensitive fields are redacted
- summary exists
- command provenance exists
- evidence level is visible
- limitations are visible
- recommendation is present or the report explains why it is inconclusive
- negative/regressed candidates are not hidden
