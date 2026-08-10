# ast-grep Pattern Syntax

## What a Pattern Is

A pattern is valid code with `$`-prefixed metavariable holes substituted for the parts you want to match. ast-grep parses the pattern with the same tree-sitter grammar as the source file, so matches are structurally exact — whitespace, comments, and formatting differences don't matter.

```
# Pattern          Matches
eval($X)           eval(foo), eval(bar + 1), eval(dangerous())
console.log($A)    console.log("hi"), console.log(x)
$A + $B            x + y, 1 + getVal(), arr[0] + 2
```

## Metavariable Types

| Syntax | Name | What it matches |
|--------|------|----------------|
| `$NAME` | Single capture | Any single AST node |
| `$$$NAME` | Multi-node capture | Zero or more consecutive sibling nodes |
| `$$NAME` | Unnamed capture | Any unnamed tree-sitter node (parser internals) |
| `$_NAME` | Non-capturing single | Single node — matched but not stored |
| `$_` | Anonymous wildcard | Single node — discard completely |
| `$$$` | Anonymous multi-wildcard | Zero or more nodes — discard completely |

**Naming rules:**
- Must be uppercase letters, underscores, and/or digits
- Valid: `$VAR`, `$A`, `$NODE_1`, `$_IGNORED`
- Invalid: `$var`, `$camelCase`, `$kebab-case`

## Single Capture `$NAME`

Matches exactly one AST node of any kind:

```bash
# Match any function call with one argument
sg run -p 'foo($ARG)' -l js

# Match any property access
sg run -p '$OBJ.$PROP' -l js

# Match any binary expression
sg run -p '$LEFT + $RIGHT' -l js
```

## Identity Enforcement

Reusing the same metavariable name within a pattern enforces that both occurrences match **identical** source text:

```bash
# Only matches when both sides of == are literally the same
sg run -p '$A === $A' -l js

# Match repeated condition: if (x && x)
sg run -p '$COND && $COND' -l ts

# Match x.foo && x.foo() — property checked and called
sg run -p '$PROP && $PROP()' -l ts
```

This is one of ast-grep's most powerful features — no regex can do this.

## Multi-node Capture `$$$NAME`

Matches zero or more consecutive sibling nodes. Essential for:
- Function argument lists (any number of args)
- Array/object elements
- Statement sequences
- Import specifiers

```bash
# Match any function call regardless of arg count
sg run -p 'console.log($$$ARGS)' -l js

# Match array with any elements
sg run -p '[$$$ITEMS]' -l js

# Match template literal with any expressions
sg run -p '`$$$PARTS`' -l js
```

In YAML rules, use `$$$` for sequences in fix strings:

```yaml
rule:
  pattern: fn($$$ARGS, extraArg)
fix: fn($$$ARGS)
```

## Non-Capturing Wildcards

Use `$_NAME` or `$_` when you want to match something but don't need to reference it in a fix or constraint:

```bash
# Match any call to any method named "log", ignore the object
sg run -p '$_OBJ.log($MSG)' -l js

# Match any two-argument function call, only care about the second
sg run -p 'create($_, $CONFIG)' -l js
```

## Matching by Node Kind

Use `-k KIND` (CLI) or `kind:` (YAML) to match by AST node type alone, without a pattern. Find node type names with `--debug-query`:

```bash
# Find node type of a piece of code
sg run -p 'console.log(x)' -l js --debug-query

# Match ALL call expressions
sg run -k call_expression -l js src/

# In YAML:
rule:
  kind: arrow_function
```

Common node kinds (language-specific, found in tree-sitter grammars):
- JS/TS: `call_expression`, `arrow_function`, `identifier`, `string`, `binary_expression`, `import_statement`
- Python: `call`, `function_definition`, `import_statement`, `assignment`
- Go: `call_expression`, `func_literal`, `short_var_decl`

## Combining Pattern and Kind

`pattern` and `kind` inside the same rule object apply as AND — both must match:

```yaml
rule:
  all:
    - pattern: $FN($$$ARGS)
    - kind: call_expression
```

## Regex on Node Text

Use `regex:` to filter by the text content of an AST node:

```yaml
rule:
  regex: "^(foo|bar)$"
```

Combines with `pattern`:

```yaml
rule:
  all:
    - kind: identifier
    - regex: "^use[A-Z]"   # matches React hook names
```

## Pattern Pitfalls

**Statement vs. expression context**: A pattern like `var $X = $Y` is a statement. Inside an expression context it won't match. Use the right statement form.

**Language-specific syntax**: Patterns must be valid syntax for the target language. `$X?.()` is valid TS but not plain JS if the parser doesn't support optional chaining.

**Incomplete nodes**: Single expressions like `$A + $B` match anywhere that syntax appears, including inside larger expressions. Use `inside:` to restrict context.
