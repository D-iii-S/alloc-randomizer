/*

Copyright 2012 Petr Tuma

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#include <dlfcn.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>


//---------------------------------------------------------------
// Utility Functions


#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define MASKED_POINTER(p,m) ((typeof (p)) (((uintptr_t) (p)) & (m)))

#define BITS_TO_SIZE(x) (((size_t) 1) << (x))
#define BITS_TO_MASK_IN(x) ((((uintptr_t) 1) << (x)) - ((uintptr_t) 1))
#define BITS_TO_MASK_OUT(x) (~ BITS_TO_MASK_IN(x))

#define SPIN_LOCK(x) { while (!__sync_bool_compare_and_swap (&(x), false, true)) { }; }
#define SPIN_UNLOCK(x) { (x) = false; __sync_synchronize (); }


//---------------------------------------------------------------
// Platform Constants


/// Alignment used by the standard heap functions.
/// Setting this too low increases space overhead.
/// Setting this too high breaks alignment functionality.
#define MALLOC_ALIGN_BITS 4
#define MALLOC_ALIGN_MASK_IN BITS_TO_MASK_IN (MALLOC_ALIGN_BITS)
#define MALLOC_ALIGN_MASK_OUT BITS_TO_MASK_OUT (MALLOC_ALIGN_BITS)


//---------------------------------------------------------------
// Random Generator


#define RAND_BITS 31
#define RAND_SEED 1103515245u
#define RAND_INC 12345u

static volatile uint_fast32_t seed_value = 0;
static volatile bool seed_lock = false;

static inline uint_fast32_t rand (int bits)
{
  SPIN_LOCK (seed_lock);
  seed_value = 1103515245u * seed_value + 12345u;
  register uint_fast32_t result = (seed_value & BITS_TO_MASK_IN (RAND_BITS)) >> (RAND_BITS - bits);
  SPIN_UNLOCK (seed_lock);
  return (result);
}


//---------------------------------------------------------------
// Library Configuration


/// Address alignment, expressed as number of bits.
static volatile unsigned int align_bits = 0;
static volatile size_t align_size = BITS_TO_SIZE (0);
static volatile uintptr_t align_mask_in = BITS_TO_MASK_IN (0);
static volatile uintptr_t align_mask_out = BITS_TO_MASK_OUT (0);

/// Address randomization, expressed as number of bits.
static volatile unsigned int random_bits = 0;


/** Set align bits in global configuration.
 *
 * This function is not thread safe and should not be called in parallel with allocation functions.
 */
static void set_align_bits (unsigned int ab)
{
  align_bits = ab;
  align_size = BITS_TO_SIZE (ab);
  align_mask_in = BITS_TO_MASK_IN (ab);
  align_mask_out = BITS_TO_MASK_OUT (ab);
}


/** Set random bits in global configuration.
 *
 * This function is not thread safe and should not be called in parallel with allocation functions.
 */
static void set_random_bits (unsigned int rb)
{
  random_bits = rb;
}


#define ENV_ALIGN_BITS "AR_ALIGN_BITS"
#define ENV_RANDOM_BITS "AR_RANDOM_BITS"

/**
 * Initialize the configuration using the environment variables.
 */
static void read_configuration ()
{
  // This function is called during initialization.
  // Hence, it needs to limit allocation as much as possible.

  const char *config_align_bits = getenv (ENV_ALIGN_BITS);
  if (config_align_bits) set_align_bits (atoi (config_align_bits));

  const char *config_random_bits = getenv (ENV_RANDOM_BITS);
  if (config_random_bits) set_random_bits (atoi (config_random_bits));
}


//---------------------------------------------------------------
// Library Installation


static int (*original_pthread_create) (pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg) = NULL;
static void *(*original_realloc) (void *ptr, size_t size) = NULL;
static void *(*original_calloc) (size_t nmemb, size_t size) = NULL;
static void *(*original_malloc) (size_t size) = NULL;
static void (*original_free) (void *ptr) = NULL;


void intercept_functions ()
{
  // This function is called during initialization.
  // Hence, it needs to limit allocation as much as possible.

  original_pthread_create = (int (*) (pthread_t *, const pthread_attr_t *, void *(*) (void *), void *)) dlsym (RTLD_NEXT, "pthread_create");
  original_realloc = (void *(*) (void *, size_t)) dlsym (RTLD_NEXT, "realloc");
  original_calloc = (void *(*) (size_t, size_t)) dlsym (RTLD_NEXT, "calloc");
  original_malloc = (void *(*) (size_t)) dlsym (RTLD_NEXT, "malloc");
  original_free = (void (*) (void *)) dlsym (RTLD_NEXT, "free");
}


//---------------------------------------------------------------
// Backup Allocator
//
// This allocator is used during initialization, when
// library calls might require allocation but wrapper
// code is not yet in place everywhere.


/// Maximum size of the backup heap.
/// Increase if initialization runs out of backup heap.
#define BACKUP_SIZE 16384
/// Alignment of the backup heap.
/// Pick any reasonable value and the code should adjust.
#define BACKUP_ALIGN_BITS MALLOC_ALIGN_BITS
#define BACKUP_ALIGN_SIZE BITS_TO_SIZE (BACKUP_ALIGN_BITS)
#define BACKUP_ALIGN_MASK_IN BITS_TO_MASK_IN (BACKUP_ALIGN_BITS)
#define BACKUP_ALIGN_MASK_OUT BITS_TO_MASK_OUT (BACKUP_ALIGN_BITS)

static char backup_heap [BACKUP_SIZE] __attribute__ ((aligned (BACKUP_ALIGN_SIZE)));
static char *backup_last = backup_heap;
static volatile bool backup_lock = false;


static inline bool backup_pointer (void *ptr)
{
  return ((ptr >= backup_heap) && (ptr < (backup_heap + sizeof (backup_heap))));
}


static inline void *backup_malloc (size_t size)
{
  size_t size_aligned = (size + BACKUP_ALIGN_SIZE - 1) & BACKUP_ALIGN_MASK_OUT;
  SPIN_LOCK (backup_lock);
  void *block = backup_last;
  backup_last += size_aligned;
  if (!backup_pointer (backup_last)) _exit (1);
  SPIN_UNLOCK (backup_lock);
  
  return (block);
}


//---------------------------------------------------------------
// Wrapper Allocator


/// Block header used by the backup allocator.
struct block_header_t
{
  /// Original block address before alignment and randomization.
  void *address;
  /// Original block size before alignment and randomization.
  size_t size;
};


static volatile bool initialized = false;
static volatile bool initializing = false;


static void initialize (void)
{
  // We should never be called recursively while initializing.
  if (!__sync_bool_compare_and_swap (&initializing, false, true)) _exit (1);

  read_configuration ();
  intercept_functions ();

  // Remember we are now initialized.
  initialized = true;
  __sync_synchronize ();
  initializing = false;
  __sync_synchronize ();
}


/** Calculates the additional space that has to be allocated by the wrapper.
 *
 * The additional space consists of three parts.
 * 1. Reserve for block header.
 * 2. Reserve for alignment.
 * 3. Randomization.
 */
size_t calculate_reserve (uintptr_t original_align_mask_out)
{
  // Part one, reserve for block header.
  // Calculated as minimum aligned size sufficient to hold the header.
  size_t reserve_block_header = (sizeof (block_header_t) + align_size - 1) & align_mask_out;

  // Part two, reserve for alignment.
  // Calculated as maximum difference between alignments.
  size_t reserve_alignment = align_mask_in & original_align_mask_out;

  // Part three, randomization.
  // Calculated as random offset with alignment.
  size_t reserve_random = rand (random_bits) & align_mask_out;

  // Reserve for block header and reserve for alignment can overlap.
  size_t reserve = MAX (reserve_block_header, reserve_alignment) + reserve_random;

  return (reserve);
}


extern "C" void *realloc (void *source_address, size_t destination_size)
{
  // The functions called from here take care of initialization and alignment and randomization.

  // It is legal to resize null pointers.
  if (!source_address) return (malloc (destination_size));

  // Resizing the block while preserving data, alignment and randomization is difficult.
  // We therefore simply always allocate a new one and copy the data.
  block_header_t *source_header = (block_header_t *) source_address - 1;
  size_t source_size = source_header->size;
  
  void *destination_address = malloc (destination_size);
  memcpy (destination_address, source_address, MIN (source_size, destination_size));
  free (source_address);

  return (destination_address);
}


extern "C" void *calloc (size_t item_count, size_t item_size)
{
  // The functions called from here take care of initialization and alignment and randomization.

  size_t total_size = item_count * item_size;
  void *block_address = malloc (total_size);
  memset (block_address, 0, total_size);
  
  return (block_address);
}


extern "C" void *malloc (size_t size_original)
{
  // The wrapper can handle backup allocation while initializing.
  if (!initialized && !initializing) initialize ();

  // Allocate extra space, enough for header and random sized block.
  // Some parameters depend on whether this is backup allocation.
  size_t size_changed;
  size_t reserve;
  void *block_original;
  if (initializing)
  {
    reserve = calculate_reserve (BACKUP_ALIGN_MASK_OUT);
    size_changed = size_original + reserve;
    block_original = backup_malloc (size_changed);
    assert (!MASKED_POINTER (block_original, BACKUP_ALIGN_MASK_IN));
  }
  else
  {
    reserve = calculate_reserve (MALLOC_ALIGN_MASK_OUT);
    size_changed = size_original + reserve;
    block_original = (*original_malloc) (size_changed);
    assert (!MASKED_POINTER (block_original, MALLOC_ALIGN_MASK_IN));
  }

  // Out of memory conditions are not handled gracefully.
  if (!block_original) _exit (1);
  
  // Fill the header before shifted and aligned position and return that position.
  void *block_shifted = MASKED_POINTER ((char *) block_original + reserve, align_mask_out);
  assert (block_shifted >= block_original);
  assert ((char *) block_shifted + size_original <= (char *) block_original + size_changed);
  block_header_t *block_header = (block_header_t *) block_shifted - 1;
  assert (block_header >= block_original);
  block_header->address = block_original;
  block_header->size = size_original;

  return (block_shifted);
}


extern "C" void free (void *block_shifted)
{
  if (!initialized) initialize ();

  // It is legal to free null pointers.
  if (!block_shifted) return;

  // We never free backup pointers.
  if (backup_pointer (block_shifted)) return;

  // Free the original block.
  block_header_t *block_header = (block_header_t *) block_shifted - 1;
  void *block_original = block_header->address;
  assert (block_header >= block_original);
  (*original_free) (block_original);
}
