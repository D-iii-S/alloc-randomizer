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

// Somewhat unusually, the library code is included in the test code.
// This is because we need to limit the visibility of the library
// symbols, which makes it difficult to implement the tests
// in a separate module.

#include "alloc-randomizer.c"


#include <boost/dynamic_bitset.hpp>


#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>


/// How many times the tests repeat when the outcome involves random numbers.
#define RANDOM_TEST_CYCLES 1024

/// Maximum reasonable align bit count to test for.
#define ALIGN_MAX 16
/// Maximum reasonable random bit count to test for.
#define RANDOM_MAX 16


/// Size aligned to given number of bits.
/// Not suitable when speed is an issue since
/// some values can be calculated ahead of time.
#define ALIGNED_SIZE(s,x) (((s) + BITS_TO_SIZE (x) - 1) & BITS_TO_MASK_OUT (x))

/// Unsigned difference that does not go below zero.
#define UNSIGNED_DIFFERENCE(a,b) (((a) > (b)) ? ((a) - (b)) : 0)


//---------------------------------------------------------------
// Test Initialization


struct global_fixture
{
  global_fixture ()
  {
    // Force library initialization if it did not happen yet.
    // All allocation functions perform initialization.
    // Hence we just do something innocent.
    free (NULL);
  }
};


//---------------------------------------------------------------
// Reserve Calculation Tests


BOOST_AUTO_TEST_SUITE (calculate_heap_reserve_test)

BOOST_AUTO_TEST_CASE (calculate_heap_reserve_align_test)
{
  // We do not care about randomization for now.
  set_random_bits (0);

  // If some alignment is required and some original alignment is guaranteed,
  // reserve one original alignment block smaller than the alignment block
  // or equal to the aligned block header is needed.
  for (int ab = 0 ; ab <= ALIGN_MAX ; ab ++)
  {
    set_align_bits (ab);
    BOOST_CHECK_EQUAL (
      calculate_heap_reserve (),
      MAX (
        ALIGNED_SIZE (sizeof (block_header_t), ab),
        UNSIGNED_DIFFERENCE (BITS_TO_SIZE (ab), MALLOC_ALIGN_SIZE)));
  }
}

BOOST_AUTO_TEST_CASE (calculate_heap_reserve_random_test)
{
  // Alignment should mask randomization.
  for (int xb = 0 ; xb <= RANDOM_MAX ; xb ++)
  {
    set_align_bits (xb);
    set_random_bits (xb);
    BOOST_CHECK_EQUAL (
      calculate_heap_reserve (),
      MAX (
        ALIGNED_SIZE (sizeof (block_header_t), xb),
        BITS_TO_SIZE (xb) - 1));
  }

  // For any randomization, we should not see too large offsets.
  // The test outcome depends on random number generation.
  // Spurious false negatives are possible.
  set_align_bits (0);
  for (int rb = 0 ; rb <= RANDOM_MAX ; rb ++)
  {
    set_random_bits (rb);
    for (int i = 0 ; i < RANDOM_TEST_CYCLES ; i ++)
    {
      BOOST_CHECK_LT (
        calculate_heap_reserve (),
        BITS_TO_SIZE (rb) + sizeof (block_header_t));
    }
  }

  // For any randomization, we should not observe too many equal values.
  // The test outcome depends on random number generation.
  // Spurious false positives are possible.
  set_align_bits (0);
  for (int rb = 1 ; rb <= RANDOM_MAX ; rb ++)
  {
    set_random_bits (rb);
    size_t first_offset = calculate_heap_reserve ();
    bool different = false;
    for (int i = 0 ; i < RANDOM_TEST_CYCLES ; i ++)
    {
      if (calculate_heap_reserve () != first_offset)
      {
        different = true;
        break;
      }
    }
    BOOST_CHECK (different);
  }
}

BOOST_AUTO_TEST_SUITE_END ()


//---------------------------------------------------------------
// Malloc And Free Tests


BOOST_AUTO_TEST_SUITE (malloc_free_test)

BOOST_AUTO_TEST_CASE (malloc_free_align_test)
{
  // We do not care about randomness for now.
  set_random_bits (0);

  // Every allocated block should be aligned.
  for (int ab = 1 ; ab <= ALIGN_MAX ; ab ++)
  {
    set_align_bits (ab);
    for (int i = 0 ; i < RANDOM_TEST_CYCLES ; i ++)
    {
      void *block = malloc (rand (ab + 1));
      BOOST_CHECK (!MASKED_POINTER (block, BITS_TO_MASK_IN (ab)));
      free (block);
    }
  }
}

BOOST_AUTO_TEST_CASE (malloc_free_random_test)
{
  // We do not care about alignment for now.
  set_align_bits (0);

  // Most random combinations should occur.
  // We tolerate certain percentage missing.
  for (int rb = 1 ; rb <= RANDOM_MAX ; rb ++)
  {
    set_random_bits (rb);
    boost::dynamic_bitset <> observed_values (BITS_TO_SIZE (rb), false);
    for (int i = 0 ; i < RANDOM_TEST_CYCLES ; i ++)
    {
      void *block = malloc (rand (rb + 1));
      observed_values [(uintptr_t) MASKED_POINTER (block, BITS_TO_MASK_IN (rb))] = true;
    }
    // The threshold is above half, to catch single stuck bit, but otherwise liberal.
    size_t different_values_ideal = MIN (RANDOM_TEST_CYCLES, BITS_TO_SIZE (rb));
    size_t different_values_threshold = different_values_ideal * 6 / 10;
    BOOST_CHECK_GE (
      observed_values.count (),
      different_values_threshold);
  }

  // Freeing after allocation would limit addresses.
  // Freeing later would require more complex code.
  // This is just a test hence we do not free.
}

BOOST_AUTO_TEST_SUITE_END ()


//---------------------------------------------------------------
// New And Delete Tests


BOOST_AUTO_TEST_SUITE (new_delete_test)

BOOST_AUTO_TEST_CASE (new_delete_align_test)
{
  // We do not care about randomness for now.
  set_random_bits (0);

  // Every allocated block should be aligned.
  for (int ab = 1 ; ab <= ALIGN_MAX ; ab ++)
  {
    set_align_bits (ab);
    for (int i = 0 ; i < RANDOM_TEST_CYCLES ; i ++)
    {
      char *block = new char [rand (ab + 1)];
      BOOST_CHECK (!MASKED_POINTER (block, BITS_TO_MASK_IN (ab)));
      delete [] (block);
    }
  }
}

BOOST_AUTO_TEST_CASE (new_delete_random_test)
{
  // We do not care about alignment for now.
  set_align_bits (0);

  // Most random combinations should occur.
  // We tolerate certain percentage missing.
  for (int rb = 1 ; rb <= RANDOM_MAX ; rb ++)
  {
    set_random_bits (rb);
    boost::dynamic_bitset <> observed_values (BITS_TO_SIZE (rb), false);
    for (int i = 0 ; i < RANDOM_TEST_CYCLES ; i ++)
    {
      char *block = new char [rand (rb + 1)];
      observed_values [(uintptr_t) MASKED_POINTER (block, BITS_TO_MASK_IN (rb))] = true;
    }
    // The threshold is above half, to catch single stuck bit, but otherwise liberal.
    size_t different_values_ideal = MIN (RANDOM_TEST_CYCLES, BITS_TO_SIZE (rb));
    size_t different_values_threshold = different_values_ideal * 6 / 10;
    BOOST_CHECK_GE (
      observed_values.count (),
      different_values_threshold);
  }

  // Deleting after allocation would limit addresses.
  // Deleting later would require more complex code.
  // This is just a test hence we do not delete.
}

BOOST_AUTO_TEST_SUITE_END ()


//---------------------------------------------------------------
// Multiple Thread Tests


#define BLOCKS_PER_CYCLE 1000
#define CYCLES_PER_THREAD 1000

BOOST_AUTO_TEST_SUITE (thread_test)

/// The test library does not support threads.
/// I wonder if this helps.
static volatile bool test_lock = false;

void *workload_thread (void *)
{
  void *blocks [BLOCKS_PER_CYCLE];
  for (int cycle = 0 ; cycle < CYCLES_PER_THREAD ; cycle ++)
  {
    for (int block = 0 ; block < BLOCKS_PER_CYCLE ; block ++)
    {
      blocks [block] = malloc (rand (8));
    }
    SPIN_LOCK (test_lock);
    for (int block = 0 ; block < BLOCKS_PER_CYCLE ; block ++)
    {
      BOOST_CHECK (!MASKED_POINTER (block [blocks], BITS_TO_MASK_IN (ALIGN_MAX / 2)));
    }
    SPIN_UNLOCK (test_lock);
    for (int block = 0 ; block < BLOCKS_PER_CYCLE ; block ++)
    {
      free (blocks [block]);
    }
  }
  
  return (NULL);
}

BOOST_AUTO_TEST_CASE (thread_align_test)
{
  pthread_t thread_one;
  pthread_t thread_two;

  set_align_bits (ALIGN_MAX / 2);
  set_random_bits (RANDOM_MAX);
  
  pthread_create (&thread_one, NULL, workload_thread, NULL);
  pthread_create (&thread_two, NULL, workload_thread, NULL);
  pthread_join (thread_one, NULL);
  pthread_join (thread_two, NULL);
}

BOOST_AUTO_TEST_SUITE_END ()
