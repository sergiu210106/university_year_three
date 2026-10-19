import itertools
import os
import re
import sys

from fa import FA, fa_to_rg

HERE = os.path.dirname(os.path.abspath(__file__))

# the regular expressions used by the lexer in ../3/lexer.py
CASES = [
    ("identifier.fa", r"[A-Za-z][A-Za-z0-9_]*",
     ["carbonara_non_italiana", "done_x", "i", "omleta", "ml", "Z9_"],
     ["2supa", "_x", "x-y", '"oua"', ""]),
    ("constant.fa", r'0|[1-9][0-9]*|"[A-Za-z]*"',
     ["0", "200", "100000", '"oua"', '""', '"Lapte"'],
     ["007", "00", '"sare2"', '"piure', "piure", "2supa", ""]),
]

# every string up to this length over this sample alphabet is checked
SAMPLE = 'aZ01_"-'
MAX_LEN = 4


def check(fa, rg, regex, word):
    fa_ok = fa.accepts(word)
    re_ok = re.fullmatch(regex, word) is not None
    rg_ok = rg.derive(word) is not None
    if not fa_ok == re_ok == rg_ok:
        return f"{word!r}: FA={fa_ok} regex={re_ok} RG={rg_ok}"
    return None


def main():
    failed = False
    for name, regex, accepted, rejected in CASES:
        fa = FA.load(os.path.join(HERE, "fa", name))
        rg = fa_to_rg(fa)
        errors = []
        if not fa.is_deterministic():
            errors.append("FA is not deterministic")

        words = ["".join(w) for n in range(MAX_LEN + 1) for w in itertools.product(SAMPLE, repeat=n)]
        errors += filter(None, (check(fa, rg, regex, w) for w in words + accepted + rejected))
        errors += [f"{w!r} should be accepted" for w in accepted if not fa.accepts(w)]
        errors += [f"{w!r} should be rejected" for w in rejected if fa.accepts(w)]

        if errors:
            failed = True
            print(f"{name}: FAILED")
            for e in errors:
                print(f"  {e}")
        else:
            print(f"{name}: {len(words)} generated + {len(accepted) + len(rejected)} named words, FA = regex = RG")

    if failed:
        sys.exit(1)
    print("all tests passed")


if __name__ == "__main__":
    main()
