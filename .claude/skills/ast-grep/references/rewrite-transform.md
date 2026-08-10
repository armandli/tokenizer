# Rewriting, Transform, and Rewriters

## CLI Rewriting with `--rewrite`

The simplest rewrite: match a pattern and replace with a template.

```bash
# Replace parseInt with Number
sg run -p 'parseInt($STR)' -r 'Number($STR)' -l js src/

# Migrate optional chaining
sg run -p '$PROP && $PROP()' -r '$PROP?.()' -l ts src/

# Delete all console.log calls
sg run -p 'console.log($$$ARGS)' -r '' -l js src/

# Apply interactively (review each change)
sg run -p 'var $X = $Y' -r 'const $X = $Y' -l js -i src/

# Apply all changes without prompting
sg run -p 'var $X = $Y' -r 'const $X = $Y' -l js -U src/
```

Metavariables in the replacement string are substituted with matched source text. `$$$ARGS` expands as the entire matched sequence.

---

## `fix:` in YAML Rules

The `fix:` field in a YAML rule provides the same replacement as `--rewrite`, applied during `sg scan`.

### String fix (most common)

```yaml
id: use-number-parse
language: javascript
rule:
  pattern: parseInt($STR, 10)
fix: "Number($STR)"
```

```yaml
id: remove-debug-log
language: javascript
rule:
  pattern: console.debug($$$ARGS)
fix: ""   # empty string deletes the matched node
```

### FixConfig object (for deletion with surrounding punctuation)

When deleting items from a list, naive deletion leaves orphan commas. Use `expandStart`/`expandEnd` to consume them:

```yaml
fix:
  template: ""
  expandStart:
    regex: ",\\s*"     # consume the comma before the deleted item
  expandEnd:
    regex: "\\s*,"     # or consume the trailing comma
```

`stopBy` in expand rules: `neighbor` (adjacent whitespace/comma only) or `ancestor`.

---

## `transform:` — Metavariable Manipulation

`transform:` creates new metavariables by processing captured ones. The resulting names are then usable in `fix:`.

### replace — regex substitution

```yaml
transform:
  CLEAN_NAME:
    replace:
      source: $RAW_NAME     # input metavariable
      replace: "[^a-zA-Z0-9]"  # Rust regex pattern
      by: "_"               # replacement (supports $1, $2 capture groups)
fix: "const $CLEAN_NAME = $VALUE"
```

### substring — slice text

```yaml
transform:
  SHORT:
    substring:
      source: $FULL_STRING
      startChar: 0          # inclusive, 0-based
      endChar: 10           # exclusive; negative values count from end
```

Negative indices (Python-style):
- `startChar: -5` = 5 chars from end
- `endChar: -1` = all but last char

### convert — case conversion

```yaml
transform:
  SNAKE:
    convert:
      source: $CAMEL_VAR
      toCase: snakeCase
```

| `toCase` value | Example output |
|---------------|----------------|
| `lowerCase` | `foobar` |
| `upperCase` | `FOOBAR` |
| `capitalize` | `Foobar` |
| `camelCase` | `fooBar` |
| `snakeCase` | `foo_bar` |
| `kebabCase` | `foo-bar` |
| `pascalCase` | `FooBar` |

`separatedBy` controls how the input is split before converting:

```yaml
convert:
  source: $NAME
  toCase: camelCase
  separatedBy:
    - Underscore   # split on _
    - Dash         # split on -
    - CaseChange   # split on uppercase transitions
```

Options: `Space`, `Dash`, `Dot`, `Slash`, `Underscore`, `CaseChange`

### rewrite — apply rewriter rules to a metavariable

```yaml
transform:
  CONVERTED_ARGS:
    rewrite:
      source: $$$ARGS       # typically a multi-node capture
      rewriters:
        - convert-arg       # applied in order; first match wins
        - fallback-arg
      joinBy: ", "          # separator between results
fix: "newFn($CONVERTED_ARGS)"
```

---

## `rewriters:` — Multi-node Transformation

When you need to transform each node in a sequence differently, use `rewriters`. Each rewriter is a mini-rule with its own `fix`.

```yaml
id: convert-kwargs-to-dict
language: python
rule:
  pattern: func($$$KWARGS)
rewriters:
  - id: kwarg-to-kv
    rule:
      kind: keyword_argument
      has:
        field: name
        pattern: $KEY
      has:
        field: value
        pattern: $VAL
    fix: "'$KEY': $VAL"
transform:
  DICT_ARGS:
    rewrite:
      source: $$$KWARGS
      rewriters: [kwarg-to-kv]
      joinBy: ", "
fix: "func({$DICT_ARGS})"
```

### Rewriter scope rules

- Metavariables captured inside a rewriter (e.g., `$KEY`, `$VAL`) are **local** to that rewriter
- Metavariables from the **parent rule** are available inside rewriters
- `transform` variables defined in a rewriter are also scoped to that rewriter
- The **first** matching rewriter in the array wins; order matters

### When to use rewriters vs. simple fix

| Scenario | Use |
|----------|-----|
| Uniform replacement | `fix:` string |
| Text manipulation on captured vars | `transform:` |
| Each node in a list needs different treatment | `rewriters:` + `transform.rewrite` |
| Delete with surrounding punctuation | `fix:` FixConfig with `expandStart`/`expandEnd` |

---

## Indentation Preservation

ast-grep preserves the relative indentation of metavariable text in `fix` strings. If `$BODY` is a multi-line block, it will be re-indented to match its position in the fix template. You don't need to manually adjust indentation in most cases.

---

## Worked Example: Barrel Import Expansion

Convert a single barrel import into individual imports:

```yaml
id: expand-barrel-import
language: typescript
rule:
  pattern: "import {$$$IDENTS} from './barrel'"
rewriters:
  - id: each-ident
    rule:
      kind: identifier
      pattern: $IDENT
    fix: "import $IDENT from './barrel/$IDENT'"
transform:
  EXPANDED:
    rewrite:
      source: $$$IDENTS
      rewriters: [each-ident]
      joinBy: "\n"
fix: $EXPANDED
```

Input:
```ts
import { Alpha, Beta, Gamma } from './barrel'
```

Output:
```ts
import Alpha from './barrel/Alpha'
import Beta from './barrel/Beta'
import Gamma from './barrel/Gamma'
```
