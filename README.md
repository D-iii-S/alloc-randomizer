# Heap Allocation Randomizer

Performance of certain application classes (in particular memory bound computation kernels)
may depend on alignment of dynamically allocated memory structures. The Heap Allocation
Randomizer enforces particular address alignment and address randomization behavior,
facilitating experiments with the associated performance effects.

## Usage

The alignment and randomization requirements are set through environment variables `AR_ALIGN_BITS`
and `AR_RANDOM_BITS` variables. In every allocated address, `AR_ALIGN_BITS` bits starting from
the least significant bit will be zero, and next `AR_RANDOM_BITS - AR_ALIGN_BITS` bits will be
random. Use `LD_PRELOAD` to load the Heap Allocation Randomizer.

```
> export AR_ALIGN_BITS=4
> export AR_RANDOM_BITS=6
> export LD_PRELOAD=alloc-randomizer.so
> your-command-here
```

Check the `experiment-speccpu` and `experiment-sysbench` scripts to see usage with SPEC CPU2006 or CPU2017 and SysBench benchmarks.

## Notes

The Heap Allocation Randomizer wraps standard memory allocation (`malloc`, `calloc`, `realloc`) and thread creation (`pthread_create`) functions.
Extra data is inserted at the beginning of the allocated blocks and at the top of the allocated stacks to meet the alignment and randomization requirements.
This will increase the memory consumption depending on the amount of address bits changed, hence the application behavior with different settings should not be compared directly.

Some notes from the documentation on the actual allocation alignment performed by standard library functions:

- `man malloc` says "the `malloc()` and `calloc()` functions return a pointer to the allocated memory that is suitably aligned for any built in type".
- `man memalign` says "the glibc `malloc(3)` always returns 8-byte aligned memory addresses".
- The C standard says "the pointer is suitably aligned to allow access to any data type".

As a bug, the Heap Allocation Randomizer does not intercept calls to `posix_memalign` and `memalign` and `valloc` and `alloca` but it should because these are released with free, which is already intercepted.
