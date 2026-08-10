# ast-grep Project Setup

## Initialize a Project

```bash
sg new
```

Interactive wizard creates the standard directory layout.

## Standard Directory Layout

```
my-project/
├── sgconfig.yml          # root config (required for sg scan / sg test)
├── rules/                # YAML lint/rewrite rules
│   ├── no-eval.yml
│   ├── prefer-const.yml
│   └── security/
│       └── no-sql-injection.yml
├── rule-tests/           # test cases for each rule
│   ├── no-eval.test.yml
│   └── prefer-const.test.yml
├── utils/                # global utility rules (reused via matches:)
│   ├── is-test-file.yml
│   └── common-patterns.yml
└── src/                  # your source code
```

---

## `sgconfig.yml` — Full Reference

### Minimal config

```yaml
ruleDirs:
  - rules
```

### Complete config

```yaml
# Required
ruleDirs:
  - rules
  - vendor-rules/security  # multiple dirs supported

# Test configuration
testConfigs:
  - testDir: rule-tests
    snapshotDir: rule-tests/__snapshots__  # default: "snapshots"

# Utility rule directories (accessible via matches: in any rule)
utilDirs:
  - utils
  - shared/utils

# Override which file extensions map to which language
languageGlobs:
  javascript: "**/*.{js,mjs,cjs}"
  typescript: "**/*.{ts,mts,cts}"
  python: "**/*.{py,pyi}"

# Custom language support (requires a tree-sitter shared library)
customLanguages:
  mylang:
    libraryPath: /path/to/mylang.so
    extensions:
      - ".ml"
    expandoChar: "_"         # optional: char used for identifier expansion
    languageSymbol: mylang   # optional: symbol name in the library

# Experimental: embedded language support
languageInjections:
  - hostLanguage: html
    injected: javascript
    rule:
      kind: script_element
  - hostLanguage: html
    injected: css
    rule:
      kind: style_element
```

### Field-by-field notes

**`ruleDirs`** (required, string list)
- Paths relative to sgconfig.yml
- All `.yml` files under these directories are loaded as rules
- Subdirectories are included recursively

**`testConfigs`** (optional, array)
- `testDir`: directory containing test YAML files
- `snapshotDir`: where `sg test -U` writes snapshot files
- Multiple `testConfigs` entries allowed for different rule sets

**`utilDirs`** (optional, string list)
- Like `ruleDirs` but for utility rules only
- Utility rules are NOT run as lint rules — only usable via `matches:` in other rules
- Enables sharing common patterns (e.g., "is this inside a test block?") across all rules

**`languageGlobs`** (optional, object)
- Keys: language names (same aliases used in `-l` flag)
- Values: glob patterns
- Takes precedence over default file extension detection
- Useful for monorepos with non-standard extensions or mixed JS/TS files

**`customLanguages`** (optional, object)
- Adds support for languages not built into ast-grep
- Requires a tree-sitter parser compiled as a shared library (`.so`/`.dylib`/`.dll`)
- `expandoChar`: the character used for "magic" identifier nodes in the grammar
- `languageSymbol`: the C symbol name exported by the library (auto-detected if omitted)

**`languageInjections`** (optional, experimental)
- Enables scanning embedded languages (JS inside HTML script tags, etc.)
- `hostLanguage`: the outer language
- `injected`: the inner language
- `rule`: selects which nodes in the host language contain the embedded language

---

## Project Commands

```bash
# Initialize project
sg new

# Add a new rule (creates rules/your-rule-id.yml)
sg new rule

# Add a test for an existing rule
sg new test

# Scan all files with all rules
sg scan

# Scan only errors
sg scan --error

# Scan specific paths
sg scan src/ lib/

# Apply all fixes from scan
sg scan -U

# Override config location
sg scan -c path/to/sgconfig.yml

# Run tests
sg test

# Update all snapshots
sg test -U
```

---

## Utility Rules Pattern

Utility rules live in `utilDirs`. They are regular rule YAML files but are never reported as findings — they exist to be referenced by `matches:`.

`utils/is-test-context.yml`:
```yaml
id: is-test-context
language: javascript
rule:
  any:
    - inside:
        kind: call_expression
        pattern: "describe($$$ARGS)"
    - inside:
        kind: call_expression
        pattern: "it($$$ARGS)"
    - inside:
        kind: call_expression
        pattern: "test($$$ARGS)"
```

Use in any rule:

```yaml
id: no-console-in-prod
language: javascript
rule:
  all:
    - pattern: "console.log($$$ARGS)"
    - not:
        matches: is-test-context
severity: warning
message: "Remove console.log before shipping"
```

---

## Config Discovery

`sg scan` and `sg test` search for `sgconfig.yml` by walking up from the current directory. Override with `--config PATH`.

If no `sgconfig.yml` is found, `sg scan` fails. `sg run` never needs it (unless you use `matches:` referencing utility rules).
