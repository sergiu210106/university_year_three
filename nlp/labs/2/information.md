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
