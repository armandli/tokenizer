# ast-grep CLI Reference

## `sg run` — One-shot Search and Rewrite

```
sg run [OPTIONS] [PATHS]...
sg [OPTIONS] [PATHS]...   # shorthand (run is default)
```

### Flags

| Flag | Description |
|------|-------------|
| `-p, --pattern PATTERN` | AST pattern to match |
| `-l, --lang LANG` | Language to parse files as (auto-detected if omitted) |
| `-r, --rewrite REWRITE` | Replacement template; may reference captured metavariables |
| `-k, --kind KIND` | Match nodes by AST node type (e.g., `call_expression`) |
| `-i, --interactive` | Interactively accept/reject each change |
| `-U, --update-all` | Apply all rewrites without prompting |
| `--json [STYLE]` | Output matches as JSON (styles: `compact`, `pretty`) |
| `--color WHEN` | Color output: `auto` (default), `always`, `ansi`, `never` |
| `--no-ignore` | Respect no ignore files (`.gitignore`, etc.) |
| `--hidden` | Search hidden files and directories |
| `--follow` | Follow symbolic links |
| `--threads N` | Number of parallel threads |
| `-c, --config PATH` | Path to `sgconfig.yml` (for rule utilities) |
| `--debug-query` | Print the parsed AST of the pattern for debugging |
| `-h, --help` | Display help |

### Examples

```bash
# Find all console.log calls
sg run -p 'console.log($$$ARGS)' -l js

# Rewrite: replace parseInt with Number
sg run -p 'parseInt($STR)' -r 'Number($STR)' -l js src/

# Interactive rewrite
sg run -p 'var $X = $Y' -r 'let $X = $Y' -l js -i .

# Match by node kind only (no pattern)
sg run -k call_expression -l js src/

# Read from stdin
echo "eval(x)" | sg run -p 'eval($X)' -l js --stdin

# JSON output for piping to jq
sg run -p 'console.log($X)' -l js --json | jq '.[].text'

# Suppress applying changes (dry run of rewrite)
sg run -p '$A && $A()' -r '$A?.()' -l ts --json .
```

---

## `sg scan` — Project-wide Linting

```
sg scan [OPTIONS] [PATHS]...
```

Requires `sgconfig.yml`. Runs all rules from configured `ruleDirs`.

### Flags

| Flag | Description |
|------|-------------|
| `-c, --config PATH` | Path to `sgconfig.yml` (default: searches upward) |
| `-r, --rule RULE_FILE` | Run a single rule file instead of all project rules |
| `--error` | Show only error-severity findings |
| `--warning` | Show only warning-severity findings |
| `--info` | Show only info-severity findings |
| `--json [STYLE]` | Output results as JSON |
| `--format FORMAT` | Output format: `rich` (default), `json`, `github`, `sarif` |
| `-i, --interactive` | Interactively apply fixes |
| `-U, --update-all` | Apply all fixes automatically |
| `--no-ignore` | Do not respect gitignore |
| `--threads N` | Number of parallel threads |

### Examples

```bash
# Run all project rules
sg scan

# Run a single rule file
sg scan -r rules/no-eval.yml src/

# Only errors, JSON output
sg scan --error --json

# Apply all fixes automatically
sg scan -U

# SARIF format for CI integration
sg scan --format sarif > results.sarif
```

---

## `sg test` — Validate Rules

```
sg test [OPTIONS]
```

### Flags

| Flag | Description |
|------|-------------|
| `-c, --config PATH` | Path to `sgconfig.yml` |
| `-t, --test-dir PATH` | Directory containing test YAML files |
| `--snapshot-dir PATH` | Where to store/read snapshots (default: `__snapshots__`) |
| `--skip-snapshot-tests` | Run match/non-match tests only, skip snapshot diffs |
| `-U, --update-all` | Accept and write all changed snapshots |
| `-i, --interactive` | Interactively accept/reject snapshot changes |
| `-f, --filter GLOB` | Run only test files matching the glob |
| `--include-off` | Also test rules with `severity: off` |
| `-h, --help` | Display help |

### Examples

```bash
# Run all tests
sg test

# Generate/update snapshots after polishing rules
sg test -U

# Run tests for one specific rule
sg test -f no-eval

# Skip snapshot validation during development
sg test --skip-snapshot-tests
```

---

## `sg new` — Scaffold Projects and Rules

```
sg new [ITEM_TYPE]
```

Interactive wizard. Item types:

| Type | Creates |
|------|---------|
| _(no arg)_ | Full project: `sgconfig.yml`, `rules/`, `rule-tests/`, `utils/` |
| `rule` | New rule YAML file in `rules/` |
| `test` | New test YAML file in `rule-tests/` |
| `util` | New utility rule in `utils/` |

### Examples

```bash
# Initialize a new ast-grep project
sg new

# Add a new rule interactively
sg new rule

# Add a test for an existing rule
sg new test
```

---

## `sg lsp` — Language Server

```
sg lsp [OPTIONS]
```

Starts an LSP server for editor integration (VS Code, Neovim, etc.). Provides:
- Inline diagnostics from project rules
- Hover information

---

## `sg completions` — Shell Completions

```
sg completions SHELL
```

Supported shells: `bash`, `zsh`, `fish`, `powershell`, `elvish`

```bash
# Install zsh completions
sg completions zsh > ~/.zsh/completions/_sg
```
