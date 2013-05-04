/*
 * Generic client/server connection handler...
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
 * $Id: conn_loc.h,v 1.10 2000/03/09 03:37:47 gray Exp $
 */

/*
 * Header file for CONN routines
 */
 
#ifndef __CONN_LOC_H__
#define __CONN_LOC_H__

#define CONN_T_MAGIC		0xBEEF1997	/* conn_t structure magic */

/* initial connection magic and size */
#define CONN_STREAM_MAGIC	"\121q\128\212jf0\312"
#define CONN_STREAM_MAGIC_LEN	(sizeof(CONN_STREAM_MAGIC) - 1)

#define CONN_MAX_TYPE_SIZE	64		/* max size of type in bytes */
#define CONN_DEFAULT_IO_BUFFER	1024		/* default i/o buffer */
#define CONN_MAX_DECIMAL_SIZE	64		/* max decimal size in bytes */

/*
 * bitflag tools for Variable and a Flag
 */
#define BIT_FLAG(x)		(1 << (x))
#define BIT_SET(v,f)		(v) |= (f)
#define BIT_CLEAR(v,f)		(v) &= ~(f)
#define BIT_IS_SET(v,f)		((v) & (f))
#define BIT_TOGGLE(v,f)		(v) ^= (f)

#define SET_POINTER(pnt, val) \
	do { \
	  if ((pnt) != NULL) { \
	    (*(pnt)) = (val); \
          } \
        } while(0)

/*
 * Connection flags
 */
#define CONN_CLIENT			BIT_FLAG(0)	/* client connection */
#define CONN_SERVER			BIT_FLAG(1)	/* server connection */
#define CONN_ACCEPTED			BIT_FLAG(2)	/* accept connection */
#define CONN_AUTHENTICATED		BIT_FLAG(3)	/* connection authed */
#define CONN_NET_ORDER			BIT_FLAG(4)	/* data in net order */

/*
 * Bit flags to verify each type.
 */
#define CONN_ALLOW_CHAR			BIT_FLAG(0)	/* character */
#define CONN_ALLOW_UNSIGNED_CHAR	BIT_FLAG(1)	/* unsigned char */
#define CONN_ALLOW_SHORT		BIT_FLAG(2)	/* short integer */
#define CONN_ALLOW_UNSIGNED_SHORT	BIT_FLAG(3)	/* unsigned short int*/
#define CONN_ALLOW_INT			BIT_FLAG(4)	/* integer */
#define CONN_ALLOW_UNSIGNED_INT		BIT_FLAG(5)	/* unsigned integer */
#define CONN_ALLOW_LONG			BIT_FLAG(6)	/* long integer */
#define CONN_ALLOW_UNSIGNED_LONG	BIT_FLAG(7)	/* unsigned long int */
#define CONN_ALLOW_FLOAT		BIT_FLAG(8)	/* floating point num*/
#define CONN_ALLOW_DOUBLE		BIT_FLAG(9)	/* 2x precision float*/

/*
 * Type sample values
 */
#define CONN_SAMPLE_CHAR		((char)'a')
#define CONN_SAMPLE_UNSIGNED_CHAR	((unsigned char)'\341')
#define CONN_SAMPLE_SHORT		((short)32666)
#define CONN_SAMPLE_UNSIGNED_SHORT	((unsigned short)65444U)
#define CONN_SAMPLE_INT			((int)2144444444)
#define CONN_SAMPLE_UNSIGNED_INT	((unsigned int)4222222222U)
#define CONN_SAMPLE_LONG		((long)2133333333L)
#define CONN_SAMPLE_UNSIGNED_LONG	((unsigned long)4294444444LU)
#define CONN_SAMPLE_FLOAT		((float)12.34567)
#define CONN_SAMPLE_DOUBLE		((double)1234.56789012345)

/* connection structure */
typedef struct {
  unsigned int		co_magic;		/* magic number for struct */
  char			*co_host;		/* remote hostname or NULL */
  unsigned short	co_port;		/* port we are connected to */
  int			co_socket;		/* socket for connection */
  unsigned int		co_flags;		/* flags for the connection */
  int			co_send_recv_b;		/* use send/recv? separate */
  unsigned int		co_types;		/* allowable types */
  char			*co_encrypt;		/* enscryption string */
  char			*co_encrypt_pos_p;	/* position in encrypt str */
  char			*co_encrypt_max_p;	/* max pointer past encrypt */
  
  unsigned long		co_read_bytes_c;	/* count of read bytes */
  unsigned long		co_write_bytes_c;	/* count of written bytes */
  unsigned long		co_read_c;		/* count of read calls */
  unsigned long		co_write_c;		/* count of write calls */
  
  int			co_buffer_size;		/* size of the i/o buffer */
  void			*co_read_buf;		/* i/o buffer */
  void			*co_read_max_p;		/* maximum buffer location */
  void			*co_read_pos_p;		/* our location in the buffer*/
  void			*co_write_buf;		/* i/o buffer */
  void			*co_write_max_p;	/* maximum buffer location */
  void			*co_write_pos_p;	/* our location in the buffer*/
} conn_t;

typedef conn_t	conn_ext_t;			/* for debuggers */

/*
 * Typedef for our type handling routines.
 *
 * DESCRIPTION:
 *
 * Routines of this type handle the sending of the type to the remote
 * side of the connection.
 *
 * RETURNS:
 *
 * Connection error code.
 *
 * ARGUMENTS:
 *
 * conn_p - The connection we are sending the type through.
 *
 * io_timeout - Millisecond timeout value for input/output with the
 * server.
 *
 * do_send - If set to 1 then use the send function instead of the write
 * function.  NT must have this set to 1.
 *
 * data - The type's information which we are sending.
 *
 * data_many - How many of the type that we are sending.
 */
typedef int	(*type_send_t)(conn_t *conn_p, const int io_timeout,
			       const void *data, const unsigned int data_many);

/*
 * to map error to string
 */
typedef struct {
  int		es_error;		/* error number */
  char		*es_string;		/* assocaited string */
} error_str_t;

static	error_str_t	errors[] = {
  { CONN_ERROR_NONE,		"no error" },
  { CONN_ERROR_ALLOC,		"memory allocation error" },
  { CONN_ERROR_ARG_NULL,	"important argument was null" },
  { CONN_ERROR_ARG_INVALID,	"important argument was invalid" },
  { CONN_ERROR_TYPE_ARG,	"type argument is invalid" },
  { CONN_ERROR_TYPE_SYNC,	"type argument does not match remote" },
  { CONN_ERROR_TYPE_ALLOW,	"type argument is not allowed" },
  { CONN_ERROR_MANY_ARG,	"many argument is invalid" },
  { CONN_ERROR_MANY_SYNC,	"many argument does not match remote" },
  { CONN_ERROR_MANY_UNKNOWN,	"must know how many with no alloc flag" },
  { CONN_ERROR_PNT,		"invalid connection structure argument" },
  { CONN_ERROR_TIMEOUT,		"i/o timeout was exceeded" },
  { CONN_ERROR_SOCKET,		"could not create socket" },
  { CONN_ERROR_SOCKETOPTS,	"could not set socket options" },
  { CONN_ERROR_NONBLOCK,	"setting socket to non-blocking failed" }, 
  { CONN_ERROR_HOSTNAME,	"could not lookup hostname" },
  { CONN_ERROR_CONNECT,		"could not connect to remote" },
  { CONN_ERROR_BIND,		"could not bind socket to port" },
  { CONN_ERROR_LISTEN,		"could not listen on socket" },
  { CONN_ERROR_SELECT,		"could not select on server socket" },
  { CONN_ERROR_ACCEPT,		"could not accept on server socket" },
  { CONN_ERROR_SOCKET_CLOSED,	"socket closed prematurely" },
  { CONN_ERROR_MAGIC,		"improper connection magic" },
  { CONN_ERROR_SYNCHRONIZE,	"could not synchronize connect with remote" },
  { CONN_ERROR_NEED_TYPES,	"needed types didn't match with remote" },
  { CONN_ERROR_TYPE_INVALID,	"type on remote host is invalid" },
  { CONN_ERROR_TYPE_SIZE,	"type on remote host has different size" },
  { CONN_ERROR_TYPE_DATA,	"type on remote host has different data" },
  { CONN_ERROR_AUTHENTICATE,	"could not authenticate with remote" },
  { CONN_ERROR_CLIENT,		"improper use of client structure" },
  { CONN_ERROR_SERVER,		"improper use of server structure" },
  { CONN_ERROR_REMOTE_READ,	"problems reading from remote" },
  { CONN_ERROR_REMOTE_WRITE,	"problems writing to remote" },
  { CONN_ERROR_REMOTE_DATA,	"improper data from remote" },
  { 0 }
};

#define INVALID_ERROR	"invalid error code"

#endif /* __CONN_LOC_H__ */
