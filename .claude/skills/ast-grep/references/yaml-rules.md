# YAML Rule Schema Reference

## Minimal Required Fields

```yaml
id: rule-id          # unique identifier (kebab-case recommended)
language: javascript # target language alias
rule: {}             # at least one matching criterion
```

## Complete Field Reference

### Identification

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `id` | string | yes | Unique rule identifier. Used in test files and reports. |
| `language` | string | yes | Language alias: `js`, `ts`, `tsx`, `py`, `go`, `rust`, `java`, etc. |
| `url` | string | no | Documentation link shown in diagnostic output. |
| `metadata` | object | no | Arbitrary key-value pairs for external tooling. |

### Matching

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `rule` | rule object | yes | Defines which AST nodes to match. See [rule-operators.md](rule-operators.md). |
| `constraints` | object | no | Extra filters on captured metavariables. Keys are variable names (without `$`). |
| `utils` | object | no | Local utility rules referenced via `matches:` inside this rule. |

### Modification

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `fix` | string or FixConfig | no | Auto-fix replacement. May reference metavariables. |
| `transform` | object | no | Manipulate metavariable text before use in `fix`. |
| `rewriters` | array | no | Named rewriter rules for complex multi-node transforms. |

### Reporting

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `severity` | string | no | `hint`, `info`, `warning` (default), `error`, `off` |
| `message` | string | no | Single-line diagnostic message. May reference `$METAVAR`. |
| `note` | string | no | Extended guidance. Supports markdown. Cannot reference metavariables. |
| `labels` | object | no | Customize code span highlighting. Keys: `primary`, `secondary`. |

### File Targeting

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `files` | string or array | no | Glob patterns — only scan matching files. |
| `ignores` | string or array | no | Glob patterns — skip matching files. |

---

## constraints

Filter matches by applying additional rules to captured metavariables:

```yaml
rule:
  pattern: $FN($$$ARGS)
constraints:
  FN:                    # must match without the $ prefix
    regex: "^use[A-Z]"  # only match React hooks
```

Multiple constraints combine as AND:

```yaml
constraints:
  X:
    kind: identifier
  Y:
    not:
      regex: "^_"   # Y must not start with underscore
```

---

## utils

Define reusable sub-rules within the same file, referenced via `matches:`:

```yaml
rule:
  all:
    - matches: is-hook-call
    - inside:
        kind: function_declaration

utils:
  is-hook-call:
    all:
      - kind: call_expression
      - pattern: $FN($$$ARGS)
      - constraints:
          FN:
            regex: "^use[A-Z]"
```

---

## fix — String Form

Simple string replacement with metavariable interpolation:

```yaml
fix: "Number($STR)"          # replaces matched node
fix: "$OBJ?.$PROP"           # optional chaining
fix: "$$$ARGS"               # spread captured args
fix: ""                      # delete the matched node
```

Indentation is preserved relative to the fix template.

---

## fix — FixConfig Object Form

For deletions that must consume surrounding punctuation (commas, brackets):

```yaml
fix:
  template: ""               # empty = delete
  expandStart:
    regex: ",\s*"            # expand range start to consume preceding comma
    stopBy: ancestor         # stop at: neighbor | ancestor | end
  expandEnd:
    regex: "\s*,"            # expand range end to consume trailing comma
```

Use when deleting list elements, function arguments, or object properties where naive deletion leaves orphan commas.

---

## transform

Manipulate captured metavariable text before inserting into `fix`. Each key is a new variable name (no `$`) available in `fix` as `$NEW_NAME`.

### replace (regex substitution)

```yaml
transform:
  CLEAN:
    replace:
      source: $RAW
      replace: "\\s+"        # Rust regex
      by: "_"
```

### substring

```yaml
transform:
  FIRST5:
    substring:
      source: $FULL
      startChar: 0           # inclusive, 0-based
      endChar: 5             # exclusive; negative indices supported
```

### convert (case conversion)

```yaml
transform:
  SNAKE:
    convert:
      source: $CAMEL_VAR
      toCase: snakeCase      # lowerCase | upperCase | capitalize | camelCase | snakeCase | kebabCase | pascalCase
      separatedBy:           # optional: how to split the input
        - CaseChange
        - Underscore
        - Dash
```

### rewrite (apply rewriter rules to metavariable)

```yaml
transform:
  RESULT:
    rewrite:
      source: $$$ITEMS       # often a multi-node capture
      rewriters:
        - rewriter-id-1
        - rewriter-id-2
      joinBy: "\n"           # join multiple results
```

---

## rewriters

Named transformation rules for complex multi-node rewrites. Each rewriter is a mini-rule applied to each node within a metavariable:

```yaml
rewriters:
  - id: wrap-in-parens
    rule:
      kind: binary_expression
    fix: "($NODE)"
  - id: stringify-number
    rule:
      kind: number
    fix: "String($NODE)"
```

**Rewriter scope rules:**
- Metavariables captured in a rewriter are local to that rewriter only
- Metavariables from the parent rule ARE available in rewriters
- The first matching rewriter in the array wins; order matters

---

## Severity Levels

| Level | Effect |
|-------|--------|
| `error` | Exits with non-zero code; blocks CI |
| `warning` | Shown but doesn't fail CI by default |
| `info` | Informational, lower visibility |
| `hint` | Editor hints only |
| `off` | Rule disabled (skipped unless `--include-off`) |

---

## labels

Customize which code spans are highlighted and how:

```yaml
labels:
  primary:
    - source: $DANGEROUS_ARG
      style: underline
      message: "This argument is dangerous"
  secondary:
    - source: $FN
      style: warning
```

---

## Complete Rule Example

```yaml
id: no-console-debug
language: javascript
files:
  - "src/**/*.js"
ignores:
  - "**/*.test.js"
  - "**/*.spec.js"

rule:
  all:
    - pattern: "console.debug($$$ARGS)"
    - not:
        inside:
          kind: comment

severity: warning
message: "Prefer console.log over console.debug in production code"
note: |
  `console.debug` is filtered out by default in many browsers and log aggregators.
  Use `console.log` for messages that should always appear.
url: "https://developer.mozilla.org/en-US/docs/Web/API/console/debug_static"

fix: "console.log($$$ARGS)"

metadata:
  category: logging
  auto-fixable: true
```
