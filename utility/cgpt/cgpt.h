/* Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Header file for cgpt.
 */
#ifndef VBOOT_REFERENCE_UTILITY_CGPT_CGPT_H_
#define VBOOT_REFERENCE_UTILITY_CGPT_CGPT_H_

#include <getopt.h>
#include <stdint.h>
#include "cgptlib.h"
#include "gpt.h"

enum {
  CGPT_OK = 0,
  CGPT_FAILED,  /* generic error */
};

#define NOT_INITED (-1)  /* to indicated a signed integer is not initialed. */

#define ARRAY_COUNT(array) (sizeof(array)/sizeof((array)[0]))

/* 'struct option' of getopt_long() is not enough for our usage.
 * Therefore, we define the extra information to make option parsing
 * more organizable.
 * Note that please make sure every entry in struct option is mapped into an
 * individual entry in this struct. */
struct option_details {
  char *comment;

  /* If has_arg is 'required_argument', 'validator' is called to check whether
   * the 'argument' is valid or not. Once the argument is valid, the value is
   * stored in 'parsed'.
   *
   * If has_arg is 'no_argument', 'validator' is called to load 'valid_range'
   * into 'parsed' ('argument' is 0 in this case). Since getopt_long() only
   * supports integer type for 'flag' and 'val', this can support for any type.
   *
   * If has_arg is 'optional_argument', like 'required_argument', 'validator' is
   * called to check if 'argument' is valid or not. 'argument' indicates if
   * argument is present or not.
   *
   * 'validator' returns CGPT_OK if argument is valid; otherwise CGPT_FAILED
   * if invalid. */
  int (*validator)(const char *argument, void *valid_range, void *parsed);
  void *valid_range;  /* The structure passed to validator. */
  void *parsed;  /* The structure passed to validator. */
};

/* This is a special 'validator'. It assists those options without an argument,
 * i.e. help, to indicate the option is present. */
int AssignTrue(const char *argument, void *pointer, void *integer);

/* Special validator. Copy string to 'parsed' with max 'valid_range' bytes. */
int CopyString(const char *argument, void *max_len, void *dst);

/* Validator function. Returns 1 if 'argument' is between 'max' and 'min'
 * in 'valid_range'. */
struct number_range {
  int max;
  int min;
};
int InNumberRange(const char *argument, void *valid_range, void *parsed);

void ShowOptions(const struct option *opts,
                 const struct option_details *details,
                 const int num);

/* Handles all options from given argc and argv. This function supports both
 * short and long options.
 *
 * Assumptions:
 *   1. every short option has a corresponding long option and the short option
 *      is equal to 'val' of that long option.
 *   2. every entry in 'options' has a corresponding entry in 'details'.
 *      One by one and in order.
 *
 * Returns CGPT_OK if given options in argv are good, otherwise CGPT_FAILED.
 * Note that the global variable 'optind' points to next non-option after
 * this function returns.
 */
int HandleOptions(const int argc,
                  char *const *argv,
                  const char *short_option,
                  const int option_count,
                  const struct option *options,
                  const struct option_details *details);

struct drive;
int OpenDriveInLastArgument(const int argc,
                            char *const *argv,
                            struct drive *drive);

/* GUID conversion functions. Accepted format:
 *
 *   "C12A7328-F81F-11D2-BA4B-00A0C93EC93B"
 *
 * At least GUID_STRLEN bytes should be reserved in 'str' (included the tailing
 * '\0').
 */
#define GUID_STRLEN 37
int StrToGuid(const char *str, Guid *guid);
void GuidToStr(const Guid *guid, char *str);

/* Convert UTF16 string to UTF8. Rewritten from gpt utility.
 * Caller must prepare enough space for UTF8. The rough estimation is:
 *
 *   utf8 length = bytecount(utf16) * 1.5
 */
void UTF16ToUTF8(const uint16_t *utf16, uint8_t *utf8);
/* Convert UTF8 string to UTF16. Rewritten from gpt utility.
 * Caller must prepare enough space for UTF16. The conservative estimation is:
 *
 *   utf16 bytecount = bytecount(utf8) / 3 * 4
 */
void UTF8ToUTF16(const uint8_t *utf8, uint16_t *utf16);

/* Helper functions for supported GPT types. */
int ResolveType(const Guid *type, char *buf);
int SupportedType(const char *name, Guid *type);
void PrintTypes(void);
void EntryDetails(GptEntry *entry, int index, int raw);

/* Describes the drive storing the GPT. */
struct drive {
  int inited;       /* indicated if this structure is valid */
  int fd;           /* file descriptor */
  uint64_t size;    /* total size (in bytes) */
  GptData gpt;
};

extern const char* progname;

/* Given a hard drive path, this function loads GPT sectors from that drive,
 * and fills 'drive' structure. All memory allocated in drive_open() will be
 * freed at drive_close().
 *
 * If 'drive_path' starts with '/', it is treated as absolute path.
 * If 'drive_path' starts with '.', it is treated as relative path.
 * Otherwise, it will be prepended with '/dev/' to comply with gpt.
 *
 * Returns CGPT_FAILED if any error happens.
 * Returns CGPT_OK if success and information are stored in 'drive'.
 */
int DriveOpen(const char *drive_path, struct drive *drive);
int DriveClose(struct drive *drive);
int CheckValid(const struct drive *drive);

/* Function declarations for commands.
 * The return value of these functions is passed to main()'s exit value. */
int CgptAdm(int argc, char *argv[]);
int CgptAttribute(int argc, char *argv[]);
int CgptDev(int argc, char *argv[]);
int CgptRepair(int argc, char *argv[]);
int CgptShow(int argc, char *argv[]);

#endif  /* VBOOT_REFERENCE_UTILITY_CGPT_CGPT_H_ */