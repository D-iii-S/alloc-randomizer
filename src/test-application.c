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


//---------------------------------------------------------------
// Test Initialization

bool init_unit_test ()
{
  // Enforce library initialization.
  free (NULL);

  return (true);
}


//---------------------------------------------------------------
// Reserve Calculation Tests

BOOST_AUTO_TEST_SUITE (calculate_reserve)

BOOST_AUTO_TEST_CASE (calculate_reserve_align)
{
  // We do not care about randomness for now.
  set_random_bits (0);

  // If no alignment is required, no reserve should be needed regardless of original alignment.
  set_align_bits (0);
  BOOST_CHECK_EQUAL (calculate_reserve (0), 0);
  BOOST_CHECK_EQUAL (calculate_reserve (1), 0);

  // If some alignment is required and no original alignment is guaranteed,
  // reserve one byte smaller than the alignment block is needed.
  for (int ab = 0 ; ab < 16 ; ab ++)
  {
    set_align_bits (ab);
    BOOST_CHECK_EQUAL (calculate_reserve (0), BITS_TO_SIZE (ab) - 1);
  }

  // If some alignment is required and some original alignment is guaranteed,
  // reserve one original alignment block smaller than the alignment block is needed.
  set_align_bits (16);
  for (int oa = 0 ; oa < 16 ; oa ++)
  {
    BOOST_CHECK_EQUAL (calculate_reserve (oa), BITS_TO_SIZE (16) - BITS_TO_SIZE (oa));
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

int thread_test (void)
{
  pthread_t thread_one;
  pthread_t thread_two;
  
  pthread_create (&thread_one, NULL, workload_thread, NULL);
  pthread_create (&thread_two, NULL, workload_thread, NULL);
  pthread_join (thread_one, NULL);
  pthread_join (thread_two, NULL);
}
