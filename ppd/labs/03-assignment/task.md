# Laboratory 3: MPI Large Number Addition
**Deadline:** Week 7

Write an MPI-based program in C++11 that computes the sum of two large numbers (numbers with more than 10 digits).

---

## 1. General Considerations

* **Number Representation:** Represented as an array of digits (`unsigned char` / byte), where index `0` holds the least significant digit (LSD).
  * *Example:* For the number `123`, the least significant digit is `3`, so `t[0] = 3`, `t[1] = 2`, `t[2] = 1`.
* **Input Files:** The two large numbers are read from files:
  * `Numar1.txt`: contains $N_1$ digits.
  * `Numar2.txt`: contains $N_2$ digits.
  * *File Format:* Each file starts with an integer $N$ representing the count of digits, followed by the digits themselves.
* **Output File:** The final sum is written by Process 0 to `Numar3.txt`.
* **Implementation Language:** C++11.
* **Execution Timing:** For all variants, include the time spent reading the numbers from files in the total execution time measurement.
* **Validation:** Verify correctness by comparing the result file (`Numar3.txt`) from each parallel run against the sequential implementation output across all test cases.

---

## 2. Implementations & Variants

### Variant 0 - Sequential Implementation
A single-threaded C++ program that reads `Numar1.txt` and `Numar2.txt`, computes the sum sequentially with carry propagation, and outputs to `Numar3.txt`. Used as a baseline for performance and correctness verification.

---

### Variant 1 - Standard Point-to-Point Communication

> **Deadlock Prevention Requirement:** Regardless of the MPI implementation (OpenMPI, MPICH, MS-MPI), the program must be deadlock-free. Assume standard `MPI_Send` is completely unbuffered (blocking until receive completes). To test correctness, replace standard `MPI_Send` calls with synchronous `MPI_Ssend`.

#### Execution Steps:
1. **Process 0 (I/O & Master Process):**
   * Process 0 does **not** perform digit additions or read all digits at once at the start.
   * Sets `id_proces_curent = 1`.
   * Loops until all digits are read:
     1. Reads $N / (p - 1)$ digits from both `Numar1.txt` and `Numar2.txt`.
     2. Sends the digit chunks to process `id_proces_curent`.
     3. Increments `id_proces_curent`.
2. **Worker Processes ($id > 0$):**
   * **Receive Order Constraint:** Must first receive their assigned digit chunks, and *then* receive the carry from process $(id - 1)$.
   * Compute the element-wise sum and local carry.
   * Process $id = 1$ assumes an initial carry of `0`.
3. **Carry Propagation:**
   * Each worker process (except the last process) sends its computed carry to process $(id + 1)$.
   * Process $(id + 1)$ uses this carry to update its result sum.
4. **Result Collection:**
   * Worker processes ($id > 0$) send their computed sum digit segments to Process 0.
   * Process 0 receives sum digit segments sequentially from worker processes ($id > 0$) and writes each segment directly to `Numar3.txt` upon receipt (without waiting to gather all segments first).

---

#### Bonus: Variant 1.1 - Optimization (+2 Bonus Points)
* **Concept:** Can the addition of digits in a process ($id > 1$) begin *before* waiting for the carry from $(id - 1)$?
* **Task:** Implement this speculative addition optimization, analyze its requirements/trade-offs, and provide a comparative performance analysis against Variant 1.

---

#### Test Cases for Variant 1 & 1.1:
1. $N_1 = N_2 = 16$; `Numar1` = `"9999 4444 4444 9999"`, `Numar2` = `"9999 5555 5555 9999"` with **5 processes**.
2. $N_1 = 10000$ and $N_2 = 10000$ (random digits) with **5, 9, 17 processes**.
3. $N_1 = 100$ and $N_2 = 100000$ (random digits) with **5, 9, 17 processes**.

---

### Variant 2 - Collective Communication (`MPI_Scatter` / `MPI_Gather`)

In this variant, Process 0 participates in digit addition alongside worker processes.

#### Execution Steps:
1. **Process 0:** Reads both numbers completely into two arrays. If one number has fewer digits, pad it with non-significant zeroes (`0`).
2. **Scatter Digits:** 
   * Distribute digits across all processes ($0$ to $p-1$) using `MPI_Scatter`.
   * If $N = \max(N_1, N_2)$ is not divisible by $p$ ($p \nmid N$), increase $N$ to the next multiple of $p$ and pad the arrays with zeroes.
3. **Computation & Carry Propagation:**
   * All processes (including Process 0) sum their assigned digit slice.
   * Send the computed carry to process $(id + 1)$ (except process $p - 1$).
4. **Gather Results:**
   * Collect the partial sums back to Process 0 using `MPI_Gather`.
5. **Output:** Process 0 writes the final result to `Numar3.txt`.

---

#### Test Cases for Variant 2:
1. $N_1 = N_2 = 16$; `Numar1` = `"9999 4444 4444 9999"`, `Numar2` = `"9999 5555 5555 9999"` with **4 processes**.
2. $N_1 = 1000$ and $N_2 = 1000$ (random digits) with **4, 8, 16 processes**.
3. $N_1 = 100$ and $N_2 = 100000$ (random digits) with **4, 8, 16 processes**.

---

### Variant 3 - Non-Blocking Communication (`MPI_Isend` / `MPI_Irecv`)

#### Execution Steps:
1. **Process 0 (Master Process):**
   * Sets `id_proces_curent = 1`.
   * Does not perform additions or read all digits upfront.
   * Iteratively reads $N / p$ digits from `Numar1.txt` and `Numar2.txt` and sends them asynchronously to process `id_proces_curent` using `MPI_Isend`.
2. **Worker Processes ($id > 0$):**
   * Receive digit chunks from Process 0 and carry from Process $(id - 1)$.
   * **Constraint:** The arrival order between Process 0's digits and Process $(id - 1)$'s carry is non-deterministic; you **must use `MPI_Irecv`** to handle asynchronous receives.
   * Compute addition, update carry, and forward the carry to process $(id + 1)$.
3. **Asynchronous Aggregation:**
   * Worker processes send partial sums back to Process 0 asynchronously.
   * Process 0 aggregates the results from all worker processes using non-blocking/asynchronous operations.
4. **Output:** Process 0 writes the aggregated result to `Numar3.txt`.

---

#### Test Cases for Variant 3:
1. $N_1 = N_2 = 16$; `Numar1` = `"9999 4444 4444 9999"`, `Numar2` = `"9999 5555 5555 9999"` with **5 processes**.
2. $N_1 = 10000$ and $N_2 = 10000$ (random digits) with **5, 9, 17 processes**.
3. $N_1 = 100$ and $N_2 = 100000$ (random digits) with **5, 9, 17 processes**.