# Related Work: Hidden Markov Models for Part-of-Speech Tagging

Four core papers, one per role in the report: theory, the tagger we implement, how to train it, and how it compares with modern taggers.

---

## 1. Rabiner (1989): *A Tutorial on Hidden Markov Models and Selected Applications in Speech Recognition*
- **Venue:** Proceedings of the IEEE, 77(2), pp. 257–286. DOI: 10.1109/5.18626
- **Links:** [IEEE Xplore](https://ieeexplore.ieee.org/document/18626) · [Free PDF (TAMU)](https://people.engr.tamu.edu/rgutier/web_courses/cpsc689_s07/rabiner1989tutorialHMM.pdf)
- **Description:** The standard HMM tutorial. It defines the model λ = (A, B, π) and the three fundamental problems: evaluation (Forward), decoding (Viterbi) and learning (Baum–Welch). It also covers numerical scaling.
- **Why it is relevant:** This is the theoretical basis and notation for our report. The scaling section is why we implement the model in log-space.

## 2. Brants (2000): *TnT – A Statistical Part-of-Speech Tagger*
- **Venue:** ANLP 2000
- **Links:** [ACL Anthology A00-1031](https://aclanthology.org/A00-1031/) · [arXiv cs/0003055](https://arxiv.org/abs/cs/0003055)
- **Description:** A trigram HMM tagger with deleted linear interpolation of transition probabilities, suffix-based guessing for unknown words, and capitalisation features. It reaches about 96.7% on the Penn Treebank, competitive with Maximum Entropy taggers of the time.
- **Why it is relevant:** **The main reference for our implementation.** Every component is simple enough to re-implement and explain, and NLTK's `nltk.tag.tnt` gives us a sanity check.

## 3. Merialdo (1994): *Tagging English Text with a Probabilistic Model*
- **Venue:** Computational Linguistics, 20(2), pp. 155–171
- **Link:** [ACL Anthology J94-2001](https://aclanthology.org/J94-2001/)
- **Description:** Compares supervised estimation by counting on tagged text with Baum–Welch (EM) training on untagged text. EM usually *degrades* accuracy unless very little tagged text is available.
- **Why it is relevant:** It justifies training our HMM supervised, and it gives an optional experiment: run Baum–Welch from a small tagged sample and watch accuracy drop.

## 4. Plank, Søgaard & Goldberg (2016): *Multilingual Part-of-Speech Tagging with Bidirectional LSTM Models and Auxiliary Loss*
- **Venue:** ACL 2016 (short)
- **Links:** [ACL Anthology P16-2067](https://aclanthology.org/P16-2067/) · [arXiv 1604.05529](https://arxiv.org/abs/1604.05529)
- **Description:** BiLSTM taggers on 22 Universal Dependencies languages, compared with TnT (an HMM) and a CRF across training-set sizes.
- **Why it is relevant:** It places our HMM against neural taggers on the same UD data we plan to use, and shows HMMs stay competitive when training data is small.

---

## How They Fit Together

- **Theory (Rabiner):** formal HMM with tags as hidden states and words as observations; the two independence assumptions; Forward, Viterbi and Baum–Welch.
- **Implementation (Brants):** supervised trigram HMM with interpolated transitions, suffix-based unknown-word model, log-space Viterbi.
- **Training choice (Merialdo):** count-based estimation; Baum–Welch only as an optional experiment.
- **Evaluation and positioning (Plank et al.):** train and test on UD English-EWT (UPOS tags, standard splits), optionally UD Romanian-RRT. Report overall, known-word and unknown-word accuracy, plus ablations (bigram vs. trigram, smoothing, unknown-word model, training-set size).

**Questions for the professor:**
1. Supervised HMM only, or also unsupervised (Baum–Welch)?
2. English only, or also Romanian?
3. Is a discriminative comparison (perceptron/CRF) wanted?
