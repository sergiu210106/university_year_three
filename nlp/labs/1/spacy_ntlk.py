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

os.makedirs("trees", exist_ok=True)
all_triplets = []

def get_chunks(doc):
    try:
        return [c.text for c in doc.noun_chunks]
    except NotImplementedError:
        chunks = []
        for t in doc:
            if t.pos_ in ("NOUN", "PROPN") and t.dep_ not in ("det", "amod", "flat", "flat:name"):
                ids = [t.i] + [c.i for c in t.children if c.dep_ in ("det", "amod", "flat", "flat:name")]
                chunks.append(doc[min(ids):max(ids) + 1].text)
        return chunks


def process(lang, sentences, nlp, stemmer, f):
    for i, sentence in enumerate(sentences, start=1):
        doc = nlp(sentence)
        print("\nSentence:", sentence, file=f)

        print("Tokens:", [t.text for t in doc], file=f)

        print(f"{'TOKEN':<15}{'STEM':<13}{'LEMMA':<15}{'POS':<7}{'DEP':<11}HEAD", file=f)
        for t in doc:
            print(f"{t.text:<15}{stemmer.stem(t.text):<13}{t.lemma_:<15}{t.pos_:<7}{t.dep_:<11}{t.head.text}", file=f)

        svg = displacy.render(doc, style="dep")
        path = f"trees/{lang}_{i}.svg"
        with open(path, "w", encoding="utf-8") as img:
            img.write(svg)
        print("Dependency tree:", path, file=f)

        triplets = [(t.head.text, t.dep_, t.text) for t in doc]
        print("Triplets:", triplets, file=f)
        all_triplets.append({"lang": lang, "sentence": sentence, "triplets": triplets})

        print("Chunks:", get_chunks(doc), file=f)
        print("Entities:", [(e.text, e.label_) for e in doc.ents], file=f)


with open("results.txt", "w", encoding="utf-8") as f:
    print("===== ENGLISH =====", file=f)
    process("en", english, spacy.load("en_core_web_sm"), SnowballStemmer("english"), f)

    print("\n===== ROMANIAN =====", file=f)
    process("ro", romanian, spacy.load("ro_core_news_sm"), SnowballStemmer("romanian"), f)

with open("triplets.json", "w", encoding="utf-8") as f:
    json.dump(all_triplets, f, ensure_ascii=False, indent=2)
