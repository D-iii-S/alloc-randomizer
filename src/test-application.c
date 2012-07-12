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


#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>


/// How many times the tests repeat when the outcome involves random numbers.
#define RANDOM_TEST_CYCLES 1024

/// Maximum reasonable align bit count to test for.
#define ALIGN_MAX 16
/// Maximum reasonable random bit count to test for.
#define RANDOM_MAX 16


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


BOOST_AUTO_TEST_SUITE (calculate_reserve_test)

BOOST_AUTO_TEST_CASE (calculate_reserve_align_test)
{
  // We do not care about randomization for now.
  set_random_bits (0);

  // If no alignment is required, no reserve should be needed regardless of original alignment.
  set_align_bits (0);
  for (int oa = 0 ; oa <= ALIGN_MAX ; oa ++)
  {
    BOOST_CHECK_EQUAL (calculate_reserve (BITS_TO_MASK_OUT (oa)), sizeof (void *));
  }

  // If some alignment is required and no original alignment is guaranteed,
  // reserve one byte smaller than the alignment block is needed.
  for (int ab = 0 ; ab <= ALIGN_MAX ; ab ++)
  {
    set_align_bits (ab);
    BOOST_CHECK_EQUAL (calculate_reserve (BITS_TO_MASK_OUT (0)), MAX (sizeof (void *), BITS_TO_SIZE (ab) - 1));
  }

  // If some alignment is required and some original alignment is guaranteed,
  // reserve one original alignment block smaller than the alignment block is needed.
  set_align_bits (ALIGN_MAX);
  for (int oa = 0 ; oa <= ALIGN_MAX ; oa ++)
  {
    BOOST_CHECK_EQUAL (calculate_reserve (BITS_TO_MASK_OUT (oa)), MAX (sizeof (void *), BITS_TO_SIZE (ALIGN_MAX) - BITS_TO_SIZE (oa)));
  }
}

BOOST_AUTO_TEST_CASE (calculate_reserve_random_test)
{
  // Alignment should mask randomization.
  for (int xb = 0 ; xb <= RANDOM_MAX ; xb ++)
  {
    set_align_bits (xb);
    set_random_bits (xb);
    BOOST_CHECK_EQUAL (calculate_reserve (BITS_TO_MASK_OUT (0)), MAX (sizeof (void *), BITS_TO_SIZE (xb) - 1));
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
      BOOST_CHECK_LT (calculate_reserve (BITS_TO_MASK_OUT (0)), BITS_TO_SIZE (rb) + sizeof (void *));
    }
  }

  // For any randomization, we should not observe too many equal values.
  // The test outcome depends on random number generation.
  // Spurious false positives are possible.
  set_align_bits (0);
  for (int rb = 1 ; rb <= RANDOM_MAX ; rb ++)
  {
    set_random_bits (rb);
    size_t first_offset = calculate_reserve (BITS_TO_MASK_OUT (0));
    bool different = false;
    for (int i = 0 ; i < RANDOM_TEST_CYCLES ; i ++)
    {
      if (calculate_reserve (BITS_TO_MASK_OUT (0) != first_offset))
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
  for (int ab = 0 ; ab <= ALIGN_MAX ; ab ++)
  {
    for (int i = 0 ; i < RANDOM_TEST_CYCLES ; i ++)
    {
      void *block = malloc (rand (ab));
      BOOST_CHECK_EQUAL ((uintptr_t) block & BITS_TO_MASK_IN (ab), 0);
      free (block);
    }
  }
}

BOOST_AUTO_TEST_SUITE_END ()


//---------------------------------------------------------------
// Multiple Thread Tests


#define BLOCKS_PER_CYCLE 1000
#define CYCLES_PER_THREAD 1000

void *workload_thread (void *dummy)
{
  void *blocks [BLOCKS_PER_CYCLE];
  for (int cycle = 0 ; cycle < CYCLES_PER_THREAD ; cycle ++)
  {
    for (int block = 0 ; block < BLOCKS_PER_CYCLE ; block ++)
    {
      blocks [block] = malloc (rand (8));
    }
    for (int block = 0 ; block < BLOCKS_PER_CYCLE ; block ++)
    {
      free (blocks [block]);
    }
  }
  
  return (NULL);
}

void thread_test (void)
{
  pthread_t thread_one;
  pthread_t thread_two;
  
  pthread_create (&thread_one, NULL, workload_thread, NULL);
  pthread_create (&thread_two, NULL, workload_thread, NULL);
  pthread_join (thread_one, NULL);
  pthread_join (thread_two, NULL);
}
