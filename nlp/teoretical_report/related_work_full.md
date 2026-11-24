# Related Work: Hidden Markov Models for Part-of-Speech Tagging

This file collects the papers and references we can build the theoretical report on. They are grouped by role:

1. HMM theory and algorithms
2. Classic HMM-based POS taggers
3. Unknown words, smoothing and higher-order models
4. Unsupervised HMM training (EM and Bayesian)
5. Discriminative successors of the HMM (to compare against)
6. Neural baselines (state of the art, for context)
7. Corpora and tagsets

Each entry gives the reference, links, a short description and why it matters for our project.

---

## 1. HMM Theory and Algorithms

### 1.1 Rabiner (1989): *A Tutorial on Hidden Markov Models and Selected Applications in Speech Recognition*
- **Venue:** Proceedings of the IEEE, 77(2), pp. 257–286. DOI: 10.1109/5.18626
- **Links:** [IEEE Xplore](https://ieeexplore.ieee.org/document/18626) · [Free PDF (TAMU)](https://people.engr.tamu.edu/rgutier/web_courses/cpsc689_s07/rabiner1989tutorialHMM.pdf)
- **Description:** The standard tutorial on HMMs. It defines the model λ = (A, B, π) and the **three fundamental problems**: evaluation (Forward algorithm), decoding (Viterbi) and learning (Baum–Welch / Forward–Backward). It also covers practical problems such as numerical scaling and initialisation.
- **Why it is relevant:** This is the main theoretical reference for the formal part of our report. Our notation and our algorithm descriptions should follow it. The section on scaling explains why we need to compute in log-space when we implement the model.

### 1.2 Viterbi (1967): *Error Bounds for Convolutional Codes and an Asymptotically Optimum Decoding Algorithm*
- **Venue:** IEEE Transactions on Information Theory, 13(2), pp. 260–269
- **Links:** [IEEE Xplore](https://ieeexplore.ieee.org/abstract/document/1054010) · [Forney, *The Viterbi Algorithm: A Personal History* (arXiv)](https://arxiv.org/abs/cs/0504020)
- **Description:** The paper that first presented the dynamic-programming algorithm now named after Viterbi. It was introduced to decode convolutional codes, and it was later recognised as an exact shortest-path search through a trellis.
- **Why it is relevant:** Viterbi decoding is how an HMM tagger picks the most probable tag sequence. We should cite the original source, and Forney's history is a good short read for a "historical background" paragraph.

### 1.3 Jurafsky & Martin: *Speech and Language Processing* (3rd ed. draft), Appendix A "Hidden Markov Models" and the sequence-labelling chapter
- **Links:** [Appendix A (HMMs)](https://web.stanford.edu/~jurafsky/slp3/A.pdf) · [Book homepage](https://web.stanford.edu/~jurafsky/slp3/) · [Older chapter 8: Sequence Labeling for POS and NER](https://web.stanford.edu/~jurafsky/slp3/old_oct19/8.pdf)
- **Description:** A textbook account of HMMs (built on Eisner's ice-cream example) and of HMM POS tagging: estimating transitions and emissions by counting, Viterbi decoding, trigram HMMs, unknown-word handling and evaluation.
- **Why it is relevant:** This is the clearest NLP-oriented treatment. It connects Rabiner's general theory to POS tagging, and it is a good model for the structure of our report and for worked examples.

### 1.4 Collins: *Tagging Problems, and Hidden Markov Models* (Columbia course notes)
- **Link:** [PDF](https://www.cs.columbia.edu/~mcollins/hmms-spring2013.pdf)
- **Description:** Rigorous lecture notes covering generative tagging models, trigram HMMs, maximum-likelihood estimation with linear interpolation, the `_RARE_` / pseudo-word method for unknown words, and Viterbi for trigram HMMs with full pseudocode.
- **Why it is relevant:** These are the most implementation-ready derivations available. We can follow them almost step by step for the trigram Viterbi and the smoothing.

---

## 2. Classic HMM-Based POS Taggers

### 2.1 Church (1988): *A Stochastic Parts Program and Noun Phrase Parser for Unrestricted Text*
- **Venue:** ANLP 1988, pp. 136–143
- **Links:** [ACL Anthology A88-1019](https://aclanthology.org/A88-1019/) · [PDF](https://www.cis.upenn.edu/~danroth/Teaching/CS598-05/Papers/Church-pos88.pdf)
- **Description:** One of the first successful statistical POS taggers. It uses lexical and contextual (n-gram tag) probabilities estimated from the tagged Brown corpus, and a linear-time dynamic-programming search.
- **Why it is relevant:** It is the historical starting point of statistical tagging. It shows that a simple Markov model over tags beats hand-written rules, which is the motivation for our report.

### 2.2 Cutting, Kupiec, Pedersen & Sibun (1992): *A Practical Part-of-Speech Tagger* (Xerox tagger)
- **Venue:** ANLP 1992
- **Links:** [ACL Anthology A92-1018](https://aclanthology.org/A92-1018/) · [PDF mirror](https://khoury.northeastern.edu/home/futrelle/bionlp/papers/cutting-postagger-anlp92.pdf)
- **Description:** An HMM tagger that needs only a lexicon and **untagged** text, trained with Baum–Welch. It introduces *ambiguity classes* (words grouped by their set of possible tags) to reduce the number of parameters.
- **Why it is relevant:** It is the canonical example of applying all three of Rabiner's problems to POS tagging. It also gives a ready-made design idea (ambiguity classes) for when labelled data is scarce.

### 2.3 Brants (2000): *TnT – A Statistical Part-of-Speech Tagger*
- **Venue:** ANLP 2000
- **Links:** [ACL Anthology A00-1031](https://aclanthology.org/A00-1031/) · [arXiv cs/0003055](https://arxiv.org/abs/cs/0003055)
- **Description:** A second-order (trigram) HMM tagger with:
  - **deleted linear interpolation** of unigram, bigram and trigram transition probabilities
  - **suffix-based guessing** of tags for unknown words
  - capitalisation features
  - beam search

  It reached about 96.7% on the Penn Treebank and about 96.7% on NEGRA (German), and it was the fastest tagger in comparisons at the time.
- **Why it is relevant:** **This is the main reference for our implementation.** It shows that a well-engineered HMM is competitive with Maximum Entropy taggers, and every one of its components is simple enough to re-implement and explain in a theoretical report. NLTK ships an implementation (`nltk.tag.tnt`) that we can use as a reference or sanity check.

### 2.4 Thede & Harper (1999): *A Second-Order Hidden Markov Model for Part-of-Speech Tagging*
- **Venue:** ACL 1999, pp. 175–182
- **Link:** [ACL Anthology P99-1023](https://aclanthology.org/P99-1023/)
- **Description:** Extends the HMM to second order for **both** the transition probabilities (P(tᵢ | tᵢ₋₁, tᵢ₋₂)) and the emission probabilities (P(wᵢ | tᵢ, tᵢ₋₁)). It uses a smoothing scheme based on counts, and it handles unknown words with suffix and capitalisation statistics.
- **Why it is relevant:** It shows clearly what changes when we relax the HMM independence assumptions, which gives us a natural "bigram vs. trigram vs. trigram plus context-dependent emissions" comparison.

---

## 3. Unknown Words, Smoothing and Training Data

### 3.1 Weischedel, Meteer, Schwartz, Ramshaw & Palmucci (1993): *Coping with Ambiguity and Unknown Words through Probabilistic Models*
- **Venue:** Computational Linguistics, 19(2), pp. 359–382
- **Links:** [ACL Anthology J93-2006](https://aclanthology.org/J93-2006/) · [PDF](https://aclanthology.org/J93-2006.pdf)
- **Description:** A probabilistic (HMM-style) tagging study at BBN. It models the emission probability of an unknown word from features such as capitalisation, hyphenation and inflectional endings, and it studies how much training data is needed.
- **Why it is relevant:** Unknown (out-of-vocabulary) words are the main weakness of HMM taggers. This paper gives a principled way to estimate P(w | t) for unseen words, and we can compare it with TnT's suffix tries.

### 3.2 Merialdo (1994): *Tagging English Text with a Probabilistic Model*
- **Venue:** Computational Linguistics, 20(2), pp. 155–171
- **Link:** [ACL Anthology J94-2001](https://aclanthology.org/J94-2001/)
- **Description:** Compares supervised estimation by relative frequencies from tagged text with maximum-likelihood (Baum–Welch) training on untagged text. **Main finding:** EM training usually *degrades* tagging accuracy unless only a very small amount of tagged text is available. The best results come from using as much tagged text as possible.
- **Why it is relevant:** This is a key empirical result for the report. It justifies training our HMM **supervised** (by counting) and treating Baum–Welch as a secondary experiment.

---

## 4. Unsupervised HMM Training: EM vs. Bayesian Methods

### 4.1 Johnson (2007): *Why Doesn't EM Find Good HMM POS-Taggers?*
- **Venue:** EMNLP-CoNLL 2007, pp. 296–305
- **Link:** [ACL Anthology D07-1031](https://aclanthology.org/D07-1031/)
- **Description:** Shows that EM tends to spread words evenly across hidden states, while real tag distributions are very skewed. It compares EM with Gibbs sampling and Variational Bayes, which converge to better taggers when given sparse priors.
- **Why it is relevant:** It explains the theory behind Merialdo's negative result. It is a strong discussion point in a theoretical report on the limits of maximum-likelihood learning.

### 4.2 Goldwater & Griffiths (2007): *A Fully Bayesian Approach to Unsupervised Part-of-Speech Tagging*
- **Venue:** ACL 2007, pp. 744–751
- **Links:** [ACL Anthology P07-1094](https://aclanthology.org/P07-1094/) · [PDF](https://sites.socsci.uci.edu/~lpearl/courses/readings/GoldwaterGriffiths2007_BHMM.pdf)
- **Description:** The Bayesian HMM (BHMM). It is a trigram HMM with Dirichlet priors that integrates over the parameters (with Gibbs sampling) instead of using point estimates. It improves on MLE/EM by up to 14 percentage points, especially when data is sparse.
- **Why it is relevant:** It is the main modern extension of the HMM itself, and it shows how priors fix EM's problems. It is optional for the implementation but valuable for the "extensions" section.

---

## 5. Discriminative Successors of the HMM (for Comparison)

### 5.1 Ratnaparkhi (1996): *A Maximum Entropy Model for Part-of-Speech Tagging*
- **Venue:** EMNLP 1996
- **Links:** [ACL Anthology W96-0213](https://aclanthology.org/W96-0213/) · [PDF](https://www.seas.upenn.edu/~danroth/Teaching/CS598-05/Papers/Ratna-POS.pdf)
- **Description:** A tagger that models P(t | context) with a log-linear model over many overlapping features (word, prefixes and suffixes, neighbouring tags). It reaches 96.6% on the WSJ.
- **Why it is relevant:** It is the main contemporary competitor that TnT is compared against. It shows the move from *generative* P(w, t) to *conditional* P(t | w) modelling.

### 5.2 Lafferty, McCallum & Pereira (2001): *Conditional Random Fields: Probabilistic Models for Segmenting and Labeling Sequence Data*
- **Venue:** ICML 2001, pp. 282–289
- **Links:** [Penn repository](https://repository.upenn.edu/cis_papers/159) · [PDF](https://home.cs.colorado.edu/~mozer/Teaching/syllabi/6622/papers/LaffertyMcCallumPereira2001.pdf)
- **Description:** Introduces linear-chain CRFs, a globally normalised discriminative counterpart of the HMM that avoids the *label bias* problem of MEMMs. It includes POS experiments comparing HMMs, MEMMs and CRFs.
- **Why it is relevant:** It is the clearest theoretical contrast with the HMM: the same chain structure and the same Viterbi decoding, but discriminative training with arbitrary features. It is ideal for a "generative vs. discriminative" discussion.

### 5.3 Collins (2002): *Discriminative Training Methods for Hidden Markov Models: Theory and Experiments with Perceptron Algorithms*
- **Venue:** EMNLP 2002 (Best Paper; NAACL 2018 Test-of-Time Award)
- **Links:** [ACL Anthology W02-1001](https://aclanthology.org/W02-1001/) · [PDF](https://people.csail.mit.edu/mcollins/papers/tagperc.pdf)
- **Description:** Trains the HMM's scores with the structured (averaged) perceptron. Each step decodes a sentence with Viterbi and makes additive updates. It comes with a convergence proof, and it beats a MaxEnt tagger on POS tagging.
- **Why it is relevant:** It is the smallest step from our HMM to a discriminative model, because the Viterbi code can be reused unchanged. It is an easy and convincing extension if the professor wants a comparison.

### 5.4 Toutanova, Klein, Manning & Singer (2003): *Feature-Rich Part-of-Speech Tagging with a Cyclic Dependency Network*
- **Venue:** HLT-NAACL 2003, pp. 252–259
- **Links:** [ACL Anthology N03-1033](https://aclanthology.org/N03-1033/) · [PDF](https://nlp.stanford.edu/kristina/papers/tagging.pdf)
- **Description:** The Stanford POS tagger. It uses both left and right tag context and rich lexical features, and it reaches 97.24% on the WSJ.
- **Why it is relevant:** It is the classical (pre-neural) upper bound we can quote. It illustrates what the HMM's left-to-right, local-emission assumptions cost.

---

## 6. Neural Baselines (State of the Art, for Context)

### 6.1 Huang, Xu & Yu (2015): *Bidirectional LSTM-CRF Models for Sequence Tagging*
- **Link:** [arXiv 1508.01991](https://arxiv.org/abs/1508.01991)
- **Description:** A BiLSTM encoder with a CRF output layer. It reaches state-of-the-art or near state-of-the-art results on POS tagging, chunking and NER.
- **Why it is relevant:** The CRF layer is still a Markov chain over tags decoded with Viterbi, so the HMM ideas are still present inside modern neural taggers. It is a good bridge for the conclusion of the report.

### 6.2 Lample et al. (2016): *Neural Architectures for Named Entity Recognition*
- **Venue:** NAACL-HLT 2016
- **Links:** [arXiv 1603.01360](https://arxiv.org/abs/1603.01360) · [ACL Anthology N16-1030](https://aclanthology.org/N16-1030/)
- **Description:** BiLSTM-CRF with character-level word representations, which handle unknown words without hand-made features.
- **Why it is relevant:** It shows how the HMM's unknown-word problem (Section 3) is solved in neural models. It is optional context for the discussion.

### 6.3 Plank, Søgaard & Goldberg (2016): *Multilingual Part-of-Speech Tagging with Bidirectional LSTM Models and Auxiliary Loss*
- **Venue:** ACL 2016 (short)
- **Links:** [ACL Anthology P16-2067](https://aclanthology.org/P16-2067/) · [arXiv 1604.05529](https://arxiv.org/abs/1604.05529)
- **Description:** BiLSTM taggers evaluated on 22 Universal Dependencies languages and compared with traditional taggers, **including TnT (an HMM)** and a CRF, across different training-set sizes.
- **Why it is relevant:** It directly compares an HMM tagger and neural taggers on UD data, across languages and data sizes. It is the best source for honestly positioning our HMM against modern methods, and it shows that HMMs stay competitive when data is small.

---

## 7. Corpora and Tagsets

### 7.1 Marcus, Santorini & Marcinkiewicz (1993): *Building a Large Annotated Corpus of English: The Penn Treebank*
- **Venue:** Computational Linguistics, 19(2), pp. 313–330
- **Link:** [ACL Anthology J93-2004](https://aclanthology.org/J93-2004/)
- **Description:** Describes the Penn Treebank and its 45-tag POS tagset, which was designed to be small because of data-sparsity concerns in stochastic models.
- **Why it is relevant:** It is the standard English benchmark (WSJ), and almost every paper above reports on it. A 10% sample is freely available in NLTK (`nltk.corpus.treebank`).

### 7.2 Petrov, Das & McDonald (2012): *A Universal Part-of-Speech Tagset*
- **Venue:** LREC 2012
- **Links:** [ACL Anthology L12-1115](https://aclanthology.org/L12-1115/) · [arXiv 1104.2086](https://arxiv.org/abs/1104.2086)
- **Description:** Proposes 12 universal coarse POS categories and mappings from 25 treebank tagsets. It is the basis of the UPOS tagset (17 tags) used by Universal Dependencies.
- **Why it is relevant:** A small, language-independent tagset makes the HMM smaller (fewer states, so fewer transition parameters) and lets us run the same code on several languages.

### 7.3 Universal Dependencies: UD_Romanian-RRT (RoRefTrees) and UD_English-EWT
- **Links:** [UD Romanian RRT](https://universaldependencies.org/treebanks/ro_rrt) · [UD for Romanian](https://universaldependencies.org/ro/index.html) · [UD project](https://universaldependencies.org/)
- **Description:** Freely licensed (CC BY-SA) treebanks in CoNLL-U format with UPOS and XPOS (MSD) tags and fixed train/dev/test splits.
- **Why it is relevant:** These are freely available training and evaluation data with standard splits, so our results are comparable with published numbers (for example Plank et al. 2016). Romanian is morphologically rich, which makes it an interesting test of the HMM's unknown-word handling.

### 7.4 Tufiș (1999) and Tufiș & Dragomirescu (2004): *Tiered Tagging* / *Tiered Tagging Revisited*
- **Links:** [Tiered Tagging Revisited, LREC 2004 (L04-1158)](https://aclanthology.org/L04-1158/) · [Boroș, Ion & Tufiș, ACL 2013 (P13-1068)](https://aclanthology.org/P13-1068/)
- **Description:** A method for languages with very large tagsets (Romanian has more than 600 MSD tags). It first tags with a smaller, automatically derived tagset (CTAG), which works well for statistical taggers, and then maps the result back to full MSDs using a lexicon and rules.
- **Why it is relevant:** If we tag Romanian with detailed tags, a plain HMM suffers badly from data sparsity. Tiered tagging is the established fix, and it is local research we can discuss with the professor.
  - **Note:** the sources we found do not say explicitly which statistical tagger the original tiered-tagging setup used. Check the 1999/2000 papers before claiming it was an HMM.

---

## Conclusion: How to Combine These Works in Our Project

We recommend structuring the project as **one core HMM tagger and a few controlled extensions**. Each extension is backed by a specific paper, so the theoretical report and the implementation support each other.

**1. Theory section (Rabiner 1989, Viterbi 1967, Jurafsky & Martin App. A, Collins notes).**
Define the HMM formally with tags as hidden states and words as observations. State the two independence assumptions (Markov on tags, output independence on words) and present the three problems:
- Forward for evaluation
- Viterbi for decoding
- Baum–Welch for learning

Use Rabiner's notation throughout. Briefly place the work historically with Church (1988) and Cutting et al. (1992).

**2. Core implementation: a TnT-style supervised trigram HMM (Brants 2000, Collins notes, Thede & Harper 1999).**
- Estimate transitions and emissions by **counting** from a tagged corpus. Merialdo (1994) shows that this is better than EM when tagged data exists.
- Smooth the trigram transitions with **deleted linear interpolation** (TnT).
- Handle unknown words with **suffix-based emission estimates** plus capitalisation (TnT, Weischedel et al. 1993), and compare with Collins' simpler `_RARE_` pseudo-word classes.
- Decode with trigram Viterbi in **log-space** (Rabiner's scaling discussion).

**3. Data and evaluation (Penn Treebank / UD, Petrov et al. 2012).**
- Train and evaluate on **UD English-EWT** with UPOS tags (free, standard splits). Optionally add **UD Romanian-RRT** to show behaviour on a morphologically rich language.
- Report overall accuracy, **known-word vs. unknown-word accuracy** and a confusion matrix.
- Use NLTK's TnT implementation as a sanity check for our numbers.

**4. Ablations, which are cheap and very informative for a theoretical report:**
- bigram vs. trigram HMM
- no smoothing vs. interpolation
- naive vs. suffix-based unknown-word model
- learning curve over training-set size (connects to Merialdo and Plank et al.)

**5. Optional extension A, the limits of unsupervised learning (Cutting 1992, Merialdo 1994, Johnson 2007, Goldwater & Griffiths 2007).**
Implement Baum–Welch, initialise it from a small tagged sample, and show empirically that accuracy degrades with more EM iterations (Merialdo's result). Explain the cause with Johnson's analysis, and mention the Bayesian HMM as the principled fix.

**6. Optional extension B, generative vs. discriminative (Collins 2002, Lafferty et al. 2001, Ratnaparkhi 1996, Toutanova et al. 2003).**
Reuse the same Viterbi decoder and train it with the **averaged structured perceptron** using simple features (suffixes, capitalisation). This is the smallest change that demonstrates the discriminative gain. CRFs are discussed theoretically as the globally normalised version.

**7. Positioning and conclusion (Huang et al. 2015, Lample et al. 2016, Plank et al. 2016).**
Quote neural results as the upper bound. Then argue that the HMM is still valuable:
- it is interpretable
- it trains in seconds
- it is competitive on small data
- its Markov-chain-plus-Viterbi structure is still the output layer of modern BiLSTM-CRF taggers

**Proposed split for discussion with the professor:**
- Student A: theory (Sections 1, 4 and 5) and Baum–Welch / extension A
- Student B: the supervised TnT-style tagger, unknown-word models, data pipeline and evaluation (Sections 2, 3 and 7)
- Both: ablations and the comparison write-up

**Questions to ask the professor:**
1. Should the focus be supervised HMM tagging only, or also unsupervised (Baum–Welch)?
2. English only, or also Romanian?
3. Is a discriminative comparison (perceptron/CRF) wanted, or should we keep to generative models?
