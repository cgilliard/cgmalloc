# cgmalloc
A simple memory allocator. Implements the 4 main memory allocation functions:
* malloc (cg_malloc)
* free (cg_free)
* realloc (cg_realloc)
* calloc (cg_calloc)

The goal of the library is to provide a simple lightweight library that performs around as well as other libraries like je_malloc, tc_malloc, or libc's malloc while maintaining a simple codebase with essentially no dependencies and can be directly integrated into other projects by copying a few files. The result is that alloc.c is around 400-500 lines and lock.c is around 50-100 lines. If you set the _NO_SPIN_LOCK_ option you don't need lock.c but then it's only a single threaded allocator.

The design is basically a combination of a slab allocator for smaller allocations with a fallback to mmap/munmap for larger allocations. We use an alignment trick so that no header data is needed for the slabs. This is similar to how some of the other allocators work, but a little simpler. We don't implement thread local storage which can be implemented around the allocator. This is meant to be the core of the allocation code, but not handle all needs.

Should you use cgmalloc? Currently it hasn't had extensive testing, so obviously you should take that into consideration. But it's designed to be simple and lightweight. It might make sense in a nostdlib project like an embeded system or some other use case where you need a minimal memory allocator. It's smaller than dlmalloc (500 lines vs. 5,000 lines) and the performance is pretty comparable to the other libraries I've tested with. I would say that it's faster, but perhaps my benchmarks are contrived as I can make other allocators faster depending on the test. If you do decide to use it, please report any bug reports or comments, etc at the github repo.


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

# Benchmarks
The benchmark program's output is not very fancy, but here are some sample runs:

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
