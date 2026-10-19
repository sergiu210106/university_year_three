import os
import json

import spacy
from spacy import displacy
from nltk.stem.snowball import SnowballStemmer

english = [
    "Apple is looking at buying a U.K. startup for $1 billion.",
    "Barack Obama was born in Hawaii and served as the 44th President of the United States.",
    "The quick brown fox jumps over the lazy dog near the river.",
    "Google opened a new office in Zurich in 2019.",
    "Marie Curie won the Nobel Prize in Physics in 1903.",
    "The students are studying natural language processing at Babes-Bolyai University.",
    "Elon Musk announced that Tesla will build a factory in Berlin.",
    "She quickly finished her homework before the football match started.",
    "The European Union headquarters are located in Brussels.",
    "Researchers at MIT have developed a faster algorithm for image recognition.",
]

romanian = [
    "Universitatea Babeș-Bolyai este situată în Cluj-Napoca.",
    "Mihai Eminescu a fost cel mai important poet român.",
    "Studenții învață procesarea limbajului natural la laborator.",
    "Bucureștiul este capitala României și cel mai mare oraș din țară.",
    "Ion Creangă a scris povestea Amintiri din copilărie.",
    "Pisica neagră doarme liniștită pe canapeaua veche.",
    "Compania Microsoft a deschis un birou nou la Timișoara în 2020.",
    "Copiii se joacă în parc împreună cu prietenii lor.",
    "Președintele a vizitat Parisul în luna martie.",
    "Am citit o carte interesantă despre istoria Transilvaniei.",
]

BASE = os.path.dirname(os.path.abspath(__file__))
os.makedirs(os.path.join(BASE, "trees"), exist_ok=True)
all_triplets = []

NOUNS = ("NOUN", "PROPN")
CHUNK_MODS = ("det", "det:poss", "amod", "advmod", "nummod", "flat", "flat:name", "fixed")


def in_chunk(t):
    # nmod without a preposition (genitive / name apposition) stays inside the NP:
    # "capitala României", "Universitatea Babeș-Bolyai"; "din țară" does not
    if t.dep_ == "nmod":
        return not any(c.dep_ == "case" for c in t.children)
    return t.dep_ in CHUNK_MODS


def chunk_ids(t):
    # recurse so modifiers of modifiers are kept: "cel mai important poet român"
    ids = [t.i]
    for c in t.children:
        if in_chunk(c):
            ids += chunk_ids(c)
    return ids


def get_chunks(doc):
    try:
        return [c.text for c in doc.noun_chunks]
    except NotImplementedError:
        # no noun_chunks iterator for Romanian in spaCy, build base NPs from the parse
        chunks = []
        for t in doc:
            if t.pos_ in NOUNS and not (in_chunk(t) and t.head.pos_ in NOUNS and t.head != t):
                ids = chunk_ids(t)
                chunks.append(doc[min(ids):max(ids) + 1].text)
        return chunks


def head_of(t):
    return "ROOT" if t.dep_ == "ROOT" else t.head.text


def stem(stemmer, t):
    return stemmer.stem(t.text) if any(ch.isalpha() for ch in t.text) else t.text


def process(lang, sentences, nlp, stemmer, f):
    for i, sentence in enumerate(sentences, start=1):
        doc = nlp(sentence)
        print("\nSentence:", sentence, file=f)

        print("Tokens:", [t.text for t in doc], file=f)

        print(f"{'TOKEN':<15}{'STEM':<15}{'LEMMA':<15}{'POS':<7}{'DEP':<11}HEAD", file=f)
        for t in doc:
            print(f"{t.text:<15}{stem(stemmer, t):<15}{t.lemma_:<15}{t.tag_:<7}{t.dep_:<11}{head_of(t)}", file=f)

        svg = displacy.render(doc, style="dep")
        path = f"trees/{lang}_{i}.svg"
        with open(os.path.join(BASE, path), "w", encoding="utf-8") as img:
            img.write(svg)
        print("Dependency tree:", path, file=f)

        triplets = [(head_of(t), t.dep_, t.text) for t in doc]
        print("Triplets:", triplets, file=f)
        all_triplets.append({"lang": lang, "sentence": sentence, "triplets": triplets})

        print("Chunks:", get_chunks(doc), file=f)
        print("Entities:", [(e.text, e.label_) for e in doc.ents], file=f)


with open(os.path.join(BASE, "results.txt"), "w", encoding="utf-8") as f:
    print("===== ENGLISH =====", file=f)
    process("en", english, spacy.load("en_core_web_sm"), SnowballStemmer("english"), f)

    print("\n===== ROMANIAN =====", file=f)
    process("ro", romanian, spacy.load("ro_core_news_sm"), SnowballStemmer("romanian"), f)

with open(os.path.join(BASE, "triplets.json"), "w", encoding="utf-8") as f:
    json.dump(all_triplets, f, ensure_ascii=False, indent=2)
