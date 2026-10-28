import os
import zipfile

import numpy as np

# 3 nouns, 3 verbs, 2 adjectives with their MSD prefix in the lemma+POS model
words = [
    ("casă", "Nc"), ("carte", "Nc"), ("câine", "Nc"),
    ("merge", "Vm"), ("citi", "Vm"), ("mânca", "Vm"),
    ("frumos", "Af"), ("mare", "Af"),
]

# CoRoLa embeddings, 300 dims, min freq 20 (http://89.38.230.23/word_embeddings*/we/corola.300.20.vec.zip)
BASE = os.path.dirname(os.path.abspath(__file__))
MODELS = {
    "word": "data/word.vec.zip",
    "lemma": "data/lemma.vec.zip",
    "lemma+POS": "data/lemma_pos.vec.zip",
}


def load(path):
    # gensim does not build on Python 3.14, so read the .vec text format by hand
    with zipfile.ZipFile(os.path.join(BASE, path)) as z, z.open(z.namelist()[0]) as f:
        n, dim = map(int, f.readline().split())
        vocab, vecs = [], np.zeros((n, dim), dtype=np.float32)
        for i, line in enumerate(f):
            parts = line.decode("utf-8", errors="replace").rstrip().split(" ")
            vocab.append(" ".join(parts[:-dim]))
            vecs[i] = parts[-dim:]
    vecs /= np.linalg.norm(vecs, axis=1, keepdims=True) + 1e-9
    return {w: i for i, w in enumerate(vocab)}, vocab, vecs


def nearest(index, vocab, vecs, query, k=10):
    sims = vecs @ vecs[index[query]]
    best = np.argsort(-sims)[: k + 1]
    return [(vocab[i], sims[i]) for i in best if vocab[i] != query][:k]


for name, path in MODELS.items():
    index, vocab, vecs = load(path)
    print(f"===== {name} embeddings ({len(vocab)} entries) =====\n")
    for word, msd in words:
        query = f"{msd}_{word}" if name == "lemma+POS" else word
        print(f"{query}:")
        if query not in index:
            print("  not in vocabulary\n")
            continue
        for neighbour, score in nearest(index, vocab, vecs, query):
            print(f"  {neighbour:25} {score:.4f}")
        print()
