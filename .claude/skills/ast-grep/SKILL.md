---
name: ast-grep
description: Reference guide for ast-grep (sg) — a syntax-aware CLI tool for code search, linting, and rewriting using AST patterns. Use when writing ast-grep patterns, creating YAML lint rules, rewriting code with --rewrite or fix:, setting up an ast-grep project (sgconfig.yml), writing sg test cases, or asking "how do I search/lint/rewrite code with ast-grep". Do NOT use for non-ast-grep code search tools (ripgrep, grep, semgrep).
argument-hint: "[task: search|lint|rewrite|project|test]"
---

## Quick Start

```bash
# Search: find all eval() calls in JavaScript
sg run -p 'eval($X)' -l js src/

# Rewrite: migrate optional chaining (interactive)
sg run -p '$PROP && $PROP()' -r '$PROP?.()' -l ts -i src/

# Lint: run all project rules
sg scan
```

## CLI Commands

| Command | Purpose |
|---------|---------|
| `sg run -p PATTERN -l LANG [PATH]` | One-shot search or rewrite |
| `sg scan` | Run all YAML rules in the project |
| `sg test` | Validate rules against test cases |
| `sg new` | Scaffold project, rule, or test |
| `sg lsp` | Start language server for editor integration |

Key `sg run` flags:

| Flag | Description |
|------|-------------|
| `-p, --pattern PATTERN` | AST pattern to match |
| `-l, --lang LANG` | Target language (js, ts, py, go, rust, java, …) |
| `-r, --rewrite REWRITE` | Replacement string (uses captured metavariables) |
| `-k, --kind KIND` | Match by AST node type only |
| `-i, --interactive` | Review each change before applying |
| `-U, --update-all` | Apply all changes without prompting |
| `--json` | Machine-readable JSON output |

For complete flag reference, see [references/cli.md](references/cli.md).

## Pattern Syntax

Patterns are valid code snippets with `$`-prefixed metavariable holes:

| Syntax | Matches | Example |
|--------|---------|---------|
| `$NAME` | Any single AST node | `$A == $A` (both sides identical) |
| `$$$NAME` | Zero or more nodes (sequence) | `fn($$$ARGS)` matches any arg list |
| `$$NAME` | Any unnamed tree-sitter node | Internal parser nodes |
| `$_NAME` | Single node, not captured | Use when match but don't need the value |
| `$_` | Anonymous wildcard | Discard a single node |

Rules:
- Metavariable names: uppercase letters, digits, underscores only
- Reusing the same name enforces **identity**: `$A + $A` only matches when both sides are equal
- `$$$` matches zero or more siblings — use for argument lists, statement sequences

For full pattern syntax, see [references/patterns.md](references/patterns.md).

## YAML Rule Quick Reference

```yaml
id: no-eval
language: javascript
rule:
  pattern: eval($CODE)
severity: error
message: "eval() is a security risk"
fix: "/* eval removed */"
files:
  - "src/**/*.js"
ignores:
  - "**/*.test.js"
```

Rule operators at a glance:

| Category | Operators |
|----------|-----------|
| Atomic | `pattern`, `kind`, `regex`, `nthChild`, `range` |
| Relational | `inside`, `has`, `precedes`, `follows` |
| Composite | `all`, `any`, `not`, `matches` |

For the full YAML schema, see [references/yaml-rules.md](references/yaml-rules.md).
For all rule operators with examples, see [references/rule-operators.md](references/rule-operators.md).

## Supported Languages (selected)

`js` / `ts` / `tsx` / `jsx` · `py` · `go` · `rust` · `java` · `kotlin` · `scala`
`c` / `cpp` · `cs` · `rb` · `php` · `swift` · `bash` · `html` · `css` · `json`

## References

- [CLI commands and all flags](references/cli.md)
- [Pattern syntax and metavariables](references/patterns.md)
- [Full YAML rule schema](references/yaml-rules.md)
- [Rule operators (atomic / relational / composite)](references/rule-operators.md)
- [Rewriting: fix, transform, rewriters](references/rewrite-transform.md)
- [Project setup: sgconfig.yml](references/project-setup.md)
- [Testing rules with sg test](references/testing.md)
- [End-to-end worked examples](references/examples.md)
