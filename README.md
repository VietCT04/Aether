# Aether

Aether is a high-performance C++ event-processing engine for ordered market-data workloads, designed to explore predictable latency, deterministic state processing, and systems-level performance.

## Current Scope

Aether v0.1 focuses on a single-process event-processing pipeline:

```text
event ingestion
      ↓
event representation
      ↓
event transport
      ↓
market state
