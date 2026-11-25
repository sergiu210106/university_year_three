# The Four Core Papers, Explained

Presentation notes for the four papers in `related_work.md`. Numbers and quotations were checked against the papers themselves. Points marked **Our comment** are our own interpretation, not claims made by the authors.

Contents:
1. Rabiner (1989): HMM theory
2. Brants (2000): TnT, the tagger we implement
3. Merialdo (1994): supervised counting vs. Baum–Welch
4. Plank, Søgaard & Goldberg (2016): HMM vs. neural taggers
5. How they fit together, one-sentence summaries, likely questions

---

## 1. Rabiner (1989): *A Tutorial on Hidden Markov Models and Selected Applications in Speech Recognition*

*Proceedings of the IEEE 77(2), 257–286*

### What it is
This is the standard reference on HMMs. It was written for speech recognition, but its notation (λ = (A, B, π), α, β, δ, γ, ξ) is what everyone uses, including the NLP textbooks. It is not a research paper with an experiment. It is a tutorial that:
- introduces HMMs with toy examples (coin tossing, balls in urns)
- poses and solves the **three basic problems**
- discusses model types and practical issues: scaling, initialisation, insufficient data, multiple sequences
- ends with isolated-word and connected-word speech recognisers

### The model
A discrete HMM has these elements:

| Symbol | Meaning | In POS tagging |
|---|---|---|
| N states `S₁…S_N` | hidden states | the tags (e.g. 17 UPOS tags) |
| M symbols `v₁…v_M` | what we observe | the words (the vocabulary) |
| `A = {a_ij}`, `a_ij = P(q_{t+1}=S_j \| q_t=S_i)` | transition probabilities | P(next tag \| current tag) |
| `B = {b_j(k)}`, `b_j(k) = P(O_t=v_k \| q_t=S_j)` | emission probabilities | P(word \| tag) |
| `π_i = P(q₁=S_i)` | initial state distribution | P(first tag of the sentence) |

It rests on two assumptions:
1. **Markov assumption:** the next state depends only on the current state.
2. **Output independence:** an observation depends only on the state that emitted it.

For tagging, these mean that a tag depends only on the previous tag (or two tags, in a trigram model), and a word depends only on its own tag.

### The three basic problems
Rabiner states them as follows.

1. **Evaluation:** given O and λ, compute `P(O | λ)`. Solved by the **Forward algorithm.**
2. **Decoding:** given O and λ, find the state sequence Q that best "explains" the observations. Solved by the **Viterbi algorithm.**
3. **Learning:** adjust λ to maximise `P(O | λ)`. Solved by **Baum–Welch (Forward–Backward).**

In tagging, problem 2 *is* the tagging task: the observations are the words and the hidden state sequence is the tag sequence. Problem 3 is training when you have no tags, which is what Merialdo tests.

### Problem 1: the Forward algorithm
Summing over every possible state sequence directly costs about `2T·N^T` operations. Rabiner's example: for N = 5 and T = 100 that is about 10⁷² computations. The forward algorithm reuses partial sums instead:

```
α₁(i)     = π_i · b_i(O₁)
α_{t+1}(j) = [ Σ_i α_t(i) · a_ij ] · b_j(O_{t+1})
P(O | λ)  = Σ_i α_T(i)
```

This costs about `N²T`, roughly 3000 computations for the same example. Rabiner calls it "a savings of about 69 orders of magnitude." The computation runs on a **trellis** (lattice): states on one axis, time on the other.

The backward variable `β_t(i) = P(O_{t+1} … O_T | q_t = S_i, λ)` is computed the same way, from right to left. It is not needed for problem 1, but it is needed for problem 3.

### Problem 2: two definitions of "optimal"
Rabiner points out that "optimal" can mean two different things:
- **The individually most likely state at each time step**, using `γ_t(i) = P(q_t = S_i | O, λ) = α_t(i)β_t(i) / P(O|λ)`. This maximises the expected number of correct individual states. The resulting sequence can be impossible, though: it can contain a transition with `a_ij = 0`.
- **The single best whole sequence**: the **Viterbi algorithm**.

```
δ₁(i)     = π_i · b_i(O₁)
δ_{t+1}(j) = max_i [ δ_t(i) · a_ij ] · b_j(O_{t+1})
ψ_{t+1}(j) = argmax_i [ δ_t(i) · a_ij ]      (back-pointer)
```

At the end, take the best final state and follow the ψ back-pointers backwards.

**Our comment:** these are exactly Merialdo's "ML tagging" and "Viterbi tagging" (paper 3). He found they give almost the same accuracy.

### Problem 3: Baum–Welch
Define the expected counts:

```
ξ_t(i,j) = α_t(i) · a_ij · b_j(O_{t+1}) · β_{t+1}(j) / P(O|λ)    expected i→j transition at time t
γ_t(i)   = Σ_j ξ_t(i,j)                                        expected time in state i at time t
```

Then re-estimate:

```
π̄_i    = γ₁(i)
ā_ij   = Σ_t ξ_t(i,j) / Σ_t γ_t(i)
b̄_j(k) = Σ_{t: O_t = v_k} γ_t(j) / Σ_t γ_t(j)
```

- Each iteration is guaranteed not to decrease `P(O | λ)`.
- It only reaches a **local** maximum, so initialisation matters.
- Rabiner notes that the Baum–Welch equations "are essentially identical to the EM steps" for this problem.

**Our comment, a good point to make:** when the tags *are* observed (supervised training), ξ and γ become 0/1 indicator counts. The re-estimation formulas then reduce to **counting relative frequencies**. So supervised training by counting is the special case of Baum–Welch where nothing is hidden.

### Practical sections we use
- **Scaling (Section V.A):** α_t(i) is a product of many numbers below 1, so it heads exponentially to zero. For long sequences (T ≈ 100 or more) it underflows even double precision.
  - For forward–backward, Rabiner multiplies each α_t by a scaling coefficient. With scaling you can compute log P(O|λ), but not P(O|λ) itself.
  - For **Viterbi, "no scaling is required if we use logarithms"**: products become sums of logs. **This is why we implement Viterbi in log-space.**
- **Initial estimates (Section V.C):** random or uniform starting values are usually fine for A and π. Good initial estimates of B help.
  - **Our comment:** this connects to Merialdo, where the starting point decides everything.
- **Insufficient training data (Section V.D):** the options are more data, a smaller model, or **interpolating** the estimates with those of a smaller, better-estimated model.
  - The weight ε is set on held-out data. Cycling the held-out part through the training set is called **deleted interpolation.** Rabiner notes it was used for trigram word probabilities in language models.
  - This is the technique TnT uses for its trigram transitions.
- **Model types (Section IV):** ergodic (fully connected) models vs. left-right (Bakis) models for speech.
  - **Our comment:** a tagging HMM is ergodic, because any tag can follow any tag.

### Why it matters for us
It gives the formal backbone of the report:
- the definitions and the two assumptions
- the three problems and their algorithms, with complexity
- the justification for log-space computation and for interpolation smoothing

**Our comment:** a trigram tagger (TnT) fits Rabiner's first-order framework if each state is a **pair of tags** (t_{i−1}, t_i). Viterbi then costs O(T·N³) instead of O(T·N²).

---

## 2. Brants (2000): *TnT – A Statistical Part-of-Speech Tagger*

*ANLP 2000, pp. 224–231*

### The claim
Some earlier literature said Markov-model taggers were worse than other approaches. Brants argues the opposite: "a tagger based on Markov models performs at least as well as other current approaches, including the Maximum Entropy framework."
- In an independent comparison of 7 taggers (Zavrel & Daelemans 1999), TnT had the highest accuracy *and* was the fastest to train and to tag.
- His explanation for the earlier bad results: the basic HMM equations "leave several decisions to the implementor." These details decide the result, and earlier papers left them out. Examples are sentence boundaries, the smoothing method, how the weights are set, and how unknown words are handled.
- Our report argues the same thing.

### The model
A second-order (trigram) HMM. It picks the tag sequence that maximises:

```
argmax_{t₁…t_T}  [ Π_{i=1..T}  P(t_i | t_{i−1}, t_{i−2}) · P(w_i | t_i) ] · P(t_{T+1} | t_T)
```

- t₋₁ and t₀ are beginning-of-sequence tags, and t_{T+1} is an end-of-sequence tag.
- Brants says modelling the end explicitly "slightly improves tagging results." Other formulations "just stop with a 'loose end' at the last word."

All probabilities start as maximum-likelihood estimates from a tagged corpus:
- unigram `f(t₃)/N`
- bigram `f(t₂,t₃)/f(t₂)`
- trigram `f(t₁,t₂,t₃)/f(t₁,t₂)`
- lexical `f(w₃,t₃)/f(t₃)`

### Component 1: smoothing transitions with linear interpolation

```
P(t₃ | t₁,t₂) = λ₁·P̂(t₃) + λ₂·P̂(t₃ | t₂) + λ₃·P̂(t₃ | t₁,t₂),     λ₁+λ₂+λ₃ = 1
```

- The λs are **context-independent**: one set for the whole model. Brants tried grouping trigrams by frequency with separate λs, and "all groupings that we have tested yielded at most equivalent results"; some were worse.
- The λs are set by **deleted interpolation** (his Figure 1):

```
λ₁ = λ₂ = λ₃ = 0
for each trigram (t₁,t₂,t₃) with f(t₁,t₂,t₃) > 0:
    compare   (f(t₁,t₂,t₃) − 1) / (f(t₁,t₂) − 1)    → trigram estimate
              (f(t₂,t₃) − 1)    / (f(t₂) − 1)       → bigram estimate
              (f(t₃) − 1)       / (N − 1)           → unigram estimate
    add f(t₁,t₂,t₃) to the λ of whichever estimate is largest
    (a fraction with a 0 denominator counts as 0)
normalise λ₁, λ₂, λ₃ to sum to 1
```

- **The "−1" is the key idea.** It removes the current occurrence from the counts, as if that trigram were unseen data (leave-one-out). Brants: without it "the model would overfit the training data and would generally yield worse results."
- It is cheap: one pass over the distinct trigrams.

### Component 2: unknown words through suffix analysis
This follows Samuelsson (1993). A word's ending predicts its tag: in the WSJ, words ending in *-able* are adjectives in 98% of cases (*fashionable*) and nouns in the other 2% (*cable*).

- Tag probabilities given the last i letters are built up from the shortest suffix to the longest ("successive abstraction"):

```
P(t | suffix of length i) = [ P̂(t | suffix of length i) + θ_i · P(t | suffix of length i−1) ] / (1 + θ_i)
starting from P(t | empty suffix) = P̂(t)
```

- The HMM needs `P(suffix | t)`, so the result is converted with **Bayes' rule** (Brants calls this "Bayesian inversion").
- TnT's specific choices:
  1. **Maximum suffix length m:** the longest suffix of the word that occurs in training, but at most 10 characters.
  2. **θ:** one value for all i, the **standard deviation of the unconditioned tag probabilities** P̂(t). This is usually 0.03–0.10.
  3. **Two suffix tries,** one for capitalised and one for lowercase words.
  4. **Only rare words feed the suffix statistics** (frequency ≤ 10). The reasoning is that unknown words behave like rare words, not like frequent ones.

### Component 3: capitalisation
- A flag c_i (capitalised or not) is added to the context, so the model uses `P(t₃, c₃ | t₁, c₁, t₂, c₂)`.
- This "is equivalent to doubling the size of the tagset."
- It helps more for English, where capitals mostly mark proper names, than for German, where all nouns are capitalised.

### Component 4: beam search
- Inside Viterbi, a state is dropped if its δ is smaller than the best δ divided by a threshold θ.
- With θ = 1000, the tagger runs about twice as fast with no loss of accuracy.
- It tags 30,000–60,000 tokens per second on a Pentium 500.
- **Our comment:** beam search is optional for us, since the UPOS tagset is small.

### Evaluation setup
- 90% train / 10% test, repeated 10 times and averaged.
- The test sets are **contiguous** (whole unseen articles), not every 10th sentence. Brants argues this is more realistic, because the round-robin split puts parts of the same article in training and lowers the number of unknown words.
- He reports **overall, known-word and unknown-word accuracy** separately. Our evaluation plan copies this.

### Results

| Corpus | Size | Unknown tokens | Known acc. | Unknown acc. | Overall |
|---|---|---|---|---|---|
| NEGRA (German news) | 20,000 sent. / 355k tokens | 11.9% | 97.7% | 89.0% | **96.7%** |
| Penn Treebank WSJ | ~50,000 sent. / 1.2M tokens | 2.9% | 97.0% | 85.5% | **96.7%** |

- On the WSJ, Ratnaparkhi's Maximum Entropy tagger reports **96.6%** and TnT gets **96.7%**. Brants adds that the comparison "needs to be re-examined," because he averages 10 runs and Ratnaparkhi reports a single run.
- **Learning curves:** known words are tagged well even with very little data: "As few as 1000 tokens are sufficient to achieve 95%–96% accuracy for them." With 1,000 training tokens overall accuracy is only about 78% (78.1% NEGRA, 78.6% WSJ), because about half the test tokens are then unknown.
  - Conclusion: **unknown words are what limit an HMM tagger.**
- **Reliability:** the ratio between the probabilities of the best and second-best tag measures how confident the tagger is. On a high-confidence subset accuracy is **above 99%**. This is useful for flagging tags for human correction.

### Why it matters for us
**This is the blueprint for our implementation.** Every component is simple, explicitly specified and easy to explain:
- trigram transitions
- deleted interpolation with the −1 trick
- the suffix model with its four concrete choices
- capitalisation
- start and end tags

NLTK has an implementation (`nltk.tag.tnt`) we can use as a sanity check, and its known / unknown / overall breakdown is the evaluation we report.

---

## 3. Merialdo (1994): *Tagging English Text with a Probabilistic Model*

*Computational Linguistics 20(2), pp. 155–171*

### The question
There are two ways to train an HMM tagger:
- **Relative Frequency (RF):** count on hand-tagged text. This is supervised.
- **Maximum Likelihood (ML):** run Baum–Welch / Forward–Backward on **untagged** text. This is unsupervised.

Untagged text is cheap and tagged text is expensive. Which gives the better tagger? And can a little tagged data be improved with a lot of untagged data?

### Setup
- **Model:** a "triclass" model, which is a **trigram HMM**:
  - transitions `h(t_i | t_{i−2}, t_{i−1})`
  - emissions `k(w_i | t_i)`

  This is the same model we implement. He notes the number of parameters grows only *linearly* with the vocabulary.
- **Dictionary constraint:** each word may only get tags listed for it in a dictionary, so `k(w|t) = 0` otherwise.
- **Smoothing:** the RF estimates are interpolated with uniform distributions, with λ set by **deleted interpolation** on held-out data. The interpolation weight is itself estimated with Forward–Backward.
- **Data:** 42,186 Associated Press sentences (about 1M words), hand-tagged at Lancaster. The original 159 tags were mapped to **76**.
  - Training: 40,186 sentences
  - Test: 2,000 sentences (45,583 words)

### Experiment 1: supervised counting only (Table 1, Viterbi tagging)

| Tagged sentences | 0 | 100 | 2,000 | 5,000 | 10,000 | 20,000 | all |
|---|---|---|---|---|---|---|---|
| λ | 0 | .48 | .77 | .85 | .90 | .92 | .94 |
| Accuracy (%) | 77.0 | 90.0 | 95.4 | 96.2 | 96.6 | 96.9 | **97.0** |

- **With 0 sentences** the model is uniform, so each choice is random among the dictionary's tags. It still scores 77%, because "almost half of the words of the text have a single possible tag" and about a quarter have only two. **Good line for the presentation:** the dictionary alone does a lot of the work.
- **2,000 sentences** (under 50k words) already give less than 5% error. 10× more data adds only about 1.5 points.
- **λ grows with the amount of data:** the more data, the more the smoothed model trusts the counts.

### Experiment 2: Baum–Welch starting from those models (Table 2, the key result)
Each RF model is the starting point. Baum–Welch then runs on all 40k training sentences **with their tags removed.**

| Iteration | N = 0 | N = 100 | N = 2,000 | N = 5,000 | N = 10,000 | N = all |
|---|---|---|---|---|---|---|
| 0 | 77.0 | 90.0 | 95.4 | 96.2 | 96.6 | 97.0 |
| 1 | 80.5 | 92.6 | **95.8** | **96.3** | 96.6 | 96.8 |
| 2 | 81.8 | 93.0 | 95.7 | 96.1 | 96.3 | 96.4 |
| 3 | 83.0 | **93.1** | 95.4 | 95.8 | 96.1 | 96.2 |
| 5 | 84.8 | 92.9 | 95.1 | 95.4 | 95.6 | 95.8 |
| 10 | **86.6** | 92.6 | 94.4 | 94.8 | 95.0 | 95.2 |

(N = number of tagged sentences used for the initial model. The 20,000 column is left out; it behaves like "all".)

How to read it:
- **No tagged data (N = 0):** EM helps (77.0 → 86.6), but it still ends **below** a model counted from only 100 tagged sentences (90.0).
- **Little tagged data (N = 100):** EM helps for about 3 iterations (90.0 → 93.1), then accuracy starts to fall.
- **Moderate data (N = 2,000 or 5,000):** one iteration gives a tiny gain, then accuracy drops.
- **More than 5,000 tagged sentences:** "even the first iteration of ML training degrades the tagging." With all the data, accuracy falls steadily from 97.0 to 95.2.

### Why EM hurts (be ready for this question)
EM maximises **P(words)**, the likelihood of the text. It does not maximise tagging accuracy.
- Merialdo: ML training "is guaranteed to improve perplexity," but "perplexity is not necessarily related to tagging accuracy, and it is possible to improve one while degrading the other."
- Nothing ties the hidden states to *linguistic* tags. EM is free to reshape them into whatever best predicts the words.
- EM works well in speech recognition because the link between phonemes and the signal is fuzzy. The link between words and tags "is much more precise."
- **Our comment:** Johnson (2007), in our full list, later explains this more deeply. EM tends to spread words too evenly over the hidden states, while real tag distributions are very skewed.

### Side experiments (useful for questions)
- **Viterbi vs. per-word "ML tagging"** (Table 3): these are Rabiner's two definitions of optimal from Problem 2.
  - The difference is at most 16 errors out of 45,583 words, for example 1365 vs. 1362 errors at iteration 0.
  - Merialdo: "the choice of the tagging procedure is not as critical as the kind of training material."
  - This justifies using plain Viterbi.
- **Constrained EM** (Tables 4 and 5):
  - *tw-constraint:* freeze p(t|w) for the 1,000 most frequent words.
  - *t-constraint:* keep p(t) fixed. This was tested on a bigram model only.
  - Both slow the damage but do not stop it. With all data, after 10 iterations: 95.21% for standard EM vs. 96.09% with the tw-constraint, both below the 97.01% start.

### His recommended recipe
1. Get as much hand-tagged text as you can afford, and count to build an initial model M₀.
2. Optionally run Forward–Backward on untagged text, starting from M₀.
3. Check accuracy on held-out tagged text after each iteration, and **stop as soon as a model is worse than the previous one.**

### Caveat (say it yourself before the teacher does)
The dictionary was built from the **whole** corpus, test set included. So there are **no unknown words** in his experiments, and the numbers are optimistic. In our setup unknown words are a real problem, which is why we follow TnT's suffix model.

### Why it matters for us
- It justifies **supervised training by counting** as our main method.
- It gives an **optional experiment**: start from about 100 tagged sentences, run Baum–Welch on the rest without tags, and plot accuracy per iteration. We should see a rise and then a drop.

---

## 4. Plank, Søgaard & Goldberg (2016): *Multilingual Part-of-Speech Tagging with Bidirectional Long Short-Term Memory Models and Auxiliary Loss*

*ACL 2016 (short paper), pp. 412–418*

### The question
Neural taggers (BiLSTMs) were taking over, but most were evaluated on English WSJ with large training sets. The paper asks:
1. Which input representation is best: words, characters, or bytes?
2. Does that hold across **22 languages**?
3. Are neural taggers really more data-hungry and noise-sensitive than classic taggers such as **TnT (an HMM)**?

### Models
- **Baselines:**
  - **TnT** (Brants 2000): "a second order HMM with suffix trie handling for OOVs", essentially what we build.
  - A **CRF** tagger built on crfsuite.
- **BiLSTM variants.** A context BiLSTM reads the sentence in both directions and predicts a tag at each word. They vary its input:
  - **w:** word embeddings only.
  - **c:** characters only. A small BiLSTM reads each word's characters and produces one vector for the word.
  - **w + c:** the character vector is concatenated with the word embedding (hierarchical model).
  - **+Polyglot:** word embeddings initialised from pre-trained Polyglot vectors.
  - **FREQBIN (their new model):** a multi-task model.
- **How FREQBIN works:**
  - At each position the network predicts the POS tag *and* a frequency class `int(log(freq_train(w)))` for the next token.
  - The loss is the sum of the two cross-entropy losses.
  - Intuition: the representation is pushed to be "predictive for frequency," so rare and common words stop sharing representations. This helps rare and unseen words.
- **Training:** SGD with no mini-batches, 20 epochs, word embeddings of size 128, character embeddings of size 100.

### Data
- **Universal Dependencies v1.2:** 17 UPOS tags, canonical splits, and every language with at least 60k tokens (22 languages).
- Also **WSJ** (45 Penn Treebank tags, standard splits).

### Main results (Table 2, accuracy on UD 1.2 test sets)

| Tagger | Avg (22 langs) | English |
|---|---|---|
| **TnT (HMM)** | **94.61** | 92.66 |
| CRF | 94.27 | 93.35 |
| BiLSTM w | 92.37 | 92.10 |
| BiLSTM c | 94.29 | 91.62 |
| BiLSTM w + c | 96.08 | 94.61 |
| w + c + Polyglot | 96.50 | 95.17 |
| FREQBIN | 96.50 | 95.16 |

Talking points:
- **The HMM from 2000 has the best average among the baselines.** It beats the CRF and the word-only BiLSTM. The authors: TnT "performs remarkably well across the 22 languages, closely followed by CRF."
- The word-only BiLSTM beats the traditional taggers on only 3 languages. **The neural gain comes from characters.**
  - Characters alone beat TnT on 9 languages.
  - w + c beats TnT on every language except Indonesian, with gains above 2 points on Hebrew and Slovene.
  - The gains are largest for morphologically rich languages (Slavic, non-Indo-European), and "most of the improvement is obtained in the Zipfian tail", that is, on rare words.
- **Our comment:** so what modern taggers do better is handling **rare and unknown words**. This is the HMM's known weak spot, which TnT patches with suffix statistics.
- **FREQBIN** ties w + c + Polyglot on average (96.50). It is best on 12 of 22 languages and helps most on unknown words for Arabic, Farsi, Hebrew and Finnish.
  - The unknown-word accuracy table covers only the two Polyglot models (average 87.80 vs. 87.98). There is **no unknown-word figure for TnT**, so don't claim one.
- **English UD:** TnT gets **92.66**, much lower than its 96.7% on the WSJ.
  - **Our comment:** UD English is web text with more unseen words, so this is a realistic target for our own English results.
- **WSJ:** their BiLSTM w + c gets 97.22, close to other neural taggers (for example 97.36 for Ling et al. 2015).

### Training-set size (Figure 3)
- "TnT is better with little data, bi-LSTM is better with more data, and bi-LSTM always wins over CRF."
- The BiLSTM "performs already surprisingly well after only 500 training sentences." For non-Indo-European languages it is on par with the others even at 100 sentences.
- Their summary: BiLSTMs "often need more data than the generative markovian model, but this is definitely less than what we expected."
- The exact crossover point is only in the plot, not the text. Say "a few hundred sentences" or show the figure.

### Label noise
They randomly corrupt a percentage of the training tags:
- At low noise, the BiLSTM and TnT "are affected similarly."
- Above about **30% corrupted labels**, the BiLSTM is "less robust, showing higher drops in accuracy compared to TnT," in every language family tested.

### Their conclusions
- Sub-word (character) representations "are necessary to obtain a state-of-the-art POS tagger," especially for non-Indo-European and Slavic languages.
- The auxiliary loss improves accuracy on rare words.
- The BiLSTM is "as effective as the CRF and HMM taggers with already as little as 500 training sentences, but is less robust to label noise (at higher noise rates)."

### Caveats
- They use **UD v1.2**. We will use a newer UD release, so our numbers are only approximately comparable.
- **Romanian is not among their 22 languages**, so if we tag Romanian there is no reference number from this paper.

### Why it matters for us
- It gives **published HMM numbers on the same kind of data** (UD, UPOS). We can expect our English result to be in the low 90s, like TnT's 92.66.
- It supports our **learning-curve ablation**: the HMM is competitive or better with little training data.
- It frames the **unknown-word discussion**: modern taggers win mainly through better handling of rare words, which is exactly what TnT's suffix model targets.

---

## 5. How They Fit Together

```
Rabiner (1989)      ──►  what an HMM is; how to evaluate, decode, train it; log-space; interpolation
      │
      ▼
Brants (2000)       ──►  how to turn it into a strong POS tagger (trigram, deleted interpolation,
      │                  suffix model, capitalisation) — what we implement
      │
Merialdo (1994)     ──►  how to train it: count on tagged data; Baum–Welch hurts unless tags are scarce
      │
      ▼
Plank et al. (2016) ──►  where it stands today: still a strong baseline, best on small data;
                         neural taggers win mainly on rare/unknown words
```

### One-sentence summaries for the teacher
- **Rabiner:** "The formal HMM: λ = (A, B, π), two independence assumptions, and the three problems with their dynamic-programming solutions (Forward, Viterbi, Baum–Welch). We use its notation and its log-space advice."
- **Brants:** "A trigram HMM tagger that matches Maximum Entropy (96.7% vs. 96.6% on the WSJ) because the 'small' details are done right: deleted interpolation, suffix-based unknown words and capitalisation. It is our implementation blueprint."
- **Merialdo:** "Counting on tagged data beats Baum–Welch. EM optimises the likelihood of the words, not tagging accuracy, so it only helps when you have almost no tagged data. That is why we train supervised."
- **Plank et al.:** "On 22 UD languages, TnT is still a strong baseline: it beats a CRF, wins on small data and is more robust to heavy label noise. Neural taggers win mainly through character-level handling of rare words, the same problem our suffix model addresses."

### Questions the teacher might ask
- **Why an HMM and not a neural tagger?** It is interpretable, trains in seconds, needs no GPU and is competitive on small data (Plank et al.). Its chain-plus-Viterbi structure also survives inside BiLSTM-CRF taggers.
- **Why not train with Baum–Welch, since it needs no annotation?** Merialdo's Table 2: with enough tagged data it lowers accuracy. Likelihood ≠ accuracy.
- **What is the difference between Viterbi and choosing the best tag per word?** Viterbi gives the best whole sequence, while per-word posteriors maximise the expected number of correct tags (Rabiner, Problem 2). In practice the difference is negligible (Merialdo, Table 3).
- **Why log-space?** Products of many probabilities underflow. For Viterbi, logs remove the need for scaling (Rabiner, Section V.A).
- **How do you handle words never seen in training?** TnT's suffix model: P(tag | last ≤ 10 letters), learned from rare words and smoothed from short to long suffixes, plus separate handling of capitalised words.
- **Why does the trigram model need smoothing?** Most tag trigrams are rare or unseen, and a single zero makes the whole sentence probability zero. Interpolating with bigram and unigram estimates fixes this. The λs come from deleted interpolation with the −1 (leave-one-out) trick.
- **What accuracy should we expect?** About 96–97% on WSJ-style news (Brants), and the low 90s on UD English web text (TnT 92.66 in Plank et al.). Unknown-word accuracy will be much lower than known-word accuracy (85.5% vs. 97.0% on the WSJ).
