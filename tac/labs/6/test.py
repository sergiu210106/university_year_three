import os
import tempfile

import lexer
import parser

HERE = os.path.dirname(os.path.abspath(__file__))
PROGRAMS = os.path.join(HERE, "programs")


def replay(derivation):
    nonterminals = {rhs.split(" -> ")[0] for rhs in parser.PRODUCTIONS.values()}
    form = ["program"]
    for n in derivation:
        left, right = parser.PRODUCTIONS[n].split(" -> ")
        i = max(i for i, s in enumerate(form) if s in nonterminals)
        assert form[i] == left, f"production {n} does not apply to {form[i]}"
        form[i:i + 1] = right.split()
    assert not any(s in nonterminals for s in form), "derivation is not complete"
    return form


def token_types(source):
    lx = lexer.make_lexer()
    lx.input(source)
    return [t.type for t in lx]


def main():
    os.chdir(tempfile.mkdtemp())
    ok = True
    for name, valid in [("p1", True), ("p2", True), ("p_err", False),
                        ("p_err2", False), ("p_lex", False)]:
        with open(os.path.join(PROGRAMS, name + ".txt")) as f:
            source = f.read()
        derivation, syntax_error = parser.parse(source)
        accepted = not syntax_error and not lexer.errors
        result = accepted == valid
        if valid and accepted:
            lexer.reset()
            result = replay(derivation) == token_types(source)
        ok &= result
        print(f"{name:8} {'accepted' if accepted else 'rejected':9} {'ok' if result else 'FAIL'}")
    print("all tests passed" if ok else "tests failed")
    raise SystemExit(0 if ok else 1)


if __name__ == "__main__":
    main()
