import os
import sys

import ply.yacc as yacc

import lexer
from lexer import tokens

PRODUCTIONS = {
    1: "program -> statement_list",
    2: "statement_list -> statement",
    3: "statement_list -> statement statement_list",
    4: "statement -> assignment",
    5: "statement -> for_loop",
    6: "statement -> input",
    7: "statement -> print",
    8: "assignment -> ID EQ expression",
    9: "for_loop -> FOR ID IN expression COLON statement_list DONE",
    10: "input -> READ ID",
    11: "print -> PUTS expression",
    12: "expression -> creation",
    13: "expression -> ID",
    14: "expression -> expression PLUS term",
    15: "expression -> expression MINUS term",
    16: "creation -> LIST LPAREN LBRACKET ingredient_list RBRACKET RPAREN",
    17: "ingredient_list -> ingredient",
    18: "ingredient_list -> ingredient COMMA ingredient_list",
    19: "ingredient -> CONST CONST ID",
    20: "term -> ingredient",
    21: "term -> ID",
}

applied = []


class ParseError(Exception):
    pass


def rule(n):
    applied.append(n)


def p_program(p):
    "program : statement_list"
    rule(1)


def p_statement_list_one(p):
    "statement_list : statement"
    rule(2)


def p_statement_list_more(p):
    "statement_list : statement statement_list"
    rule(3)


def p_statement_assignment(p):
    "statement : assignment"
    rule(4)


def p_statement_for(p):
    "statement : for_loop"
    rule(5)


def p_statement_input(p):
    "statement : input"
    rule(6)


def p_statement_print(p):
    "statement : print"
    rule(7)


def p_assignment(p):
    "assignment : ID EQ expression"
    rule(8)


def p_for_loop(p):
    "for_loop : FOR ID IN expression COLON statement_list DONE"
    rule(9)


def p_input(p):
    "input : READ ID"
    rule(10)


def p_print(p):
    "print : PUTS expression"
    rule(11)


def p_expression_creation(p):
    "expression : creation"
    rule(12)


def p_expression_id(p):
    "expression : ID"
    rule(13)


def p_expression_add(p):
    "expression : expression PLUS term"
    rule(14)


def p_expression_remove(p):
    "expression : expression MINUS term"
    rule(15)


def p_creation(p):
    "creation : LIST LPAREN LBRACKET ingredient_list RBRACKET RPAREN"
    rule(16)


def p_ingredient_list_one(p):
    "ingredient_list : ingredient"
    rule(17)


def p_ingredient_list_more(p):
    "ingredient_list : ingredient COMMA ingredient_list"
    rule(18)


def p_ingredient(p):
    "ingredient : CONST CONST ID"
    rule(19)


def p_ingredient_term(p):
    "term : ingredient"
    rule(20)


def p_id_term(p):
    "term : ID"
    rule(21)


def p_error(t):
    if t is None:
        raise ParseError(f"line {lexer.current.lineno}: unexpected end of program")
    raise ParseError(f"line {t.lineno}, col {lexer.column(t)}: unexpected '{t.value}' ({t.type})")


def parse(source):
    lexer.reset()
    applied.clear()
    lx = lexer.current = lexer.make_lexer()
    parser = yacc.yacc(debug=True, debugfile="parser.out", write_tables=False,
                         errorlog=yacc.NullLogger())
    try:
        parser.parse(source, lexer=lx)
        error = None
    except ParseError as e:
        error = str(e)
    # reductions come bottom-up, so reverse them to get the derivation
    return applied[::-1], error


def write_outputs(derivation):
    with open("PIF.out", "w") as f:
        f.write(f"{'token':<10}{'st_pos':>6}\n")
        for token, pos in lexer.pif:
            f.write(f"{token:<10}{pos:>6}\n")
    with open("ST.out", "w") as f:
        f.write(f"{'pos':>4}  lexeme\n")
        for pos in range(lexer.lib.st_size(lexer.st)):
            f.write(f"{pos:>4}  {lexer.lib.st_get(lexer.st, pos).decode()}\n")
    with open("productions.out", "w") as f:
        f.write(" ".join(map(str, derivation)) + "\n")
        for n in dict.fromkeys(derivation):
            f.write(f"{n:>3}  {PRODUCTIONS[n]}\n")


def main():
    if len(sys.argv) < 2:
        sys.exit("usage: python3 parser.py <program>")
    with open(sys.argv[1]) as f:
        source = f.read()

    derivation, syntax_error = parse(source)

    for line, col, lexeme, message in lexer.errors:
        print(f"lexical error, line {line}, col {col}: {lexeme} - {message}")
    if syntax_error:
        print(f"syntax error, {syntax_error}")
    if lexer.errors or syntax_error:
        sys.exit(1)

    write_outputs(derivation)
    print("string of productions:")
    print(" ".join(map(str, derivation)))


if __name__ == "__main__":
    main()
