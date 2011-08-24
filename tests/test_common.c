/* Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Common functions used by tests.
 */

#include "test_common.h"

#include <stdio.h>

#include "cryptolib.h"
#include "file_keys.h"
#include "utility.h"

/* Global test success flag. */
int gTestSuccess = 1;

int TEST_EQ(int result, int expected_result, char* testname) {
  if (result == expected_result) {
    fprintf(stderr, "%s Test " COL_GREEN "PASSED\n" COL_STOP, testname);
    return 1;
  }
  else {
    fprintf(stderr, "%s Test " COL_RED "FAILED\n" COL_STOP, testname);
    fprintf(stderr, "  Expected: %d, got: %d\n", expected_result, result);
    gTestSuccess = 0;
    return 0;
  }
}

int TEST_NEQ(int result, int not_expected_result, char* testname) {
  if (result != not_expected_result) {
    fprintf(stderr, "%s Test " COL_GREEN "PASSED\n" COL_STOP, testname);
    return 1;
  }
  else {
    fprintf(stderr, "%s Test " COL_RED "FAILED\n" COL_STOP, testname);
    fprintf(stderr, "  Didn't expect %d, but got it.\n", not_expected_result);
    gTestSuccess = 0;
    return 0;
  }
}

int TEST_PTR_EQ(const void* result, const void* expected_result,
                char* testname) {
  if (result == expected_result) {
    fprintf(stderr, "%s Test " COL_GREEN "PASSED\n" COL_STOP, testname);
    return 1;
  }
  else {
    fprintf(stderr, "%s Test " COL_RED "FAILED\n" COL_STOP, testname);
    fprintf(stderr, "  Expected: 0x%lx, got: 0x%lx\n", (long)expected_result,
            (long)result);
    gTestSuccess = 0;
    return 0;
  }
}
