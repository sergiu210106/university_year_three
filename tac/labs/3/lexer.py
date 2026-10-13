import ctypes
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
IDENTIFIER = r"[A-Za-z][A-Za-z0-9_]*"

ERRORS = {
    "BAD_STRING": "invalid string constant",
    "BAD_NUMBER": "invalid number or identifier",
    "UNKNOWN": "illegal character",
}


def load_tokens(path):
    codes = {}
    with open(path) as f:
        for line in f:
            if line.strip():
                token, code = line.split()
                codes[token] = int(code)
    return codes


def build_regex(codes):
    tokens = [t for t in codes if t not in ("identifier", "constant")]
    keywords = {t for t in tokens if re.fullmatch(IDENTIFIER, t)}
    operators = sorted(set(tokens) - keywords, key=len, reverse=True)
    patterns = [
        ("NEWLINE", r"\n"),
        ("SPACE", r"[ \t\r]+"),
        ("STRING", r'"[A-Za-z]*"'),
        ("BAD_STRING", r'"[^"\n]*"?'),
        ("BAD_NUMBER", r"0[0-9]+|[0-9]+[A-Za-z_][A-Za-z0-9_]*"),
        ("NUMBER", r"0|[1-9][0-9]*"),
        ("IDENTIFIER", IDENTIFIER),
        ("OPERATOR", "|".join(map(re.escape, operators))),
        ("UNKNOWN", r"."),
    ]
    regex = "|".join(f"(?P<{name}>{p})" for name, p in patterns)
    return re.compile(regex), keywords


def load_st():
    try:
        lib = ctypes.CDLL(os.path.join(HERE, "libst.so"))
    except OSError:
        sys.exit("libst.so not found, run make first")
    lib.st_create.restype = ctypes.c_void_p
    lib.st_insert.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.st_get.argtypes = [ctypes.c_void_p, ctypes.c_int]
    lib.st_get.restype = ctypes.c_char_p
    lib.st_size.argtypes = [ctypes.c_void_p]
    lib.st_free.argtypes = [ctypes.c_void_p]
    return lib


def scan(source, codes, lib, st):
    regex, keywords = build_regex(codes)
    pif, errors = [], []
    line, line_start = 1, 0
    for m in regex.finditer(source):
        kind, lexeme = m.lastgroup, m.group()
        if kind == "NEWLINE":
            line, line_start = line + 1, m.end()
        elif kind == "SPACE":
            pass
        elif kind in ("STRING", "NUMBER"):
            pif.append((lexeme, codes["constant"], lib.st_insert(st, lexeme.encode())))
        elif kind == "IDENTIFIER" and lexeme not in keywords:
            pif.append((lexeme, codes["identifier"], lib.st_insert(st, lexeme.encode())))
        elif kind in ("IDENTIFIER", "OPERATOR"):
            pif.append((lexeme, codes[lexeme], -1))
        else:
            errors.append((line, m.start() - line_start + 1, lexeme, ERRORS[kind]))
    return pif, errors


def main():
    if len(sys.argv) < 2:
        sys.exit("usage: python3 lexer.py <program> [tokens.txt]")
    tokens_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "tokens.txt")
    codes = load_tokens(tokens_path)
    with open(sys.argv[1]) as f:
        source = f.read()

    lib = load_st()
    st = lib.st_create(101)
    pif, errors = scan(source, codes, lib, st)

    with open("PIF.out", "w") as f:
        f.write(f"{'token':<24}{'code':>6}{'st_pos':>8}\n")
        for token, code, pos in pif:
            f.write(f"{token:<24}{code:>6}{pos:>8}\n")

    with open("ST.out", "w") as f:
        f.write(f"{'pos':>4}  lexeme\n")
        for pos in range(lib.st_size(st)):
            f.write(f"{pos:>4}  {lib.st_get(st, pos).decode()}\n")

    lib.st_free(st)

    for line, col, lexeme, message in errors:
        print(f"line {line}, col {col}: {lexeme} - {message}")
    if not errors:
        print("lexically correct")


if __name__ == "__main__":
    main()
