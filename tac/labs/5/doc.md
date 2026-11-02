# Lab 5 - Lexer with finite automata

The assignment integrates finite automata (FA) into the lexical analyzer for
the recipe language. The main source file is `lexer.py`: it reads a program,
recognizes tokens, and produces the Symbol Table (ST) and Program Internal
Form (PIF). Identifier and constant recognition uses FA transitions rather
than regular expressions.

## Main functions in `lexer.py`

- `load_tokens()` reads keywords, operators, and separators from `tokens.txt`.
- `lexemes()` splits the source into candidate lexemes and records their
  starting offsets. It skips whitespace, keeps letter/digit/underscore runs
  together, and reads quoted strings through the closing quote or line end.
  Operators are matched longest first.
- `load_st()` loads the C symbol-table library, `libst.so`, through `ctypes`.
- `scan()` validates candidates, updates the ST, builds the PIF, and records
  lexical errors with their line and column.
- `main()` reads the input, calls the scanner, writes `ST.out` and `PIF.out`,
  frees the symbol table, and reports the result.

## How the FA is used

At startup, `FA.load()` from `fa.py` loads two automata:

| File | Accepted lexemes | Examples |
|---|---|---|
| `identifier.fa` | An ASCII letter followed by letters, digits, or underscores | `x`, `recipe_1` |
| `constant.fa` | `0`, a nonzero digit followed by digits, or double-quoted ASCII letters | `0`, `12`, `"oua"`, `""` |

For each candidate, `FA.accepts()` follows transitions from the initial state,
one character at a time. Acceptance requires consuming the entire lexeme and
ending in a final state. A missing transition rejects the candidate.
Candidate extraction determines boundaries; the automata decide validity.
For example, `007` and `2supa` remain whole candidates and are rejected.

## Constructing the ST and PIF

The automata classify lexemes; `scan()` uses their results to construct the
tables in source order:

1. If `CONSTANT_FA.accepts(lexeme)` succeeds, insert the constant into the ST
   and append `("const", position)` to the PIF.
2. Otherwise, if `IDENTIFIER_FA.accepts(lexeme)` succeeds, check whether it is
   a keyword. Keywords produce `(keyword, -1)`. Other identifiers enter the ST
   and produce `("id", position)`.
3. A recognized operator or separator produces `(token, -1)`.
4. An invalid candidate produces a lexical error and no ST or PIF entry.
   Scanning continues with the next candidate.

The ST, implemented in `st.c`, is a hash table shared by identifiers and
constants. Each distinct lexeme receives a stable, zero-based position in
insertion order. Repeated lexemes reuse their position; string quotes are
retained. The PIF contains an entry for every valid token occurrence, using
ST positions to refer to identifiers and constants. Position `-1` means that
the token does not need an ST entry.

For example:

```text
read x
x = 12 + x
```

ST:

| Position | Lexeme |
|---|---|
| 0 | `x` |
| 1 | `12` |

PIF, in order:

```text
(read, -1), (id, 0), (id, 0), (=, -1), (const, 1), (+, -1), (id, 0)
```

All three occurrences of `x` reference the same ST position.

## Running the assignment

Requires Python 3, Make, and a C compiler. From the assignment directory:

```sh
make
python3 lexer.py programs/p1.txt
```

The scanner writes `ST.out` and `PIF.out` in the current working directory.
It prints `lexically correct` and exits with status `0` on success, or reports
lexical errors and exits with status `1`. When errors occur, the output tables
contain only valid tokens. This stage checks lexical correctness; syntax and
semantics are handled by later compiler stages.
