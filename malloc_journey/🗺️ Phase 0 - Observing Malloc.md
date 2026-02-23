First, I'll build a mental model of what malloc does at the systems level by observing it's behavior.


## 0.1 : Using strace to see how kernel manages memory for `ls`

Command to run 
```
anu@laptop:~$ strace -e trace=brk,mmap,munmap ls 2>&1 | head -40
```

[[dump/strace mmap, brk, munmap|Actual code output]]

- there are 2 ways to give memory to a program
	- brk: extending the heap
	- mmap: maps all the shared library files `.so` (that ls depends on) 

```
── PHASE 1: Library Loading ──────────────────────────────────
brk(NULL)  = 0x555555579000          ← "where does my heap end?" (just a question, heap doesn't grow yet)
mmap(NULL, 8192,  ..., -1, 0)        ← anonymous memory for the loader itself
mmap(NULL, 81463, ...,  3, 0)        ← loading a .so file (fd=3 means a library file)
mmap(NULL, 181960,...,  3, 0)        ← loading another .so file
mmap(...,  118784,...,  3, 0x6000)   ← loading another .so file
... (10 more mmap lines, all fd=3) ...

── PHASE 2: Heap Actually Grows ──────────────────────────────
brk(NULL)            = 0x555555579000   ← asking again: "where is my heap?"
brk(0x55555559a000)  = 0x55555559a000   ← NOW the heap actually moves forward ✅

── PHASE 3: Last Big mmap (locale/directory data) ────────────
mmap(NULL, 5719296, PROT_READ, MAP_PRIVATE, 3, 0) = 0x7ffff7600000

── OUTPUT ────────────────────────────────────────────────────
anu
backup.log
Desktop
...

```


---


## 0.2 Watching the heap using a C program

This was my C program:
```
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>

int main()
{
        printf("Heap Pointer locataion:\nBefore:%p", sbrk(0));
        malloc(1000);

        printf("\nAfter:%p", sbrk(0));



        return 0;

}

```

and this is my output:
```
brk(NULL)                               = 0x555555559000
brk(NULL)                               = 0x555555559000
brk(0x55555557a000)                     = 0x55555557a000

Heap Pointer locataion:
Before:0x555555559000
After:0x55555557a000+++ exited with 0 +++

```


I asked for 1000 bytes using malloc however, The difference: 

`0x55555557a000 - 0x555555559000 = 0x21000` = **135,168 bytes ≈ 132 KB**

I got more than what I asked for! this is because glibc is very greedy. it 
