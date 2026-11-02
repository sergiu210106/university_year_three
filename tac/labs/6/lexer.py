import ctypes
import os
import sys

import ply.lex as lex

HERE = os.path.dirname(os.path.abspath(__file__))

reserved = {
    "List": "LIST",
    "for": "FOR",
    "in": "IN",
    "done": "DONE",
    "read": "READ",
    "puts": "PUTS",
}

tokens = [
    "ID", "CONST",
    "EQ", "PLUS", "MINUS", "LPAREN", "RPAREN", "LBRACKET", "RBRACKET",
    "COMMA", "COLON",
] + list(reserved.values())

lib = None
st = None
pif = []
errors = []
last_newline = -1
current = None


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


def reset():
    global lib, st, last_newline
    lib = lib or load_st()
    if st is not None:
        lib.st_free(st)
    st = lib.st_create(101)
    pif.clear()
    errors.clear()
    last_newline = -1


def column(t):
    return t.lexpos - last_newline


def error(t, lexeme, message):
    errors.append((t.lineno, column(t), lexeme, message))


# operators and separators
operators = {
    "=": "EQ", "+": "PLUS", "-": "MINUS", "(": "LPAREN", ")": "RPAREN",
    "[": "LBRACKET", "]": "RBRACKET", ",": "COMMA", ":": "COLON",
}
t_ignore = " \t\r"


def t_newline(t):
    r"\n"
    global last_newline
    t.lexer.lineno += 1
    last_newline = t.lexpos


# tried in this order, like in Lab 3
def t_STRING(t):
    r'"[A-Za-z]*"'
    t.type = "CONST"
    pif.append(("const", lib.st_insert(st, t.value.encode())))
    return t


def t_BAD_STRING(t):
    r'"[^"\n]*"?'
    error(t, t.value, "invalid string constant")


def t_BAD_NUMBER(t):
    r"0[0-9]+|[0-9]+[A-Za-z_][A-Za-z0-9_]*"
    error(t, t.value, "invalid number or identifier")


def t_NUMBER(t):
    r"0|[1-9][0-9]*"
    t.type = "CONST"
    pif.append(("const", lib.st_insert(st, t.value.encode())))
    return t


def t_IDENTIFIER(t):
    r"[A-Za-z][A-Za-z0-9_]*"
    t.type = reserved.get(t.value, "ID")
    if t.type == "ID":
        pif.append(("id", lib.st_insert(st, t.value.encode())))
    else:
        pif.append((t.value, -1))
    return t


def t_OPERATOR(t):
    r"[-=+()\[\],:]"
    t.type = operators[t.value]
    pif.append((t.value, -1))
    return t


def t_error(t):
    error(t, t.value[0], "illegal character")
    t.lexer.skip(1)


def make_lexer():
    return lex.lex()
