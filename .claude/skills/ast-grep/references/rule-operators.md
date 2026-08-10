# Rule Operators Reference

A rule object selects AST nodes. All fields present in a single rule object act as **AND** — a node must satisfy every field to match. You need at least one "positive" operator (one that pins what type of node is targeted).

---

## Atomic Operators

These match properties of a single node.

### `pattern`

Matches nodes whose source code fits an ast-grep pattern.

```yaml
rule:
  pattern: "console.log($$$ARGS)"
```

```yaml
rule:
  pattern: |
    if ($COND) {
      $$$BODY
    }
```

Multi-line patterns: use YAML block scalar (`|`).

### `kind`

Matches nodes by their tree-sitter node type name.

```yaml
rule:
  kind: call_expression
```

Find kind names by running: `sg run -p 'your.code()' -l js --debug-query`

Common kinds by language:

| Language | Common kinds |
|----------|-------------|
| JS/TS | `call_expression`, `arrow_function`, `identifier`, `string`, `template_string`, `import_statement`, `binary_expression` |
| Python | `call`, `function_definition`, `import_statement`, `assignment`, `attribute` |
| Go | `call_expression`, `func_literal`, `short_var_decl`, `go_statement` |
| Rust | `call_expression`, `closure_expression`, `let_declaration`, `use_declaration` |

### `regex`

Matches nodes whose full source text satisfies a Rust regular expression.

```yaml
rule:
  regex: "^(foo|bar)$"
```

```yaml
rule:
  all:
    - kind: identifier
    - regex: "^use[A-Z]"     # React hook names
```

Note: Rust regex syntax — no lookahead/lookbehind.

### `nthChild`

Matches nodes by their position among their parent's children (0-indexed).

```yaml
# First child
rule:
  nthChild: 0

# Second child
rule:
  nthChild: 1
```

Object form with formula (`An+B`) or named position:

```yaml
rule:
  nthChild:
    position: 2
    ofRule:
      kind: argument   # only count siblings matching this rule
    isReversed: false  # count from end if true
```

### `range`

Matches nodes at exact source positions — primarily useful for testing and tooling integration.

```yaml
rule:
  range:
    start:
      line: 5      # 1-based
      column: 0    # 0-based
    end:
      line: 5
      column: 20
```

---

## Relational Operators

These filter based on structural relationships between nodes.

All relational operators share two optional sub-fields:
- **`stopBy`**: controls how far to search. Values: `neighbor` (adjacent only), `ancestor` (any ancestor, default for `inside`/`has`), `end` (stop at specific node)
- **`field`**: restrict to a named tree-sitter field (e.g., `field: "condition"` inside an `if_statement`)

### `inside`

The matched node must be a descendant of a node matching the sub-rule.

```yaml
# Match console.log only inside async functions
rule:
  all:
    - pattern: "console.log($$$ARGS)"
    - inside:
        kind: async_function
```

```yaml
# Match variable only inside a for loop (immediate parent)
rule:
  all:
    - kind: identifier
    - inside:
        kind: for_statement
        stopBy: neighbor
```

### `has`

The matched node must contain a descendant matching the sub-rule.

```yaml
# Match functions that contain an await expression
rule:
  all:
    - kind: function_declaration
    - has:
        kind: await_expression
```

```yaml
# Match objects that have a key named "password"
rule:
  all:
    - kind: object
    - has:
        field: key
        pattern: password
```

### `follows`

The matched node must come **after** (in source order) a node matching the sub-rule. Matches siblings by default.

```yaml
# Match return statement that follows a console.log
rule:
  all:
    - kind: return_statement
    - follows:
        pattern: "console.log($$$ARGS)"
        stopBy: neighbor
```

### `precedes`

The matched node must come **before** a node matching the sub-rule.

```yaml
# Match variable declaration before a return
rule:
  all:
    - kind: variable_declaration
    - precedes:
        kind: return_statement
        stopBy: neighbor
```

---

## Composite Operators

Combine multiple rule objects with logical operations.

### `all`

Every sub-rule must match. Equivalent to AND.

```yaml
rule:
  all:
    - pattern: "$FN($$$ARGS)"
    - inside:
        kind: class_declaration
    - not:
        kind: arrow_function
```

Note: multiple fields in a single rule object are already implicitly `all`.

### `any`

At least one sub-rule must match. Equivalent to OR.

```yaml
rule:
  any:
    - pattern: "var $X = $Y"
    - pattern: "let $X = $Y"
message: "Use const instead of var/let when the value is not reassigned"
```

```yaml
rule:
  any:
    - kind: call_expression
    - kind: new_expression
```

### `not`

Inverts the sub-rule — matches nodes that do NOT satisfy it.

```yaml
# Match identifiers that are not named 'undefined'
rule:
  all:
    - kind: identifier
    - not:
        regex: "^undefined$"
```

```yaml
# Match console.log calls not inside test files
rule:
  all:
    - pattern: "console.log($$$ARGS)"
    - not:
        inside:
          kind: describe_statement   # test block
```

### `matches`

References a named utility rule defined in `utils:` or in `utilDirs`. Enables reuse.

```yaml
rule:
  all:
    - matches: is-hook-call
    - inside:
        kind: component_function

utils:
  is-hook-call:
    all:
      - kind: call_expression
      - pattern: "$FN($$$ARGS)"
      - constraints:
          FN:
            regex: "^use[A-Z]"
```

Global utility rules (in `utilDirs`) can be referenced by `matches:` across all rule files in the project.

---

## Composing Operators: Practical Patterns

### AND within a single rule object

Fields at the same level are AND:

```yaml
rule:
  pattern: "foo($X)"
  inside:
    kind: class_body
```

### Nested AND + OR

```yaml
rule:
  all:
    - any:
        - pattern: "alert($MSG)"
        - pattern: "confirm($MSG)"
    - not:
        inside:
          kind: test_block
```

### Field selector

Use `field:` to target a specific named child (tree-sitter field names are grammar-specific):

```yaml
rule:
  has:
    field: condition
    regex: "true"
```

This is useful for `if_statement` conditions, `for` initializers, etc.
