# ast-grep Worked Examples

## 1. One-liner Search

**Goal**: Find all `eval()` calls across a JavaScript codebase.

```bash
sg run -p 'eval($CODE)' -l js src/
```

What it does:
- `eval($CODE)` matches any call to `eval` with any single argument
- `-l js` sets the language
- Results show file path, line, and matched code

Variations:
```bash
# With nested calls
sg run -p 'eval($$$ARGS)' -l js src/

# JSON output for tooling
sg run -p 'eval($CODE)' -l js --json src/ | jq '.[].text'

# Restrict to specific directories
sg run -p 'eval($CODE)' -l js src/ lib/
```

---

## 2. Interactive Rewrite — Optional Chaining Migration

**Goal**: Migrate `x && x()` to `x?.()` in TypeScript.

```bash
sg run -p '$PROP && $PROP()' -r '$PROP?.()' -l ts -i src/
```

How it works:
- `$PROP && $PROP()` matches the pattern where the same expression is checked and then called
- Identity enforcement (`$PROP` used twice) ensures only patterns where both sides are identical are matched
- `-r '$PROP?.()` substitutes the captured expression
- `-i` shows each match and asks before applying

Apply all without prompting:
```bash
sg run -p '$PROP && $PROP()' -r '$PROP?.()' -l ts -U src/
```

---

## 3. YAML Lint Rule — no-console-log

**Goal**: Warn about `console.log` calls in production source (not in test files).

`rules/no-console-log.yml`:
```yaml
id: no-console-log
language: javascript
files:
  - "src/**/*.js"
ignores:
  - "**/*.test.js"
  - "**/*.spec.js"

rule:
  pattern: "console.log($$$ARGS)"

severity: warning
message: "Remove console.log before committing"
fix: ""
```

Test file `rule-tests/no-console-log.test.yml`:
```yaml
id: no-console-log

valid:
  - "console.error('critical')"
  - "console.warn('notice')"

invalid:
  - "console.log('debug')"
  - "console.log(x, y, z)"
```

Run:
```bash
sg scan src/
sg test
```

---

## 4. Multi-Condition Rule — Detect Duplicate Conditions

**Goal**: Flag `if (x === x)` and similar tautologies where both operands are identical.

`rules/no-self-compare.yml`:
```yaml
id: no-self-compare
language: javascript
rule:
  any:
    - pattern: "$A === $A"
    - pattern: "$A == $A"
    - pattern: "$A !== $A"
    - pattern: "$A != $A"
severity: error
message: "Comparing a value to itself is always true or false"
```

This uses `any:` to catch all comparison operators, and identity enforcement (`$A` used twice) to ensure both sides of the comparison are literally the same source text.

---

## 5. Barrel Import Expansion with Rewriters

**Goal**: Expand `import { A, B, C } from './barrel'` into separate per-module imports.

`rules/expand-barrel.yml`:
```yaml
id: expand-barrel-import
language: typescript
rule:
  pattern: "import {$$$IDENTS} from './barrel'"

rewriters:
  - id: each-identifier
    rule:
      kind: identifier
      pattern: $IDENT
    fix: "import $IDENT from './barrel/$IDENT'"

transform:
  EXPANDED_IMPORTS:
    rewrite:
      source: $$$IDENTS
      rewriters: [each-identifier]
      joinBy: "\n"

fix: $EXPANDED_IMPORTS
```

Input:
```typescript
import { Button, Input, Modal } from './barrel'
```

Output:
```typescript
import Button from './barrel/Button'
import Input from './barrel/Input'
import Modal from './barrel/Modal'
```

How it works:
1. Parent rule captures `$$$IDENTS` — all the identifiers in `{ Button, Input, Modal }`
2. `each-identifier` rewriter processes each identifier node individually
3. `transform.rewrite` applies the rewriter to each node and joins results with newlines
4. `fix` replaces the original import with the expanded result

---

## 6. Contextual Rule — console.log Inside Async Functions Only

**Goal**: Warn about `console.log` inside async functions (where async flow should use proper logging).

```yaml
id: no-console-in-async
language: javascript
rule:
  all:
    - pattern: "console.log($$$ARGS)"
    - inside:
        kind: async_function

severity: info
message: "Use structured logging inside async functions instead of console.log"
```

---

## 7. Case Conversion with transform

**Goal**: Rename Python function arguments from camelCase to snake_case.

```yaml
id: camel-to-snake-arg
language: python
rule:
  pattern: "def $FN($CAMEL_ARG):"
  constraints:
    CAMEL_ARG:
      regex: "[a-z][a-zA-Z]+"   # has at least one uppercase

transform:
  SNAKE_ARG:
    convert:
      source: $CAMEL_ARG
      toCase: snakeCase
      separatedBy:
        - CaseChange

fix: "def $FN($SNAKE_ARG):"
```

---

## 8. Utility Rule: "Is Inside Test"

`utils/is-in-test.yml` (in `utilDirs`):
```yaml
id: is-in-test
language: javascript
rule:
  any:
    - inside:
        pattern: "describe($$$ARGS)"
    - inside:
        pattern: "it($$$ARGS)"
    - inside:
        pattern: "test($$$ARGS)"
    - inside:
        pattern: "beforeEach($$$ARGS)"
    - inside:
        pattern: "afterEach($$$ARGS)"
```

Any other rule can now use:
```yaml
rule:
  all:
    - pattern: "console.log($$$ARGS)"
    - not:
        matches: is-in-test
```

---

## 9. Debugging a Pattern

Use `--debug-query` to inspect the AST of code and find the right node kinds:

```bash
# See AST of a snippet
echo "const x = foo?.bar()" | sg run -p '$X' -l ts --debug-query

# Or write to a file and inspect
sg run -p 'await $EXPR' -l ts --debug-query src/main.ts
```

This prints the tree-sitter parse tree, showing node kinds at each position — essential for writing `kind:` rules.
