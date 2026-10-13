# Lab 3: Lexical analysis

The lexer reads a program written in the recipe language from Lab 1 (grammar in `../2/bnf.txt`) and the list of tokens from `tokens.txt`. It writes the Program Internal Form to `PIF.out`, the Symbol Table to `ST.out`, and prints the lexical errors.

Files:

- `tokens.txt` - list of tokens and their codes
- `lexer.py` - the scanner, written in Python with regular expressions
- `st.h`, `st.c` - the Symbol Table, a hash table written in C
- `Makefile` - builds `libst.so`
- `programs/p1.txt`, `programs/p2.txt` - the programs from Lab 2
- `programs/p_err.txt` - a program with lexical errors

## 1. Tools

The scanner is written in Python using the `re` module. Every token class has a regular expression and they are joined into one expression with named groups, `(?P<NAME>pattern)`. `re.finditer` goes through the source and `m.lastgroup` gives the name of the class that matched.

The Symbol Table is written in C and compiled into a shared library (`libst.so`). Python loads it with `ctypes` and calls the C functions directly (`st_create`, `st_insert`, `st_get`, `st_size`, `st_free`).

Build and run:

```
make
python3 lexer.py programs/p1.txt
```

A different token file can be given as the second argument.

## 2. Tokens

| Token | Code |
|-------|------|
| identifier | 0 |
| constant | 1 |
| `List` `for` `in` `done` `read` `puts` | 2 - 7 |
| `=` `+` `-` | 8 - 10 |
| `(` `)` `[` `]` `,` `:` | 11 - 16 |

The lexer reads `tokens.txt` at start. `identifier` and `constant` are the two token classes stored in the ST. The entries that look like identifiers are reserved words. The rest are operators and separators, and the regular expression that matches them is built from this list.

## 3. Token detection

At each position the patterns are tried in this order and the first one that matches is used. Error patterns are placed before the correct ones they would otherwise be confused with.

| Class | Regex | Result |
|-------|-------|--------|
| newline | `\n` | skipped, line number + 1 |
| whitespace | `[ \t\r]+` | skipped |
| string constant | `"[A-Za-z]*"` | constant, added to ST |
| bad string | `"[^"\n]*"?` | error |
| bad number | `0[0-9]+` or `[0-9]+[A-Za-z_][A-Za-z0-9_]*` | error |
| number constant | `0` or `[1-9][0-9]*` | constant, added to ST |
| identifier | `[A-Za-z][A-Za-z0-9_]*` | reserved word or identifier |
| operator / separator | built from `tokens.txt` | code from `tokens.txt` |
| anything else | `.` | error |

### Identifiers

The grammar defines an identifier as a letter followed by letters, digits or `_`, which gives `[A-Za-z][A-Za-z0-9_]*`. Reserved words have the same form, so after a match the lexeme is checked against the reserved words from `tokens.txt`. If it is one, its code is written in the PIF; otherwise it is an identifier and is added to the ST.

The match is greedy, so `carbonara_non_italiana` is one identifier and `done_x` is an identifier, not `done` followed by `_x`.

Measuring units (`g`, `ml`, `buc`) look exactly like identifiers, so the lexer treats them as identifiers. The parser will tell them apart by their position after a number.

### Constants

There are two kinds of constants:

- numbers: `0` or a non-zero digit followed by digits, `0|[1-9][0-9]*`
- strings: letters between double quotes, `"[A-Za-z]*"`. The quotes are kept in the lexeme, so `"oua"` and `oua` are different ST entries.

### Errors

Errors do not stop the lexer. It reports the line, column and lexeme, then continues. The detected errors are:

- a number with a leading zero (`007`)
- a number followed by letters (`2supa`)
- a string that contains something other than letters (`"sare2"`) or has no closing quote (`"piure`)
- a character that is not part of the language (`;`, `@`)

```
$ python3 lexer.py programs/p_err.txt
line 1, col 20: 007 - invalid number or identifier
line 2, col 15: "sare2" - invalid string constant
line 3, col 1: 2supa - invalid number or identifier
line 3, col 26: ; - illegal character
line 4, col 11: @ - illegal character
line 5, col 6: "piure - invalid string constant
```

## 4. Symbol Table

Identifiers and constants are kept in one Symbol Table, a hash table with separate chaining.

```c
typedef struct Node {
    char *lexeme;
    int pos;
    struct Node *next;
} Node;

struct ST {
    Node **buckets;
    Node **by_pos;
    int capacity;
    int size;
};
```

`buckets` is an array of linked lists. A lexeme goes in the list at index `hash(lexeme) % capacity`, and lexemes with the same index share the list.

Every lexeme gets a position when it is first inserted (0, 1, 2, ...). This position is what the PIF stores. `by_pos[p]` points to the node with position `p`, so a lexeme can be found from its position in O(1).

The hash function is djb2: start with `h = 5381` and, for each character `c`, do `h = h * 33 + c`.

The table starts with 101 buckets. When an insertion would make it more than 3/4 full, the number of buckets becomes `2 * capacity + 1` and every node is moved to its new list. Positions do not depend on the bucket, so they stay the same and the PIF remains valid.

`st.h` only declares the type `ST` and the functions, so the table is used only through its operations:

| Operation | Description | Complexity |
|-----------|-------------|------------|
| `st_create(capacity)` | creates an empty table | O(capacity) |
| `st_lookup(st, lexeme)` | searches the lexeme's list; returns its position or -1 | O(1) average |
| `st_insert(st, lexeme)` | returns the position if the lexeme exists; otherwise adds it at the front of its list with the next position | O(1) average |
| `st_get(st, pos)` | returns the lexeme at a position | O(1) |
| `st_size(st)` | returns the number of entries | O(1) |
| `st_free(st)` | frees all the memory | O(n) |

Since `st_insert` returns the existing position for a lexeme already in the table, every occurrence of the same identifier or constant gets the same position.

## 5. Program Internal Form

The PIF lists the tokens in the order they appear, as pairs (code, ST position). Identifiers and constants have code 0 or 1 and their position in the ST. Reserved words, operators and separators have their own code and position -1. The lexeme is also printed to make the file easier to read.

## 6. Example: `programs/p2.txt`

```
supa = List(["apa" 1000 ml, "oase" 200 g, "taietei" 200 g])
puts supa
supa = supa + "delicat" 10 g
read piure
...
```

`PIF.out` (first lines):

```
token                     code  st_pos
supa                         0       0
=                            8      -1
List                         2      -1
(                           11      -1
[                           13      -1
"apa"                        1       1
1000                         1       2
ml                           0       3
,                           15      -1
"oase"                       1       4
200                          1       5
g                            0       6
,                           15      -1
"taietei"                    1       7
200                          1       5
g                            0       6
]                           14      -1
)                           12      -1
puts                         7      -1
supa                         0       0
```

`ST.out`:

```
 pos  lexeme
   0  supa
   1  "apa"
   2  1000
   3  ml
   4  "oase"
   5  200
   6  g
   7  "taietei"
   8  "delicat"
   9  10
  10  piure
  11  "unt"
  12  "lapte"
  13  100
  14  i
```
