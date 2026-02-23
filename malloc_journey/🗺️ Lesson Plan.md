## Phase 0: Recon Before You Write Anything

> _"Before you build the map, walk the territory."_

The goal here is to _watch_ what malloc is already doing live on your system — no writing code yet.

**🔬 Experiment 0.1 — strace a real program:**

```bash
strace -e trace=brk,mmap,munmap ls 2>&1 | head -30
```

You'll see every `brk()` and `mmap()` call `ls` makes just to start up. Notice how many there are — that's glibc's malloc at work.

**🔬 Experiment 0.2 — Inspect your own process's memory map:** Write a tiny C program that calls `malloc(100)` and then does:

```c
system("cat /proc/self/maps");
```

You'll see the `[heap]` segment in the output. That's the region `brk()` controls.

**🔬 Experiment 0.3 — Watch the program break move:** Call `sbrk(0)` before and after a `malloc(1000)` and print the addresses. See the heap grow in real time.

**📖 Glibc Source Stop #1:** Open `malloc.c` in your browser: `https://codebrowser.dev/glibc/glibc/malloc/malloc.c.html` Search for the struct `malloc_chunk`. That's the real header glibc puts in front of every allocation — the equivalent of what you'll build.

---

## Phase 1: The Dumbest Malloc That Works

> _Concept: `sbrk()` as a linear bump allocator — no free._

You'll build a malloc that just shovels heap space forward every time it's called. No tracking, no free. It's "correct" for allocation-only programs.

**Key insight to figure out yourself:** What does `sbrk(0)` return vs `sbrk(n)`? How do you use them together to carve out exactly `n` bytes?

**🔬 Experiment 1.1 — Inject it into a real binary:** Compile your malloc as a shared library and inject it using `LD_PRELOAD`:

```bash
gcc -shared -fPIC -o mymalloc.so mymalloc.c
LD_PRELOAD=./mymalloc.so ls
```

If `ls` runs without segfaulting, you have a working (if terrible) malloc. This is the most satisfying moment of the whole project.

**✋ Blocker to think about:** What happens when `free()` is called on something from your bump allocator? Programs will crash. Why? And what does that tell you about what `free()` actually needs?

---

## Phase 2: The Metadata Header

> _Concept: Hiding a struct just before the pointer you return._

This is the core trick in all allocators. You request more bytes than the user asked for, use the extra space to store a `block_meta` struct, then return a pointer _just past_ that struct.

**Key insight to figure out yourself:** If your struct is `N` bytes large and the user wants `size` bytes, you call `sbrk(N + size)`. What pointer do you return? What pointer do you keep for yourself?

```
[  block_meta  |        user data        ]
^              ^
internal ptr   what malloc() returns
```

**🔬 Experiment 2.1 — Make your metadata visible:** Add a magic number field to your struct (e.g. `0xDEADBEEF`). After allocating, use `gdb` or just pointer arithmetic to walk backwards from the returned pointer and confirm the magic number is sitting there.

**⚠️ Alignment trap:** `malloc` must return memory aligned to at minimum 8 or 16 bytes, otherwise programs that store `double` or SIMD types will crash with bus errors. Think about this: if your struct size isn't a multiple of 16, what does that mean for the user pointer?

---

## Phase 3: The Free List

> _Concept: A linked list of all chunks, alive and free._

Instead of discarding blocks, you mark them as free and link them into a list. Next time `malloc()` is called, you search the list first.

**Key insight to figure out yourself:** Your `block_meta` struct needs three things: `size`, `is_free`, and `*next`. Think about _first-fit_ vs _best-fit_ — first-fit grabs the first free block big enough; best-fit searches for the tightest fit. Which is simpler? Which causes less fragmentation?

**🔬 Experiment 3.1 — Watch fragmentation happen:** Write a test that allocates 100 chunks of 32 bytes, frees all the even-numbered ones, then tries to allocate one chunk of 64 bytes. Print how many nodes your free list traverses. You'll feel the inefficiency.

**📖 Glibc Source Stop #2:** In `malloc.c`, search for `unsorted_chunks`. glibc starts freed chunks in an "unsorted bin" before categorizing them — your free list is a primitive version of this.

---

## Phase 4: Block Splitting

> _Concept: Don't waste half a 512-byte block for a 10-byte request._

When you find a free block that's bigger than needed, split it into two: one that satisfies the request, one that's a new free block.

**Key insight to figure out yourself:** After splitting, the new second block needs its own `block_meta`. Where do you put it? Given that you know the address of the original block and the requested size, you can calculate exactly where the second header should live.

**🔬 Experiment 4.1 — Visualize your heap:** Write a function `heap_dump()` that walks your linked list from `global_base` and prints each block's address, size, and free status. Run it before and after a split. You'll see the list gain a new node.

---

## Phase 5: Block Coalescing (Boundary Tags — Knuth's trick)

> _Concept: Merge adjacent free blocks to fight fragmentation._

This is where it gets elegant. When you free a block, you check if the block immediately before or after it is also free — and if so, merge them.

**Key insight to figure out yourself:** Finding the _next_ block is easy (current address + current size). Finding the _previous_ block requires storing a footer at the _end_ of each block that duplicates the header. This is called a **boundary tag**, and it's how dlmalloc (which glibc is derived from) works.

```
[ header | ... user data ... | footer ]
                                      ^
                   next block's header starts here
```

**📖 Glibc Source Stop #3:** In `malloc.c`, search for `PREV_INUSE` flag and `prev_size` field in `malloc_chunk`. glibc stores the previous chunk's size in the low bits of the current chunk's size — a bit-packing trick to save space.

**🔬 Experiment 5.1:** Allocate A, B, C. Free B, then free A. Without coalescing, you have two adjacent free blocks. With coalescing, they merge into one. Print your `heap_dump()` before and after to witness it.

---

## Phase 6: mmap for Large Allocations

> _Concept: Stop using `sbrk` for big chunks — use `mmap` instead._

glibc uses `mmap()` for allocations above ~128KB (the `MMAP_THRESHOLD`). These chunks live outside the heap, and freeing them with `munmap()` actually _returns memory to the OS_ — unlike `sbrk`-based chunks.

**🔬 Experiment 6.1 — Find the threshold on YOUR system:** Write a loop that malloc's increasing sizes (64KB, 128KB, 256KB...) and after each one, run `strace` to see whether `brk` or `mmap` was called. You'll find the exact threshold where glibc switches.

> On a typical modern system this switches around 16–128MB, but the `MMAP_THRESHOLD` is tunable via `mallopt()`.

---

## Phase 7: Bins — How glibc Gets Fast

> _Concept: Instead of one free list, use many lists sorted by size._

Real ptmalloc2 (glibc's allocator) uses multiple **bins**:

- **Fastbins** — tiny chunks (<= 80 bytes), never coalesced, super fast
- **Smallbins** — chunks < 512 bytes, exact-fit buckets
- **Largebins** — bigger chunks with range-based bucketing
- **Unsorted bin** — a staging area for newly freed chunks

You won't fully implement this, but understanding the concept is crucial — especially if you go into kernel development, because the kernel's SLAB/SLUB allocator is built on similar ideas.

**📖 Glibc Source Stop #4:** In `malloc.c`, search for `#define NBINS` and `fastbinsY`. Look at how the bin array is indexed — it's a beautiful hack where the index is derived directly from the chunk size.

---

## Phase 8 (Bonus): Thread Safety & Arenas

glibc's allocator uses **arenas** — multiple independent heaps for threads — protected by per-arena mutexes. This is why it's called `ptmalloc` (pthreads malloc). You don't need to implement this, but read about it if you want to understand why kernel memory allocators are architecturally very different (they're per-CPU, not per-thread).

---

## 🔗 Kernel Connection

Everything you learn here directly maps to kernel allocators:

|Userspace|Kernel Equivalent|
|---|---|
|`sbrk()` / `mmap()`|`get_free_pages()`|
|`malloc()` with free list|`kmalloc()` / SLUB allocator|
|Bins by size class|SLUB's per-size `kmem_cache`|
|Coalescing|Buddy allocator page merging|
|Arenas (per-thread)|Per-CPU caches in SLUB|

The glibc malloc code itself lives at `glibc/malloc/malloc.c` — the full readable version is at `codebrowser.dev/glibc/glibc/malloc/malloc.c.html`, and the original dlmalloc paper by Doug Lea is worth reading once you finish your toy version.

---

## 🗓️ Suggested Order

1. Phase 0 (recon experiments) → feel the shape of the problem
2. Phase 1 → working LD_PRELOAD moment (huge motivation spike)
3. Phase 2 → metadata + alignment
4. Phase 3 → free list + first-fit
5. Phase 4 → splitting
6. Phase 5 → coalescing + boundary tags ← most intellectually satisfying
7. Phase 6 → mmap threshold experiment
8. Phase 7 → read glibc bins, don't implement fully

**Whenever you're ready, just say "Phase N, go" and I'll give you clues for that phase — no full code, just the key insight unlocks and the experiments to run!** 🔧