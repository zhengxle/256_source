/*
 * shell script argument processor
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
 * $Id: argv_shell.c,v 1.15 2001/03/25 09:20:27 gray Exp $
 */

/*
 * This program processes arguments of the form of:
 *     VARIABLE-NAME,SMALL,LARGE,TYPE,ARG-NAME,DESCRIPTION
 * where:
 * - SMALL is the small argument version: a character or MAND: a
 * - LARGE is the large form of the argument: apple
 * - TYPE is the type of the field: character
 * - ARG-NAME is the name of the argument for usage messages: letter
 * - DESCRIPTION is the description of argument for usage messages.
 */

#include <stdio.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef LOCAL
#include "dmalloc.h"
#endif

#include "conf.h"

#include "argv.h"
#include "argv_loc.h"

static	char	*rcs_id =
  "$Id: argv_shell.c,v 1.15 2001/03/25 09:20:27 gray Exp $";

#define SHELL_ENVIRON	"SHELL"			/* for the type of shell */

/* argument variables */
static	char		bourne = ARGV_FALSE;	/* bourne shell flag */
static	char		cshell = ARGV_FALSE;	/* c-shell flag */
static	char		httpd = ARGV_FALSE;	/* httpd cgi script */
static	char		perl = ARGV_FALSE;	/* perl script flag */
static	char		tclsh = ARGV_FALSE;	/* tclsh script flag */

static	argv_array_t	shell_descs;		/* shell argument descs */
static	char		verbose = ARGV_FALSE;	/* verbose messages flag */
static	char		*command_name = NULL;	/* name of shell script */
static	argv_array_t	command_args;		/* command-line arguments */

static	argv_t	args[] = {
  { 'b',	"bourne",	ARGV_BOOL,		&bourne,
      NULL,			"set output for bourne shells" },
  { ARGV_OR, NULL, 0, NULL, NULL, NULL },
  { 'c',	"c-shell",	ARGV_BOOL,		&cshell,
      NULL,			"set output for C-type shells" },
  { ARGV_OR, NULL, 0, NULL, NULL, NULL },
  { 'h',	"httpd",	ARGV_BOOL,		&httpd,
      NULL,			"httpd cgi-bin script using stdin" },
  { ARGV_OR, NULL, 0, NULL, NULL, NULL },
  { 'p',	"perl",		ARGV_BOOL,		&perl,
      NULL,			"set output for perl scripts" },
  { ARGV_OR, NULL, 0, NULL, NULL, NULL },
  { 't',	"tclsh",	ARGV_BOOL,		&tclsh,
      NULL,			"set output for tclsh scripts" },
  { 'a',	"argument",	ARGV_CHAR_P | ARGV_FLAG_ARRAY, &shell_descs,
      "strings",		"comma-separated arg strings" },
  { 'v',	"verbose",	ARGV_BOOL,		&verbose,
      NULL,			"verbose messages" },
  { ARGV_MAND,	NULL,		ARGV_CHAR_P,		&command_name,
      "name",			"arg 0 or name of shell script" },
  { ARGV_MAND,	NULL,		ARGV_CHAR_P | ARGV_FLAG_ARRAY, &command_args,
      "arguments",		"command-line args to process" },
  { ARGV_LAST, NULL, 0, NULL, NULL, NULL },
};

/*
 * list of bourne shells
 */
static	char	*sh_shells[] = { "sh", "ash", "bash", "ksh", "zsh", NULL };

/***************************** utility routines ******************************/

/*
 * try a check out the shell env variable to see what form of shell
 * commands we should output
 */
static	void	choose_shell(void)
{
  const char	*shell, *shell_p;
  int		shell_c;
  
  shell = (const char *)getenv(SHELL_ENVIRON);
  if (shell == NULL) {
    cshell = ARGV_TRUE;
    return;
  }
  
  shell_p = strrchr(shell, '/');
  if (shell_p == NULL)
    shell_p = shell;
  else
    shell_p++;
  
  for (shell_c = 0; sh_shells[shell_c] != NULL; shell_c++)
    if (strcmp(sh_shells[shell_c], shell_p) == 0) {
      bourne = ARGV_TRUE;
      return;
    }
  
  cshell = ARGV_TRUE;
}

/*
 * basically a strdup for compatibility sake
 */
static	char	*string_copy(const char *ptr)
{
  const char	*ptr_p;
  char		*ret, *ret_p;
  int		len;
  
  len = strlen(ptr);
  ret = (char *)malloc(len + 1);
  if (ret != NULL) {
    for (ptr_p = ptr, ret_p = ret; *ptr_p != '\0';)
      *ret_p++ = *ptr_p++;
    *ret_p = '\0';
  }
  
  return ret;
}

/******************************* type routines *******************************/

/*
 * find the type in list
 */
static	argv_type_t	*find_type(const char * str)
{
  argv_type_t	*type_p;
  
  for (type_p = argv_types; type_p->at_value != 0; type_p++)
    if (strcmp(type_p->at_name, str) == 0)
      break;
  
  if (type_p->at_value == 0)
    return NULL;
  else
    return type_p;
}

/*
 * Transfer a value VAL of TYPE to a string
 */
static	void	type_init(const void *var, const int type)
{
  switch (ARGV_TYPE(type)) {
    
  case ARGV_BOOL:
    *(char *)var = ARGV_FALSE;
    break;
    
  case ARGV_BOOL_NEG:
    *(char *)var = ARGV_TRUE;
    break;
    
  case ARGV_CHAR:
    *(char *)var = '\0';
    break;
    
  case ARGV_CHAR_P:
    *(char **)var = NULL;
    break;
    
  case ARGV_FLOAT:
    *(float *)var = 0.0;
    break;
    
  case ARGV_SHORT:
    *(short *)var = 0;
    break;
    
  case ARGV_INT:
    *(int *)var = 0;
    break;
    
  case ARGV_LONG:
    *(long *)var = 0L;
    break;
    
  case ARGV_BIN:
    *(int *)var = 0;
    break;
    
  case ARGV_OCT:
    *(int *)var = 0;
    break;
    
  case ARGV_HEX:
    *(int *)var = 0;
    break;
    
  default:
    (void)fprintf(stderr, "%s: improper field type %d\n", argv_program, type);
    break;
  }
}

/*
 * Transfer a value VAL of TYPE to a string
 */
static	char	*type_to_str(const void *var, const int type)
{
  static char	buf[1024];
  
  switch (ARGV_TYPE(type)) {
    
  case ARGV_BOOL:
  case ARGV_BOOL_NEG:
    if (*(char *)var)
      (void)sprintf(buf, "1");
    else
      (void)sprintf(buf, "0");
    break;
    
  case ARGV_CHAR:
    /* NOTE: do I need quotes here? */
    (void)sprintf(buf, "%c", *(char *)var);
    break;
    
  case ARGV_CHAR_P:
    if (*(char **)var == NULL)
      buf[0] = '\0';
    else
      /* NOTE: do I need quotes here? */
      (void)sprintf(buf, "%s", *(char **)var);
    break;
    
  case ARGV_FLOAT:
    (void)sprintf(buf, "%f", *(float *)var);
    break;
    
  case ARGV_SHORT:
    (void)sprintf(buf, "%d", *(short *)var);
    break;
    
  case ARGV_INT:
    (void)sprintf(buf, "%d", *(int *)var);
    break;
    
  case ARGV_LONG:
    (void)sprintf(buf, "%ld", *(long *)var);
    break;
    
  case ARGV_BIN:
    (void)sprintf(buf, "%d", *(int *)var);
    break;
    
  case ARGV_OCT:
    (void)sprintf(buf, "%d", *(int *)var);
    break;
    
  case ARGV_HEX:
    (void)sprintf(buf, "%d", *(int *)var);
    break;
    
  default:
    (void)fprintf(stderr, "%s: improper field type %d\n", argv_program, type);
    buf[0] = '\0';
    break;
  }
  
  return buf;
}

/***************************** process routines ******************************/

/*
 * Setup the GRID of arguments and COMM command-line args
 */
static	void	preprocess(argv_t *grid, char **vars, char **comm)
{
  int		argc;
  char		**var_p = vars;
  argv_t	*arg_p = grid;
  
  for (argc = 0; argc < shell_descs.aa_entry_n; argc++) {
    char	*desc = ARGV_ARRAY_ENTRY(shell_descs, char *, argc);
    char	*desc_p, *hold, *variable;
    argv_type_t	*type_p;
    
    /* have to save a copy */
    hold = string_copy(desc);
    if (hold == NULL) {
      (void)fprintf(stderr, "%s: not malloc memory to string copy '%s'\n",
		    argv_program, hold);
      continue;
    }
    
    /*
     * handle the variable name
     */
    desc_p = strtok(hold, ",");
    if (desc_p == NULL) {
      (void)fprintf(stderr, "%s: not enough fields in entry '%s'\n",
		    argv_program, desc);
      free(hold);
      continue;
    }
    variable = desc_p;
    
    /*
     * handle the short-arg: x or MAND or MAYBE
     */
    desc_p = strtok(NULL, ",");
    if (desc_p == NULL) {
      (void)fprintf(stderr, "%s: not enough fields in entry '%s'\n",
		    argv_program, desc);
      free(hold);
      continue;
    }
    if (*desc_p == '\0'
	|| (*(desc_p + 1) != '\0' && strcmp(desc_p, "MAND") != 0
	    && strcmp(desc_p, "MAYBE") != 0)) {
      (void)fprintf(stderr, "%s: improper short-arg field in entry '%s'\n",
		    argv_program, desc);
      free(hold);
      continue;
    }
    if (*(desc_p + 1) == '\0')
      arg_p->ar_short_arg = *desc_p;
    else if (strcmp(desc_p, "MAND") == 0)
      arg_p->ar_short_arg = ARGV_MAND;
    else
      arg_p->ar_short_arg = ARGV_MAYBE;
    
    /*
     * handle the long-arg: NULL or string
     */
    desc_p = strtok(NULL, ",");
    if (desc_p == NULL) {
      (void)fprintf(stderr, "%s: not enough fields in entry '%s'\n",
		    argv_program, desc);
      free(hold);
      continue;
    }
    if (strcmp(desc_p, "NULL") == 0)
      arg_p->ar_long_arg = NULL;
    else {
      arg_p->ar_long_arg = string_copy(desc_p);
      if (arg_p->ar_long_arg == NULL) {
	(void)fprintf(stderr, "%s: not malloc memory to string copy '%s'\n",
		      argv_program, desc_p);
	free(hold);
	continue;
      }
    }
    
    /*
     * handle the type-string
     */
    desc_p = strtok(NULL, ",");
    if (desc_p == NULL) {
      (void)fprintf(stderr, "%s: not enough fields in entry '%s'\n",
		    argv_program, desc);
      free(hold);
      continue;
    }
    type_p = find_type(desc_p);
    if (type_p == NULL) {
      (void)fprintf(stderr, "%s: improper type field in entry '%s'\n",
		    argv_program, desc);
      free(hold);
      continue;
    }
    arg_p->ar_type = type_p->at_value;
    
    /*
     * allocate variable space and init with 0's
     */
    arg_p->ar_variable = (ARGV_PNT)malloc(type_p->at_size);
    if (arg_p->ar_variable == NULL) {
      (void)fprintf(stderr, "%s: not malloc %d bytes of memory\n",
		    argv_program, type_p->at_size);
      free(hold);
      continue;
    }
    type_init(arg_p->ar_variable, arg_p->ar_type);
    
    /*
     * label if not bool or NULL
     */
    desc_p = strtok(NULL, ",");
    if (desc_p == NULL) {
      (void)fprintf(stderr, "%s: not enough fields in entry '%s'\n",
		    argv_program, desc);
      free(hold);
      continue;
    }
    if (strcmp(desc_p, "NULL") == 0)
      arg_p->ar_var_label = NULL;
    else {
      arg_p->ar_var_label = string_copy(desc_p);
      if (arg_p->ar_var_label == NULL) {
	(void)fprintf(stderr, "%s: not malloc memory to string copy '%s'\n",
		      argv_program, desc_p);
	free(hold);
	continue;
      }
    }
    
    /*
     * comment or NULL
     */
    /* NOTE: we don't want to strip on commas in the comment */
    desc_p = strtok(NULL, "");
    if (desc_p == NULL) {
      (void)fprintf(stderr, "%s: not enough fields in entry '%s'\n",
		    argv_program, desc);
      free(hold);
      continue;
    }
    if (strcmp(desc_p, "NULL") == 0)
      arg_p->ar_comment = NULL;
    else {
      arg_p->ar_comment = string_copy(desc_p);
      if (arg_p->ar_comment == NULL) {
	(void)fprintf(stderr, "%s: not malloc memory to string copy '%s'\n",
		      argv_program, desc_p);
	free(hold);
	continue;
      }
    }
    
    /* set variable name */
    *var_p = string_copy(variable);
    if (*var_p == NULL) {
      (void)fprintf(stderr, "%s: not malloc memory to string copy '%s'\n",
		    argv_program, variable);
      free(hold);
      continue;
    }
    
    arg_p++;
    var_p++;
    free(hold);
  }
  
  arg_p->ar_short_arg = ARGV_LAST;
  
  comm[0] = command_name;
  for (argc = 0; argc < command_args.aa_entry_n; argc++)
    comm[argc + 1] = ARGV_ARRAY_ENTRY(command_args, char *, argc);
}

/******************************* dump routines *******************************/

/*
 * Output the code to set env VAR to VALUE.
 */
static	void	set_variable(const char *var, const char *value)
{
  if (bourne) {
    (void)printf("%s=%s;\n", var, value);
    if (verbose)
      (void)fprintf(stderr, "Outputed: %s=%s;\n", var, value);
  }
  else if (cshell) {
    (void)printf("set %s=%s;\n", var, value);
    if (verbose)
      (void)fprintf(stderr, "Outputed: set %s=%s;\n", var, value);
  }
  else if (perl) {
    (void)printf("$%s=%s;\n", var, value);
    if (verbose)
      (void)fprintf(stderr, "Outputed: $%s=%s;\n", var, value);
  }
  else if (tclsh) {
    (void)printf("set %s %s;\n", var, value);
    if (verbose)
      (void)fprintf(stderr, "Outputed: set %s %s;\n", var, value);
  }
}

/*
 * Dump arguments from GRID to output
 */
static	void	dump_args(argv_t *grid, char **vars)
{
  argv_t	*arg_p;
  char		**var_p, *str;
  
  for (arg_p = grid, var_p = vars; arg_p->ar_short_arg != ARGV_LAST;
       arg_p++, var_p++) {
    str = type_to_str(arg_p->ar_variable, arg_p->ar_type);
    if (str != NULL)
      set_variable(*var_p, str);
  }
}

/********************************** cleanup **********************************/

/*
 * Cleanup the GRID of arguments
 */
static	void	cleanup(argv_t *grid, char **vars)
{
  argv_t	*arg_p;
  char		**var_p;
  
  for (arg_p = grid, var_p = vars; arg_p->ar_short_arg != ARGV_LAST;
       arg_p++, var_p++) {
    free(*var_p);
    
    if (arg_p->ar_variable != NULL)
      free(arg_p->ar_variable);
    if (arg_p->ar_long_arg != NULL)
      free(arg_p->ar_long_arg);
    if (arg_p->ar_var_label != NULL)
      free(arg_p->ar_var_label);
    if (arg_p->ar_comment != NULL)
      free(arg_p->ar_comment);
  }
}

int	main(int argc, char **argv)
{
  argv_t	*grid;
  char		**comm, **vars;
  int		size;
  
  argv_process(args, argc, argv);
  
  /* try to figure out the shell we are using */
  if (! bourne && ! cshell)
    choose_shell();
  
  /* allocate the argument arrays */
  size = (shell_descs.aa_entry_n + 1) * sizeof(argv_t);
  grid = (argv_t *)malloc(size);
  if (grid == NULL) {
    (void)fprintf(stderr, "%s: could not malloc %d bytes of memory\n",
		  argv_program, size);
    exit(1);
  }
  size = (shell_descs.aa_entry_n + 1) * sizeof(char *);
  vars = (char **)malloc(size);
  if (vars == NULL) {
    (void)fprintf(stderr, "%s: could not malloc %d bytes of memory\n",
		  argv_program, size);
    exit(1);
  }
  size = (command_args.aa_entry_n + 1) * sizeof(char *);
  comm = (char **)malloc(size);
  if (comm == NULL) {
    (void)fprintf(stderr, "%s: could not malloc %d bytes of memory\n",
		  argv_program, size);
    exit(1);
  }
  
  /* process and dump the args */
  preprocess(grid, vars, comm);
  argv_process(grid, command_args.aa_entry_n + 1, comm);
  dump_args(grid, vars);
  
  cleanup(grid, vars);
  
  free(grid);
  free(vars);
  free(comm);
  
  argv_cleanup(args);
  
  exit(0);
}
