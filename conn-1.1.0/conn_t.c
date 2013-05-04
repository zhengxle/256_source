/*
 * Test program for connection routines.
 *
 * Copyright 2000 by Gray Watson.
 *
 * This file is part of the connection library package.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose and without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies,
 * and that the name of Gray Watson not be used in advertising or
 * publicity pertaining to distribution of the document or software
 * without specific, written prior permission.
 *
 * Gray Watson makes no representations about the suitability of the
 * software described herein for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * The author may be contacted via http://256.com/gray/
 *
 * $Id: conn_t.c,v 1.8 2000/03/09 03:37:48 gray Exp $
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "conn.h"

static	char	*rcs_id =
  "$Id: conn_t.c,v 1.8 2000/03/09 03:37:48 gray Exp $";

#define DEFAULT_ITER			100
#define DEFAULT_PORT			8016
#define SERVE_TIMEOUT			-1
#define IO_TIMEOUT			5000
#define DEFAULT_LISTEN_QUEUE		5

/* local variables */
static	int		quiet_b = 0;		/* be quiet */
static	int		verbose_b = 0;		/* be verbose */

/*
 * usage
 *
 * DESCRIPTION:
 *
 * Print the usage message to stderr.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUEMNTS:
 *
 * None.
 */
static	void	usage(void)
{
  (void)fprintf(stderr,
		"Usage: conn_t [-qv] [-c host] [-p port] [-b bufsize] "
		"[-i iterations] [-s accept-timeout] [-t i/o timeout]\n");
  exit(1);
}

/**************************** type info routines *****************************/

/*
 * type_info
 *
 * DESCRIPTION:
 *
 * Get information about a type
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUEMNTS:
 *
 * conn_p - Our connection pointer.
 *
 * type - The type name.
 *
 * type_num - The number of the type.
 */
static	void	type_info(const conn_t *conn_p, const char *type,
			  const int type_num)
{
  int	ret;
  int	okay;
  
  ret = conn_type_okay(conn_p, type_num, &okay);
  if (ret != CONN_ERROR_NONE) {
    (void)fprintf(stderr, "Problems checking connection type '%s': %s\n",
		  type, conn_strerror(ret));
  }
  
  if (okay) {
    (void)printf("Type '%s' is okay\n", type);
  }
  else {
    (void)printf("Type '%s' is not okay\n", type);
  }
}

/*
 * dump_types
 *
 * DESCRIPTION:
 *
 * Dump the types that can be exchanged across the pipe.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUEMNTS:
 *
 * conn_p - Our connection pointer.
 */
static	void	dump_types(const conn_t *conn_p)
{
  type_info(conn_p, "char", CONN_TYPE_CHAR);
  type_info(conn_p, "unsigned char", CONN_TYPE_UNSIGNED_CHAR);
  type_info(conn_p, "short", CONN_TYPE_SHORT);
  type_info(conn_p, "unsigned short", CONN_TYPE_UNSIGNED_SHORT);
  type_info(conn_p, "int", CONN_TYPE_INT);
  type_info(conn_p, "unsigned int", CONN_TYPE_UNSIGNED_INT);
  type_info(conn_p, "long", CONN_TYPE_LONG);
  type_info(conn_p, "unsigned long", CONN_TYPE_UNSIGNED_LONG);
  type_info(conn_p, "float", CONN_TYPE_FLOAT);
  type_info(conn_p, "double", CONN_TYPE_DOUBLE);
}

/************************** data checking routines ***************************/

/*
 * send_data
 *
 * DESCRIPTION:
 *
 * Send some test data across the pipe.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * conn_p - Our connection pointer.
 *
 * io_timeout - Milliseconds timeout i/o with the remote connection.
 */
static	void	send_data(conn_t *conn_p, const int io_timeout)
{
  int			ret, val1;
  unsigned short	val2[2];
  
  ret = conn_send_data(conn_p, io_timeout,
		       CONN_TYPE_CHAR, -1, "Hello",
		       CONN_TYPE_CHAR, -1, "There",
		       CONN_TYPE_LAST);
  if (ret != CONN_ERROR_NONE) {
    (void)fprintf(stderr, "Could not send data: %s\n",
		  conn_strerror(ret));
    return;
  }
  
  val1 = 1234567890;
  ret = conn_send_data(conn_p, io_timeout,
		       CONN_TYPE_INT, 1, &val1,
		       CONN_TYPE_LAST);
  if (ret != CONN_ERROR_NONE) {
    (void)fprintf(stderr, "Could not send data: %s\n",
		  conn_strerror(ret));
    return;
  }
  
  val2[0] = 1234;
  val2[1] = 65535;
  ret = conn_send_data(conn_p, io_timeout,
		       CONN_TYPE_SHORT, 2, val2,
		       CONN_TYPE_LAST);
  if (ret != CONN_ERROR_NONE) {
    (void)fprintf(stderr, "Could not send data: %s\n",
		  conn_strerror(ret));
    return;
  }
}

/*
 * receive_data
 *
 * DESCRIPTION:
 *
 * Receive our test data across the pipe.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * conn_p - Our connection pointer.
 *
 * io_timeout - Milliseconds timeout i/o with the remote connection.
 */
static	void	receive_data(conn_t *conn_p, const int io_timeout)
{
  int			len, ret, val1;
  unsigned short	val2[2];
  char			buf[128];
  
  /* receive the 1st string */
  ret = conn_receive_data(conn_p, io_timeout,
			  CONN_TYPE_CHAR, 0, sizeof(buf), &len, buf,
			  CONN_TYPE_LAST);
  if (ret != CONN_ERROR_NONE) {
    (void)fprintf(stderr, "Could not receive data: %s\n",
		  conn_strerror(ret));
    return;
  }
  if (verbose_b) {
    printf("Data len %d: '%.*s'\n", len, len, buf);
  }
  
  /* receive the 2nd string */
  ret = conn_receive_data(conn_p, io_timeout,
			  CONN_TYPE_CHAR, 0, sizeof(buf), &len, buf,
			  CONN_TYPE_LAST);
  if (ret != CONN_ERROR_NONE) {
    (void)fprintf(stderr, "Could not receive data: %s\n",
		  conn_strerror(ret));
    return;
  }
  if (verbose_b) {
    printf("Data len %d: '%.*s'\n", len, len, buf);
  }
  
  /* receive the 2nd string */
  ret = conn_receive_data(conn_p, io_timeout,
			  CONN_TYPE_INT, 0, 1, NULL, &val1,
			  CONN_TYPE_SHORT, 0, 2, NULL, val2,
			  CONN_TYPE_LAST);
  if (ret != CONN_ERROR_NONE) {
    (void)fprintf(stderr, "Could not receive data: %s\n",
		  conn_strerror(ret));
    return;
  }
  if (verbose_b) {
    printf("Val1: %d\n", val1);
    printf("Val2: %u, %u\n", val2[0], val2[1]);
  }
}

/*
 * flush_data
 *
 * DESCRIPTION:
 *
 * Flush our data.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * conn_p - Our connection pointer.
 *
 * io_timeout - Milliseconds timeout i/o with the remote connection.
 */
static	void	flush_data(conn_t *conn_p, const int io_timeout)
{
  int	ret;
  
  ret = conn_flush(conn_p, io_timeout);
  if (ret != CONN_ERROR_NONE) {
    (void)fprintf(stderr, "Could not flush data: %s\n",
		  conn_strerror(ret));
    /* not a critical error */
  }
}

/*
 * static void get_info
 *
 * DESCRIPTION:
 *
 * Get information about the connection.
 *
 * RETURNS:
 *
 * Number of bytes written and read through connection.
 *
 * ARGUMENTS:
 *
 * conn_p - Our connection pointer.
 */
static	unsigned long	get_info(conn_t *conn_p)
{
  int		ret, sd;
  unsigned long	read_n, write_n;
  unsigned long	read_bytes_n, write_bytes_n, tot_byte_n = 0;
  
  ret = conn_info(conn_p, &read_bytes_n, &write_bytes_n, &read_n, &write_n,
		  &sd);
  if (ret == CONN_ERROR_NONE) {
    if (! quiet_b) {
      (void)printf("Read %ld bytes with %ld reads.  "
		   "Wrote %ld bytes with %ld writes.  Used sd %d.\n",
		   read_bytes_n, read_n, write_bytes_n, write_n, sd);
    }
    tot_byte_n = read_bytes_n + write_bytes_n;
  }
  else {
    (void)fprintf(stderr, "Could not get info about connection: %s\n",
		  conn_strerror(ret));
    /* not critical */
  }
  
  return tot_byte_n;
}

/************************** client/server routines ***************************/

/*
 * me_client
 *
 * DESCRIPTION:
 *
 * Be the client side.
 *
 * RETURNS:
 *
 * Number of bytes written and read through connection.
 *
 * ARGUEMNTS:
 *
 * host - Remote host to connect to.
 *
 * port - Remote port to connect to.
 *
 * buf_size - Buffer size to use.
 *
 * io_timeout - Milliseconds timeout i/o with the remote connection.
 *
 * iter_n - Number of test iterations to run.
 */
static	unsigned long	me_client(const char *host, const unsigned short port,
				  const int buf_size, const int io_timeout,
				  const int iter_n)
{
  int		ret, iter_c;
  unsigned long	byte_n;
  conn_t	*conn_p;
  
  /* connect our client side */
  conn_p = conn_client(host, port, buf_size, io_timeout, io_timeout, 0, &ret);
  if (conn_p == NULL) {
    (void)fprintf(stderr, "Connecting to '%s:%d' failed: %s\n",
		  host, port, conn_strerror(ret));
    exit(1);
  }
  
  if (verbose_b) {
    dump_types(conn_p);
  }
  
  /* check remote magic */
  ret = conn_check_magic(conn_p, "hello there", -1, io_timeout);
  if (ret != CONN_ERROR_NONE) {
    (void)fprintf(stderr, "Check magic failed: %s\n", conn_strerror(ret));
    exit(1);
  }

  /* first send our iteration count */
  ret = conn_send_data(conn_p, io_timeout,
		       CONN_TYPE_INT, 1, &iter_n,
		       CONN_TYPE_LAST);
  if (ret != CONN_ERROR_NONE) {
    (void)fprintf(stderr, "Could not send iteration count: %s\n",
		  conn_strerror(ret));
    exit(1);
  }
  
  /* write a bunch */
  for (iter_c = 0; iter_c < iter_n; iter_c++) {
    send_data(conn_p, io_timeout);
  }
  flush_data(conn_p, io_timeout);
  
  /* now read a bunch */
  for (iter_c = 0; iter_c < iter_n; iter_c++) {
    receive_data(conn_p, io_timeout);
  }
  
  send_data(conn_p, io_timeout);
  flush_data(conn_p, io_timeout);
  send_data(conn_p, io_timeout);
  flush_data(conn_p, io_timeout);
  
  receive_data(conn_p, io_timeout);
  
  send_data(conn_p, io_timeout);
  flush_data(conn_p, io_timeout);
  
  byte_n = get_info(conn_p);
  
  ret = conn_close(conn_p);
  if (ret != CONN_ERROR_NONE) {
    (void)fprintf(stderr, "Could not close connection: %s\n",
		  conn_strerror(ret));
    exit(1);
  }
  
  return byte_n;
}

/*
 * me_server
 *
 * DESCRIPTION:
 *
 * Be the server
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUEMNTS:
 *
 * port - Port to listen on for connections.
 *
 * serve_timeout - Milliseconds timeout i/o waiting for a connection.
 *
 * buf_size - Buffer size to use.
 *
 * io_timeout - Milliseconds timeout i/o with the remote connection.
 */
static	void	me_server(const unsigned short port, const int serve_timeout,
			  const int buf_size, const int io_timeout)
{
  int		ret, iter_c, iter_n;
  conn_t	*conn_p, *accept_p;
  char		client[64];
  
  /* create the server socket */
  conn_p = conn_server(port, DEFAULT_LISTEN_QUEUE, buf_size, &ret);
  if (conn_p == NULL) {
    (void)fprintf(stderr, "Could not serve on port %d: %s\n",
		  port, conn_strerror(ret));
    exit(1);
  }
  
  while (1) {
    /* now accept a client connection */
    accept_p = conn_accept(conn_p, 0, serve_timeout, io_timeout,
			   client, sizeof(client), &ret);
    if (accept_p == NULL) {
      (void)fprintf(stderr, "Could not accept from port %d: %s\n",
		    port, conn_strerror(ret));
      continue;
    }
    if (! quiet_b) {
      (void)printf("Connected to %s\n", client);
    }
    
    /* do the testing */
    if (verbose_b) {
      dump_types(accept_p);
    }
    
    /* check remote magic */
    ret = conn_check_magic(accept_p, "hello there", -1, io_timeout);
    if (ret != CONN_ERROR_NONE) {
      (void)fprintf(stderr, "Check magic failed: %s\n", conn_strerror(ret));
      exit(1);
    }
    
    /* first send our iteration count */
    ret = conn_receive_data(accept_p, io_timeout,
			    CONN_TYPE_INT, 0, sizeof(iter_n), NULL, &iter_n,
			    CONN_TYPE_LAST);
    if (ret != CONN_ERROR_NONE) {
      (void)fprintf(stderr, "Could not receive iteration count: %s\n",
		    conn_strerror(ret));
      exit(1);
    }
    
    for (iter_c = 0; iter_c < iter_n; iter_c++) {
      receive_data(accept_p, io_timeout);
    }
    for (iter_c = 0; iter_c < iter_n; iter_c++) {
      send_data(accept_p, io_timeout);
    }
    flush_data(accept_p, io_timeout);
    
    receive_data(accept_p, io_timeout);
    receive_data(accept_p, io_timeout);
    
    send_data(accept_p, io_timeout);
    flush_data(accept_p, io_timeout);
    
    receive_data(accept_p, io_timeout);
    
    (void)get_info(accept_p);
    
    ret = conn_close(accept_p);
    if (ret != CONN_ERROR_NONE) {
      (void)fprintf(stderr, "Could not close accepted connection: %s\n",
		    conn_strerror(ret));
      (void)conn_close(conn_p);
      exit(1);
    }
    
    if (verbose_b) {
      printf("------------------------------------------------\n");
    }
  }
  
  ret = conn_close(conn_p);
  if (ret != CONN_ERROR_NONE) {
    (void)fprintf(stderr, "Could not close server connection: %s\n",
		  conn_strerror(ret));
    exit(1);
  }
}

int	main(int argc, char ** argv)
{
  int			client_b = 0, server_b = 0;
  char			*host = NULL;
  int			buffer_size = 0;
  int			serve_timeout = SERVE_TIMEOUT;
  int			iter_n = DEFAULT_ITER;
  int			io_timeout = IO_TIMEOUT;
  unsigned short	port = DEFAULT_PORT;
  unsigned long		byte_n;
  time_t		when;
  
  argc--, argv++;
  
  /* process the args */
  for (; *argv != NULL; argv++, argc--) {
    if (**argv != '-') {
      continue;
    }
    
    switch (*(*argv + 1)) {
      
    case 'b':
      argv++, argc--;
      if (argc == 0) {
	usage();
      }
      buffer_size = atoi(*argv);
      break;
      
    case 'c':
      client_b = 1;
      argv++, argc--;
      if (argc == 0) {
	usage();
      }
      host = *argv;
      break;
      
    case 'i':
      argv++, argc--;
      if (argc == 0) {
	usage();
      }
      iter_n = atoi(*argv);
      break;
      
    case 't':
      argv++, argc--;
      if (argc == 0) {
	usage();
      }
      io_timeout = atoi(*argv);
      break;
      
    case 'p':
      argv++, argc--;
      if (argc == 0) {
	usage();
      }
      port = atoi(*argv);
      break;
      
    case 's':
      argv++, argc--;
      if (argc == 0) {
	usage();
      }
      serve_timeout = atoi(*argv);
      server_b = 1;
      break;
      
    case 'q':
      quiet_b = 1;
      break;
      
    case 'v':
      verbose_b = 1;
      break;
      
    default:
      usage();
      break;
    }
  }
  
  if (argc > 0) {
    usage();
  }
  if (! (client_b || server_b)) {
    usage();
  }
  
  (void)signal(SIGPIPE, SIG_IGN);
  
  if (client_b) {
    /* time and print the data rate for the client */
    when = time(NULL);
    byte_n = me_client(host, port, buffer_size, io_timeout, iter_n);
    when = time(NULL) - when;
    (void)printf("Total I/O: %ld bytes over %d secs or %ld bytes/sec\n",
		 byte_n, (int)when,
		 (when == 0 ? (byte_n / 1) : (byte_n / when)));
  }
  else {
    me_server(port, serve_timeout, buffer_size, io_timeout);
  }
  
  exit(0);
}
