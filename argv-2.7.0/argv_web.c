/*
 * Web argument processor...
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
 * $Id: argv_web.c,v 1.3 2000/03/07 23:07:15 gray Exp $
 */

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>				/* for read */
#endif

#ifdef LOCAL
#include "dmalloc.h"
#endif

#include "conf.h"

#include "argv.h"
#include "argv_web.h"
#include "argv_loc.h"

#if INCLUDE_RCS_IDS
static	char	*rcs_id =
  "$Id: argv_web.c,v 1.3 2000/03/07 23:07:15 gray Exp $";
#endif

/*
 * int argv_web_process_string
 *
 * DESCRIPTION:
 *
 * Processes arguments sent in via a string that a web-server might
 * send to program.  We divide up the string using a supplied
 * delimiters string.
 *
 * WARNING: you cannot use argv_copy_args after this is called because
 * a temporary grid is created.
 *
 * RETURNS:
 *
 * Success - 0
 *
 * Failure - -1
 *
 * ARGUMENTS:
 *
 * args - Array of argv_t structures.
 *
 * arg0 - Argument 0 which will be used in various error messages.
 *
 * string - Web string that we are processing.
 *
 * delim - Delimiter string to divide up the tokens in string.
 * QUERY_STRING and POST processing should have "&" while PATH_INFO
 * should have "/".  You may want to add "=" if you use arg=value type
 * of arguments.  The '=' delimiter is treated as special so //x=//
 * will strip the extra /'s in a row but will create a null argument
 * for x.
 */
int	argv_web_process_string(argv_t *args, const char *arg0,
				const char *string, const char *delim)
{
  const char	*str_p, *delim_p, *delim_str;
  char		*copy, *copy_p, **argv;
  int		arg_c, ret, alloced;
  
  if (delim == NULL) {
    delim_str = "";
  }
  else {
    delim_str = delim;
  }
  
  /* copy incoming string so we can punch nulls */
  copy = malloc(strlen(string) + 1);
   if (copy == NULL) {
    if (argv_error_stream != NULL) {
      (void)fprintf(argv_error_stream,
		    "%s: memory error during argument processing\n",
		    argv_program);
    }
    if (argv_interactive) {
      (void)exit(EXIT_CODE);
    }
    return ERROR;
  }
  
  /* create argv array */
  alloced = ARG_MALLOC_INCR;
  argv = (char **)malloc(sizeof(char *) * alloced);
  if (argv == NULL) {
    free(copy);
    if (argv_error_stream != NULL) {
      (void)fprintf(argv_error_stream,
		    "%s: memory error during argument processing\n",
		    argv_program);
    }
    if (argv_interactive) {
      (void)exit(EXIT_CODE);
    }
    return ERROR;
  }
  
  arg_c = 0;
  argv[arg_c++] = (char *)arg0;
  str_p = string;
  /*  skip starting multiple arg delimiters */
  for (; *str_p != '\0'; str_p++) {
    for (delim_p = delim_str; *delim_p != '\0'; delim_p++) {
      if (*str_p == *delim_p) {
	break;
      }
    }
    if (*delim_p == '\0') {
      break;
    }
  }
  
  /* start of the string is argv[1] */
  if (*str_p != '\0') {
    if (arg_c >= alloced) {
      alloced += ARG_MALLOC_INCR;
      argv = (char **)realloc(argv, sizeof(char *) * alloced);
      if (argv == NULL) {
	free(copy);
	if (argv_error_stream != NULL) {
	  (void)fprintf(argv_error_stream,
			"%s: memory error during argument processing\n",
			argv_program);
	}
	if (argv_interactive) {
	  (void)exit(EXIT_CODE);
	}
	return ERROR;
      }
    }
    argv[arg_c++] = copy;
  }
  
  for (copy_p = copy;; str_p++) {
    int		val;
    
    /* are we done? */
    if (*str_p == '\0') {
      *copy_p = '\0';
      break;
    }
    
    /* is this a argument seperator? */
    for (delim_p = delim_str; *delim_p != '\0'; delim_p++) {
      if (*str_p == *delim_p) {
	break;
      }
    }
    if (*delim_p != '\0') {
      *copy_p++ = '\0';
      
      /*
       * look ahead and skip multiple arg delimiters.  we have a
       * special case if the delimiter is '='.  This means that we
       * need to generate a null string argument.
       */
      if (*str_p != '=') {
	for (;; str_p++) {
	  for (delim_p = delim_str; *delim_p != '\0'; delim_p++) {
	    if (*(str_p + 1) == *delim_p) {
	      break;
	    }
	  }
	  if (*delim_p == '\0') {
	    break;
	  }
	}
      }
      
      /* if we are not at the end of the string, create a new arg */
      if (*str_p == '=' || *(str_p + 1) != '\0') {
	if (arg_c >= alloced) {
	  alloced += ARG_MALLOC_INCR;
	  argv = (char **)realloc(argv, sizeof(char *) * alloced);
	  if (argv == NULL) {
	    if (argv_error_stream != NULL) {
	      (void)fprintf(argv_error_stream,
			    "%s: memory error during argument processing\n",
			    argv_program);
	    }
	    if (argv_interactive) {
	      (void)exit(EXIT_CODE);
	    }
	    return ERROR;
	  }
	}
	argv[arg_c++] = copy_p;
      }
      continue;
    }
    
    /* a space */
    if (*str_p == '+') {
      *copy_p++ = ' ';
      continue;
    }
    
    /* no binary character, than it is normal */
    if (*str_p != '%') {
      *copy_p++ = *str_p;
      continue;
    }      
    
    str_p++;
    
    if (*str_p >= 'a' && *str_p <= 'f') {
      val = 10 + *str_p - 'a';
    }
    else if (*str_p >= 'A' && *str_p <= 'F') {
      val = 10 + *str_p - 'A';
    }
    else if (*str_p >= '0' && *str_p <= '9') {
      val = *str_p - '0';
    }
    else {
      continue;
    }
    
    str_p++;
    
    if (*str_p >= 'a' && *str_p <= 'f') {
      val = val * 16 + (10 + *str_p - 'a');
    }
    else if (*str_p >= 'A' && *str_p <= 'F') {
      val = val * 16 + (10 + *str_p - 'A');
    }
    else if (*str_p >= '0' && *str_p <= '9') {
      val = val * 16 + (*str_p - '0');
    }
    else {
      str_p--;
    }
    
    *copy_p++ = (char)val;
  }
  
  /* now we process this argument array without handling env vars */
  ret = argv_process_no_env(args, arg_c, argv);
  
  free(copy);
  free(argv);
  
  if (ret == NOERROR) {
    return NOERROR;
  }
  else {
    return ERROR;
  }
}

/*
 * int argv_web_process
 *
 * DESCRIPTION:
 *
 * Processes arguments sent in via the QUERY_STRING environmental
 * variable that a web-server might send to program in ARG0.
 *
 * RETURNS:
 *
 * Success - 0
 *
 * Failure - -1
 *
 * ARGUMENTS:
 *
 * args - Array of argv_t structures.
 *
 * sd - Socket descriptor we should read from if this is a POST
 * operation.  For most programs, this should probably be set to the
 * number of STDIN which is 0.
 *
 * arg0 - Argument 0 which will be used in various error messages.
 */
int	argv_web_process(argv_t *args, const int sd, const char *arg0)
{
  char	*env = NULL, *work = NULL, null[1];
  int	ret, len;
  
  /*
   * See if we have a POST operation here.  This means that we get the
   * CONTENT_LENGTH environmental variable, convert its value into an
   * integer and read from the socket-descriptor.
   */
  env = getenv("REQUEST_METHOD");
  if (env != NULL && strcmp(env, "POST") == 0) {
    env = getenv("CONTENT_LENGTH");
    if (env != NULL) {
      len = atoi(env);
      if (len > 0) {
	work = (char *)malloc(len + 1);
	if (work == NULL) {
	  if (argv_error_stream != NULL) {
	    (void)fprintf(argv_error_stream,
			  "%s: memory error during argument processing\n",
			  argv_program);
	  }
	  if (argv_interactive) {
	    (void)exit(EXIT_CODE);
	  }
	  return ERROR;
	}
	(void)read(sd, work, len);
	work[len] = '\0';
      }
    }
  }
  
  /*
   * if we've yet to find the argument string, then process the
   * QUERY_STRING env variable
   */ 
  if (work == NULL) {
    env = getenv("QUERY_STRING");
    
    /* if it is not set or empty, then nothing to do */
    if (env == NULL) {
      null[0] = '\0';
      work = null;
      /* we set env to null here so it won't be freed below */
      env = null;
    }
    else {
      work = env;
    }
  }
  
  /* now process the resulting string */
  ret = argv_web_process_string(args, arg0, work, "&=");
  
  if (work != env) {
    free(work);
  }
  
  if (ret == NOERROR) {
    return NOERROR;
  }
  else {
    return ERROR;
  }
}
