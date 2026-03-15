# hedis-cql

CQL-based distributed HEDIS measure evaluation engine. Replaces hardcoded C++ HEDIS rules with [CQL (Clinical Quality Language)](https://cql.hl7.org/) measure definitions that can be updated without recompilation.

## Architecture

```
hedis-cql/
  src/
    cql/           CQL parser, evaluator, three-valued logic engine
    measure/       Measure definitions, value set management, results
    engine/        Orchestrator and thread pool
    distributed/   24-server partition model, staging writer, aggregator
    data/          Patient data loading and type adapters
    main.cpp       Worker (runs on each server)
    main_ctl.cpp   Partition controller
    main_merge.cpp Staging-to-production merge
  measures/        CQL measure definitions (.cql files)
  valuesets/        Value set definitions (JSON)
  schema/          Oracle DDL for results tables
  test/            Unit tests (22 tests)
```

### Key Components

- **CQL Parser** — Hand-written recursive-descent parser for the HEDIS-relevant CQL subset. Tokenizes CQL source into an AST with support for `between`, `during`, `overlaps`, `start of`/`end of`, retrieve expressions, and quantity literals.
- **CQL Evaluator** — Tree-walking interpreter with CQL three-valued logic (true/false/null). Supports short-circuit evaluation, define memoization, and built-in functions (`AgeInYearsAt`, `Exists`, `Count`, `DurationBetween`, etc.).
- **Value Set Manager** — O(1) code membership lookup via hash index. Loads from JSON or Oracle.
- **Partition Manager** — Distributes work across up to 24 servers using exclusive partition claiming. Each server processes a range of patient IDs.
- **Worker Pool** — Multi-threaded per-partition processing with mutex-protected patient loading and result writing.

### Included HEDIS Measures

| ID  | Measure | Description |
|-----|---------|-------------|
| CCS | Cervical Cancer Screening | Pap smear within 3 years or pap+HPV within 5 years (women 24-64) |
| BCS | Breast Cancer Screening | Mammogram within 2 years (women 52-74) |
| COL | Colorectal Cancer Screening | Colonoscopy/FOBT/FIT-DNA (age 46-75) |
| CBP | Controlling High Blood Pressure | Last BP < 140/90 (age 18-85 with hypertension) |
| CDC | Comprehensive Diabetes Care | HbA1c < 8.0% (age 18-75 with diabetes) |

## Building

Requires C++17 compiler and [SCons](https://scons.org/). Oracle OCI and ANTLR4 are optional — the system auto-detects and builds in stub mode without them.

```bash
# Full build (3 executables + test binary)
scons -j8

# Tests only
scons hedis_cql_tests

# Debug build
scons -j8 BUILD_TYPE=debug

# Clean
scons -c
```

### Optional Dependencies

| Library | Purpose | Without it |
|---------|---------|------------|
| OCILIB + Oracle Instant Client | Oracle database access | Stub mode (filesystem-only) |
| ANTLR4 C++ runtime | Full CQL grammar parser | Uses built-in recursive-descent parser |

Set paths via environment or `custom.py`:
```bash
export OCI_HOME=/opt/oracle/instantclient
export ANTLR4_HOME=/usr/local
```

## Running Tests

```bash
scons hedis_cql_tests && ./hedis_cql_tests
```

Tests cover the CQL parser, evaluator (three-valued logic, date arithmetic, intervals), measure evaluation (CCS end-to-end), value set lookup, and partition management.

## Usage

### Worker (runs on each of 24 servers)
```bash
./hedis_cql -s oracle-host -t 8 -c config/hedis_cql.ini
```

### Partition Controller (run once to set up job)
```bash
./hedis_cql_ctl -s oracle-host -j JOB001 -n 24 -b 25000 -y 2026
```

### Merge (run after all partitions complete)
```bash
./hedis_cql_merge -s oracle-host -j JOB001
```

### Filesystem Mode (no Oracle)
```bash
./hedis_cql -m measures/ -v valuesets/hedis_2026_valuesets.json -y 2026
```
