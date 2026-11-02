# Theory behind Lab 6: syntactic analysis

This file has the minimal theory needed to present Lab 6: what a parser does, how an LR/LALR parser works, what the "string of productions" is, and how scanning and parsing are integrated. `doc.md` describes the implementation; this file explains why it works.

## 0. Quick brief: what to know for the presentation

**The task in one line.** Check that a recipe program follows the grammar of the language, and output the list of grammar rules used to build it.

**Scanner vs parser.** The scanner (Lab 3) cuts the text into tokens and knows nothing about order: `for for for` is three valid tokens. The parser (this lab) checks the order of the tokens against the grammar and finds the structure of the program: `x = List(...)` is an assignment, `for ... done` is a loop.

**Context-free grammar (CFG).** A set of rules `A -> something`, where A is a non-terminal (a part of the program, like `statement`) and the right side lists terminals (tokens) and non-terminals. Starting from `program` and replacing non-terminals with right sides, you can produce every valid program. Lab 4's regular grammars cannot do this: `for ... done` has to be balanced and a regular grammar cannot count.

**The string of productions.** Every rule of the grammar has a number. The parser outputs the numbers of the rules used, in order. For `programs/p_small.txt` it is `1 3 2 7 11 13 4 8 12 16 17 19`. The string shows the program is valid and it is enough to rebuild the derivation tree. The same string can be replayed from `program` to get the tokens back, and `test.py` does that.

**How the parser works (LALR, bottom-up).** It reads tokens left to right and keeps a stack.
- **Shift:** push the next token on the stack.
- **Reduce:** if the top of the stack matches the right side of a rule, replace it with the left side, and record the rule number.
- The decision (shift, reduce, or error) comes from a table, built from the grammar, using the state on top of the stack and the next token.
- If the whole program reduces to `program`, it is accepted. If the table has no action, it is a syntax error at that token.

**Order of the string.** Reducing finds the rules from the leaves up, which is a rightmost derivation backwards. `parser.py` reverses the list, so the string starts with rule 1 (`program -> statement_list`) and shows the rightmost derivation.

**yacc and lex.** `lex` turns regular expressions into a scanner, `yacc` turns grammar rules into an LALR parser. We use PLY, their Python version. The parser calls the scanner whenever it needs a token, so there is no separate pass, and that is the "integration" from requirement 3.

**What I changed in the grammar, and why.** The Lab 2 grammar was written for people, not for a parser:
- characters (`<letter>`, `<digit>`, ...) became the tokens `ID` and `CONST`, because the scanner already handles them;
- ingredients inside `List([...])` now have a `,` between them, as in the programs;
- a variable can be an expression (`puts omleta`), and `+`/`-` can take a variable (`omleta - ingredient`);
- no ε rules: a program and a loop body have at least one statement.

**How to demo it.**
```
make
.venv/bin/python parser.py programs/p_small.txt     # string of productions
.venv/bin/python parser.py programs/p1.txt          # the 20-statement program
.venv/bin/python parser.py programs/p_err2.txt      # syntax error with line and column
.venv/bin/python parser.py programs/p_lex.txt       # lexical error, then syntax error
.venv/bin/python test.py                            # 2 accepted, 3 rejected, derivations replayed
```

**Likely questions, with short answers.**

| Question | Answer |
|----------|--------|
| Why yacc and not ANTLR? | Either is allowed. I used PLY because it is real lex + yacc and runs in Python, so it reuses the Lab 3 scanner and symbol table without a C toolchain. |
| Is the grammar ambiguous? | No. If it were, the table would have shift/reduce or reduce/reduce conflicts. `parser.out` shows 43 states and 0 conflicts. |
| Why is `expression` left-recursive but `statement_list` right-recursive? | Left recursion makes `a + b - c` group from the left, and it uses little stack in LR parsing. `statement_list` is right-recursive because it is simpler; the stack grows with the number of statements, which is fine here. |
| How does the parser tell `ml` (a unit) from a variable? | It doesn't need to at scan time. Both are `ID`. After two constants, the grammar expects an `ID`, and that position makes it the unit. |
| Why does `x = a` followed by `y = b` not confuse the parser? | After `x = a` the next token is `ID`, which cannot continue an expression, so the parser reduces `a` to an expression and starts a new statement. The lookahead decides. |
| What happens on an error? | The scanner reports lexical errors (bad number, bad string, illegal character) and skips the token. The parser stops at the first syntax error and reports the line, the column and the unexpected token. No string of productions is written. |
| Is the string the same as the parse tree? | It carries the same information. With the grammar, the rightmost derivation determines the tree. |
| Why a PIF as input in the statement? | The parser works on tokens, and the PIF is the token list. Here the scanner produces the same PIF while the parser runs, and `PIF.out` shows it. |

The sections below go deeper, if you need them.

## 1. Context-free grammars

A context-free grammar is G = (N, T, P, S):

- N - non-terminals (`statement`, `expression`, ...)
- T - terminals, which are the tokens (`ID`, `CONST`, `List`, `+`, ...)
- P - productions `A -> α`, with A ∈ N and α a string of terminals and non-terminals
- S ∈ N - the start symbol (`program`)

A **derivation step** replaces one non-terminal by the right side of one of its productions. The **language** L(G) is the set of token strings derivable from S.

A **rightmost derivation** always replaces the rightmost non-terminal. Its reverse is what a bottom-up parser finds.

A **derivation tree** has S at the root, and each node A has the symbols of the right side of the production used as its children. The leaves, read left to right, give the program.

A grammar is **ambiguous** if some program has two different derivation trees. A parser generator reports this as a conflict.

## 2. The language, in short

The grammar of this lab (`grammar.txt`, 21 productions):

```
program         ::= statement_list
statement_list  ::= statement | statement statement_list
statement       ::= assignment | for_loop | input | print
assignment      ::= ID "=" expression
for_loop        ::= "for" ID "in" expression ":" statement_list "done"
input           ::= "read" ID
print           ::= "puts" expression
expression      ::= creation | ID | expression "+" term | expression "-" term
creation        ::= "List" "(" "[" ingredient_list "]" ")"
ingredient_list ::= ingredient | ingredient "," ingredient_list
ingredient      ::= CONST CONST ID
term            ::= ingredient | ID
```

It is not regular because loops nest: a `for` can contain another `for`, and each one is closed by its own `done`.

## 3. Bottom-up parsing

A bottom-up parser builds the tree from the leaves to the root. It has a stack and the remaining input, and it chooses between two actions:

- **shift:** move the next token from the input onto the stack
- **reduce by A -> α:** the top of the stack is α, so replace it with A

The **handle** is the right side of a production that is on top of the stack and must be reduced now. Finding the handle correctly is the whole problem; the parser must not reduce too early or too late.

Example, `read x` followed by end of input, with rule 10 (`input -> READ ID`) and rule 6 (`statement -> input`):

| Stack | Input | Action |
|-------|-------|--------|
| | READ ID $ | shift |
| READ | ID $ | shift |
| READ ID | $ | reduce 10 (`input -> READ ID`) |
| input | $ | reduce 6 (`statement -> input`) |
| statement | $ | reduce 2 (`statement_list -> statement`) |
| statement_list | $ | reduce 1 (`program -> statement_list`) |
| program | $ | accept |

The reductions in order are 10, 6, 2, 1. The rightmost derivation is the reverse: 1 2 6 10, which is the string of productions.

## 4. LR parsing

LR(k) means: scan the input **L**eft to right, build a **R**ightmost derivation in reverse, using **k** tokens of lookahead. The parser here uses k = 1.

An **LR(0) item** is a production with a dot that shows how much has been read, for example `assignment -> ID . EQ expression`. A **state** is a set of items. The states and the transitions between them form a deterministic automaton over the symbols of the stack (a finite automaton, like in Lab 4).

The table has two parts:

- **ACTION[state, token]:** shift to a state, reduce by a production, accept, or error
- **GOTO[state, non-terminal]:** the state to enter after a reduce

The parser keeps a stack of states. For the next token it looks up ACTION. On a reduce by A -> α it pops |α| states, looks up GOTO[state, A], and pushes that state.

**Conflicts.** The table cell must have one action:

- shift/reduce conflict: the parser cannot decide between shifting and reducing
- reduce/reduce conflict: two different productions could be reduced

Conflicts mean the grammar is not LR(1) at that point (usually it is ambiguous). yacc resolves them with default rules, but they should be removed from the grammar. This grammar has none.

### SLR, LALR, LR(1)

| Method | Lookahead used for a reduce | States |
|--------|------------------------------|--------|
| SLR(1) | FOLLOW(A) of the left side | LR(0) states |
| LALR(1) | exact lookaheads, merged for states with the same items | LR(0) states |
| LR(1) | exact lookaheads, no merging | many more |

LALR(1) has the size of SLR and is almost as strong as LR(1), which is why yacc and PLY use it. For this lab the grammar has 43 LALR states, shown in `parser.out`.

FOLLOW(A) is the set of tokens that can come right after A in some derivation. For example, FOLLOW(expression) contains `+`, `-`, `:`, `ID`, `read`, `for`, `puts`, `done` and the end of input. When the parser has `ID` on the stack after `puts`, it uses the next token to know whether `ID` is complete or `+ term` follows.

## 5. Left and right recursion

- **Left-recursive:** `expression -> expression "+" term`. The parser can reduce after each term, so the stack stays small, and `a + b - c` is grouped as `(a + b) - c`.
- **Right-recursive:** `statement_list -> statement statement_list`. The parser pushes all statements before it can reduce the list, so the stack grows with the number of statements. This is why the string for `p1.txt` (20 statements) starts with `1` and nineteen `3`s, and the last statement ends the list with `2`.

Both forms work in LR parsing. Left recursion is a problem only for top-down (LL) parsers, which would loop forever.

## 6. Syntax errors

The parser detects an error when ACTION[state, token] is empty. Because LR parsers never shift a token that cannot lead to a valid program, the error is found at the earliest possible token (the **viable prefix** property). That is why `p_err2.txt` is reported at the missing `,` and not later.

`parser.py` stops at the first error and reports its line and column. Error recovery (skipping tokens until a safe point) is possible in yacc with an `error` token, but it is not needed here.

## 7. Integrating the scanner with the parser

In the classical design a **lex** scanner and a **yacc** parser work together:

- the parser calls `yylex()` (here `lexer.token()`) when it needs a token
- the scanner returns the token type for the parser (`ID`, `CONST`, `PLUS`, ...), and keeps the lexeme as its value
- the scanner does the work from Lab 3 on the side: it adds identifiers and constants to the Symbol Table and records the PIF entries

So the PIF is built while parsing. The parser sees only token types. It never looks inside an identifier; that is why `ml` is just an `ID` for the parser, and the grammar decides it is a unit by position.

The reserved words are recognised by the scanner (it checks an identifier-shaped lexeme against the list), which keeps the grammar free of character-level rules.

## 8. What is PLY

PLY (Python Lex-Yacc) is a Python implementation of lex and yacc:

- `ply.lex`: each token has a regular expression, given as a variable `t_NAME = r"..."` or as a function with the pattern in its docstring. Function rules are tried in the order they are defined, which is the same idea as the order of patterns in Lab 3.
- `ply.yacc`: each production is a function whose docstring is the rule, for example `"assignment : ID EQ expression"`. The function runs when the parser reduces by that rule. In `parser.py`, that function records the production number.
- With `debug=True`, PLY writes `parser.out` with the grammar, the states and all conflicts.

## 9. Notation summary

| Notation | Meaning |
|----------|---------|
| N, T, P, S | non-terminals, terminals, productions, start symbol |
| A -> α | production |
| ⇒, ⇒\* | one / zero or more derivation steps |
| LR(1), LALR(1) | left-to-right, rightmost derivation reversed, one token of lookahead |
| shift, reduce | push a token / replace a handle by its non-terminal |
| handle | the right side to reduce now |
| FOLLOW(A) | tokens that may come right after A |
| `$` | end of input |

## References

- A. V. Aho, M. S. Lam, R. Sethi, J. D. Ullman, *Compilers: Principles, Techniques, and Tools*: chapter 4 (syntax analysis, LR parsers)
- PLY documentation: https://www.dabeaz.com/ply/ply.html
