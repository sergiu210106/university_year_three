import ctypes
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
from fa import FA

IDENTIFIER_FA = FA.load(os.path.join(HERE, "identifier.fa"))
CONSTANT_FA = FA.load(os.path.join(HERE, "constant.fa"))

ERRORS = {
    "BAD_STRING": "invalid string constant",
    "BAD_NUMBER": "invalid number or identifier",
    "UNKNOWN": "illegal character",
}


def load_tokens(path):
    with open(path) as f:
        return [line.strip() for line in f if line.strip()]


def lexemes(source, operators):
    """Split candidates; only the automata decide identifiers and constants.

    Keep digit/letter runs whole so 007 and 2supa cannot become valid tokens.
    Strings extend through a closing quote or to the end of their line.
    """
    i = 0
    while i < len(source):
        start = i
        char = source[i]
        if char in " \t\r\n":
            i += 1
            continue
        if char == '"':
            i += 1
            while i < len(source) and source[i] not in '\n"':
                i += 1
            if i < len(source) and source[i] == '"':
                i += 1
        elif char.isascii() and (char.isalnum() or char == "_"):
            i += 1
            while i < len(source) and source[i].isascii() and (
                source[i].isalnum() or source[i] == "_"
            ):
                i += 1
        else:
            operator = next((op for op in operators if source.startswith(op, i)), None)
            i += len(operator) if operator else 1
        yield start, source[start:i]


def load_st():
    try:
        lib = ctypes.CDLL(os.path.join(HERE, "libst.so"))
    except OSError:
        sys.exit("libst.so not found, run make first")
    lib.st_create.argtypes = [ctypes.c_int]
    lib.st_create.restype = ctypes.c_void_p
    lib.st_insert.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.st_get.argtypes = [ctypes.c_void_p, ctypes.c_int]
    lib.st_get.restype = ctypes.c_char_p
    lib.st_size.argtypes = [ctypes.c_void_p]
    lib.st_free.argtypes = [ctypes.c_void_p]
    return lib


def scan(source, tokens, lib, st):
    keywords = {token for token in tokens if IDENTIFIER_FA.accepts(token)}
    operators = sorted(set(tokens) - keywords, key=lambda token: (-len(token), token))
    pif, errors = [], []
    line, line_start, previous = 1, 0, 0
    for start, lexeme in lexemes(source, operators):
        gap = source[previous:start]
        line += gap.count("\n")
        last_newline = source.rfind("\n", previous, start)
        if last_newline != -1:
            line_start = last_newline + 1
        previous = start + len(lexeme)
        if CONSTANT_FA.accepts(lexeme):
            pif.append(("const", lib.st_insert(st, lexeme.encode())))
        elif IDENTIFIER_FA.accepts(lexeme):
            if lexeme in keywords:
                pif.append((lexeme, -1))
            else:
                pif.append(("id", lib.st_insert(st, lexeme.encode())))
        elif lexeme in operators:
            pif.append((lexeme, -1))
        else:
            kind = ("BAD_STRING" if lexeme.startswith('"') else
                    "BAD_NUMBER" if lexeme[0] in "0123456789_" else "UNKNOWN")
            errors.append((line, start - line_start + 1, lexeme, ERRORS[kind]))
    return pif, errors


def main():
    if len(sys.argv) < 2:
        sys.exit("usage: python3 lexer.py <program> [tokens.txt]")
    tokens_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "tokens.txt")
    tokens = load_tokens(tokens_path)
    with open(sys.argv[1]) as f:
        source = f.read()

    lib = load_st()
    st = lib.st_create(101)
    pif, errors = scan(source, tokens, lib, st)

    with open("PIF.out", "w") as f:
        f.write(f"{'token':<10}{'st_pos':>6}\n")
        for token, pos in pif:
            f.write(f"{token:<10}{pos:>6}\n")

    with open("ST.out", "w") as f:
        f.write(f"{'pos':>4}  lexeme\n")
        for pos in range(lib.st_size(st)):
            f.write(f"{pos:>4}  {lib.st_get(st, pos).decode()}\n")

    lib.st_free(st)

    for line, col, lexeme, message in errors:
        print(f"line {line}, col {col}: {lexeme} - {message}")
    if not errors:
        print("lexically correct")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
