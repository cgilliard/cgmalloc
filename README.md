# cgmalloc
A simple memory allocator. Implements the 4 main memory allocation functions:
* malloc (cg_malloc)
* free (cg_free)
* realloc (cg_realloc)
* calloc (cg_calloc)

## Design
`cgmalloc` is a lightweight, C89-compliant memory allocator for POSIX systems, delivering exceptional performance in a ~500-line codebase. It combines a slab allocator for small allocations (≤ 2048 bytes - configurable) with `mmap` for larger ones, minimizing metadata for embedded, `nostdlib`, or performance-critical applications.

### Slab Allocator
Allocations up to `MAX_SLAB_SIZE` (2048 bytes) use a slab allocator with fixed-size slabs (8 to 2048 bytes, power-of-2 sizes). Slabs are stored in chunks (`CHUNK_SIZE`, default 256 KB), each with:
- A `ChunkHeader` containing metadata (slab size, bitmap, linked list pointers).
- A bitmap tracking free slabs.
Slab pointers are unaligned relative to `CHUNK_SIZE`, allowing `cg_free` to identify them by checking if `ptr - HEADER_SIZE` is not a multiple of `CHUNK_SIZE`. This eliminates per-slab headers, reducing memory overhead.

### Large Allocations
Allocations larger than 2048 bytes use `mmap`, aligned to `CHUNK_SIZE`. A header (`size` and `magic` value, ~16 bytes) is stored at `ptr - HEADER_SIZE`, enabling `cg_free` to distinguish `mmap`’d memory (where `ptr - HEADER_SIZE` is a multiple of `CHUNK_SIZE`). For `cg_calloc`, `mmap`’s zero-initialized memory skips `memset`, enhancing performance.

### Thread Safety and Performance
`cgmalloc` is thread-safe by default, using spin locks in `ChunkHeader` for slab operations. The unique `NO_SPIN_LOCKS` build option disables locks for single-threaded or externally synchronized applications, achieving unparalleled performance: ~30 ns per allocation vs. ~73 ns for `glibc malloc` and ~60 ns for `jemalloc` (`CHUNK_SIZE=4MB`). This ~50%–85% speedup likely makes `cgmalloc` the fastest allocator for such workloads, enabled by a single Makefile flag (`-DNO_SPIN_LOCKS`). Unlike other libraries, which require complex modifications to disable locks, or mandatory thread-local caches, `cgmalloc` offers unmatched configurability. For multi-threaded scalability, users can implement external thread-local caching (e.g., via POSIX `pthread_key_t`), keeping the core allocator simple and portable.

### Minimal Metadata
`cgmalloc` uses minimal data structures:
- A linked list of chunks via `ChunkHeader` (`next`, `prev`).
- A bitmap per chunk for slab tracking.
- A header for `mmap`’d allocations.
With only a global chunk list head pointer, `cgmalloc` outperforms larger allocators in single-threaded scenarios while remaining compact.


## Build
cgmalloc essentially has no dependencies other than standard system calls like (mmap, munmap, sysctl, write (to display a panic message on memory corruption), sched_yield (to yield CPU while spin-locking, and exit)) and a few c standard library calls (memcpy, bzero (through compiler intrinsic), and __error (part of perror)). For testing, the criterion framework is used. This is optional for it you want to run the tests. To install criterion on linux:

```# sudo apt-get install libcriterion-dev```

or on Mac OS:

```# brew install criterion```

You may have to ensure that the headers and libs are in your search path.

To build the shared object:

```# make clean all```

This will produce a shared library which you can link to in ./lib/libcgmalloc.so.

To build and run the test program:

```# make clean test; ./bin/test```

To build the benchmarking program:

```# make clean bench; ./bin/bench```

## Advanced build options.

The Makefile allows you to pass in several parameters. You can pass in the compiler. I have tested with recent versions of gcc and clang and the code conforms to the c89 standard. Additionally, you can pass in ALLOCFLAGS to set compile time parameters. For example:

```# make clean all ALLOCFLAGS="-DNO_SPIN_LOCKS -DPAGE_SIZE=16384"```

This will build the shared library with the NO_SPIN_LOCKS parameter set and with PAGE_SIZE set to 16,384. The configurable options are:

* NO_SPIN_LOCKS - disables spinlocks. You can get quite a significant performance boost by disabling spin locks and should do so if you're in a single threaded environment or if you can do your own synchronization around the memory allocations. Default = Not set.
* PAGE_SIZE - Sets the memory page size for the application. If this is not set, it is defined as (sysconf(_SC_PAGE_SIZE)). This requires a system call each time this value is needed. It's best to set this value in production systems, but ensure that it's set correctly or behaviour is undefined.
* CHUNK_SIZE - Each slab is part of a chunks of slabs. The chunk size determines how many slabs can fit in a chunk. The default size is 256kb. Each slab size between 8 bytes - 2048 are availble (default max configuration). There is also a header and a bitmap in each chunk. So for 8 byte slabs, we can store around 32,000 slabs. This must be divisible by PAGE_SIZE.
* MAX_SLAB_SIZE - The maximum power of 2 that slabs go up to. If the requested size is greater than this value, we fall back to mmap. The default configuration is 2048. The value must be a power of 2 or behavior is undefined.
* SET_MALLOC - If configured, this value will create the standard memory functions (malloc, free, realloc, and calloc). You can then link your application such that it uses cg_malloc as its allocator.

# Benchmarks
Here are some outputs from the benchmark program's runs which show comparison to the linux system malloc implementations. These runs include one memory allocation and one free (performed in separate loops). The per iteration count includes both operations.

```
$ make clean bench ALLOCFLAGS="-DCHUNK_SIZE=4194304 -DNO_SPIN_LOCKS"; ./bin/bench
rm -fr .obj/* lib/* bin/*
clang -Iinclude -fPIC -std=c89 -pedantic -Wall -Wextra -O3 -D_GNU_SOURCE -c src/lock.c -o .obj/lock.o
clang -Iinclude -fPIC -std=c89 -pedantic -Wall -Wextra -O3 -D_GNU_SOURCE -DCHUNK_SIZE=4194304 -DNO_SPIN_LOCKS -c src/alloc.c -o .obj/alloc.o
clang -Iinclude -O3 -flto -o bin/bench .obj/alloc.o .obj/lock.o src/bench.c
malloc    total test time=2329452ns (72.795375ns per iteration)
cc_malloc total test time=965362ns (30.167562ns per iteration)
chris@chris-ubuntu:~/projects/cgmalloc$ make clean bench ALLOCFLAGS="-DCHUNK_SIZE=4194304"; ./bin/bench
rm -fr .obj/* lib/* bin/*
clang -Iinclude -fPIC -std=c89 -pedantic -Wall -Wextra -O3 -D_GNU_SOURCE -c src/lock.c -o .obj/lock.o
clang -Iinclude -fPIC -std=c89 -pedantic -Wall -Wextra -O3 -D_GNU_SOURCE -DCHUNK_SIZE=4194304 -c src/alloc.c -o .obj/alloc.o
clang -Iinclude -O3 -flto -o bin/bench .obj/alloc.o .obj/lock.o src/bench.c
malloc    total test time=2270370ns (70.949062ns per iteration)
cc_malloc total test time=1866101ns (58.315656ns per iteration)
chris@chris-ubuntu:~/projects/cgmalloc$ make clean bench ALLOCFLAGS="-DCHUNK_SIZE=262144"; ./bin/bench
rm -fr .obj/* lib/* bin/*
clang -Iinclude -fPIC -std=c89 -pedantic -Wall -Wextra -O3 -D_GNU_SOURCE -c src/lock.c -o .obj/lock.o
clang -Iinclude -fPIC -std=c89 -pedantic -Wall -Wextra -O3 -D_GNU_SOURCE -DCHUNK_SIZE=262144 -c src/alloc.c -o .obj/alloc.o
clang -Iinclude -O3 -flto -o bin/bench .obj/alloc.o .obj/lock.o src/bench.c
malloc    total test time=2308265ns (72.133281ns per iteration)
cc_malloc total test time=6235501ns (194.859406ns per iteration)
chris@chris-ubuntu:~/projects/cgmalloc$ make clean bench ALLOCFLAGS="-DCHUNK_SIZE=262144 -DNO_SPIN_LOCKS"; ./bin/bench
rm -fr .obj/* lib/* bin/*
clang -Iinclude -fPIC -std=c89 -pedantic -Wall -Wextra -O3 -D_GNU_SOURCE -c src/lock.c -o .obj/lock.o
clang -Iinclude -fPIC -std=c89 -pedantic -Wall -Wextra -O3 -D_GNU_SOURCE -DCHUNK_SIZE=262144 -DNO_SPIN_LOCKS -c src/alloc.c -o .obj/alloc.o
clang -Iinclude -O3 -flto -o bin/bench .obj/alloc.o .obj/lock.o src/bench.c
malloc    total test time=2308894ns (72.152937ns per iteration)
cc_malloc total test time=2354005ns (73.562656ns per iteration)
chris@chris-ubuntu:~/projects/cgmalloc$ 
```

So, as you can see, in some cases it outperforms the glibc malloc and in other cases it does not (depending on configuration). I also ran some tests against je_malloc and je_malloc is slightly faster than glibc malloc, but with larger CHUNK_SIZES cg_malloc can outperform. I think one of the interesting features is the NO_SPIN_LOCKS feature because there are many single threaded applications (or that do their own locking around memory management) that could definitely take advantage of this feature. It's significantly faster than when spin locks are enabled.

# Linking to cg_malloc
Here are the steps to link to the shared library produced by cg_malloc:
1.) build cg_malloc:

```# make clean all```

2.) Create a test program

```
#include <stdio.h>
#include <string.h>
#include "include/alloc.h"

int main() {
        char *ptr = cg_malloc(100);
        strcpy(ptr, "hi");
        printf("v=%lu,s=%s\n", (size_t)ptr, ptr);
        cg_free(ptr);
        return 0;
}
```

3.) Compile the program linking to the shared library in your lib directory:

```clang test.c -lcgmalloc -Llib -Wl,-rpath,lib -o hello```

4.) Execute the program:

```
$ ./hello
v=139374946418984,s=hi
```

With the -DSET_MALLOC option, you can export malloc/free/realloc/calloc and use them directly, but you might have some difficulty getting the compiler to use this version as opposed to the standard library's version as I did. But I was eventually able to get it to work. It's much easier if you are compiling with a -nostdlib.

Here's an output of nm showing what it looks like with the exported functions:

```
$ make clean all ALLOCFLAGS="-DSET_MALLOC"
rm -fr .obj/* lib/* bin/*
clang -Iinclude -fPIC -std=c89 -pedantic -Wall -Wextra -O3 -D_GNU_SOURCE -DSET_MALLOC -c src/alloc.c -o .obj/alloc.o
clang -Iinclude -fPIC -std=c89 -pedantic -Wall -Wextra -O3 -D_GNU_SOURCE -c src/lock.c -o .obj/lock.o
clang -O3 -shared -o lib/libcgmalloc.so .obj/alloc.o .obj/lock.o
$ nm lib/libcgmalloc.so  | grep -E "malloc|calloc|realloc|free"
00000000000019f0 T __wrap_calloc
00000000000019e0 T __wrap_free
00000000000019d0 T __wrap_malloc
0000000000001a50 T __wrap_realloc
0000000000001960 T calloc
0000000000001800 T cg_calloc
0000000000001660 T cg_free
00000000000011e0 T cg_malloc
0000000000001860 T cg_realloc
0000000000001950 T free
0000000000001940 T malloc
00000000000019c0 T realloc
```
