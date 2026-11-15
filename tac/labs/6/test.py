import os
import subprocess
import sys
import tempfile
from pathlib import Path
from unittest.mock import patch

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


def check_pipeline():
    with open(os.path.join(PROGRAMS, "p_small.txt")) as f:
        source = f.read()
    expected = [1, 3, 2, 7, 11, 13, 4, 8, 12, 16, 17, 19]
    assert parser.parse(source) == (expected, None)
    assert parser.parse(source, lexer.DEFAULT_TOKENS) == (expected, None)

    pif, locations = lexer.scan(source)
    with patch.object(lexer.current, "token", side_effect=AssertionError("rescanned source")):
        assert parser.parse_pif(pif, parser, locations) == (expected, None)

    tokens_path = Path("tokens.txt")
    tokens_path.write_text(Path(lexer.DEFAULT_TOKENS).read_text().replace("puts\n", ""))
    assert parser.parse("puts x", str(tokens_path))[1] is not None
    assert lexer.pif == [("id", 0), ("id", 1)]
    tokens_path.write_text(Path(lexer.DEFAULT_TOKENS).read_text().replace("+\n", ""))
    assert parser.parse("x = x + y", str(tokens_path)) == ([], None)
    assert lexer.errors == [(1, 7, "+", "illegal character")]

    source = Path(PROGRAMS, "p_lex.txt").read_text()
    with patch.object(parser, "parse_pif", side_effect=AssertionError("parsed lexical errors")):
        assert parser.parse(source) == ([], None)
    assert [(line, col) for line, col, _, _ in lexer.errors] == [(1, 20), (2, 11)]

    _, error = parser.parse('x = List(["a" 1 g "b" 2 g])\nputs x\n')
    assert error == 'line 1, col 19: unexpected \'"b"\' (CONST)', error
    _, error = parser.parse("for x in y:\nputs x\n")
    assert error == "line 3: unexpected end of program", error

    command = [sys.executable, os.path.join(HERE, "parser.py"),
               os.path.join(PROGRAMS, "p_small.txt")]
    for args in ([], [lexer.DEFAULT_TOKENS]):
        result = subprocess.run(command + args, capture_output=True, text=True)
        assert result.returncode == 0, result.stderr
        assert " ".join(map(str, expected)) in result.stdout
    for content, message in [("@\n", "unsupported token definitions: @"),
                             ("", "token list is empty"),
                             (None, "No such file")]:
        if content is None:
            path = "missing-tokens.txt"
        else:
            tokens_path.write_text(content)
            path = str(tokens_path)
        result = subprocess.run(command + [path], capture_output=True, text=True)
        assert result.returncode == 1 and message in result.stderr, result.stderr
    print("pipeline, token configuration and CLI checks passed")


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
    check_pipeline()
    print("all tests passed" if ok else "tests failed")
    raise SystemExit(0 if ok else 1)


if __name__ == "__main__":
    main()
