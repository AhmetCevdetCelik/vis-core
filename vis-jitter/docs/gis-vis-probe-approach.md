# GIS-Oriented VIS Probe Approach

This note explains how the current VIS Core probe can evolve toward a smaller
runtime probe that fits RTOS-oriented environments without overclaiming what
the current code proves today.

## What VIS Measures

VIS measures timing behavior and the evidence surface around that timing
behavior.

Today that means:

- which timer source was selected
- whether the timer appears monotonic
- timer read overhead
- whether scheduler and affinity surfaces are visible
- whether privileged counters or Linux-only signals are visible
- what evidence scope the report actually contains

On Linux/x86, VIS can also emit richer hosted timing evidence through the
Linux/x86 probe path.

## What VIS Does Not Measure

VIS does not directly measure or prove:

- certified RTOS conformance
- temporal isolation as a formal property
- WCET as a formal bound
- interrupt behavior on a target that VIS did not actually access
- partition scheduling on ARINC 653 unless a target backend exposes it
- target runtime evidence when the report is only a host observation or
  contract-only placeholder

## How It Supports Temporal Isolation Work

VIS can support temporal isolation analysis by making interference evidence
explicit:

- timer quality is stated rather than assumed
- scheduler surface visibility is recorded
- hosted observations are separated from target-runtime observations
- target contract placeholders can document which partition and scheduling
  fields must eventually be collected

This helps engineers ask the right question: what evidence do we really have
for this runtime surface?

## How It Supports WCET Work

VIS can support WCET-oriented engineering as supporting runtime evidence:

- timing distributions reveal spread and outliers
- backend status shows whether the evidence came from a real backend or only a
  contract stub
- timer and execution evidence levels show whether timing data is hosted,
  target-side, or contract-only
- claim ceilings prevent reports from sounding stronger than they are

That makes VIS useful as a measurement and evidence companion to WCET
activities, not as a replacement for them.

## Why It Is Not Direct Proof

VIS is not direct proof of temporal isolation or WCET because:

- a measurement report samples behavior; it does not prove all future runs
- many safety claims depend on full system context, not only one timer stream
- hosted Linux evidence is not identical to target RTOS evidence
- target APIs may be unavailable, partial, or vendor-specific
- certification arguments require broader evidence chains and process controls

The honest claim is narrower:

> VIS can provide supporting timing evidence, explicit limitations, and target
> contract structure for later RTOS backend work.

## Near-Term Probe Direction

The next useful backend shape is:

- small
- low-intrusion
- timer-centric
- explicit about hosted versus target evidence
- able to model scheduler and partition surfaces even when data collection is
  not yet implemented

That direction matches ARINC 653, POSIX PSE53, and similar runtime profiles
better than a direct Linux collector port.
