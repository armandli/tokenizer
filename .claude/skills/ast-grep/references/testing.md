# Testing ast-grep Rules

## Test File Format

Create one test file per rule in your `testDir` (configured in `sgconfig.yml`). Name convention: `<rule-id>.test.yml` or `<rule-id>.yml`.

```yaml
id: no-eval              # must match the rule's id exactly

valid:                   # code that should NOT trigger the rule
  - "eval"               # string (not a call, should not match)
  - |
    function safe() {
      return 42;
    }

invalid:                 # code that SHOULD trigger the rule
  - "eval(userInput)"
  - |
    const result = eval(expr);
```

### Rules

- `id` must exactly match the rule file's `id:` field
- Each item under `valid`/`invalid` is a code snippet (string or YAML block scalar)
- Snippets are standalone — they don't need to be complete files
- If the rule has a `fix:`, the test verifies the fix produces correct output via snapshots

---

## Four Test Outcomes

| Outcome | Meaning | Action |
|---------|---------|--------|
| **Validated** | Rule correctly skipped valid code | Pass |
| **Reported** | Rule correctly flagged invalid code | Pass |
| **Noisy** | False positive — rule fired on valid code | Fix the rule |
| **Missing** | False negative — rule missed invalid code | Fix the rule |

---

## Running Tests

```bash
# Run all tests (reads testConfigs from sgconfig.yml)
sg test

# Run tests with a specific config
sg test -c path/to/sgconfig.yml

# Run tests matching a filter
sg test -f no-eval        # runs no-eval.test.yml only
sg test -f "no-*"         # all rules starting with "no-"

# Skip snapshot validation (useful during development)
sg test --skip-snapshot-tests

# Include rules with severity: off
sg test --include-off
```

---

## Snapshot Testing

When a rule has a `fix:`, `sg test` captures the fix output in snapshots and diffs them on subsequent runs. This ensures:
- Error messages and matched spans don't silently change
- Fix output is reproducible

### Snapshot workflow

```bash
# 1. Write rule and test cases
# 2. First run: no snapshots yet
sg test --skip-snapshot-tests   # verify match/no-match logic first

# 3. Polish the rule, then generate snapshots
sg test -U                       # write all snapshots

# 4. Future runs diff against snapshots
sg test                          # passes if nothing changed

# 5. After intentional rule changes, update snapshots
sg test -U                       # accept all new snapshots
sg test -i                       # interactive: accept/reject each change
```

### Snapshot directory

Default: `__snapshots__/` inside the test directory.
Configure in `sgconfig.yml`:

```yaml
testConfigs:
  - testDir: rule-tests
    snapshotDir: rule-tests/__snapshots__
```

Snapshots are YAML files — commit them to version control so CI catches regressions.

---

## Test Command Flags

| Flag | Description |
|------|-------------|
| `-c, --config PATH` | Path to sgconfig.yml |
| `-t, --test-dir PATH` | Override testDir from config |
| `--snapshot-dir PATH` | Override snapshotDir from config |
| `--skip-snapshot-tests` | Only test match/non-match, skip snapshot diffs |
| `-U, --update-all` | Write/accept all changed snapshots |
| `-i, --interactive` | Prompt for each snapshot change |
| `-f, --filter GLOB` | Only run test files matching glob |
| `--include-off` | Also test `severity: off` rules |
| `--color WHEN` | `auto` / `always` / `ansi` / `never` |

---

## Example: Full Test File with Fix Verification

Rule (`rules/prefer-number.yml`):
```yaml
id: prefer-number
language: javascript
rule:
  pattern: parseInt($STR, 10)
severity: warning
message: "Use Number() instead of parseInt for decimal conversion"
fix: "Number($STR)"
```

Test file (`rule-tests/prefer-number.test.yml`):
```yaml
id: prefer-number

valid:
  - "parseInt(str, 16)"    # base 16 — different semantics, don't flag
  - "parseInt(str)"        # no base argument, skip
  - "Number(str)"          # already using Number

invalid:
  - "parseInt(str, 10)"
  - |
    const x = parseInt(userInput, 10);
    const y = parseInt(other, 10);
```

After `sg test -U`, the snapshot records the exact match position and fix output. On every future `sg test` run, changes to either are caught immediately.
