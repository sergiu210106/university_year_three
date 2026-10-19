import os
import sys

USAGE = "usage: python3 fa.py <file.fa> [word ...]"


def quote(s):
    return f"'{s}'"


def describe(chars, classes):
    """Writes a set of characters with class names where a whole class is included."""
    chars, parts = set(chars), []
    for name, members in sorted(classes.items(), key=lambda c: -len(c[1])):
        if set(members) <= chars:
            parts.append(name)
            chars -= set(members)
    return parts + [quote(c) for c in sorted(chars)]


def expand(items, where):
    chars = []
    for item in items:
        if len(item) == 4 and item[1:3] == "..":
            chars += [chr(c) for c in range(ord(item[0]), ord(item[3]) + 1)]
        elif len(item) == 1:
            chars.append(item)
        else:
            raise ValueError(f"{where}: bad class item {item!r}")
    return chars


class FA:
    def __init__(self, states, initial, finals, delta, classes):
        self.states = states
        self.initial = initial
        self.finals = set(finals)
        self.delta = delta
        self.alphabet = sorted({a for _, a in delta})
        self.classes = classes

    @classmethod
    def load(cls, path):
        classes, states, initial, finals, delta = {}, [], None, [], {}
        with open(path) as f:
            for lineno, line in enumerate(f, 1):
                where = f"{path}:{lineno}"
                words = line.split("#", 1)[0].split()
                if not words:
                    continue
                if len(words) >= 3 and words[1] == "=":
                    classes[words[0]] = expand(words[2:], where)
                elif words[0] == "states":
                    states = words[1:]
                elif words[0] == "initial" and len(words) == 2:
                    initial = words[1]
                elif words[0] == "final":
                    finals = words[1:]
                elif len(words) == 3:
                    p, sym, q = words
                    for s in (p, q):
                        if s not in states:
                            raise ValueError(f"{where}: unknown state {s}")
                    if sym in classes:
                        chars = classes[sym]
                    elif len(sym) == 1:
                        chars = [sym]
                    else:
                        raise ValueError(f"{where}: unknown symbol {sym}")
                    for a in chars:
                        delta.setdefault((p, a), set()).add(q)
                else:
                    raise ValueError(f"{where}: cannot parse {line.strip()!r}")
        if initial not in states:
            raise ValueError(f"{path}: initial state missing or unknown")
        for s in finals:
            if s not in states:
                raise ValueError(f"{path}: unknown final state {s}")
        return cls(states, initial, finals, delta, classes)

    def run(self, word):
        """Returns the set of current states after each prefix of word, stopping when it is empty."""
        current = {self.initial}
        steps = [current]
        for a in word:
            current = {q for p in current for q in self.delta.get((p, a), ())}
            steps.append(current)
            if not current:
                break
        return steps

    def accepts(self, word):
        return bool(self.run(word)[-1] & self.finals)

    def is_deterministic(self):
        return all(len(qs) == 1 for qs in self.delta.values())

    def __str__(self):
        order = {s: i for i, s in enumerate(self.states)}
        moves = {}
        for (p, a), qs in self.delta.items():
            for q in qs:
                moves.setdefault((p, q), []).append(a)
        lines = [
            f"Q  = {{{', '.join(self.states)}}}",
            f"Σ  = {', '.join(describe(self.alphabet, self.classes))}  ({len(self.alphabet)} symbols)",
            f"q0 = {self.initial}",
            f"F  = {{{', '.join(s for s in self.states if s in self.finals)}}}",
        ]
        for p, q in sorted(moves, key=lambda m: (order[m[0]], order[m[1]])):
            lines.append(f"δ({p}, {' | '.join(describe(moves[p, q], self.classes))}) = {q}")
        return "\n".join(lines)


class RG:
    def __init__(self, nonterminals, start, productions):
        self.nonterminals = nonterminals
        self.start = start
        # A -> list of (terminal, B), where B is None for A -> a and ("", None) is A -> ε
        self.productions = productions
        self.terminals = sorted({t for ps in productions.values() for t, _ in ps if t})

    def derive(self, word):
        """Returns the sentential forms of a derivation of word as (terminals, nonterminal) pairs, or None."""
        def search(a, i):
            for t, b in self.productions[a]:
                if t == "" and i == len(word):
                    return [(word, None)]
                if t == "" or i == len(word) or t != word[i]:
                    continue
                if b is None:
                    if i + 1 == len(word):
                        return [(word, None)]
                else:
                    rest = search(b, i + 1)
                    if rest is not None:
                        return [(word[:i + 1], b)] + rest
            return None

        steps = search(self.start, 0)
        return None if steps is None else [("", self.start)] + steps

    def format(self, classes={}):
        lines = [
            f"N     = {{{', '.join(self.nonterminals)}}}",
            f"Σ     = {', '.join(describe(self.terminals, classes))}",
            f"start = {self.start}",
            "P:",
        ]
        for a in self.nonterminals:
            groups = {}
            for t, b in self.productions[a]:
                groups.setdefault(b, []).append(t)
            alts = []
            for b, ts in groups.items():
                alts += [f"{t} {b}" if b else t for t in describe([t for t in ts if t], classes)]
                if "" in ts:
                    alts.append("ε")
            lines.append(f"  {a} -> {' | '.join(alts)}")
        return "\n".join(lines)


def fa_to_rg(fa):
    order = {s: i for i, s in enumerate(fa.states)}
    nonterminals = list(fa.states)
    productions = {}
    for p in fa.states:
        to_state, to_end = [], []
        for a in fa.alphabet:
            for q in sorted(fa.delta.get((p, a), ()), key=order.get):
                to_state.append((a, q))
                if q in fa.finals:
                    to_end.append((a, None))
        productions[p] = to_state + to_end

    # ε is allowed only on a start symbol that never appears on the right-hand side
    start = fa.initial
    if start in fa.finals:
        if any(start in qs for qs in fa.delta.values()):
            while start in nonterminals:
                start += "'"
            nonterminals.insert(0, start)
            productions[start] = list(productions[fa.initial])
        productions[start].append(("", None))

    # remove the nonterminals that generate no word, and the productions that use them
    productive = set()
    changed = True
    while changed:
        changed = False
        for a in nonterminals:
            if a not in productive and any(b is None or b in productive for _, b in productions[a]):
                productive.add(a)
                changed = True
    nonterminals = [a for a in nonterminals if a in productive or a == start]
    productions = {a: [(t, b) for t, b in productions[a] if b is None or b in productive]
                   for a in nonterminals}
    return RG(nonterminals, start, productions)


def show(fa, rg, word):
    steps = fa.run(word)
    accepted = bool(steps[-1] & fa.finals)
    print(f"{word or 'ε'}: {'accepted' if accepted else 'rejected'}")

    configs = []
    for i, states in enumerate(steps):
        if not states:
            configs.append(f"no move on {quote(word[i - 1])}")
        else:
            names = ", ".join(s for s in fa.states if s in states)
            configs.append(f"({names if len(states) == 1 else '{' + names + '}'}, {word[i:] or 'ε'})")
    end = "" if accepted or not steps[-1] else "  not final"
    print(f"  FA: {' |- '.join(configs)}{end}")

    derivation = rg.derive(word)
    if derivation is None:
        print("  RG: no derivation")
    else:
        forms = [" ".join(filter(None, [quote(w) if w else "", b or ""])) or "ε" for w, b in derivation]
        print(f"  RG: {' => '.join(forms)}")


def main():
    if len(sys.argv) < 2:
        sys.exit(USAGE)
    path = sys.argv[1]
    try:
        fa = FA.load(path)
    except (OSError, ValueError) as e:
        sys.exit(str(e))
    rg = fa_to_rg(fa)

    kind = "deterministic" if fa.is_deterministic() else "nondeterministic"
    print(f"Finite automaton ({kind})")
    print(fa)
    print()
    print("Regular grammar")
    print(rg.format(fa.classes))

    out = f"RG_{os.path.splitext(os.path.basename(path))[0]}.out"
    with open(out, "w") as f:
        f.write(rg.format() + "\n")
    print(f"\nfull grammar written to {out}")

    for word in sys.argv[2:]:
        print()
        show(fa, rg, word)


if __name__ == "__main__":
    main()
