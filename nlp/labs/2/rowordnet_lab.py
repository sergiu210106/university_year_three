import warnings

warnings.filterwarnings("ignore")

import rowordnet as rwn
from rowordnet import Synset

words = [
    ("casă", Synset.Pos.NOUN),
    ("carte", Synset.Pos.NOUN),
    ("câine", Synset.Pos.NOUN),
    ("merge", Synset.Pos.VERB),
    ("citi", Synset.Pos.VERB),
    ("mânca", Synset.Pos.VERB),
    ("frumos", Synset.Pos.ADJECTIVE),
    ("mare", Synset.Pos.ADJECTIVE),
]

wn = rwn.RoWordNet()


def lits(synset_id):
    return ", ".join(wn(synset_id).literals)


for word, pos in words:
    # strict=True: exact literal match, otherwise "casă" also matches "casă de bani"
    ids = list(dict.fromkeys(wn.synsets(literal=word, pos=pos, strict=True)))
    print(f"===== {word} ({pos.name}) - {len(ids)} senses =====\n")
    for i, sid in enumerate(ids, 1):
        s = wn(sid)
        print(f"Sense {i}: {sid}")
        print(f"  Definition:     {s.definition}")
        print(f"  Literals:       {s.literals}")
        print(f"  Literal senses: {s.literals_senses}")
        print("  Relations:")
        for target, rel in wn.outbound_relations(sid):
            print(f"    {rel:20} -> {target} [{lits(target)}]")
        path = wn.synset_to_hypernym_root(sid)
        if len(path) > 1:
            print("  Hypernym path to root:")
            print("    " + "\n    -> ".join(f"{p} [{lits(p)}]" for p in path))
        print()
