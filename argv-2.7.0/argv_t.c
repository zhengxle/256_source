/*
 * Test file for the argv routines...
 *
 * Copyright 2000 by Gray Watson
 *
 * This file is part of the argv library.
 *
 * Permission to use, copy, modify, and distribute this software for
 * any purpose and without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies, and that the name of Gray Watson not be used in advertising
 * or publicity pertaining to distribution of the document or software
 * without specific, written prior permission.
 *
 * Gray Watson makes no representations about the suitability of the
 * software described herein for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * The author may be contacted via http://256.com/gray/
 *
 * $Id: argv_t.c,v 1.35 2010/02/15 12:59:49 gray Exp $
 */

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef LOCAL
#include "dmalloc.h"
#endif

#define ARGV_TEST_MAIN

#include "argv.h"

static	char	*rcs_id =
  "$Id: argv_t.c,v 1.35 2010/02/15 12:59:49 gray Exp $";

/* local variables */
#define OUTPUT_PATH		"argv_t.t"
#define FILE_ARGS		"argv_t_args.t"

/* main arguments */
static	int	verbose_b = 0;		/* verbose tests */

static	argv_t	main_args[] = {
  { 'v',	"verbose",	ARGV_BOOL_INT,			&verbose_b,
    NULL,		"verbose test messages" },
  { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
};

/***************************** utility routines ******************************/

/*
 * static void check_path
 *
 * DESCRIPTION:
 *
 * Check the contents of a file.
 *
 * RETURNS:
 *
 * Success - 1
 *
 * Failure - 0
 *
 * ARGUMENTS:
 *
 * label -> Label of the test for error output.
 *
 * path -> Pathname we are comparing against.
 *
 * contents -> Contents we are comparing against.
 */
static	int	check_path(const char *label, const char *path,
			   const char *contents)
{
  FILE		*stream;
  int		line_c = 0, len, final_b = 1;
  const char	*cont_p;
  char		line[256];
  
  /* check the paths */
  stream = fopen(path, "r");
  if (stream == NULL) {
    perror(path);
    exit(1);
  }
  
  cont_p = contents;
  while (fgets(line, sizeof(line), stream) != NULL) {
    line_c++;
    
    len = strlen(line);
    if (strncmp(line, cont_p, len) == 0) {
      cont_p += len;
      continue;
    }
    
    if (verbose_b) {
      fprintf(stderr, "Test '%s' failed.\n", label);
    }
    fprintf(stderr, "  expected: ---------------------------\n");
    if (*contents == '\0') {
      fprintf(stderr, "    <no contents>\n");
    }
    else {
      fprintf(stderr, "    %.*s", len, cont_p);
    }
    fprintf(stderr, "  got: --------------------------------\n");
    fprintf(stderr, "    %s", line);
    while (fgets(line, sizeof(line), stream) != NULL) {
      fprintf(stderr, "    %s", line);
    }
    final_b = 0;
    break;
  }
  
  fclose(stream);
  return final_b;
}

/*
 * static void do_test
 *
 * DESCRIPTION:
 *
 * Run a test and examine the results.
 *
 * RETURNS:
 *
 * Success - 1
 *
 * Failure - 0
 *
 * ARGUMENTS:
 *
 * label -> Label of the test for error output.
 *
 * args -> Argv structure array we are testing.
 *
 * argv -> String argument array we are using to process the arguments.
 *
 * argc -> Number of arguments.
 *
 * stderr_contents -> Contents we are expecting on stderr.
 */
static	int	do_test(const char *label, argv_t *args, char **argv,
			const int num_args,
			const char *stderr_contents)
{
  FILE	*argv_output;
  int	final_b = 1, arg_n = num_args;
  
  if (verbose_b) {
    fprintf(stderr, "Running test '%s':\n", label);
  }

  if (arg_n == 0) {  
    for (arg_n = 0; argv[arg_n] != NULL; arg_n++) {
    }
  }

  argv_output = fopen(OUTPUT_PATH, "w");
  if (argv_output == NULL) {
    perror(OUTPUT_PATH);
    exit(1);
  }
  
  argv_interactive = ARGV_FALSE;
  argv_error_stream = argv_output;
  if (argv_process(args, arg_n, argv) != 0) {
    final_b = 0;
  }
  
  fclose(argv_output);
  
  if (stderr_contents != NULL) {
    if (check_path(label, OUTPUT_PATH, stderr_contents)) {
      if (verbose_b) {
	fprintf(stderr, "  Output matched\n");
      }
      unlink(OUTPUT_PATH);
    }
    else {
      if (! verbose_b) {
	fprintf(stderr, "  Test '%s': ", label);
      }
      fprintf(stderr, "Output did not match\n");
      final_b = 0;
    }
  }
  
  return final_b;
}

/******************************* test special ********************************/

static	int	test_none(void)
{
  int		final_b = 1;
  argv_t	args[] = {
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  char		*argv[] = { "none", NULL };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  
  return final_b;
}
  
static	int	test_version(void)
{
  int		final_b = 1;
  argv_t	args[] = {
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  char		*argv[] = { "version", "--version", NULL };
  
  argv_version_string = "xyzzy";
  
  if (! do_test("version", args, argv, 0, "version: xyzzy\n")) {
    final_b = 0;
  }
  
  return final_b;
}
  
static	int	test_help(void)
{
  int		final_b = 1;
  argv_t	args[] = {
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  char		*argv[] = { "help", "--help", NULL };
  
  argv_help_string = "123321";
  
  if (! do_test(argv[0], args, argv, 0, "help: 123321\n")) {
    final_b = 0;
  }
  
  return final_b;
}

static	int	test_file(void)
{
  int		final_b = 1;
  int		a_arg = 0;
  char		*argv[] = { "file", "--argv-file", FILE_ARGS, NULL };
  FILE		*stream;
  argv_t	args[] = {
    { 'a', 0L, ARGV_BOOL_INT, &a_arg, 0L, "a arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  /* write out args file */
  stream = fopen(FILE_ARGS, "w");
  if (stream == NULL) {
    abort();
  }
  fprintf(stream, "-a\n");
  fclose(stream);
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (a_arg) {
    if (verbose_b) {
      fprintf(stderr, "  A argument equals %d.\n", a_arg);
    }
  }
  else {
    fprintf(stderr, "  A argument equals %d not 1.\n", a_arg);
    final_b = 0;
  }
  
  return final_b;
}

static	int	test_special(void)
{
  if (verbose_b) {
    fprintf(stderr, "Running special tests:\n");
  }
  
  if ((! test_none())
      || (! test_version())
      || (! test_help())
      || (! test_file())) {
    return 0;
  }
  
  return 1;
}

/******************************** test types *********************************/

static	int	test_bool(void)
{
  int		final_b = 1;
  char		a_arg = 0;
  char		*argv[] = { "bool", "-a", NULL };
  argv_t	args[] = {
    { 'a', 0L, ARGV_BOOL, &a_arg, 0L, "a arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (a_arg) {
    if (verbose_b) {
      fprintf(stderr, "  Argument equals %d.\n", a_arg);
    }
  }
  else {
    fprintf(stderr, "  Argument equals %d not 1.\n", a_arg);
    final_b = 0;
  }
  
  return final_b;
}

static	int	test_bool_neg(void)
{
  int		final_b = 1;
  char		a_arg = 1;
  char		*argv[] = { "bool_neg", "-n", NULL };
  argv_t	args[] = {
    { 'n', 0L, ARGV_BOOL_NEG, &a_arg, 0L, "n arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (a_arg) {
    fprintf(stderr, "  Argument equals %d not 0.\n", a_arg);
    final_b = 0;
  }
  else {
    if (verbose_b) {
      fprintf(stderr, "  Argument equals %d.\n", a_arg);
    }
  }
  
  return final_b;
}

static	int	test_bool_arg(void)
{
  int		final_b = 1;
  char		a_arg = 1;
  char		*argv[] = { "bool_arg", "-b", "yes", NULL };
  argv_t	args[] = {
    { 'b', 0L, ARGV_BOOL_ARG, &a_arg, 0L, "a_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (a_arg) {
    if (verbose_b) {
      fprintf(stderr, "  a_arg argument equals %d.\n", a_arg);
    }
  }
  else {
    fprintf(stderr, "  a_arg argument equals %d not 1.\n", a_arg);
    final_b = 0;
  }
  
  return final_b;
}

static	int	test_bool_int(void)
{
  int		final_b = 1;
  int		a_arg = 0;
  char		*argv[] = { "bool_int", "-a", NULL };
  argv_t	args[] = {
    { 'a', 0L, ARGV_BOOL_INT, &a_arg, 0L, "a arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (a_arg) {
    if (verbose_b) {
      fprintf(stderr, "  Argument equals %d.\n", a_arg);
    }
  }
  else {
    fprintf(stderr, "  Argument equals %d not 1.\n", a_arg);
    final_b = 0;
  }
  
  return final_b;
}

static	int	test_bool_int_neg(void)
{
  int		final_b = 1;
  int		a_arg = 1;
  char		*argv[] = { "bool_int_neg", "-n", NULL };
  argv_t	args[] = {
    { 'n', 0L, ARGV_BOOL_INT_NEG, &a_arg, 0L, "n arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (a_arg) {
    fprintf(stderr, "  Argument equals %d not 0.\n", a_arg);
    final_b = 0;
  }
  else {
    if (verbose_b) {
      fprintf(stderr, "  Argument equals %d.\n", a_arg);
    }
  }
  
  return final_b;
}

static	int	test_bool_int_arg(void)
{
  int		final_b = 1;
  int		a_arg = 1;
  char		*argv[] = { "bool_int_arg", "-b", "yes", NULL };
  argv_t	args[] = {
    { 'b', 0L, ARGV_BOOL_INT_ARG, &a_arg, 0L, "a_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (a_arg) {
    if (verbose_b) {
      fprintf(stderr, "  a_arg argument equals %d.\n", a_arg);
    }
  }
  else {
    fprintf(stderr, "  a_arg argument equals %d not 1.\n", a_arg);
    final_b = 0;
  }
  
  return final_b;
}

static	int	test_char(void)
{
  int		final_b = 1;
  char		a_arg = 'a';
  char		*argv[] = { "char", "-C", "y", NULL };
  argv_t	args[] = {
    { 'C', 0L, ARGV_CHAR, &a_arg, 0L, "a_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (a_arg == *(argv[2])) {
    if (verbose_b) {
      fprintf(stderr, "  a_arg argument equals '%c'.\n", a_arg);
    }
  }
  else {
    fprintf(stderr, "  a_arg argument equals '%c' not '%c'.\n",
	    a_arg, *(argv[2]));
    final_b = 0;
  }
  
  return final_b;
}

static	int	test_char_p(void)
{
  int		final_b = 1;
  char		*a_arg = "bar";
  char		*argv[] = { "char_p", "-s", "foo", NULL };
  argv_t	args[] = {
    { 's', 0L, ARGV_CHAR_P, &a_arg, 0L, "a_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (strcmp(a_arg, argv[2]) == 0) {
    if (verbose_b) {
      fprintf(stderr, "  a_arg argument equals '%s'.\n", a_arg);
    }
  }
  else {
    fprintf(stderr, "  a_arg argument equals '%s' not '%s'.\n",
	    a_arg, argv[2]);
    final_b = 0;
  }
  
  return final_b;
}

static	int	test_short(void)
{
  int		final_b = 1;
  short		a_arg = 12;
  char		*argv[] = { "short", "-h", "102", NULL };
  argv_t	args[] = {
    { 'h', 0L, ARGV_SHORT, &a_arg, 0L, "a_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (a_arg == atoi(argv[2])) {
    if (verbose_b) {
      fprintf(stderr, "  a_arg argument equals %d.\n", a_arg);
    }
  }
  else {
    fprintf(stderr, "  a_arg argument equals %d not %d.\n",
	    a_arg, atoi(argv[2]));
    final_b = 0;
  }
  
  return final_b;
}

static	int	test_u_short(void)
{
  int		final_b = 1;
  unsigned short a_arg = 12;
  char		*argv[] = { "u_short", "-u", "357", NULL };
  argv_t	args[] = {
    { 'u', 0L, ARGV_SHORT, &a_arg, 0L, "a_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (a_arg == atoi(argv[2])) {
    if (verbose_b) {
      fprintf(stderr, "  a_arg argument equals %hu.\n", a_arg);
    }
  }
  else {
    fprintf(stderr, "  a_arg argument equals %hu not %d.\n",
	    a_arg, atoi(argv[2]));
    final_b = 0;
  }
  
  /* now test neg numbers */

  {
    char	*argv1[] = { "u_short neg", "-u", "-20", NULL };
    
    a_arg = 100;
    if (! do_test(argv1[0], args, argv1, 0, "")) {
      final_b = 0;
    }
    if (a_arg == atoi(argv[2])) {
      fprintf(stderr, "  a_arg argument %hu should not be %d.\n",
	      a_arg, atoi(argv[2]));
      final_b = 0;
    }
    else {
      if (verbose_b) {
	fprintf(stderr, "  a_arg argument equals %hu.\n", a_arg);
      }
    }
  }

  return final_b;
}

static	int	test_int(void)
{
  int		final_b = 1;
  int		a_arg = 12;
  char		*argv[] = { "int", "-h", "102", NULL };
  argv_t	args[] = {
    { 'h', 0L, ARGV_INT, &a_arg, 0L, "a_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (a_arg == atoi(argv[2])) {
    if (verbose_b) {
      fprintf(stderr, "  a_arg argument equals %d.\n", a_arg);
    }
  }
  else {
    fprintf(stderr, "  a_arg argument equals %d not %d.\n",
	    a_arg, atoi(argv[2]));
    final_b = 0;
  }
  
  return final_b;
}

static	int	test_u_int(void)
{
  int		final_b = 1;
  unsigned int a_arg = 12;
  char		*argv[] = { "u_int", "-u", "357", NULL };
  argv_t	args[] = {
    { 'u', 0L, ARGV_INT, &a_arg, 0L, "a_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (a_arg == (unsigned int)atoi(argv[2])) {
    if (verbose_b) {
      fprintf(stderr, "  a_arg argument equals %hu.\n", a_arg);
    }
  }
  else {
    fprintf(stderr, "  a_arg argument equals %hu not %d.\n",
	    a_arg, atoi(argv[2]));
    final_b = 0;
  }
  
  /* now test neg numbers */
  
  {
    char	*argv1[] = { "u_int neg", "-u", "-20", NULL };
    
    a_arg = 100;
    if (! do_test(argv1[0], args, argv1, 0, "")) {
      final_b = 0;
    }
    if (a_arg == (unsigned int)atoi(argv[2])) {
      fprintf(stderr, "  a_arg argument %hu should not be %d.\n",
	      a_arg, atoi(argv[2]));
      final_b = 0;
    }
    else {
      if (verbose_b) {
	fprintf(stderr, "  a_arg argument equals %hu.\n", a_arg);
      }
    }
  }
  
  return final_b;
}

static	int	test_long(void)
{
  int		final_b = 1;
  long		a_arg = 12;
  char		*argv[] = { "long", "-h", "102", NULL };
  argv_t	args[] = {
    { 'h', 0L, ARGV_LONG, &a_arg, 0L, "a_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (a_arg == atoi(argv[2])) {
    if (verbose_b) {
      fprintf(stderr, "  a_arg argument equals %ld.\n", a_arg);
    }
  }
  else {
    fprintf(stderr, "  a_arg argument equals %ld not %d.\n",
	    a_arg, atoi(argv[2]));
    final_b = 0;
  }
  
  return final_b;
}

static	int	test_u_long(void)
{
  int		final_b = 1;
  unsigned long a_arg = 12;
  char		*argv[] = { "u_long", "-u", "357", NULL };
  argv_t	args[] = {
    { 'u', 0L, ARGV_LONG, &a_arg, 0L, "a_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (a_arg == (unsigned long)atoi(argv[2])) {
    if (verbose_b) {
      fprintf(stderr, "  a_arg argument equals %lu.\n", a_arg);
    }
  }
  else {
    fprintf(stderr, "  a_arg argument equals %lu not %d.\n",
	    a_arg, atoi(argv[2]));
    final_b = 0;
  }
  
  /* now test neg numbers */
  
  {
    char	*argv1[] = { "u_long neg", "-u", "-20", NULL };
    
    a_arg = 100;
    if (! do_test(argv1[0], args, argv1, 0, "")) {
      final_b = 0;
    }
    if (a_arg == (unsigned long)atoi(argv[2])) {
      fprintf(stderr, "  a_arg argument %lu should not be %d.\n",
	      a_arg, atoi(argv[2]));
      final_b = 0;
    }
    else {
      if (verbose_b) {
	fprintf(stderr, "  a_arg argument equals %lu.\n", a_arg);
      }
    }
  }
  
  return final_b;
}

static	int	test_float(void)
{
  int		final_b = 1;
  float		a_arg = 1.4;
  char		*argv[] = { "float", "-f", "4.5", NULL };
  argv_t	args[] = {
    { 'f', 0L, ARGV_FLOAT, &a_arg, 0L, "a_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (a_arg == atof(argv[2])) {
    if (verbose_b) {
      fprintf(stderr, "  a_arg argument equals %f.\n", a_arg);
    }
  }
  else {
    fprintf(stderr, "  a_arg argument equals %f not %f.\n",
	    a_arg, atof(argv[2]));
    final_b = 0;
  }
  
  return final_b;
}

static	int	test_double(void)
{
  int		final_b = 1;
  double	a_arg = 1.4;
  char		*argv[] = { "double", "-f", "4.5", NULL };
  argv_t	args[] = {
    { 'f', 0L, ARGV_DOUBLE, &a_arg, 0L, "a_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (a_arg == atof(argv[2])) {
    if (verbose_b) {
      fprintf(stderr, "  a_arg argument equals %lf.\n", a_arg);
    }
  }
  else {
    fprintf(stderr, "  a_arg argument equals %lf not %f.\n",
	    a_arg, atof(argv[2]));
    final_b = 0;
  }
  
  return final_b;
}

static	int	test_bin(void)
{
  int		final_b = 1;
  int		a_arg = 12, expect = 5;
  char		*argv[] = { "binary", "-b", "101", NULL };
  argv_t	args[] = {
    { 'b', 0L, ARGV_BIN, &a_arg, 0L, "a_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (a_arg == expect) {
    if (verbose_b) {
      fprintf(stderr, "  a_arg argument equals %d.\n", a_arg);
    }
  }
  else {
    fprintf(stderr, "  a_arg argument equals %d not %d.\n",
	    a_arg, expect);
    final_b = 0;
  }
  
  return final_b;
}

static	int	test_oct(void)
{
  int		final_b = 1;
  int		a_arg = 12, expect = 65;
  char		*argv[] = { "octal", "-o", "101", NULL };
  argv_t	args[] = {
    { 'o', 0L, ARGV_OCT, &a_arg, 0L, "a_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (a_arg == expect) {
    if (verbose_b) {
      fprintf(stderr, "  a_arg argument equals %d.\n", a_arg);
    }
  }
  else {
    fprintf(stderr, "  a_arg argument equals %d not %d.\n",
	    a_arg, expect);
    final_b = 0;
  }
  
  return final_b;
}

static	int	test_hex(void)
{
  int		final_b = 1;
  int		a_arg = 12, expect = 257;
  char		*argv[] = { "hex", "-h", "101", NULL };
  argv_t	args[] = {
    { 'h', 0L, ARGV_HEX, &a_arg, 0L, "a_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (a_arg == expect) {
    if (verbose_b) {
      fprintf(stderr, "  a_arg argument equals %d.\n", a_arg);
    }
  }
  else {
    fprintf(stderr, "  a_arg argument equals %d not %d.\n",
	    a_arg, expect);
    final_b = 0;
  }
  
  return final_b;
}

static	int	test_incr(void)
{
  int		final_b = 1;
  int		start = 32;
  int		a_arg = start;
  char		*argv[] = { "incr", "-i", NULL };
  argv_t	args[] = {
    { 'i', 0L, ARGV_INCR, &a_arg, 0L, "a_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (a_arg == start + 1) {
    if (verbose_b) {
      fprintf(stderr, "  a_arg argument equals %d.\n", a_arg);
    }
  }
  else {
    fprintf(stderr, "  a_arg argument equals %d not %d.\n",
	    a_arg, start + 1);
    final_b = 0;
  }
  
  return final_b;
}

/*
 * do the test size so we can try a bunch of different values
 */
static	int	do_test_size(const char *arg, const long expected)
{
  int		final_b = 1;
  long		a_arg = 12;
  char		*argv[] = { "size", "-s", (char *)arg, NULL };
  argv_t	args[] = {
    { 's', 0L, ARGV_SIZE, &a_arg, 0L, "a_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (a_arg == expected) {
    if (verbose_b) {
      fprintf(stderr, "  a_arg argument equals %ld.\n", a_arg);
    }
  }
  else {
    fprintf(stderr, "  a_arg argument equals %ld not %ld.\n",
	    a_arg, expected);
    final_b = 0;
  }
  
  return final_b;
}

static	int	test_size(void)
{
  if (do_test_size("2", 2)
      && do_test_size("2b", 2)
      && do_test_size("2B", 2)
      && do_test_size("2k", 2048)
      && do_test_size("2K", 2048)
      && do_test_size("2m", 2097152)
      && do_test_size("2M", 2097152)
      && do_test_size("1g", 1073741824)
      && do_test_size("1G", 1073741824)
      ) {
    return 1;
  }
  else {
    return 0;
  }
}

/*
 * do the test size so we can try a bunch of different values
 */
static	int	do_test_u_size(const char *arg, const unsigned long expected)
{
  int		final_b = 1;
  unsigned long	a_arg = 12;
  char		*argv[] = { "u_size", "-S", (char *)arg, NULL };
  argv_t	args[] = {
    { 'S', 0L, ARGV_U_SIZE, &a_arg, 0L, "a_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (! do_test(argv[0], args, argv, 0, "")) {
    final_b = 0;
  }
  if (a_arg == expected) {
    if (verbose_b) {
      fprintf(stderr, "  a_arg argument equals %lu.\n", a_arg);
    }
  }
  else {
    fprintf(stderr, "  a_arg argument equals %lu not %lu.\n",
	    a_arg, expected);
    final_b = 0;
  }
  
  return final_b;
}

static	int	test_u_size(void)
{
  if (do_test_u_size("2", 2)
      && do_test_u_size("2b", 2)
      && do_test_u_size("2B", 2)
      && do_test_u_size("2k", 2048)
      && do_test_u_size("2K", 2048)
      && do_test_u_size("2m", 2097152)
      && do_test_u_size("2M", 2097152)
      && do_test_u_size("1g", 1073741824)
      && do_test_u_size("1G", 1073741824)
      ) {
    return 1;
  }
  else {
    return 0;
  }
}

/*
 * test various types
 */
static	int	test_types(void)
{
  if (verbose_b) {
    fprintf(stderr, "Running type tests:\n");
  }
  
  if ((! test_bool())
      || (! test_bool_neg())
      || (! test_bool_arg())
      || (! test_bool_int())
      || (! test_bool_int_neg())
      || (! test_bool_int_arg())
      || (! test_char())
      || (! test_char_p())
      || (! test_short())
      || (! test_u_short())
      || (! test_int())
      || (! test_u_int())
      || (! test_long())
      || (! test_u_long())
      || (! test_float())
      || (! test_double())
      || (! test_bin())
      || (! test_oct())
      || (! test_hex())
      || (! test_incr())
      || (! test_size())
      || (! test_u_size())
      ) {
    return 0;
  }
  
  return 1;
}

/*
 * Or/xor tests
 */
static	int	test_or_xor_one(char **argv,
				const int or_b,
				const int a_expected, const int b_expected,
				const int c_expected,
				const int should_succeed_b)
{
  int		test_ret, final_b = 1;
  int		a_arg = 0, b_arg = 0, c_arg = 0;
  argv_t	or_args[] = {
    { 'a', 0L, ARGV_BOOL_INT, &a_arg, 0L, "a_arg" },
    { ARGV_OR, 0L, 0, 0L, 0L, 0L },
    { 'b', 0L, ARGV_BOOL_INT, &b_arg, 0L, "b_arg" },
    { ARGV_OR, 0L, 0, 0L, 0L, 0L },
    { 'c', 0L, ARGV_BOOL_INT, &c_arg, 0L, "c_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  argv_t	xor_args[] = {
    { 'a', 0L, ARGV_BOOL_INT, &a_arg, 0L, "a_arg" },
    { ARGV_XOR, 0L, 0, 0L, 0L, 0L },
    { 'b', 0L, ARGV_BOOL_INT, &b_arg, 0L, "b_arg" },
    { ARGV_XOR, 0L, 0, 0L, 0L, 0L },
    { 'c', 0L, ARGV_BOOL_INT, &c_arg, 0L, "c_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (or_b) {
    test_ret = do_test(argv[0], or_args, argv, 0, NULL);
  }
  else {
    test_ret = do_test(argv[0], xor_args, argv, 0, NULL);
  }

  if (test_ret) {
    if (should_succeed_b) {
      if (verbose_b) {
	fprintf(stderr, "  Test succeeded\n");
      }
    }
    else {
      fprintf(stderr, "  Test '%s' should fail but it succeeded\n", argv[0]);
      final_b = 0;
    }
  }
  else {
    if (should_succeed_b) {
      fprintf(stderr, "  Test '%s' should succeed but it failed\n", argv[0]);
      final_b = 0;
    }
    else {
      if (verbose_b) {
	fprintf(stderr, "  Test properly failed\n");
      }
    }
  }

  if (a_arg != a_expected) {
    fprintf(stderr, "  Test '%s': a_arg argument is %d not %d.\n",
	    argv[0], a_arg, a_expected);
    final_b = 0;
  }
  if (b_arg != b_expected) {
    fprintf(stderr, "  Test '%s': b_arg argument is %d not %d.\n", 
	    argv[0], b_arg, b_expected);
    final_b = 0;
  }
  if (c_arg != c_expected) {
    fprintf(stderr, "  Test '%s': c_arg argument is %d not %d.\n", 
	    argv[0], c_arg, c_expected);
    final_b = 0;
  }

  return final_b;
}

/*
 * Or tests
 */
static	int	test_or(void)
{
  int	final_b = 1;
  
  if (verbose_b) {
    fprintf(stderr, "Running or tests:\n");
  }

  char *argv1[] = { "or neither", NULL };
  if (! test_or_xor_one(argv1, 1, 0, 0, 0, 1)) {
    final_b = 0;
  }
  char *argv2[] = { "or -a", "-a", NULL };
  if (! test_or_xor_one(argv2, 1, 1, 0, 0, 1)) {
    final_b = 0;
  }
  char *argv3[] = { "or -b", "-b", NULL };
  if (! test_or_xor_one(argv3, 1, 0, 1, 0, 1)) {
    final_b = 0;
  }
  char *argv4[] = { "or -c", "-c", NULL };
  if (! test_or_xor_one(argv4, 1, 0, 0, 1, 1)) {
    final_b = 0;
  }
  char *argv5[] = { "or -a -b", "-a", "-b", NULL };
  if (! test_or_xor_one(argv5, 1, 1, 1, 0, 0)) {
    final_b = 0;
  }
  char *argv6[] = { "or -a -c", "-a", "-c", NULL };
  if (! test_or_xor_one(argv6, 1, 1, 0, 1, 0)) {
    final_b = 0;
  }
  char *argv7[] = { "or -b -c", "-b", "-c", NULL };
  if (! test_or_xor_one(argv7, 1, 0, 1, 1, 0)) {
    final_b = 0;
  }
  char *argv8[] = { "or -b -a", "-b", "-a", NULL };
  if (! test_or_xor_one(argv8, 1, 1, 1, 0, 0)) {
    final_b = 0;
  }
  char *argv9[] = { "or -c -a", "-c", "-a", NULL };
  if (! test_or_xor_one(argv9, 1, 1, 0, 1, 0)) {
    final_b = 0;
  }
  char *argv10[] = { "or -c -b", "-c", "-b", NULL };
  if (! test_or_xor_one(argv10, 1, 0, 1, 1, 0)) {
    final_b = 0;
  }
  
  return final_b;
}

/*
 * Xor tests
 */
static	int	test_xor(void)
{
  int	final_b = 1;
  
  if (verbose_b) {
    fprintf(stderr, "Running xor tests:\n");
  }

  char *argv1[] = { "xor neither", NULL };
  if (! test_or_xor_one(argv1, 0, 0, 0, 0, 0)) {
    final_b = 0;
  }
  char *argv2[] = { "xor -a", "-a", NULL };
  if (! test_or_xor_one(argv2, 0, 1, 0, 0, 1)) {
    final_b = 0;
  }
  char *argv3[] = { "xor -b", "-b", NULL };
  if (! test_or_xor_one(argv3, 1, 0, 1, 0, 1)) {
    final_b = 0;
  }
  char *argv4[] = { "xor -c", "-c", NULL };
  if (! test_or_xor_one(argv4, 0, 0, 0, 1, 1)) {
    final_b = 0;
  }
  char *argv5[] = { "xor -a -b", "-a", "-b", NULL };
  if (! test_or_xor_one(argv5, 0, 1, 1, 0, 0)) {
    final_b = 0;
  }
  char *argv6[] = { "xor -a -c", "-a", "-c", NULL };
  if (! test_or_xor_one(argv6, 0, 1, 0, 1, 0)) {
    final_b = 0;
  }
  char *argv7[] = { "xor -b -c", "-b", "-c", NULL };
  if (! test_or_xor_one(argv7, 0, 0, 1, 1, 0)) {
    final_b = 0;
  }
  char *argv8[] = { "xor -b -a", "-b", "-a", NULL };
  if (! test_or_xor_one(argv8, 0, 1, 1, 0, 0)) {
    final_b = 0;
  }
  char *argv9[] = { "xor -c -a", "-c", "-a", NULL };
  if (! test_or_xor_one(argv9, 0, 1, 0, 1, 0)) {
    final_b = 0;
  }
  char *argv10[] = { "xor -c -b", "-c", "-b", NULL };
  if (! test_or_xor_one(argv10, 0, 0, 1, 1, 0)) {
    final_b = 0;
  }
  
  return final_b;
}

/*
 * Mand/maybe tests
 */
static	int	test_mand_maybe(void)
{
  int		final_b = 1;
  char		*a_arg = "", *b_arg = "";
  argv_t	args[] = {
    { ARGV_MAND, 0L, ARGV_CHAR_P, &a_arg, 0L, "a_arg" },
    { ARGV_MAND, 0L, ARGV_CHAR_P, &b_arg, 0L, "b_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (verbose_b) {
    fprintf(stderr, "Running mand tests:\n");
  }

  char *argv1[] = { "mand", NULL };
  if (do_test(argv1[0], args, argv1, 0, NULL)) {
    fprintf(stderr, "  Mand test '%s' succeeded but it should have failed\n",
	    argv1[0]);
    final_b = 0;
  }
  char *argv2[] = { "mand a", "hello", NULL };
  if (do_test(argv2[0], args, argv2, 0, NULL)) {
    fprintf(stderr, "  Mand test '%s' succeeded but it should have failed\n",
	    argv2[0]);
    final_b = 0;
  }
  char *argv3[] = { "mand a b", "hello", "there", NULL };
  if (! do_test(argv3[0], args, argv3, 0, NULL)) {
    fprintf(stderr, "  Mand test '%s' failed but it should have succeeded\n",
	    argv3[0]);
    final_b = 0;
  }
  
  return final_b;
}

/*
 * Flag mand tests
 */
static	int	test_flag_mand(void)
{
  int		final_b = 1;
  int		a_arg, b_arg;
  argv_t	args[] = {
    { 'a', 0L, ARGV_BOOL_INT | ARGV_FLAG_MAND, &a_arg, 0L, "a_arg" },
    { 'b', 0L, ARGV_BOOL_INT | ARGV_FLAG_MAND, &b_arg, 0L, "b_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  
  if (verbose_b) {
    fprintf(stderr, "Running flag-mand tests:\n");
  }

  char *argv1[] = { "mand-flag", NULL };
  if (do_test(argv1[0], args, argv1, 0, NULL)) {
    fprintf(stderr, "  Mand test '%s' succeeded but it should have failed\n",
	    argv1[0]);
    final_b = 0;
  }
  char *argv2[] = { "mand-flag -a", "-a", NULL };
  if (do_test(argv2[0], args, argv2, 0, NULL)) {
    fprintf(stderr, "  Mand test '%s' succeeded but it should have failed\n",
	    argv2[0]);
    final_b = 0;
  }
  char *argv3[] = { "mand-flag -b", "-b", NULL };
  if (do_test(argv3[0], args, argv3, 0, NULL)) {
    fprintf(stderr, "  Mand test '%s' succeeded but it should have failed\n",
	    argv2[0]);
    final_b = 0;
  }
  char *argv4[] = { "mand-flag -a -b", "-a", "-b", NULL };
  if (! do_test(argv4[0], args, argv4, 0, NULL)) {
    fprintf(stderr, "  Mand test '%s' failed but it should have succeeded\n",
	    argv3[0]);
    final_b = 0;
  }
  
  return final_b;
}

/*
 * Flag env free-ing
 */
static	int	test_env_vectorizing(void)
{
  /* no command line args, all from env */
  char		*argv[] = { "env_vector", NULL };
  int		a_arg = 0, b_arg = 0, final_b = 1;
  argv_t	args[] = {
    { 'a', 0L, ARGV_INT, &a_arg, 0L, "a_arg" },
    { 'b', 0L, ARGV_INT, &b_arg, 0L, "b_arg" },
    { ARGV_LAST, 0L, 0, 0L, 0L, 0L }
  };
  setenv("ARGV_ENV_VECTOR", "-a 10 -b 11", 1);
  if (!do_test("env vectorizing", args, argv, 0, NULL)) {
    fprintf(stderr, "  Do test for env-vectoring failed\n");
    final_b = 0;
  }
  if (a_arg != 10) {
    fprintf(stderr, "  A argument should have been 10 but was %d\n", a_arg);
    final_b = 0;
  }
  if (b_arg != 11) {
    fprintf(stderr, "  A argument should have been 11 but was %d\n", b_arg);
    final_b = 0;
  }
  return final_b;
}

/*
 * Array type tests
 */
static	int	test_array(void)
{
  return 1;
}

/*
 * Specific regression tests
 */
static	int	test_specific(void)
{
  return 1;
}

int	main(int argc, char ** argv)
{
  int	final_b = 1;
  
  argv_help_string = "Argv library test program.";
  argv_version_string = "$Revision: 1.35 $";
  
  argv_process(main_args, argc, argv);
  
  if (! verbose_b) {
    (void)printf("Running regression tests.  Use -v for verbose messages.\n");
  }
  
  if ((! test_special())
      || (! test_types())
      || (! test_or())
      || (! test_xor())
      || (! test_mand_maybe())
      || (! test_flag_mand())
      || (! test_env_vectorizing())
      || (! test_array())
      || (! test_specific())
      ) {
    final_b = 0;
  }
  
  argv_cleanup(main_args);
  
  if (final_b) {
    (void)printf("Tests passed.\n");
    (void)exit(0);
  }
  else {
    (void)printf("TESTS FAILED.\n");
    (void)exit(1);
  }
}
