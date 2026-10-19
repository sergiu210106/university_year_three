# Lab 4: Finite automata and regular grammars

This lab defines finite automata (FA) for the identifiers and the constants of the recipe language from Lab 1 (grammar in `../2/bnf.txt`). It also implements the transformation of an FA into an equivalent regular grammar (RG).

Files:

- `fa/identifier.fa` - the FA for identifiers
- `fa/constant.fa` - the FA for constants (numbers and strings)
- `fa.py` - reads an FA, converts it to an RG, and checks words with both
- `test.py` - checks that each FA, its RG, and the Lab 3 regex accept the same words

Run:

```
python3 fa.py fa/identifier.fa x_1 2supa
python3 fa.py fa/constant.fa 200 007 '"oua"'
python3 test.py
```

`fa.py` prints the FA and the RG. It writes the full grammar (every terminal listed separately) to `RG_<name>.out`. For each word given after the file, it shows the run of the FA and the derivation in the RG.

## 1. Definitions

### Finite automaton

A finite automaton is a 5-tuple M = (Q, Σ, δ, q0, F):

- Q - a finite set of states
- Σ - the alphabet, a finite set of input symbols
- δ : Q × Σ → P(Q) - the transition function
- q0 ∈ Q - the initial state
- F ⊆ Q - the set of final states

The FA reads the word from left to right. It starts in q0 and, on each symbol a, moves from state p to a state of δ(p, a). A configuration is a pair (state, remaining input), and one move is written (p, aw) ⊢ (q, w). The word is **accepted** if after the last symbol the FA can be in a final state:

L(M) = { w ∈ Σ* | (q0, w) ⊢* (qf, ε), qf ∈ F }

The FA is **deterministic** (DFA) when δ(p, a) has at most one state for every p and a. Both automata below are deterministic and **partial**: some pairs (p, a) have no transition. If the FA reaches such a pair, it stops and the word is rejected. This is the same as adding an extra non-final "error" state that every missing transition goes to.

### Regular grammar

A grammar is a 4-tuple G = (N, Σ, P, S): non-terminals N, terminals Σ, productions P, and start symbol S ∈ N. A grammar is **regular** (right-linear) if every production has one of these forms:

- A → aB (A, B ∈ N, a ∈ Σ)
- A → a
- S → ε, allowed only if S does not appear on the right-hand side of any production

A derivation replaces one non-terminal at each step: S ⇒ a1A1 ⇒ a1a2A2 ⇒ … ⇒ a1a2…an. The language is L(G) = { w ∈ Σ* | S ⇒* w }.

Regular grammars and finite automata describe the same class of languages: the regular languages. The same class is also described by the regular expressions used in Lab 3.

## 2. FA for identifiers

`<identifier>` in the grammar is a letter followed by any number of letters, digits or `_`. In Lab 3 the regex is `[A-Za-z][A-Za-z0-9_]*`.

M_id = (Q, Σ, δ, q0, F):

- Q = {S, I}
- Σ = letter ∪ digit ∪ {`_`}, 63 symbols, where letter = `a..z A..Z` and digit = `0..9`
- q0 = S
- F = {I}

| δ | letter | digit | `_` |
|---|--------|-------|-----|
| → S | I | - | - |
| ← I | I | I | I |

(→ initial, ← final, `-` no transition)

```
         letter            letter, digit, _
   ->(S) -------> ((I)) ----------+
                    ^             |
                    +-------------+
```

- S means "nothing read yet". From S only a letter can be read, so `2supa` and `_x` stop in S.
- I means "a valid identifier so far". It is final, so the word may end there. It accepts every letter, digit and `_`, so it loops on itself.

Reserved words (`for`, `done`, ...) are also accepted by this FA. As in Lab 3, they are told apart from identifiers after the whole lexeme has been recognised. This means `done_x` is one identifier. Measuring units (`g`, `ml`, `buc`) are scanned as identifiers too, so they need no automaton of their own.

## 3. FA for constants

The language has two kinds of constants:

- numbers: `"0" | <non_zero_digit> <number_tail>`, regex `0|[1-9][0-9]*`
- strings: `'"' <string_characters> '"'`, only letters between quotes, regex `"[A-Za-z]*"`

Both are recognised by one FA with two branches that start from the same initial state. The first symbol decides the branch: a digit leads to a number and `"` leads to a string.

M_const = (Q, Σ, δ, q0, F):

- Q = {S, Z, N, A, C}
- Σ = letter ∪ digit ∪ {`"`}, 63 symbols
- q0 = S
- F = {Z, N, C}

| δ | `0` | `1..9` | letter | `"` |
|---|-----|--------|--------|-----|
| → S | Z | N | - | A |
| ← Z | - | - | - | - |
| ← N | N | N | - | - |
| A | - | - | A | C |
| ← C | - | - | - | - |

```
            0
      +---------> ((Z))
      |   1..9                 0..9
   ->(S) ------> ((N)) ------+
      |            ^         |
      |            +---------+
      |    "               letter          "
      +---------> (A) ------+  ------------------> ((C))
                   ^        |
                   +--------+
```

The states mean:

- Z - the number `0`. It has no transitions, so nothing can follow a leading zero. This rule is what rejects `007`.
- N - a number that started with a non-zero digit. Any digit may follow.
- A - inside a string, after the opening quote. It is not final because the string is not closed yet. Letters loop here.
- C - the closing quote was read. The string is complete and nothing may follow it.

The lexical errors from `../3/programs/p_err.txt` are rejected by these automata as follows:

| Lexeme | FA | Where it stops |
|--------|----|----------------|
| `007` | const | in Z, no move on `0` |
| `2supa` | const | in N, no move on `s` (and in S for the identifier FA) |
| `"sare2"` | const | in A, no move on `2` |
| `"piure` | const | input ends in A, which is not final |

## 4. Input file format

An FA is written in a text file. This is `fa/identifier.fa`:

```
# identifier = letter (letter | digit | _)*      <- comment
letter = a..z A..Z                                <- character class: ranges and single characters
digit  = 0..9

states  S I                                       <- Q
initial S                                         <- q0
final   I                                         <- F

S letter I                                        <- transition: state, symbol, state
I letter I
I digit  I
I _      I
```

- `name = items` defines a class of characters. `x..y` is a range and any other item is a single character.
- A transition `p sym q` uses either a class name or a single character. A class gives one transition for each of its characters. This means `S letter I` stands for the 52 transitions δ(S, a) = I, …, δ(S, Z) = I.
- Σ is the set of all characters used in transitions.
- An unknown state, a symbol that is neither a class nor a single character, or a missing initial state is reported with the file and line number.

Classes only make the file shorter. In memory, δ holds one entry per character, which is the formal definition.

## 5. Transformation FA → RG

Given M = (Q, Σ, δ, q0, F), `fa_to_rg` builds G = (N, Σ, P, S):

1. N = Q. Every state becomes a non-terminal with the same name, and S = q0.
2. For every transition q ∈ δ(p, a), add **p → a q**.
3. If also q ∈ F, add **p → a**. After reading a, the FA is in a final state, so the word may end there.
4. If q0 ∈ F, the FA accepts ε, so add **S → ε**. A regular grammar allows this only if S does not appear on any right-hand side. If q0 has an incoming transition, a new start symbol S' is created instead. S' gets a copy of all the productions of q0, plus S' → ε. S' never appears on the right, so the rule is respected.
5. Remove the non-terminals that generate no word, together with the productions that use them. A non-terminal is productive if it has a production A → a, or a production A → aB where B is productive. This is repeated until nothing changes. In our automata Z and C are final states without outgoing transitions. They have no productions, so the productions S → `0` Z and A → `"` C are dropped. The productions S → `0` and A → `"` (added by rule 3) remain, so the language does not change.

**Why L(G) = L(M).** Each step of the FA, (p, aw) ⊢ (q, w), matches one step of the derivation, x p ⇒ x a q, using the production p → a q from rule 2. An accepting run

(q0, a1a2…an) ⊢ (q1, a2…an) ⊢ … ⊢ (qn, ε), qn ∈ F

therefore gives the derivation

q0 ⇒ a1 q1 ⇒ a1a2 q2 ⇒ … ⇒ a1…an-1 qn-1 ⇒ a1…an

where the last step uses qn-1 → an, which exists because qn ∈ F (rule 3). The reverse also holds: every derivation in G is read back as an accepting run of M in the same way. The empty word is covered by rule 4.

Example with the rule 4 case: for the automaton with F = {S} and δ(S, a) = S (it accepts a*), the program gives

```
S' -> 'a' S | 'a' | ε
S -> 'a' S | 'a'
```

## 6. The resulting grammars

`fa.py` prints the grammar with class names (`letter I` stands for 52 productions `'a' I`, …, `'Z' I`). Characters are quoted because uppercase letters are terminals and would otherwise look like non-terminals.

G_id, from `python3 fa.py fa/identifier.fa`:

```
N     = {S, I}
Σ     = letter, digit, '_'
start = S
P:
  S -> letter I | letter
  I -> letter I | digit I | '_' I | letter | digit | '_'
```

This is the same as `<identifier> ::= <letter> <identifier_tail>` in `../2/bnf.txt`, written in right-linear form. I plays the role of `<identifier_tail>`. Instead of ending the tail with ε, the grammar uses the productions without a non-terminal (`S -> letter`, `I -> digit`, ...).

G_const, from `python3 fa.py fa/constant.fa`:

```
N     = {S, N, A}
Σ     = letter, digit, '"'
start = S
P:
  S -> '"' A | nonzero N | digit
  N -> digit N | digit
  A -> letter A | '"'
```

`S -> digit` contains `S -> '0'` (from S → Z, Z final) and `S -> '1' | … | '9'` (from S → N, N final). The printer joins them into one class. Z and C were removed by step 5. N corresponds to `<number_tail>` and A to `<string_characters> '"'`.

The full grammars, with every terminal listed separately, are in `RG_identifier.out` and `RG_constant.out`.

## 7. Examples

For each word, `fa.py` shows the run of the FA (configurations) and a derivation in the RG. The derivation is found by searching the productions of the grammar, without using the FA, so it shows that the grammar really generates the word.

```
$ python3 fa.py fa/identifier.fa x_1 2supa
...
x_1: accepted
  FA: (S, x_1) |- (I, _1) |- (I, 1) |- (I, ε)
  RG: S => 'x' I => 'x_' I => 'x_1'

2supa: rejected
  FA: (S, 2supa) |- no move on '2'
  RG: no derivation
```

```
$ python3 fa.py fa/constant.fa 200 '"oua"' 007 '"sare2"' '"piure'
...
200: accepted
  FA: (S, 200) |- (N, 00) |- (N, 0) |- (N, ε)
  RG: S => '2' N => '20' N => '200'

"oua": accepted
  FA: (S, "oua") |- (A, oua") |- (A, ua") |- (A, a") |- (A, ") |- (C, ε)
  RG: S => '"' A => '"o' A => '"ou' A => '"oua' A => '"oua"'

007: rejected
  FA: (S, 007) |- (Z, 07) |- no move on '0'
  RG: no derivation

"sare2": rejected
  FA: (S, "sare2") |- (A, sare2") |- (A, are2") |- (A, re2") |- (A, e2") |- (A, 2") |- no move on '2'
  RG: no derivation

"piure: rejected
  FA: (S, "piure) |- (A, piure) |- (A, iure) |- (A, ure) |- (A, re) |- (A, e) |- (A, ε)  not final
  RG: no derivation
```

## 8. Implementation

`fa.py` has two classes and the conversion function:

| Name | Description |
|------|-------------|
| `FA.load(path)` | reads a `.fa` file, expands classes into one transition per character |
| `FA.run(word)` | the sets of current states after each symbol. A set is kept so that an NFA also works. |
| `FA.accepts(word)` | true if the last set contains a final state |
| `FA.is_deterministic()` | true if every δ(p, a) has a single state |
| `RG.derive(word)` | depth-first search over the productions. Returns the sentential forms of a derivation, or `None`. |
| `RG.format(classes)` | the grammar as text, with or without class names |
| `fa_to_rg(fa)` | the transformation from section 5 |

δ is a dictionary `(state, symbol) -> set of states`. The productions are a dictionary `non-terminal -> list of (terminal, non-terminal)`, where the non-terminal is `None` for A → a, and `("", None)` is A → ε.

## 9. Testing

`test.py` compares, for each automaton, three things that must accept exactly the same words:

- the FA (`accepts`)
- the RG produced by `fa_to_rg` (`derive`)
- the regex the Lab 3 lexer uses for the same tokens

It checks every string of length 0 to 4 over the characters `a Z 0 1 _ " -` (2801 strings). These cover the cases that matter: a letter, a non-zero digit, zero, underscore, a quote, and a character outside Σ. It also checks named words from the lab programs, for example `carbonara_non_italiana` and `"oua"` (accepted) and `007` and `"sare2"` (rejected).

```
$ python3 test.py
identifier.fa: 2801 generated + 11 named words, FA = regex = RG
constant.fa: 2801 generated + 13 named words, FA = regex = RG
all tests passed
```
