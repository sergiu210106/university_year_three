# Lab 2: RoWordNet and CoRoLa word embeddings

## Files

| File | What it is |
|---|---|
| `rowordnet_lab.py` → `rowordnet_results.txt` | Part 1: senses and relations from RoWordNet |
| `embeddings.py` → `embeddings_results.txt` | Part 2: 10 nearest neighbours in 3 CoRoLa models |
| `data/` (not committed) | the 3 embedding zips, download links in `../requirements.txt` |

Run (from `nlp/labs/2/`):

```
../venv/bin/python rowordnet_lab.py > rowordnet_results.txt   # ~5 s
../venv/bin/python embeddings.py    > embeddings_results.txt  # ~1.5 min, loads ~600 MB of vectors
```

Inputs, the same in both parts: nouns **casă, carte, câine**, verbs **merge, citi, mânca**, adjectives **frumos, mare**.

---

## Theory in brief

**Word vs. sense.** A word can have several meanings (**polysemy**): *casă* = building / family / company / cash desk. Lexical resources model **senses**, while corpus statistics see only **strings**.

**WordNet** is a lexical database organised by meaning, not alphabetically:
- **Synset**: a set of synonyms that express one concept, plus a **gloss** (definition). A word with *n* senses belongs to *n* synsets.
- **Semantic relations** link synsets: hypernymy/hyponymy (is-a, e.g. *câine is-a canid*), meronymy/holonymy (part-of / member-of), entailment (verbs), similarity (adjectives).
- **Lexical relations** link individual words: antonymy (*mare – mic*) and derivation (*citi – cititor*).
- Hypernymy turns nouns and verbs into a **hierarchy** (a tree from specific up to general, e.g. *entitate*). Adjectives form **clusters** around antonym pairs instead.
- RoWordNet is the Romanian WordNet, aligned synset-by-synset to Princeton WordNet 3.0, so it can be used across languages.

**Distributional hypothesis** ("you shall know a word by the company it keeps", Firth): words that occur in similar contexts have similar meanings.

**Word embeddings** are dense vectors (here 300 numbers per word), learned from a corpus so that words with similar contexts get similar vectors:
- **word2vec** (skip-gram / CBOW) trains a tiny neural network to predict context words from a word, or the reverse. The learned weights are the vectors.
- **fastText**, used for CoRoLa, does the same but also models character n-grams. That helps a morphologically rich language like Romanian.

**Cosine similarity**: cos(a, b) = a·b / (‖a‖·‖b‖), the cosine of the angle between the two vectors. It is 1 for the same direction and 0 for unrelated vectors. The **nearest neighbours** of a word are the words with the highest cosine to it.

**Word vs. lemma vs. lemma+POS.** Romanian has many inflected forms (*carte, cartea, cărți, cărțile, cărții…*):
- **word form** gives one vector per form, so counts are split and neighbours are often just other forms.
- **lemma** groups all forms under the dictionary form (lemmatisation), which gives more data per vector and neighbours that are more semantic.
- **lemma+POS** also attaches the part of speech, so homographs are separated (*Af_mare* "big" vs. *Nc_mare* "sea").

---

## Part 1: RoWordNet

**What RoWordNet is:** the Romanian WordNet built at RACAI. It is a graph of **synsets** (sets of synonyms that share one meaning) linked by semantic relations. It is aligned to Princeton WordNet 3.0, which is why the synset ids look like `ENG30-02084071-n` (PWN offset + POS). It is used via the `rowordnet` Python package, which ships the data.

**What is printed for every sense:**
- **definition**: the gloss of the synset
- **literals**: the words in the synset (its synonyms), e.g. `['casă', 'familie']`
- **literal senses**: for each literal, its sense number in the Romanian explanatory dictionary (DEX) tree, e.g. `1.10.2`. `x` means no sense number is assigned.
- **relations**: every outbound relation, with the literals of the target synset
- **hypernym path**: the chain of "is-a" links up to the top of the hierarchy

**Sense counts:** casă 12, carte 7, câine 2, merge 21, citi 6, mânca 10, frumos 6, mare 17 (81 in total).

**Relations that appear in the output:**

| Relation | Meaning | Example from the results |
|---|---|---|
| hypernym / hyponym | is-a (more general / more specific) | câine → canid; câine → cățeluș |
| instance_hyponym | a specific named instance | |
| member_holonym / member_meronym | is a member of / has as member | câine → haită, Canis |
| part_holonym / part_meronym | is a part of / has as part | |
| near_antonym | opposite | frumos ↔ urât, mare ↔ mic |
| similar_to | adjective clusters (adjectives have no hypernyms) | frumos → frumos (satellite) |
| verb_group | verb senses with related meanings | |
| entailment | doing X implies doing Y | |
| near_eng_derivat | morphologically derived word (imported from PWN) | citi → cititor |
| attribute, also_see | noun–adjective attribute link, loose "see also" link | |

**Good examples to show:**
- `câine` sense 1: two hypernyms (canid, animal_domestic) plus holonyms. Its path climbs canid → carnivor → mamifer → vertebrat → animal → organism → … → obiect.
- `casă` sense 1 ("familie"): casă → unitate → organizație → grup_social → … → entitate. The *same word* sits in a totally different branch from the building sense, which shows why senses matter.
- `frumos` / `mare`: adjectives have no is-a hierarchy and are organised by `similar_to` and `near_antonym` instead.

**Implementation notes:**
- `wn.synsets(literal=w, pos=..., strict=True)`. Without `strict`, the lookup is a substring match: "casă" gives 61 synsets ("casă de bani", "casă regală", …) instead of 12.
- `wn.synset_to_hypernym_root(id)` follows one hypernym at each step. When a synset has several (câine), only one chain is shown, but all hypernyms appear in the relations list.
- Some target synsets print as `no Romanian literal`: they exist in the graph (aligned from PWN) but have not been translated yet.
- Some paths stop before `entitate` (e.g. at `obiect`) because the Romanian graph is missing the link above that point.

### `rowordnet_lab.py` line by line

| Line | Code | Explanation |
|---|---|---|
| 1–3 | `warnings.filterwarnings("ignore")` | rowordnet imports the deprecated `pkg_resources`, which prints a warning. This silences it so the results file stays clean. Placed before the import on purpose. |
| 5 | `import rowordnet as rwn` | the RoWordNet library (its data comes bundled) |
| 6 | `from rowordnet import Synset` | needed for the `Synset.Pos` enum (NOUN / VERB / ADJECTIVE) |
| 8–17 | `words = [...]` | the 8 inputs as (literal, part of speech) pairs: 3 nouns, 3 verbs, 2 adjectives |
| 19 | `wn = rwn.RoWordNet()` | loads the whole wordnet graph into memory, about 60k synsets |
| 22–24 | `def lits(synset_id)` | helper: `wn(id)` returns the Synset object for an id, and its literals are joined into one string. Prints `no Romanian literal` when the synset has none. |
| 27 | `for word, pos in words:` | loop over the 8 inputs |
| 29 | `wn.synsets(literal=word, pos=pos, strict=True)` | ids of all synsets that contain this exact literal with this POS, i.e. **all senses of the word**. `strict=True` means exact match rather than substring. `dict.fromkeys` drops duplicate ids and keeps their order. |
| 30 | `print(f"===== ...")` | header line: word, POS and number of senses |
| 31 | `for i, sid in enumerate(ids, 1):` | loop over the senses, numbered from 1 |
| 32 | `s = wn(sid)` | gets the Synset object |
| 33–36 | `s.definition`, `s.literals`, `s.literals_senses` | the 3 required fields: gloss, synonyms and the DEX sense number of each synonym |
| 37–39 | `wn.outbound_relations(sid)` | list of (target id, relation name) for every relation that starts at this synset. Each is printed with the target's literals. |
| 40 | `wn.synset_to_hypernym_root(sid)` | list of ids from this synset up to the root, following hypernym links |
| 41–43 | `if len(path) > 1: ...` | prints the chain, one level per line. It is skipped when there is no hypernym, as for adjectives and top-level verbs. |
| 44 | `print()` | blank line between senses |

---

## Part 2: CoRoLa word embeddings

**What they are:** CoRoLa is the reference corpus of contemporary Romanian, built by RACAI and the Iași Institute of Computer Science. RACAI publishes word vectors trained on it with fastText, using **300 dimensions** and **min. frequency 20** (the recommended configuration). They come in 3 variants:

| Model | Vocabulary | Token example | Query used |
|---|---|---|---|
| word form | 250,942 | `cărțile` | `carte` |
| lemma | 186,202 | `carte` | `carte` |
| lemma+POS | 190,653 | `Nc_carte` | `Nc_carte`, `Vm_citi`, `Af_mare` |

POS tags are MSD prefixes: `Nc` = common noun, `Vm` = main verb, `Af` = qualificative adjective, `Rg` = adverb, `Cc` = coordinating conjunction, `Sp` = preposition.

**Method:** load each `.vec` file into a matrix and L2-normalise every row. Then **cosine similarity** = the dot product between the query vector and all rows. Sort descending, drop the query itself and keep the top 10. The score is in [-1, 1], and higher means the words occur in more similar contexts.

**Observations to discuss (with numbers from the results):**
1. **The word-form model is dominated by inflections.** carte → cartea 0.78, cărțile 0.68, cărți 0.66; câine → câinele 0.83, câini 0.74. Every inflected form has its own vector, and the forms of one word share contexts.
2. **The lemma model gives more semantic neighbours.** citi → reciti, răsfoi, lectura, frunzări; frumos → minunat, încântător, superb, splendid. Some noise comes from lemmatisation errors and typos in the corpus (frumosă, mâncace, mânânca).
3. **Lemma+POS separates parts of speech.** Af_mare (big) is a different entry from Nc_mare (sea). Vm_merge → pleca, întoarce, plimba, umbla, alerga, so the neighbours are almost all motion verbs.
4. **Antonyms are near neighbours.** Af_mare → Af_mic 0.75 is the top neighbour, and mare → mică 0.71 in the word model. Antonyms appear in the same contexts, so distributional similarity ≠ synonymy. WordNet marks them explicitly as `near_antonym`.
5. **Corpus bias / dominant sense.** casă in the lemma and lemma+POS models → sănătate, asigurare, i.e. "Casa (Națională) de Asigurări de Sănătate" from administrative texts. Only the word-form model shows the "home" sense (grădină, odaie, bucătărie, vilă). One vector per string mixes all senses, weighted by frequency.

**Implementation notes:**
- gensim does not build on Python 3.14, so `embeddings.py` reads the `.vec` text format itself with numpy, straight from the zip (about 15 lines in `load` and `nearest`).
- `rowordnet` imports `pkg_resources`, which was removed in setuptools 81, hence `setuptools<81` in `requirements.txt`.

### `embeddings.py` line by line

| Line | Code | Explanation |
|---|---|---|
| 1–2 | `import os`, `import zipfile` | file paths, and reading the `.vec` file directly from the zip without unpacking it |
| 4 | `import numpy as np` | matrix operations |
| 7–11 | `words = [...]` | the 8 inputs as (lemma, MSD prefix). The prefix is used only for the lemma+POS model. |
| 14 | `BASE = os.path.dirname(...)` | folder of the script, so the script works from any working directory |
| 15–19 | `MODELS = {...}` | the 3 models: display name → zip path |
| 22 | `def load(path):` | reads one model and returns (word→row index, word list, matrix) |
| 24 | `zipfile.ZipFile(...)`, `z.open(z.namelist()[0])` | opens the zip and the single `.vec` file inside it as a byte stream |
| 25 | `n, dim = map(int, f.readline().split())` | the first line of the `.vec` format is the header: `<number of words> <dimensions>`, e.g. `250942 300` |
| 26 | `vocab, vecs = [], np.zeros((n, dim), ...)` | empty word list, plus a preallocated n × 300 `float32` matrix (about 300 MB for the word model) |
| 27 | `for i, line in enumerate(f):` | every following line is `<word> <300 numbers>` |
| 28 | `line.decode("utf-8", errors="replace").rstrip().split(" ")` | bytes → text, then split on spaces. `errors="replace"` stops the script from crashing on rare broken bytes in the corpus. |
| 29 | `vocab.append(" ".join(parts[:-dim]))` | everything before the last 300 fields is the word. This handles entries that contain spaces. |
| 30 | `vecs[i] = parts[-dim:]` | the last 300 fields are the vector. numpy converts the strings to floats. |
| 31 | `vecs /= np.linalg.norm(vecs, axis=1, keepdims=True) + 1e-9` | divides every row by its length (**L2 normalisation**), so a dot product equals the cosine. `1e-9` avoids dividing by zero. |
| 32 | `return {w: i ...}, vocab, vecs` | the dict gives O(1) lookup from word to row |
| 35 | `def nearest(index, vocab, vecs, query, k=10):` | top k neighbours of `query` |
| 36 | `sims = vecs @ vecs[index[query]]` | matrix × vector gives the cosine of the query with **every** word at once (n scores) |
| 37 | `best = np.argsort(-sims)[: k + 1]` | indices sorted by descending similarity. It takes k+1 because the query itself is first, with cosine 1. |
| 38 | `[(vocab[i], sims[i]) for i in best if vocab[i] != query][:k]` | drops the query and returns (neighbour, score) pairs |
| 41–43 | `for name, path in MODELS.items(): ...` | loads each model in turn and prints a header with the vocabulary size |
| 44–45 | `query = f"{msd}_{word}" if name == "lemma+POS" else word` | for lemma+POS the key is `Nc_casă`, `Vm_merge`, `Af_frumos`, otherwise the plain word |
| 47–49 | `if query not in index: ...` | safety check for words that are missing from a model |
| 50–51 | `for neighbour, score in nearest(...)` | prints the 10 neighbours with the score to 4 decimal places |

---

## WordNet vs. embeddings (likely question)

| | RoWordNet | CoRoLa embeddings |
|---|---|---|
| Built by | lexicographers (manual) | trained automatically on a corpus |
| Unit | sense (synset) | string (word / lemma / lemma+POS) |
| Senses | separate: casă has 12 | mixed into one vector, dominated by the most frequent |
| Relations | typed: hypernym, antonym, meronym… | a single untyped similarity score |
| Antonyms | explicit `near_antonym` | appear as *similar* (mare ~ mic) |
| Coverage | limited, some synsets untranslated | 190k–250k entries, including typos and noise |
