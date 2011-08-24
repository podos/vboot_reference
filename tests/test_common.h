/* Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 */

#ifndef VBOOT_REFERENCE_TEST_COMMON_H_
#define VBOOT_REFERENCE_TEST_COMMON_H_

extern int gTestSuccess;

/* Return 1 if result is equal to expected_result, else return 0.
 * Also update the global gTestSuccess flag if test fails. */
int TEST_EQ(int result, int expected_result, char* testname);

/* Return 0 if result is equal to not_expected_result, else return 1.
 * Also update the global gTestSuccess flag if test fails. */
int TEST_NEQ(int result, int not_expected_result, char* testname);

/* Return 1 if result is equal to expected_result, else return 0.
 * Also update the global gTestSuccess flag if test fails. */
int TEST_PTR_EQ(const void* result, const void* expected_result,
                char* testname);

/* ANSI Color coding sequences.
 *
 * Don't use \e as MSC does not recognize it as a valid escape sequence.
 */
#define COL_GREEN "\x1b[1;32m"
#define COL_RED "\x1b[0;31m"
#define COL_STOP "\x1b[m"

#endif  /* VBOOT_REFERENCE_TEST_COMMON_H_ */
