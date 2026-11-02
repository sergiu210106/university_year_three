# Lab 6: Syntactic analysis

The parser reads a program written in the recipe language from Lab 1 and checks it against the grammar in `grammar.txt`. The scanner from Lab 3 is part of the parser: the parser asks it for the next token. If the program is correct, the output is the string of productions used to derive it.

Files:

- `grammar.txt` - the grammar definition (productions numbered, changes from Lab 2)
- `lexer.py` - the scanner, written with PLY `lex`
- `parser.py` - the parser, written with PLY `yacc`; prints the string of productions
- `st.h`, `st.c`, `Makefile` - the Symbol Table in C, from Lab 3
- `test.py` - checks the programs and replays each derivation
- `programs/p1.txt`, `programs/p2.txt` - correct programs (Lab 2)
- `programs/p_small.txt` - the short program used in the example below
- `programs/p_err.txt`, `programs/p_err2.txt` - syntax errors
- `programs/p_lex.txt` - lexical errors

## 1. Tools

PLY is a Python implementation of lex and yacc. `lex` builds the scanner from regular expressions and `yacc` builds an LALR(1) parser from grammar rules. It is not installed by default:

```
python3 -m venv .venv
.venv/bin/pip install ply
make
.venv/bin/python parser.py programs/p1.txt
.venv/bin/python test.py
```

`make` builds `libst.so`, the Symbol Table from Lab 3, which `lexer.py` loads with `ctypes`.

The parser calls `lexer.token()` whenever it needs a token, so scanning and parsing run together. While the scanner runs it fills the PIF and the ST (the same as in Lab 3), and writes `PIF.out` and `ST.out`. PLY also writes `parser.out`, which has the LALR states and any conflicts; this grammar has none.

## 2. Grammar

The grammar is in `grammar.txt`. The terminals are the tokens from Lab 3: reserved words, operators and separators are tokens by themselves, and every identifier is `ID` and every constant is `CONST`.

```
 1  program         ::= statement_list
 2  statement_list  ::= statement
 3  statement_list  ::= statement statement_list
 4  statement       ::= assignment
 5  statement       ::= for_loop
 6  statement       ::= input
 7  statement       ::= print
 8  assignment      ::= ID "=" expression
 9  for_loop        ::= "for" ID "in" expression ":" statement_list "done"
10  input           ::= "read" ID
11  print           ::= "puts" expression
12  expression      ::= creation
13  expression      ::= ID
14  expression      ::= expression "+" term
15  expression      ::= expression "-" term
16  creation        ::= "List" "(" "[" ingredient_list "]" ")"
17  ingredient_list ::= ingredient
18  ingredient_list ::= ingredient "," ingredient_list
19  ingredient      ::= CONST CONST ID
20  term            ::= ingredient
21  term            ::= ID
```

The grammar from Lab 2 describes characters, and it had no separator between ingredients and no variables in expressions, which the programs use. These are fixed here; the list of changes is at the end of `grammar.txt`.

## 3. Parsing

Each rule in `parser.py` is a function whose docstring is the production, for example:

```python
def p_assignment(p):
    "assignment : ID EQ expression"
    rule(8)
```

An LR parser reduces bottom-up, so `rule(n)` is called in the order of the reductions, which is a rightmost derivation read backwards. The list is reversed at the end, so the output is the rightmost derivation: the production applied first is first in the string.

The parser stops at the first syntax error and reports its line and column. Lexical errors do not stop the scanner; they are collected and printed with the same messages as in Lab 3. If there is any error, no string of productions is written.

## 4. Example: `programs/p_small.txt`

```
supa = List(["apa" 1000 ml])
puts supa
```

```
$ python3 parser.py programs/p_small.txt
string of productions:
1 3 2 7 11 13 4 8 12 16 17 19
```

`productions.out` has the string and the productions it uses:

```
  1  program -> statement_list
  3  statement_list -> statement statement_list
  2  statement_list -> statement
  7  statement -> print
 11  print -> PUTS expression
 13  expression -> ID
  4  statement -> assignment
  8  assignment -> ID EQ expression
 12  expression -> creation
 16  creation -> LIST LPAREN LBRACKET ingredient_list RBRACKET RPAREN
 17  ingredient_list -> ingredient
 19  ingredient -> CONST CONST ID
```

The derivation, always replacing the rightmost non-terminal:

```
program
⇒ statement_list                                    (1)
⇒ statement statement_list                          (3)
⇒ statement statement                               (2)
⇒ statement print                                   (7)
⇒ statement PUTS expression                         (11)
⇒ statement PUTS ID                                 (13)
⇒ assignment PUTS ID                                (4)
⇒ ID EQ expression PUTS ID                          (8)
⇒ ID EQ creation PUTS ID                            (12)
⇒ ID EQ LIST ( [ ingredient_list ] ) PUTS ID        (16)
⇒ ID EQ LIST ( [ ingredient ] ) PUTS ID             (17)
⇒ ID EQ LIST ( [ CONST CONST ID ] ) PUTS ID         (19)
```

`test.py` does this replay for `p1.txt` and `p2.txt`: it applies the productions from `program` and checks that the result is the token sequence of the program.

## 5. Errors

```
$ python3 parser.py programs/p_err.txt          # the for loop has no "done"
syntax error, line 6: unexpected end of program

$ python3 parser.py programs/p_err2.txt         # missing "," between ingredients
syntax error, line 1, col 28: unexpected '"oase"' (CONST)

$ python3 parser.py programs/p_lex.txt
lexical error, line 1, col 20: 007 - invalid number or identifier
syntax error, line 1, col 24: unexpected 'ml' (ID)
```

A lexical error removes the token, so a syntax error can follow it, as in the last example. The parser stops at the first syntax error, so lexical errors after it (the `@` on line 2 of `p_lex.txt`) are not reached.
