# L2 Utils: Synchronization Choices

## Selected variant: 1b

1. Each worker copies the original rows it needs into private auxiliary vectors.
2. Every worker reaches a synchronization barrier.
3. Workers update their assigned rows in place, using the saved originals where necessary.

No worker may start overwriting the image before every worker has finished copying. The implementation uses C++20 `std::barrier` and Java `CyclicBarrier`, once per execution. The sequential algorithm is unchanged.

## Other variants supplied with the lab

| Variant | Work before publishing readiness | Synchronization | Work afterwards |
|---|---|---|---|
| 1b | Copy original boundary neighbours | Global barrier | Update assigned image rows |
| 1f | Copy original boundary neighbours | Atomic readiness flags; check neighbours | Update assigned image rows |
| 2b | Compute outputs; defer boundary writes in vectors | Global barrier | Copy buffered boundary outputs into the image |
| 2f | Compute boundary outputs into vectors, then publish readiness | Atomic flags; compute interior, then check neighbours | Copy buffered boundary outputs into the image |

Flags start false and must be atomic. Finer flag synchronization can use one flag per neighbouring thread. A barrier holds participants until all have arrived.

For a fixed 3×3 stencil and row-based distribution, the variants can all use O(pM) auxiliary row storage. Their constants depend on which original rows or boundary outputs are buffered; deferred-output variants still need to preserve originals while computing in place. None requires a second image-sized matrix when implemented with row vectors.

A barrier waits for the slowest snapshot phase, whereas neighbour flags may let unrelated ranges proceed earlier. Flags require additional coordination and can waste CPU while spinning. Deferred-output variants require more boundary-specific computation logic. This remake implements only 1b; it does not claim measured performance results for the other variants.

Standard library references: [C++ barrier specification](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/n4950.pdf) and [Java 17 CyclicBarrier](https://docs.oracle.com/en/java/javase/17/docs/api/java.base/java/util/concurrent/CyclicBarrier.html).
