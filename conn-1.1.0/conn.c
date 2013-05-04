/*
 * Generic client/server connection handler...
 *
 * Copyright 2000 by Gray Watson.
 *
 * This file is part of the connection library package.
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
 * $Id: conn.c,v 1.17 2000/03/09 03:37:47 gray Exp $
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef unix

#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#else

#include <winsock.h>

#ifndef EWOULDBLOCK
#define EWOULDBLOCK	WSAEWOULDBLOCK
#endif
#ifndef EINPROGRESS
#define EINPROGRESS	WSAEINPROGRESS
#endif

#endif

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define CONN_MAIN

#include "conn.h"
#include "conn_loc.h"

static char *rcs_id =
"$Id: conn.c,v 1.17 2000/03/09 03:37:47 gray Exp $";

/*
 * Version id for the library.  You also need to add an entry to the
 * NEWS and ChangeLog files.
 */
static char *version_id = "$ConnVersion: 1.1.0 September-10-1998 $";

/* local variables */
static	int		enabled_b = 0;			/* module enabled */
static	int		type_sizes[CONN_TYPE_MAX + 1];	/* sizes of types */
static	int		type_flags[CONN_TYPE_MAX + 1];	/* flags for types */

/* type sending/receiving functions for normal data and network byte order */
static	type_send_t	type_send[CONN_TYPE_MAX + 1];
static	type_send_t	type_receive[CONN_TYPE_MAX + 1];

/****************************** rawio routines *******************************/

/*
 * static int rawio_read
 *
 * DESCRIPTION:
 *
 * Read in from a descriptor into a local buffer using either the read
 * or recv routines.
 *
 * WARNING: NT must have the do_recv_b flag set because read() does
 * not exist.
 *
 * RETURNS:
 *
 * Success - The number of bytes read normally.  0 if the write timed
 * out or if it reached EOF.
 *
 * Failure - -1
 *
 * ARGUMENTS:
 *
 * fd - File descriptor to read from.
 *
 * buf - Buffer to read the bytes into.
 *
 * size - Size of the buffer.
 *
 * msecs - Milliseconds to wait for data before returning.  Set to 0
 * if you want to just poll the fd, else set to -1 to hang.
 *
 * do_recv_b - If set to 1 then use the recv function instead of the
 * read function.  NT must have this set to 1.
 *
 * eof_bp - Pointer to an integer that will be set to 1 upon return if
 * read returned 0 on the file descriptor.
 *
 * timeout_bp - Pointer to an integer that will be set to 1 upon
 * return if timed out while reading from the fd.
 */
static	int	rawio_read(const int fd, void *buf, const int size,
			   const int msecs, const int do_recv_b, int *eof_bp,
			   int *timeout_bp)
{
  struct timeval	timeout, *timeout_p;
  fd_set		rw_fds;
  int			ret;
  
  if (msecs < 0) {
    timeout_p = NULL;
  }
  else {
    timeout.tv_sec = msecs / 1000;
    timeout.tv_usec = (msecs % 1000) * 1000;
    timeout_p = &timeout;
  }
  
  if (eof_bp != NULL) {
    *eof_bp = 0;
  }
  if (timeout_bp != NULL) {
    *timeout_bp = 0;
  }
  
  FD_ZERO(&rw_fds);
  
  for (;;) {
    FD_SET(fd, &rw_fds);
    
    /* make sure we won't block when we read from fd */
    ret = select(fd + 1, &rw_fds, NULL, NULL, timeout_p);
    if (ret == -1) {
      if (errno == EINTR) {
	continue;
      }
      break;
    }
    if (ret == 0) {
      if (timeout_bp != NULL) {
	*timeout_bp = 1;
      }
      break;
    }
    
    /* huh?  this is unlikely but we will continue instread of breaking out */
    if (! FD_ISSET(fd, &rw_fds)) {
      continue;
    }
    
    /* try to read in some data */
    if (do_recv_b) {
      ret = recv(fd, buf, size, 0);
    }
    else {
#ifdef unix
      ret = read(fd, buf, size);
#else
      /* NOTE: NT does not have read */
      ret = -1;
      break;
#endif
    }
    if (ret < 0) {
      if (errno == EAGAIN) {
	continue;
      }
    }
    else if (ret == 0) {
      if (eof_bp != NULL) {
	*eof_bp = 1;
      }
    }
    
    break;
  }
  
  return ret;
}

/*
 * static int rawio_write
 *
 * DESCRIPTION:
 *
 * Write out to a descriptor from a buffer using either the write or
 * send routines.
 *
 * WARNING: NT must have the do_send_b flag set because write() does
 * not exist.
 *
 * RETURNS:
 *
 * Success - The number of bytes written normally.
 *
 * Failure - 0 if the write timed out.  -1 on error.
 *
 * ARGUMENTS:
 *
 * fd - File descriptor to write.
 *
 * buf - Buffer to read the bytes into.
 *
 * size - Size of the buffer.
 *
 * msecs - Milliseconds to wait for writability before returning.  Set
 * to 0 if you want to just poll the fd, else set to -1 to hang.
 *
 * do_send_b - If set to 1 then use the send function instead of the
 * write function.  NT must have this set to 1.
 */
static	int	rawio_write(const int fd, const void *buf, const int size,
			    const int msecs, const int do_send_b)
{
  struct timeval	timeout, *timeout_p;
  fd_set		wr_fds;
  const char		*buf_p = buf;
  int			ret;
  
  if (msecs < 0) {
    timeout_p = NULL;
  }
  else {
    timeout.tv_sec = msecs / 1000;
    timeout.tv_usec = (msecs % 1000) * 1000;
    timeout.tv_usec = 0;
    timeout_p = &timeout;
  }
  
  FD_ZERO(&wr_fds);
  
  for (;;) {
    FD_SET(fd, &wr_fds);
    
    /* make sure we won't block when we write to fd */
    ret = select(fd + 1, NULL, &wr_fds, NULL, timeout_p);
    if (ret == -1) {
      if (errno == EINTR) {
	continue;
      }
      break;
    }
    if (ret == 0) {
      break;
    }
    
    /* huh?  this is unlikely but we will continue instread of breaking out */
    if (! FD_ISSET(fd, &wr_fds)) {
      continue;
    }
    
    /* try to write some data */
    if (do_send_b) {
      ret = send(fd, buf_p, size - (buf_p - (char *)buf), 0);
    }
    else {
#ifdef unix
      ret = write(fd, buf_p, size - (buf_p - (char *)buf));
#else
      /* NOTE: NT does not have write */
      ret = -1;
      break;
#endif
    }
    if (ret < 0) {
      if (errno == EAGAIN) {
	continue;
      }
      break;
    }
    if (ret == 0) {
      continue;
    }
    
    buf_p += ret;
    ret = buf_p - (char *)buf;
    if (ret >= size) {
      break;
    }
  }
  
  return ret;
}

/*
 * static int rawio_write_size
 *
 * DESCRIPTION:
 *
 * Write out to a descriptor from a buffer using either the write or
 * send routines.  It will write until it sends a certain number of
 * bytes.
 *
 * WARNING: NT must have the do_send_b flag set because write() does
 * not exist.
 *
 * RETURNS:
 *
 * Success - The number of bytes written normally.
 *
 * Failure - -1
 *
 * ARGUMENTS:
 *
 * fd - File descriptor to write.
 *
 * buf - Buffer to read the bytes into.
 *
 * size - Size of the item to write.
 *
 * msecs - Milliseconds to wait for writability before returning.  Set
 * to 0 if you want to just poll the fd, else set to -1 to hang.
 *
 * do_send_b - If set to 1 then use the send function instead of the
 * write function.  NT must have this set to 1.
 *
 * timeout_bp - Pointer to a boolean that will be set to 1 upon return
 * if the timeout value was reached before all of the bytes were read.
 */
static	int	rawio_write_size(const int fd, const void *buf, const int size,
				 const int msecs, const int do_send_b,
				 int *timeout_bp)
{
  const char	*buf_p, *max_p;
  int		ret;
  
  buf_p = buf;
  max_p = buf_p + size;
  
  if (timeout_bp != NULL) {
    *timeout_bp = 0;
  }
  
  while (buf_p < max_p) {
    
    /* write to fd */
    ret = rawio_write(fd, buf_p, max_p - buf_p, msecs, do_send_b);
    if (ret < 0) {
      return ret;
    }
    if (ret == 0) {
      if (timeout_bp != NULL) {
	*timeout_bp = 1;
      }
      break;
    }
    buf_p += ret;
  }
  
  return buf_p - (char *)buf;
}

/****************************** socket routines ******************************/

/*
 * static int socket_close
 *
 * DESCRIPTION:
 *
 * Close a socket descriptor.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * sd - Socket descriptor we are closing.
 */
static	int	socket_close(const int sd)
{
#ifdef unix
  (void)close(sd);
#else
# ifdef WIN32
  (void)closesocket(sd);
# else
  ERROR No supported close functions for this system type;
# endif
#endif
  
  return CONN_ERROR_NONE;
}

/*
 * static int socket_create
 *
 * DESCRIPTION:
 *
 * Create a socket descriptor for our connection structure.
 *
 * RETURNS:
 *
 * Success - New socket descriptor.
 *
 * Failure - -1
 *
 * ARGUMENTS:
 *
 * None.
 */
static	int	socket_create(void)
{
  int	sd;
  
  /* create the socket */
  sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sd < 0) {
    return -1;
  }
  
  return sd;
}

/*
 * static int socket_nonblock
 *
 * DESCRIPTION:
 *
 * Set a socket descriptor to be non-blocking.
 *
 * RETURNS:
 *
 * Success - 0
 *
 * Failure - -1
 *
 * ARGUMENTS:
 *
 * sd - Socket descriptor that we are setting.
 */
static	int	socket_nonblock(const int sd)
{
  /*
   * Set the socket up as non-blocking.
   */
#ifdef unix
  {
    int		flags;
    
    flags = fcntl(sd, F_GETFL, 0);
    if (flags >= 0) {
      flags |= O_NONBLOCK;
    }
    
    /*
     * NOTE: We do it this way so we don't have to repeat our cleanup
     * code.  Also, fcntl with F_SETFL returns -1 on error not < 0.
     */
    if (flags < 0 || fcntl(sd, F_SETFL, flags) == -1) {
      return -1;
    }
  }
#endif
#ifdef WIN32
  {
    int	one = 1;
    if (ioctlsocket(sd, FIONBIO, &one) != 0) {
      return -1;
    }
  }
#endif
  
  return 0;
}

/*
 * static int socket_configure
 *
 * DESCRIPTION:
 *
 * Configure a new socket.
 *
 * RETURNS:
 *
 * Success - 0
 *
 * Failure - -1
 *
 * ARGUMENTS:
 *
 * sd - Socket descriptor which we are configuring.
 */
static	int	socket_configure(const int sd)
{
  int	one;
  
  one = 1;
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *) &one,
		 sizeof(one)) != 0) {
    return -1;
  }
  
  /*
   * Set the keep alive which will allow us to determine if the server
   * at the other end has gone south for the winter.  On write we get
   * a EPIPE errno, on read we get an end of file.
   */
  one = 1;
  if (setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, (void *)&one,
		 sizeof(one)) != 0) {
    return -1;
  }
  
  return 0;
}

/*
 * static int socket_bind
 *
 * DESCRIPTION:
 *
 * Bind the socket to an address and port.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * sock - File descriptor connection to the server.
 *
 * port - Port number to listen for connections.
 *
 * listen_queue - Number to pass to listen to set up the connection
 * queue for our socket.
 */
static	int	socket_bind(const int sd, const unsigned short port,
			    const int listen_queue)
{
  struct sockaddr_in	server;
  
  /*
   * bind to a specific address or all
   */
  memset((char *) &server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(port);
  
  /* bind to the port */
  if (bind(sd, (struct sockaddr *) &server, sizeof(server)) == -1) {
    return CONN_ERROR_BIND;
  }
  
  /* listen for connections */
  if (listen(sd, listen_queue) != 0) {
    return CONN_ERROR_LISTEN;
  }
  
  return CONN_ERROR_NONE;
}

/*
 * static int socket_connect
 *
 * DESCRIPTION:
 *
 * Connect the socket to a remote address.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * sock - File descriptor connection to the server.
 *
 * addr_p - Pointer to the address to connect the socket.
 *
 * connect_timeout - Millisecond timeout value for input/output with
 * the remote host.
 */
static	int	socket_connect(const int sd,
			       const struct sockaddr_in *addr_p,
			       const unsigned int connect_timeout)
{
  struct timeval	timeout, *timeout_p;
  fd_set		read_fds, write_fds;
  int			ret, error, len, final;
  
  /* now make the connection */
  if (connect(sd, (struct sockaddr *)addr_p,
	      sizeof(struct sockaddr_in)) == 0) {
    return CONN_ERROR_NONE;
  }
  
#ifdef WIN32
  /* set the errno here */
  errno = WSAGetLastError();
#endif
  
  /* not waiting for connection? */
  if (errno != EINPROGRESS && errno != EWOULDBLOCK
#ifdef __sparc__
      /*
       * HACK: it looks like the sparc does not propagate the errno
       * correctly in pthread mode.  Ugh.
       */
      && errno != 0
#endif
      ) {
    return CONN_ERROR_CONNECT;
  }
  
  if (connect_timeout < 0) {
    timeout_p = NULL;
  }
  else {
    timeout_p = &timeout;
  }
  
  FD_ZERO(&read_fds);
  FD_ZERO(&write_fds);
  
  FD_SET(sd, &read_fds);
  FD_SET(sd, &write_fds);
  
  /* NOTE: we set the timeout here in case the select modifies it below */
  if (connect_timeout >= 0) {
    timeout.tv_sec = connect_timeout / 1000;
    timeout.tv_usec = (connect_timeout % 1000) * 1000;
  }
  
  /* selecting on a write implements asynchronous connections */
  ret = select(sd + 1, &read_fds, &write_fds, 0L, timeout_p);
  
  /*
   * NOTE: so according to Richael Stevens' Unix Network Programming
   * book, if the socket is ready to read and write then the
   * connection got an error.  If it is ready to just write then the
   * connection was established.  It seems to me that if the remote
   * host wrote data immediately, the socket may have both the read
   * and write set.  In his test code, he checks for either and then
   * uses getsockopt to check on the status of the connection.
   */
  if (FD_ISSET(sd, &read_fds) || FD_ISSET(sd, &write_fds)) {
    len = sizeof(error);
    ret = getsockopt(sd, SOL_SOCKET, SO_ERROR, (void *)&error, &len);
    if (ret == 0) {
      if (error == 0) {
	final = CONN_ERROR_NONE;
      }
      else {
	final = CONN_ERROR_CONNECT;
	errno = error;
      }
    }
    else {
      /* Solaris returns -1 because the sd isn't valid I guess */
      final = CONN_ERROR_CONNECT;
    }
  }
  else if (ret == 0) {
    /* did we get a timeout? */
    final = CONN_ERROR_TIMEOUT;
  }
  else if (ret < 0) {
    /* a connect error? */
    final = CONN_ERROR_CONNECT;
  }
  else {
    /* a select error? */
    final = CONN_ERROR_SELECT;
  }
  
  return final;
}

/*
 * static int host_connect
 *
 * DESCRIPTION:
 *
 * Connect our socket to a host.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * sd - Socket descriptor we are connection to the server.
 *
 * host - Hostname of the server we are connection to.
 *
 * port - Port number on the server that we are connection.
 *
 * connect_timeout - Milleseconds timeout value to wait for the
 * connection.
 */
static	int	host_connect(const int sd, const char *host,
			     const unsigned short port,
			     const unsigned int connect_timeout)
{
  struct sockaddr_in	sock_addr;
  struct hostent	*host_p;
  char			**host_addr;
  int			ret, final = CONN_ERROR_CONNECT;
  
  /*
   * Now initialize our address structure
   */
  memset((char *)&sock_addr, 0, sizeof(struct sockaddr_in));
  sock_addr.sin_family = AF_INET;
  sock_addr.sin_port = htons((u_short)port);
  
  /* lookup our hostname */
  host_p = gethostbyname(host);
  
  /* verify the hostname structure we got */
  if (host_p == NULL || host_p->h_addr_list == NULL) {
    return CONN_ERROR_HOSTNAME;
  }
  
  /* run through and try each host address entry */
  for (host_addr = host_p->h_addr_list; *host_addr != NULL; host_addr++) {
    
    /* copy the address into the socket structure */
    memcpy((char *)&sock_addr.sin_addr.s_addr, *host_addr, host_p->h_length);
    
    /*
     * Now make the connection.  NOTE: this will fail to go to the
     * next address if your asynchronous connections are broken --
     * like Sun.
     */
    ret = socket_connect(sd, &sock_addr, connect_timeout);
    if (ret != CONN_ERROR_CONNECT) {
      /* if no error or timeout then we break */
      final = ret;
      break;
    }
  }
  
  return final;
}

/**************************** structure routines *****************************/

/*
 * static conn_t *conn_alloc
 *
 * DESCRIPTION:
 *
 * Allocate a new conn structure.
 *
 * RETURNS:
 *
 * Success - Valid conn_t structure that needs to be passed to
 * conn_free to deallocate.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * host - Name of the server host we are connecting.
 *
 * port - Port number on the server that we are connection.
 *
 * sd - Socket descriptor to associate with the new connection
 * structure.  If -1 then build a socket for it.
 *
 * flags - Some initial flags to set.
 *
 * send_recv_b - If set to 1 then use the send and recv functions
 * instead of the read and write functions.  NT must have this set to
 * 1.
 *
 * buffer_size - Size of our internal i/o buffer.  Set to 0 to use the
 * library default of 1k.
 *
 * error_p - Pointer to an integer which upon return contains a conn
 * error-code.
 */
static	conn_t	*conn_alloc(const char *host, const unsigned short port,
			    const int sd, const unsigned int flags,
			    const int send_recv_b, const int buffer_size,
			    int *error_p)
{
  conn_t	*conn_p;
  
  /* allocate the connection structure */
  conn_p = (conn_t *)calloc(1, sizeof(conn_t));
  if (conn_p == NULL) {
    SET_POINTER(error_p, CONN_ERROR_ALLOC);
    return NULL;
  }
  
  /* initialize the connection structure */
  conn_p->co_magic = CONN_T_MAGIC;
  conn_p->co_port = port;
  conn_p->co_socket = -1;
  conn_p->co_flags = flags;
  conn_p->co_send_recv_b = send_recv_b;
  
  /* allocate the host name */
  if (host == NULL) {
    conn_p->co_host = NULL;
  }
  else {
    conn_p->co_host = strdup(host);
    if (conn_p->co_host == NULL) {
      (void)conn_close(conn_p);
      SET_POINTER(error_p, CONN_ERROR_ALLOC);
      return NULL;
    }
  }
  
  /* create the socket */
  if (sd >= 0) {
    conn_p->co_socket = sd;
  }
  else {
    conn_p->co_socket = socket_create();
    if (conn_p->co_socket < 0) {
      (void)conn_close(conn_p);
      SET_POINTER(error_p, CONN_ERROR_SOCKET);
      return NULL;
    }
  }
  
  /* now configure the socket */
  if (socket_configure(conn_p->co_socket) != 0) {
    (void)conn_close(conn_p);
    SET_POINTER(error_p, CONN_ERROR_SOCKETOPTS);
    return NULL;
  }
  
  /* set the socket up to be non-blocking */
  if (socket_nonblock(conn_p->co_socket) != 0) {
    (void)conn_close(conn_p);
    SET_POINTER(error_p, CONN_ERROR_NONBLOCK);
    return NULL;
  }
  
  /* allocate our i/o buffers and set our buffer pointers */
  conn_p->co_buffer_size = buffer_size;
  
  if (buffer_size == 0) {
    conn_p->co_read_buf = NULL;
    conn_p->co_read_max_p = NULL;
    conn_p->co_read_pos_p = NULL;
    
    conn_p->co_write_buf = NULL;
    conn_p->co_write_max_p = NULL;
    conn_p->co_write_pos_p = NULL;
  }
  else {
    /* allocate our read buffer */
    conn_p->co_read_buf = malloc(buffer_size);
    if (conn_p->co_read_buf == NULL) {
      (void)conn_close(conn_p);
      SET_POINTER(error_p, CONN_ERROR_ALLOC);
      return NULL;
    }
    conn_p->co_read_max_p = (char *)conn_p->co_read_buf + buffer_size;
    conn_p->co_read_pos_p = conn_p->co_read_buf;
    
    /* allocate our write buffer */
    conn_p->co_write_buf = malloc(buffer_size);
    if (conn_p->co_write_buf == NULL) {
      (void)conn_close(conn_p);
      SET_POINTER(error_p, CONN_ERROR_ALLOC);
      return NULL;
    }
    conn_p->co_write_max_p = (char *)conn_p->co_write_buf + buffer_size;
    conn_p->co_write_pos_p = conn_p->co_write_buf;
  }
  
  return conn_p;
}

/****************************** buffer routines ******************************/

/*
 * static int add_chars
 *
 * DESCRIPTION:
 *
 * Add characters to a connection i/o buffer possibly writing out
 * characters to the socket if necessary.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * conn_p - Connection structure that we will be adding data to.
 *
 * data - Data that we are adding into the buffer.
 *
 * len - Length of the data that we are adding to the buffer.
 *
 * io_timeout - Milleseconds timeout value to wait for any i/o.
 */
static	int	add_chars(conn_t *conn_p, const void *data, const int len,
			  const int io_timeout)
{
  const void	*data_p = data;
  int		ret, left = len, empty, timeout_b;
  
  empty = (char *)conn_p->co_write_max_p - (char *)conn_p->co_write_pos_p;
  
  while (left > 0) {
    
    /* do we need to flush the buffer? */
    if (conn_p->co_write_pos_p == conn_p->co_write_max_p) {
      /*
       * NOTE: we write the entire buffer here to cut down on the
       * buffer rearrangement.  This semms to be faster than doing a
       * rawio_write.
       */
      conn_p->co_write_c++;
      conn_p->co_write_bytes_c += conn_p->co_buffer_size;
      ret = rawio_write_size(conn_p->co_socket, conn_p->co_write_buf,
			     conn_p->co_buffer_size, io_timeout,
			     conn_p->co_send_recv_b, &timeout_b);
      if (ret != conn_p->co_buffer_size) {
	if (timeout_b) {
	  return CONN_ERROR_TIMEOUT;
	}
	else if (ret < 0) {
	  return CONN_ERROR_REMOTE_WRITE;
	}
      }
      /* did we write out the entire buffer */
      conn_p->co_write_pos_p = conn_p->co_write_buf;
      empty = conn_p->co_buffer_size;
    }
    
    /* how much space to we have? */
    if (empty >= left) {
      memcpy(conn_p->co_write_pos_p, data_p, left);
      conn_p->co_write_pos_p = (char *)conn_p->co_write_pos_p + left;
      /* we are done with the buffer so we can quit */
      break;
    }
    else {
      memcpy(conn_p->co_write_pos_p, data_p, empty);
      conn_p->co_write_pos_p = (char *)conn_p->co_write_pos_p + empty;
      left -= empty;
      data_p = (char *)data_p + empty;
      empty = 0;
    }
  }
  
  return CONN_ERROR_NONE;
}

/*
 * static int get_chars
 *
 * DESCRIPTION:
 *
 * Get characters from a connection i/o buffer possibly reading
 * characters from the socket if the i/o buffer does not contain
 * enough.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * conn_p - Connection structure that we will be getting data from.
 *
 * data - Pointer to a buffer where we will put the data that we are
 * getting from the buffer.
 *
 * len - Length of the data that we are adding to the buffer.
 *
 * io_timeout - Milleseconds timeout value to wait for any i/o.
 */
static	int	get_chars(conn_t *conn_p, void *data, const int len,
			  const int io_timeout)
{
  void	*data_p = data;
  int	left = len, empty, full, shift, ret, eof_b, timeout_b;
  
  empty = (char *)conn_p->co_read_max_p - (char *)conn_p->co_read_pos_p;
  full = conn_p->co_buffer_size - empty;
  
  while (left > 0) {
    /*
     * If we don't have enough data, we may want to read from the
     * socket -- as much as we can and at least enough to fill the
     * buffer or satisfy the get.
     */
    while (full < left && empty > 0) {
      conn_p->co_read_c++;
      ret = rawio_read(conn_p->co_socket, conn_p->co_read_pos_p,
		       empty, io_timeout, conn_p->co_send_recv_b,
		       &eof_b, &timeout_b);
      if (ret <= 0) {
	if (eof_b) {
	  return CONN_ERROR_SOCKET_CLOSED;
	}
	else if (timeout_b) {
	  return CONN_ERROR_TIMEOUT;
	}
	else {
	  return CONN_ERROR_REMOTE_READ;
	}
      }
      conn_p->co_read_bytes_c += ret;
      conn_p->co_read_pos_p = (char *)conn_p->co_read_pos_p + ret;
      empty -= ret;
      full += ret;
    }
    
    if (full >= left) {
      if (data_p != NULL) {
	memcpy(data_p, conn_p->co_read_buf, left);
      }
      shift = full - left;
      if (shift > 0) {
	memmove(conn_p->co_read_buf, (char *)conn_p->co_read_buf + left,
		shift);
	conn_p->co_read_pos_p = (char *)conn_p->co_read_buf + shift;
      }
      else {
	conn_p->co_read_pos_p = (char *)conn_p->co_read_buf;
      }
      /* NOTE: we are done with the buffer here */
      break;
    }
    else {
      if (data_p != NULL) {
	memcpy(data_p, conn_p->co_read_buf, full);
	data_p = (char *)data_p + full;
      }
      conn_p->co_read_pos_p = (char *)conn_p->co_read_buf;
      empty += full;
      left -= full;
      full = 0;
    }
  }
  
  return CONN_ERROR_NONE;
}

/*
 * static int write_chars
 *
 * DESCRIPTION:
 *
 * Write any characters in the write buffer out to the socket.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * conn_p - Connection structure that we will are flushing.
 *
 * io_timeout - Millisecond timeout value for input/output with the
 * remote host.
 */
static	int	write_chars(conn_t *conn_p, const int io_timeout)
{
  int	full, timeout_b;
  
  full = (char *)conn_p->co_write_pos_p - (char *)conn_p->co_write_buf;
  
  /* do we need to flush the buffer? */
  if (full > 0) {
    conn_p->co_write_c++;
    conn_p->co_write_bytes_c += full;
    if (rawio_write_size(conn_p->co_socket, conn_p->co_write_buf, full,
			 io_timeout, conn_p->co_send_recv_b,
			 &timeout_b) != full) {
      if (timeout_b) {
	return CONN_ERROR_TIMEOUT;
      }
      else {
	return CONN_ERROR_REMOTE_WRITE;
      }
    }
    conn_p->co_write_pos_p = conn_p->co_write_buf;
  }
  
  return CONN_ERROR_NONE;
}

/***************************** checking routines *****************************/

/*
 * static int check_magic
 *
 * DESCRIPTION:
 *
 * Perform some basic authentication with the server.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * conn_p - Our connection structure.
 *
 * io_timeout - Millisecond timeout value for input/output with the
 * remote host.
 */
static	int	check_magic(conn_t *conn_p, const int io_timeout)
{
  char	magic[CONN_STREAM_MAGIC_LEN];
  int	ret;
  
  /* write our magic */
  ret = add_chars(conn_p, CONN_STREAM_MAGIC, CONN_STREAM_MAGIC_LEN,
		  io_timeout);
  if (ret != CONN_ERROR_NONE) {
    return ret;
  }
  /* now write our information */
  ret = write_chars(conn_p, io_timeout);
  if (ret != CONN_ERROR_NONE) {
    return ret;
  }
  
  /* get characters from the remote side */
  ret = get_chars(conn_p, magic, CONN_STREAM_MAGIC_LEN, io_timeout);
  if (ret != CONN_ERROR_NONE) {
    return ret;
  }
  
  /* did we get enough of the magic string or did the string not match? */
  if (memcmp(CONN_STREAM_MAGIC, magic, CONN_STREAM_MAGIC_LEN) == 0) {
    return CONN_ERROR_NONE;
  }
  else {
    return CONN_ERROR_MAGIC;
  }
}

/*
 * static int verify_type
 *
 * DESCRIPTION:
 *
 * Check a type by exchanging some information with the remote host.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * sd - Socket descriptor connection to the server.
 *
 * type_num - Number of the type we are checking.
 *
 * data_p - Pointer to some sample data that we are writing and
 * hopefully reading back.
 *
 * data_size - Size of the sample type data we are writing.  NOTE: the
 * type of this is unisgned char so the size cannot be more than 256
 * bytes.
 *
 * io_timeout - Millisecond timeout value for input/output with the
 * remote host.
 */
static	int	verify_type(conn_t *conn_p, const unsigned char type_num,
			    const void *data_p, const unsigned char data_size,
			    const int io_timeout)
{
  unsigned char	remote_type_num, remote_data_size, io_buf[CONN_MAX_TYPE_SIZE];
  int		ret;
  
  /* sanity check here */
  if (data_size > CONN_MAX_TYPE_SIZE) {
    return CONN_ERROR_TYPE_INVALID;
  }
  
  /*
   * write type info
   */
  ret = add_chars(conn_p, &type_num, 1, io_timeout);
  if (ret != CONN_ERROR_NONE) {
    return ret;
  }
  ret = add_chars(conn_p, &data_size, 1, io_timeout);
  if (ret != CONN_ERROR_NONE) {
    return ret;
  }
  ret = add_chars(conn_p, data_p, data_size, io_timeout);
  if (ret != CONN_ERROR_NONE) {
    return ret;
  }
  /* now write our type information */
  ret = write_chars(conn_p, io_timeout);
  if (ret != CONN_ERROR_NONE) {
    return ret;
  }
  
  /*
   * read remote type info
   */
  /* get the type number */
  ret = get_chars(conn_p, &remote_type_num, 1, io_timeout);
  if (ret != CONN_ERROR_NONE) {
    return ret;
  }
  
  /* type numbers better match */
  if (remote_type_num != type_num) {
    return CONN_ERROR_SYNCHRONIZE;
  }
  
  /* now get the data size */
  ret = get_chars(conn_p, &remote_data_size, 1, io_timeout);
  if (ret != CONN_ERROR_NONE) {
    return ret;
  }
  
  /* is the size of the remote type too much for us to read? */
  if ((int)remote_data_size > CONN_MAX_TYPE_SIZE) {
    return CONN_ERROR_TYPE_INVALID;
  }
  
  /* read the remote's information for this type */
  ret = get_chars(conn_p, io_buf, (int)remote_data_size, io_timeout);
  if (ret != CONN_ERROR_NONE) {
    return ret;
  }
  
  /* does the type on the remote host have a different size? */
  if (data_size != remote_data_size) {
    return CONN_ERROR_TYPE_SIZE;
  }
  
  /* did our samples match? */
  if (memcmp(io_buf, data_p, data_size) != 0) {
    return CONN_ERROR_TYPE_DATA;
  }
  
  return CONN_ERROR_NONE;
}

/*
 * static int check_types
 *
 * DESCRIPTION:
 *
 * Perform some basic authentication with the server.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * conn_p - Pointer to our connection structure.
 *
 * io_timeout - Millisecond timeout value for input/output with the
 * remote host.
 */
static	int	check_types(conn_t *conn_p, const int io_timeout)
{
  int			ret;
  char			test_char;
  unsigned char		test_unsigned_char;
  short			test_short;
  unsigned short	test_unsigned_short;
  int			test_int;
  unsigned int		test_unsigned_int;
  long			test_long;
  unsigned long		test_unsigned_long;
  float			test_float;
  double		test_double;
  
  /* initialize the type flags */
  conn_p->co_types = 0;
  
  /*
   * Unsigned Character - we do this first because we need unsigned
   * char to do the others.
   */
  test_unsigned_char = CONN_SAMPLE_UNSIGNED_CHAR;
  ret = verify_type(conn_p, CONN_TYPE_UNSIGNED_CHAR, &test_unsigned_char,
		    sizeof(test_unsigned_char), io_timeout);
  if (ret != CONN_ERROR_NONE) {
    /* return all errors because unsigned char is necessary for the rest */
    return ret;
  }
  BIT_SET(conn_p->co_types, CONN_ALLOW_UNSIGNED_CHAR);
  
  test_char = CONN_SAMPLE_CHAR;
  ret = verify_type(conn_p, CONN_TYPE_CHAR, &test_char, sizeof(test_char),
		    io_timeout);
  if (ret == CONN_ERROR_NONE) {
    BIT_SET(conn_p->co_types, CONN_ALLOW_CHAR);
  }
  else if (ret != CONN_ERROR_TYPE_SIZE && ret != CONN_ERROR_TYPE_DATA) {
    return ret;
  }
  
  test_short = CONN_SAMPLE_SHORT;
  ret = verify_type(conn_p, CONN_TYPE_SHORT, &test_short, sizeof(test_short),
		    io_timeout);
  if (ret == CONN_ERROR_NONE) {
    BIT_SET(conn_p->co_types, CONN_ALLOW_SHORT);
  }
  else if (ret != CONN_ERROR_TYPE_SIZE && ret != CONN_ERROR_TYPE_DATA) {
    return ret;
  }
  
  /*
   * If the short didn't match then try network byte order
   */
  if (! BIT_IS_SET(conn_p->co_types, CONN_ALLOW_SHORT)) {
    
    test_short = htons(CONN_SAMPLE_SHORT);
    ret = verify_type(conn_p, CONN_TYPE_SHORT, &test_short, sizeof(test_short),
		      io_timeout);
    if (ret == CONN_ERROR_NONE) {
      BIT_SET(conn_p->co_types, CONN_ALLOW_SHORT);
    }
    else if (ret != CONN_ERROR_TYPE_SIZE && ret != CONN_ERROR_TYPE_DATA) {
      return ret;
    }
    
    /* if the short now matches then set the net-order flag */
    if (BIT_IS_SET(conn_p->co_types, CONN_ALLOW_SHORT)) {
      BIT_SET(conn_p->co_flags, CONN_NET_ORDER);
    }
  }
  
  /* now go back and retry the short */
  if (BIT_IS_SET(conn_p->co_flags, CONN_NET_ORDER)) {
    test_unsigned_short = htons(CONN_SAMPLE_UNSIGNED_SHORT);
  }
  else {
    test_unsigned_short = CONN_SAMPLE_UNSIGNED_SHORT;
  }
  ret = verify_type(conn_p, CONN_TYPE_UNSIGNED_SHORT, &test_unsigned_short,
		    sizeof(test_unsigned_short), io_timeout);
  if (ret == CONN_ERROR_NONE) {
    BIT_SET(conn_p->co_types, CONN_ALLOW_UNSIGNED_SHORT);
  }
  else if (ret != CONN_ERROR_TYPE_SIZE && ret != CONN_ERROR_TYPE_DATA) {
    return ret;
  }
  
  if (BIT_IS_SET(conn_p->co_flags, CONN_NET_ORDER)) {
    test_int = htonl(CONN_SAMPLE_INT);
  }
  else {
    test_int = CONN_SAMPLE_INT;
  }
  ret = verify_type(conn_p, CONN_TYPE_INT, &test_int, sizeof(test_int),
		    io_timeout);
  if (ret == CONN_ERROR_NONE) {
    BIT_SET(conn_p->co_types, CONN_ALLOW_INT);
  }
  else if (ret != CONN_ERROR_TYPE_SIZE && ret != CONN_ERROR_TYPE_DATA) {
    return ret;
  }
  
  if (BIT_IS_SET(conn_p->co_flags, CONN_NET_ORDER)) {
    test_unsigned_int = htonl(CONN_SAMPLE_UNSIGNED_INT);
  }
  else {
    test_unsigned_int = CONN_SAMPLE_UNSIGNED_INT;
  }
  ret = verify_type(conn_p, CONN_TYPE_UNSIGNED_INT, &test_unsigned_int,
		    sizeof(test_unsigned_int), io_timeout);
  if (ret == CONN_ERROR_NONE) {
    BIT_SET(conn_p->co_types, CONN_ALLOW_UNSIGNED_INT);
  }
  else if (ret != CONN_ERROR_TYPE_SIZE && ret != CONN_ERROR_TYPE_DATA) {
    return ret;
  }
  
  if (BIT_IS_SET(conn_p->co_flags, CONN_NET_ORDER)) {
    test_long = htonl(CONN_SAMPLE_LONG);
  }
  else {
    test_long = CONN_SAMPLE_LONG;
  }
  ret = verify_type(conn_p, CONN_TYPE_LONG, &test_long, sizeof(test_long),
		    io_timeout);
  if (ret == CONN_ERROR_NONE) {
    BIT_SET(conn_p->co_types, CONN_ALLOW_LONG);
  }
  else if (ret != CONN_ERROR_TYPE_SIZE && ret != CONN_ERROR_TYPE_DATA) {
    return ret;
  }
  
  if (BIT_IS_SET(conn_p->co_flags, CONN_NET_ORDER)) {
    test_unsigned_long = htonl(CONN_SAMPLE_UNSIGNED_LONG);
  }
  else {
    test_unsigned_long = CONN_SAMPLE_UNSIGNED_LONG;
  }
  ret = verify_type(conn_p, CONN_TYPE_UNSIGNED_LONG, &test_unsigned_long,
		    sizeof(test_unsigned_long), io_timeout);
  if (ret == CONN_ERROR_NONE) {
    BIT_SET(conn_p->co_types, CONN_ALLOW_UNSIGNED_LONG);
  }
  else if (ret != CONN_ERROR_TYPE_SIZE && ret != CONN_ERROR_TYPE_DATA) {
    return ret;
  }
  
  test_float = CONN_SAMPLE_FLOAT;
  ret = verify_type(conn_p, CONN_TYPE_FLOAT, &test_float, sizeof(test_float),
		    io_timeout);
  if (ret == CONN_ERROR_NONE) {
    BIT_SET(conn_p->co_types, CONN_ALLOW_FLOAT);
  }
  else if (ret != CONN_ERROR_TYPE_SIZE && ret != CONN_ERROR_TYPE_DATA) {
    return ret;
  }
  
  test_double = CONN_SAMPLE_DOUBLE;
  ret = verify_type(conn_p, CONN_TYPE_DOUBLE, &test_double,
		    sizeof(test_double), io_timeout);
  if (ret == CONN_ERROR_NONE) {
    BIT_SET(conn_p->co_types, CONN_ALLOW_DOUBLE);
  }
  else if (ret != CONN_ERROR_TYPE_SIZE && ret != CONN_ERROR_TYPE_DATA) {
    return ret;
  }
  
  return CONN_ERROR_NONE;
}

/***************************** decimal routines ******************************/

/*
 * static int get_decimal
 *
 * DESCRIPTION:
 *
 * Get a packed decimal number from a connection and unpack.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * conn_p - Connection structure that we will be getting data from.
 *
 * io_timeout - Milleseconds timeout value to wait for any i/o.
 *
 * val_p - Pointer to a long integer which will be assigned the
 * unpacked value.
 */
static	int	get_decimal(conn_t *conn_p, const int io_timeout,
			    unsigned long *val_p)
{
  int		ret;
  unsigned long	answer = 0, work = 0, shift = 0;
  unsigned char	ch;
  
  while (1) {
    /* get another character from the connection */
    ret = get_chars(conn_p, &ch, 1, io_timeout);
    if (ret != CONN_ERROR_NONE) {
      return ret;
    }
    
    /* process it and add it to the value */
    work = (unsigned long)(ch & 0177);
    answer |= (work << shift);
    shift += 7;
    
    /* if we do not have the continuation bit on, then we are done */
    if ((ch & 0200) == 0) {
      break;
    }
  }
  
  SET_POINTER(val_p, answer);
  return CONN_ERROR_NONE;
}

/*
 * static int put_decimal
 *
 * DESCRIPTION:
 *
 * Add to the connection buffer a packed decimal value.  It will pack
 * the decimal into as few bytes as possible.  get_decimal must be
 * used on the other side to unpack.  This routine works up to values
 * in excess of (2^29-1).
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * str_p - Pointer to our buffer which we will be adding information
 * to and advancing.
 *
 * max_p - Maximum location in the buffer.
 *
 * val - Long integer value that we are packing into the buffer.
 */
static	int	put_decimal(conn_t *conn_p, const int io_timeout,
			    const unsigned long val)
{
  unsigned long	work = val;
  unsigned char	ch;
  int		ret;
  
  while (1) {
    /* get the least 7 bits from the value we are packing */
    ch = (unsigned char)(work & 0177);
    work >>= 7;
    
    /* turn on high bit if there is more */
    if (work > 0) {
      ch |= 0200;
    }
    
    /* add the character to the buffer */
    ret = add_chars(conn_p, &ch, 1, io_timeout);
    if (ret != CONN_ERROR_NONE) {
      return ret;
    }
    
    /* we break here because we want 0 to be reflected by '\0' */
    if (work == 0) {
      break;
    }
  }
  
  return CONN_ERROR_NONE;
}

/******************************* send routines *******************************/

/*
 * static int void_send
 *
 * DESCRIPTION:
 *
 * Takes character type data and adds the appropriate information to
 * the connection buffer.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * conn_p - Connection we are sending the type through.
 *
 * io_timeout - Millisecond timeout value for input/output with the
 * remote host.
 *
 * data - Type's information which we are sending.
 *
 * data_len - How many bytes we are sending.
 */
static	int	void_send(conn_t *conn_p, const int io_timeout,
			  const void *data, const unsigned int data_len)
{
  int		ret;
  
  /* now add the actual data */
  if (data_len > 0) {
    ret = add_chars(conn_p, data, data_len, io_timeout);
    if (ret != CONN_ERROR_NONE) {
      return ret;
    }
  }
  
  return CONN_ERROR_NONE;
}

/*
 * static int short_send
 *
 * DESCRIPTION:
 *
 * Takes a short integer data type and adds the appropriate
 * information to the connection buffer.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * conn_p - Connection we are sending the type through.
 *
 * io_timeout - Millisecond timeout value for input/output with the
 * remote_host.
 *
 * data - Type's information which we are sending.
 *
 * data_many - How many of the type that we are sending.
 */
static	int	short_send(conn_t *conn_p, const int io_timeout,
			   const short *data, const unsigned int data_many)
{
  const short	*data_p;
  short		val;
  int		ret;
  unsigned int	many_c;
  
  /* now add the actual data */
  data_p = data;
  for (many_c = 0; many_c < data_many; many_c++) {
    val = htons(*data_p);
    ret = add_chars(conn_p, &val, sizeof(short), io_timeout);
    if (ret != CONN_ERROR_NONE) {
      return ret;
    }
    data_p++;
  }
  
  return CONN_ERROR_NONE;
}

/*
 * static int int_send
 *
 * DESCRIPTION:
 *
 * Takes a int data type and adds the appropriate information to the
 * connection buffer.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * conn_p - Connection we are sending the type through.
 *
 * io_timeout - Millisecond timeout value for input/output with the
 * remote_host.
 *
 * data - Type's information which we are sending.
 *
 * data_many - How many of the type that we are sending.
 */
static	int	int_send(conn_t *conn_p, const int io_timeout,
			 const int *data, const unsigned int data_many)
{
  const int	*data_p;
  int		val;
  int		ret;
  unsigned int	many_c;
  
  /* now add the actual data */
  data_p = data;
  for (many_c = 0; many_c < data_many; many_c++) {
    val = htonl(*data_p);
    ret = add_chars(conn_p, &val, sizeof(int), io_timeout);
    if (ret != CONN_ERROR_NONE) {
      return ret;
    }
    data_p++;
  }
  
  return CONN_ERROR_NONE;
}

/***************************** receive routines ******************************/

/*
 * static int void_receive
 *
 * DESCRIPTION:
 *
 * Received anonymous byte data and other appropriate information from
 * the connection buffer.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * conn_p - Connection we are receiving the type from.
 *
 * io_timeout - Millisecond timeout value for input/output with the
 * remote host.
 *
 * data - Where we are storing the received information.
 *
 * data_len - Length of the data that we are receiving.
 */
static	int	void_receive(conn_t *conn_p, const int io_timeout,
			     void *data, const unsigned int data_len)
{
  int		ret;
  
  /* now add the actual data */
  if (data_len > 0) {
    ret = get_chars(conn_p, data, data_len, io_timeout);
    if (ret != CONN_ERROR_NONE) {
      return ret;
    }
  }
  
  return CONN_ERROR_NONE;
}

/*
 * static int short_receive
 *
 * DESCRIPTION:
 *
 * Gets a short integer data type and additional information from the
 * connection buffer.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * conn_p - Connection we are receiving the type through.
 *
 * io_timeout - Millisecond timeout value for input/output with the
 * remote host.
 *
 * data_p - Where we are storing the received information.
 *
 * data_many - How many of the type that we are receiving.
 */
static	int	short_receive(conn_t *conn_p, const int io_timeout,
			      short *data, const unsigned int data_many)
{
  short		*data_p, val;
  int		ret;
  unsigned int	many_c;
  
  /* now add the actual data */
  data_p = data;
  for (many_c = 0; many_c < data_many; many_c++) {
    ret = get_chars(conn_p, &val, sizeof(val), io_timeout);
    if (ret != CONN_ERROR_NONE) {
      return ret;
    }
    
    /* convert and add to the buffer */
    if (data_p != NULL) {
      /*
       * NOTE: We already know that this ntoh is necessary.  If it
       * wasn't then void_receive would have been used.
       */
      val = ntohs(val);
      *data_p++ = val;
    }
  }
  
  return CONN_ERROR_NONE;
}

/*
 * static int int_receive
 *
 * DESCRIPTION:
 *
 * Gets an integer data type and additional information from the
 * connection buffer.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * conn_p - Connection we are receiving the type through.
 *
 * io_timeout - Millisecond timeout value for input/output with the
 * remote host.
 *
 * data - Where we are storing the received information.
 *
 * data_many - How many of the type that we are receiving.
 */
static	int	int_receive(conn_t *conn_p, const int io_timeout,
			    int *data, const unsigned int data_many)
{
  int		*data_p, val;
  int		ret;
  unsigned int	many_c;
  
  /* now add the actual data */
  data_p = data;
  for (many_c = 0; many_c < data_many; many_c++) {
    ret = get_chars(conn_p, &val, sizeof(val), io_timeout);
    if (ret != CONN_ERROR_NONE) {
      return ret;
    }
    
    /* convert and add to the buffer */
    if (data_p != NULL) {
      /*
       * NOTE: We already know that this ntoh is necessary.  If it
       * wasn't then void_receive would have been used.
       */
      val = ntohl(val);
      *data_p++ = val;
    }
  }
  
  return CONN_ERROR_NONE;
}

/***************************** startup routines ******************************/

/*
 * static void conn_startup
 *
 * DESCRIPTION:
 *
 * Startup the connection module which sets some global constants.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * None.
 */
static	void	conn_startup(void)
{
  if (enabled_b) {
    return;
  }
  enabled_b = 1;
  
  /* set the type arrays */
  type_sizes[CONN_TYPE_CHAR] = sizeof(char);
  type_flags[CONN_TYPE_CHAR] = CONN_ALLOW_CHAR;
  type_send[CONN_TYPE_CHAR] = (type_send_t)void_send;
  type_receive[CONN_TYPE_CHAR] = (type_send_t)void_receive;
  
  type_sizes[CONN_TYPE_UNSIGNED_CHAR] = sizeof(unsigned char);
  type_flags[CONN_TYPE_UNSIGNED_CHAR] = CONN_ALLOW_UNSIGNED_CHAR;
  type_send[CONN_TYPE_UNSIGNED_CHAR] = (type_send_t)void_send;
  type_receive[CONN_TYPE_UNSIGNED_CHAR] = (type_send_t)void_receive;
  
  type_sizes[CONN_TYPE_SHORT] = sizeof(short);
  type_flags[CONN_TYPE_SHORT] = CONN_ALLOW_SHORT;
  type_send[CONN_TYPE_SHORT] = (type_send_t)short_send;
  type_receive[CONN_TYPE_SHORT] = (type_send_t)short_receive;
  
  type_sizes[CONN_TYPE_UNSIGNED_SHORT] = sizeof(unsigned short);
  type_flags[CONN_TYPE_UNSIGNED_SHORT] = CONN_ALLOW_UNSIGNED_SHORT;
  type_send[CONN_TYPE_UNSIGNED_SHORT] = (type_send_t)short_send;
  type_receive[CONN_TYPE_UNSIGNED_SHORT] = (type_send_t)short_receive;
  
  type_sizes[CONN_TYPE_INT] = sizeof(int);
  type_flags[CONN_TYPE_INT] = CONN_ALLOW_INT;
  type_send[CONN_TYPE_INT] = (type_send_t)int_send;
  type_receive[CONN_TYPE_INT] = (type_send_t)int_receive;
  
  type_sizes[CONN_TYPE_UNSIGNED_INT] = sizeof(unsigned int);
  type_flags[CONN_TYPE_UNSIGNED_INT] = CONN_ALLOW_UNSIGNED_INT;
  type_send[CONN_TYPE_UNSIGNED_INT] = (type_send_t)int_send;
  type_receive[CONN_TYPE_UNSIGNED_INT] = (type_send_t)int_receive;
  
  type_sizes[CONN_TYPE_LONG] = sizeof(long);
  type_flags[CONN_TYPE_LONG] = CONN_ALLOW_LONG;
  type_send[CONN_TYPE_LONG] = NULL;
  type_receive[CONN_TYPE_LONG] = NULL;
  
  type_sizes[CONN_TYPE_UNSIGNED_LONG] = sizeof(unsigned long);
  type_flags[CONN_TYPE_UNSIGNED_LONG] = CONN_ALLOW_UNSIGNED_LONG;
  type_send[CONN_TYPE_UNSIGNED_LONG] = NULL;
  type_receive[CONN_TYPE_UNSIGNED_LONG] = NULL;
  
  type_sizes[CONN_TYPE_FLOAT] = sizeof(float);
  type_flags[CONN_TYPE_FLOAT] = CONN_ALLOW_FLOAT;
  type_send[CONN_TYPE_FLOAT] = NULL;
  type_receive[CONN_TYPE_FLOAT] = NULL;
  
  type_sizes[CONN_TYPE_DOUBLE] = sizeof(double);
  type_flags[CONN_TYPE_DOUBLE] = CONN_ALLOW_DOUBLE;
  type_send[CONN_TYPE_DOUBLE] = NULL;
  type_receive[CONN_TYPE_DOUBLE] = NULL;
}

/***************************** exported routines *****************************/

/*
 * conn_t *conn_client
 *
 * DESCRIPTION:
 *
 * Originate a connection with a remote host.  This negotiates a
 * connection with a server running with conn_serve.
 *
 * RETURNS:
 *
 * Success - Allocated connection structure which must be passed to
 * conn_close later.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * server_hostname - Name of the server host we are connecting.
 *
 * server_port - Port number on the server that we are connection.
 *
 * buffer_size - Size of our internal i/o buffer.  Set to 0 to use the
 * library default of 1k.
 *
 * connect_timeout - Milleseconds timeout value to wait for the
 * connection.
 *
 * io_timeout - Milleseconds timeout value to wait for any i/o.
 *
 * send_recv_b - If set to 1 then use the send and recv functions
 * instead of the read and write functions.  NT must have this set to
 * 1.
 *
 * error_p - Pointer to an integer which upon return contains a conn
 * error-code.
 */
conn_t	*conn_client(const char *server_hostname,
		     const unsigned short server_port,
		     const int buffer_size, const unsigned int connect_timeout,
		     const unsigned int io_timeout, const int send_recv_b,
		     int *error_p)
{
  conn_t	*conn_p;
  int		ret, size;
  
  if (! enabled_b) {
    conn_startup();
  }
  
  if (buffer_size > 0) {
    size = buffer_size;
  }
  else {
    size = CONN_DEFAULT_IO_BUFFER;
  }
  
  /* allocate our connection structure */
  conn_p = conn_alloc(server_hostname, server_port, -1, CONN_CLIENT,
		      send_recv_b, size, error_p);
  if (conn_p == NULL) {
    return NULL;
  }
  
  /* connect to the host */
  ret = host_connect(conn_p->co_socket, server_hostname, server_port,
		     connect_timeout);
  if (ret != CONN_ERROR_NONE) {
    (void)conn_close(conn_p);
    SET_POINTER(error_p, ret);
    return NULL;
  }
  
  /* do the initial login negotiation */
  ret = check_magic(conn_p, io_timeout);
  if (ret != CONN_ERROR_NONE) {
    (void)conn_close(conn_p);
    SET_POINTER(error_p, ret);
    return NULL;
  }
  
  /* do the type negotiation */
  ret = check_types(conn_p, io_timeout);
  if (ret != CONN_ERROR_NONE) {
    (void)conn_close(conn_p);
    SET_POINTER(error_p, ret);
    return NULL;
  }
  
  SET_POINTER(error_p, CONN_ERROR_NONE);
  return conn_p;
}

/*
 * conn_t *conn_client_sd
 *
 * DESCRIPTION:
 *
 * Configurate an already established socket connection as a
 * client-side.  This assumes that the socket is already connected.
 * It will negotiate a connection with a server running with
 * conn_serve or conn_serve_sd.  This is useful when you need to
 * connect the socket yourself and negotiate a little bit before
 * firing off the transactions.
 *
 * RETURNS:
 *
 * Success - Allocated connection structure which must be passed to
 * conn_close later.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * client_sd - Socket descriptor that we are associating with this
 * client connection.
 *
 * buffer_size - Size of our internal i/o buffer.  Set to 0 to use the
 * library default of 1k.
 *
 * io_timeout - Milleseconds timeout value to wait for any i/o.
 *
 * send_recv_b - If set to 1 then use the send and recv functions
 * instead of the read and write functions.  NT must have this set to
 * 1.
 *
 * error_p - Pointer to an integer which upon return contains a conn
 * error-code.
 */
conn_t	*conn_client_sd(const int client_sd, const int buffer_size,
			const unsigned int io_timeout, const int send_recv_b,
			int *error_p)
{
  conn_t	*conn_p;
  int		ret, size;
  
  if (! enabled_b) {
    conn_startup();
  }
  
  if (buffer_size > 0) {
    size = buffer_size;
  }
  else {
    size = CONN_DEFAULT_IO_BUFFER;
  }
  
  /* allocate our connection structure */
  conn_p = conn_alloc(NULL, 0, client_sd, CONN_CLIENT, send_recv_b, size,
		      error_p);
  if (conn_p == NULL) {
    return NULL;
  }
  
  conn_p->co_socket = client_sd;
  
  /* do the initial login negotiation */
  ret = check_magic(conn_p, io_timeout);
  if (ret != CONN_ERROR_NONE) {
    (void)conn_close(conn_p);
    SET_POINTER(error_p, ret);
    return NULL;
  }
  
  /* do the type negotiation */
  ret = check_types(conn_p, io_timeout);
  if (ret != CONN_ERROR_NONE) {
    (void)conn_close(conn_p);
    SET_POINTER(error_p, ret);
    return NULL;
  }
  
  SET_POINTER(error_p, CONN_ERROR_NONE);
  return conn_p;
}

/*
 * conn_t *conn_server
 *
 * DESCRIPTION:
 *
 * Serve a port to remote clients.  This sets up the connection
 * however will not wait for a connection.  You must then use
 * conn_accept to accept connections on the port.
 *
 * RETURNS:
 *
 * Success - Allocated connection structure which must be passed to
 * conn_close later.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * port - Port number to listen for connections.
 *
 * listen_queue - Number to pass to listen to set up the connection
 * queue for our socket.
 *
 * send_recv_b - If set to 1 then use the send and recv functions
 * instead of the read and write functions.  NT must have this set to
 * 1.
 *
 * error_p - Pointer to an integer which upon return contains a conn
 * error-code.
 */
conn_t	*conn_server(const unsigned short port, const int listen_queue,
		     const int send_recv_b, int *error_p)
{
  conn_t	*conn_p;
  int		ret;
  
  if (! enabled_b) {
    conn_startup();
  }
  
  /* allocate the connection structure */
  conn_p = conn_alloc(NULL, port, -1, CONN_SERVER, send_recv_b, 0, error_p);
  if (conn_p == NULL) {
    return NULL;
  }
  
  /* bind it to our port */
  ret = socket_bind(conn_p->co_socket, port, listen_queue);
  if (ret != CONN_ERROR_NONE) {
    (void)conn_close(conn_p);
    SET_POINTER(error_p, ret);
    return NULL;
  }
  
  SET_POINTER(error_p, CONN_ERROR_NONE);
  return conn_p;
}

/*
 * conn_t *conn_serve_sd
 *
 * DESCRIPTION:
 *
 * Startup a server-side connection on an already established and
 * connected socket.  This will negotiate with the remote client and
 * after it returns you are ready to start sending an receiving data.
 *
 * RETURNS:
 *
 * Success - Allocated connection structure which must be passed to
 * conn_close later.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * server_sd - Already established socket descriptor.
 *
 * send_recv_b - If set to 1 then use the send and recv functions
 * instead of the read and write functions.  NT must have this set to
 * 1.
 *
 * buffer_size - Size of our internal i/o buffer.  Set to 0 to use the
 * library default of 1k.
 *
 * io_timeout - Milleseconds timeout value to wait for any i/o.
 *
 * send_recv_b - If set to 1 then use the send and recv functions
 * instead of the read and write functions.  NT must have this set to
 * 1.
 *
 * error_p - Pointer to an integer which upon return contains a conn
 * error-code.
 */
conn_t	*conn_serve_sd(const int server_sd, const int buffer_size,
		       const unsigned int io_timeout, const int send_recv_b,
		       int *error_p)
{
  conn_t	*conn_p;
  int		ret;
  
  if (! enabled_b) {
    conn_startup();
  }
  
  /* allocate the connection structure */
  conn_p = conn_alloc(NULL, 0, server_sd, CONN_SERVER, send_recv_b,
		      buffer_size, error_p);
  if (conn_p == NULL) {
    return NULL;
  }
  
  conn_p->co_socket = server_sd;
  
  /* do the initial login negotiation */
  ret = check_magic(conn_p, io_timeout);
  if (ret != CONN_ERROR_NONE) {
    (void)conn_close(conn_p);
    SET_POINTER(error_p, ret);
    return NULL;
  }
  
  /* do the type negotiation */
  ret = check_types(conn_p, io_timeout);
  if (ret != CONN_ERROR_NONE) {
    (void)conn_close(conn_p);
    SET_POINTER(error_p, ret);
    return NULL;
  }
  
  SET_POINTER(error_p, CONN_ERROR_NONE);
  return conn_p;
}

/*
 * conn_t *conn_accept
 *
 * DESCRIPTION:
 *
 * Wait for a connection on our port from a remote client.  If one
 * arrives then accept the connection and negotiates with the remote
 * client.
 *
 * RETURNS:
 *
 * Success - Allocated connection structure which must be passed to
 * conn_close later.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * conn_p - Connection structure that was created by conn_serve.
 *
 * buffer_size - Size of our internal i/o buffer.  Set to 0 to use the
 * library default of 1k.
 *
 * accept_timeout - Milleseconds timeout value to wait for any
 * connections.
 *
 * io_timeout - Millisecond timeout value for input/output
 * negotiations with the client.
 *
 * ip_buf - Buffer which, if not NULL, will be copied the ascii
 * version of the remote host's IP address.
 *
 * ip_len - Length of the ip_buf.
 *
 * error_p - Pointer to an integer which upon return contains a conn
 * error-code.
 */
conn_t	*conn_accept(conn_t *conn_p, const int buffer_size,
		     const int accept_timeout, const int io_timeout,
		     char *ip_buf, const int ip_len, int *error_p)
{
  struct timeval	timeout, *timeout_p;
  struct sockaddr_in	client;
  int			len, ret, size, accept_sd;
  conn_t		*accept_p;
  char			*ip_name_p;
  fd_set		listen_fds;
  
  if (conn_p == NULL) {
    SET_POINTER(error_p, CONN_ERROR_ARG_NULL);
    return NULL;
  }
  if (conn_p->co_magic != CONN_T_MAGIC) {
    SET_POINTER(error_p, CONN_ERROR_PNT);
    return NULL;
  }
  
  if (! BIT_IS_SET(conn_p->co_flags, CONN_SERVER)) {
    SET_POINTER(error_p, CONN_ERROR_CLIENT);
    return NULL;
  }
  
  if (buffer_size > 0) {
    size = buffer_size;
  }
  else {
    size = CONN_DEFAULT_IO_BUFFER;
  }
  
  if (accept_timeout < 0) {
    timeout_p = NULL;
  }
  else {
    timeout_p = &timeout;
  }
  
  FD_ZERO(&listen_fds);
  
  while (1) {
    FD_SET(conn_p->co_socket, &listen_fds);
    
    /* NOTE: we set the timeout here in case the select modifies it below */
    if (accept_timeout >= 0) {
      timeout.tv_sec = accept_timeout / 1000;
      timeout.tv_usec = (accept_timeout % 1000) * 1000;
    }
    
    /* do the master fd multiplexor */
    ret = select(conn_p->co_socket + 1, &listen_fds, NULL, NULL, timeout_p);
    if (ret == 0) {
      SET_POINTER(error_p, CONN_ERROR_TIMEOUT);
      return NULL;
    }
    if (ret < 0) {
      if (errno == EINTR) {
	SET_POINTER(error_p, CONN_ERROR_SELECT);
	return NULL;
      }
      continue;
    }
    
    /* sanity check here */
    if (FD_ISSET(conn_p->co_socket, &listen_fds)) {
      break;
    }
    
    /* huh?  main socket not available in serve loop */
  }
  
  /*
   * Accept the connection with the remote client.  We do this before
   * the alloc to tell the alloc what descriptor we ended up with.
   */
  len = sizeof(client);
  accept_sd = accept(conn_p->co_socket, (void *)&client, &len);
  if (accept_sd < 0) {
    SET_POINTER(error_p, CONN_ERROR_ACCEPT);
    return NULL;
  }
  
  /* allocate the connection structure for the new fd */
  accept_p = conn_alloc(NULL, conn_p->co_port, accept_sd, CONN_ACCEPTED,
			conn_p->co_send_recv_b, size, error_p);
  if (accept_p == NULL) {
    return NULL;
  }
  
  accept_p->co_socket = accept_sd;
  
  /* copy the remote host address into ip buffer */
  if (ip_buf != NULL) {
    ip_name_p = inet_ntoa(client.sin_addr);
    if (ip_name_p == NULL) {
      ip_buf[0] = '\0';
    }
    else {
      strncpy(ip_buf, ip_name_p, ip_len - 1);
      ip_buf[ip_len - 1] = '\0';
    }
  }
  
  /* do the initial login negotiation */
  ret = check_magic(accept_p, io_timeout);
  if (ret != CONN_ERROR_NONE) {
    (void)conn_close(accept_p);
    SET_POINTER(error_p, ret);
    return NULL;
  }
  
  /* do the type negotiation */
  ret = check_types(accept_p, io_timeout);
  if (ret != CONN_ERROR_NONE) {
    (void)conn_close(accept_p);
    SET_POINTER(error_p, ret);
    return NULL;
  }
  
  SET_POINTER(error_p, CONN_ERROR_NONE);
  return accept_p;
}

/*
 * int conn_close
 *
 * DESCRIPTION:
 *
 * Close a connection previously opened with conn_client or
 * conn_server and free any allocations associated with the
 * structures.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * conn_p - Connection we are closing.
 */
int	conn_close(conn_t *conn_p)
{
  if (conn_p == NULL) {
    return CONN_ERROR_ARG_NULL;
  }
  if (conn_p->co_magic != CONN_T_MAGIC) {
    return CONN_ERROR_PNT;
  }
  
  if (conn_p->co_host != NULL) {
    free(conn_p->co_host);
    conn_p->co_host = NULL;
  }
  if (conn_p->co_socket >= 0) {
    (void)socket_close(conn_p->co_socket);
    conn_p->co_socket = -1;
  }
  if (conn_p->co_encrypt != NULL) {
    free(conn_p->co_encrypt);
    conn_p->co_encrypt = NULL;
  }
  if (conn_p->co_read_buf != NULL) {
    free(conn_p->co_read_buf);
    conn_p->co_read_buf = NULL;
  }
  if (conn_p->co_write_buf != NULL) {
    free(conn_p->co_write_buf);
    conn_p->co_write_buf = NULL;
  }
  conn_p->co_magic = 0;
  free(conn_p);
  
  return CONN_ERROR_NONE;
}

/*
 * int conn_encrypt
 *
 * DESCRIPTION:
 *
 * Sets (or replaces) the encryption string for the connection.  This
 * is done after the connection is established.  The remote machine do
 * call this with the same string, before data flowing between the two
 * is properly decrypted.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * conn_p - Connection we are encrypting.
 *
 * encrypt_string - Encryption string of bytes which is used to
 * encrypt the data stream with the remote host.  NOTE: it can include
 * '\0' and other binary characters.
 *
 * encrypt_len - Length of the encryption_string.
 */
int	conn_encrypt(conn_t *conn_p, const char *encrypt_string,
		     const int encrypt_len)
{
  if (conn_p == NULL) {
    return CONN_ERROR_ARG_NULL;
  }
  if (conn_p->co_magic != CONN_T_MAGIC) {
    return CONN_ERROR_PNT;
  }
  if (BIT_IS_SET(conn_p->co_flags, CONN_SERVER)) {
    return CONN_ERROR_SERVER;
  }
  if (encrypt_string == NULL) {
    return CONN_ERROR_ARG_NULL;
  }
  
  /* if there is a previous encrypt string then free it */
  if (conn_p->co_encrypt != NULL) {
    free(conn_p->co_encrypt);
  }
  
  /* now allocate and copy string into connection structure and set pointers */
  conn_p->co_encrypt = malloc(encrypt_len);
  if (conn_p->co_encrypt == NULL) {
    return CONN_ERROR_ALLOC;
  }
  memcpy(conn_p->co_encrypt, encrypt_string, encrypt_len);
  conn_p->co_encrypt_pos_p = conn_p->co_encrypt;
  conn_p->co_encrypt_max_p = conn_p->co_encrypt + encrypt_len;
  
  return CONN_ERROR_NONE;
}

/*
 * int conn_buffer_sizes
 *
 * DESCRIPTION:
 *
 * Sets the socket read or write buffer sizes on a connection.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * conn_p - Connection we are modifying.
 *
 * read_buf_size - If > 0 then set the receive buffer size on the
 * socket to this value.
 *
 * write_buf_size - If > 0 then set the send buffer size on the socket
 * to this value.
 */
int	conn_buffer_sizes(conn_t *conn_p, const int read_buf_size,
			  const int write_buf_size)
{
  if (conn_p == NULL) {
    return CONN_ERROR_ARG_NULL;
  }
  if (conn_p->co_magic != CONN_T_MAGIC) {
    return CONN_ERROR_PNT;
  }
  if (BIT_IS_SET(conn_p->co_flags, CONN_SERVER)) {
    return CONN_ERROR_SERVER;
  }
  
  /* set read buffer size */
  if (read_buf_size > 0) {
    if (setsockopt(conn_p->co_socket, SOL_SOCKET, SO_RCVBUF,
		   (void *)&read_buf_size, sizeof(read_buf_size)) != 0) {
      return CONN_ERROR_SOCKETOPTS;
    }
  }
  
  /* set write buffer size */
  if (write_buf_size > 0) {
    if (setsockopt(conn_p->co_socket, SOL_SOCKET, SO_SNDBUF,
		   (void *)&write_buf_size, sizeof(write_buf_size)) != 0) {
      return CONN_ERROR_SOCKETOPTS;
    }
  }
  
  return CONN_ERROR_NONE;
}

/*
 * int conn_type_okay
 *
 * DESCRIPTION:
 *
 * Determines whether the type is safe to pass across the connection.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * conn_p - Connection structure that was created by conn_serve.
 *
 * type_num - Type number to test for safety.
 *
 * allow_bp - Pointer to an integer which will be set upon return to 1
 * if the type is okay to use else 0.
 */
int	conn_type_okay(const conn_t *conn_p, const int type_num, int *allow_bp)
{
  if (conn_p == NULL) {
    return CONN_ERROR_ARG_NULL;
  }
  if (conn_p->co_magic != CONN_T_MAGIC) {
    return CONN_ERROR_PNT;
  }
  if (BIT_IS_SET(conn_p->co_flags, CONN_SERVER)) {
    return CONN_ERROR_SERVER;
  }
  
  if (! CONN_TYPE_IS_VALID(type_num)) {
    return CONN_ERROR_ARG_INVALID;
  }
  
  if (BIT_IS_SET(conn_p->co_types, type_flags[type_num])) {
    SET_POINTER(allow_bp, 1);
  }
  else {
    SET_POINTER(allow_bp, 0);
  }
  
  return CONN_ERROR_NONE;
}

/*
 * int conn_info
 *
 * DESCRIPTION:
 *
 * Get information about the connection from the structure.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * conn_p - Connection structure that was created by conn_serve.
 *
 * read_bytes_np - Pointer to an unsigned long integer which will be
 * set upon return to the number of bytes read from the remote system
 * over the connection.
 *
 * write_bytes_np - Pointer to an unsigned long integer which will be
 * set upon return (if not NULL) to the number of bytes written to the
 * remote system over the connection.
 *
 * read_np - Pointer to an unsigned long integer which will be set
 * upon return (if not NULL) to the number of read calls made by the
 * connection.
 *
 * write_np - Pointer to an unsigned long integer which will be set
 * upon return (if not NULL) to the number of write calls made by the
 * connection.
 *
 * sd_p - Pointer to an integer which will be set upon return (if not
 * NULL) to the associated socket file descriptor.  NOTE: this is for
 * information purposes only.  Nothing should be read or written to
 * the descriptor directly.
 */
int	conn_info(const conn_t *conn_p,
		  unsigned long *read_bytes_np, unsigned long *write_bytes_np,
		  unsigned long *read_np, unsigned long *write_np,
		  int *sd_p)
{
  if (conn_p == NULL) {
    return CONN_ERROR_ARG_NULL;
  }
  if (conn_p->co_magic != CONN_T_MAGIC) {
    return CONN_ERROR_PNT;
  }
  
  SET_POINTER(read_bytes_np, conn_p->co_read_bytes_c);
  SET_POINTER(write_bytes_np, conn_p->co_write_bytes_c);
  SET_POINTER(read_np, conn_p->co_read_c);
  SET_POINTER(write_np, conn_p->co_write_c);
  SET_POINTER(sd_p, conn_p->co_socket);
  
  return CONN_ERROR_NONE;
}

/***************************** transfer routines *****************************/

/*
 * int conn_send_data
 *
 * DESCRIPTION:
 *
 * Add to a buffer a list of (const int data_type, const int
 * data_many, void *data) triplets, advancing the connection buffer
 * pointers so they are always at the end of the data.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * conn_p - Connection structure that we will be using to send the
 * data.
 *
 * io_timeout - Millisecond timeout value for input/output with the
 * remote host.
 *
 * ... - Variable arguments list of (const int data_type, const int
 * data_many, void *data) triplets.  The data_type should be one of
 * the defined connection types in the conn.h file.  The list of items
 * is terminated if the data_type is CONN_TYPE_LAST.  If the data_many
 * is < 0 then the type should be character or unsigned character.
 * The routine will then add a string including the '\0' to the
 * buffer.
 */
int	conn_send_data(conn_t *conn_p, const int io_timeout, ...)
{
  void		*buf;
  va_list	ap;
  int		ret, many;
  unsigned int	len, type;
  
  if (conn_p == NULL) {
    return CONN_ERROR_ARG_NULL;
  }
  if (conn_p->co_magic != CONN_T_MAGIC) {
    return CONN_ERROR_PNT;
  }
  if (BIT_IS_SET(conn_p->co_flags, CONN_SERVER)) {
    return CONN_ERROR_SERVER;
  }
  
  va_start(ap, io_timeout);
  while (1) {
    /* get the data type */
    type = (unsigned int)va_arg(ap, unsigned int);
    if (type == CONN_TYPE_LAST) {
      break;
    }
    /* check the type for validity */
    if (! CONN_TYPE_IS_VALID(type)) {
      return CONN_ERROR_TYPE_ARG;
    }
    /* are we allowed to transfer the type? */
    if (! BIT_IS_SET(conn_p->co_types, type_flags[type])) {
      return CONN_ERROR_TYPE_ALLOW;
    }
    
    /* get the data length */
    many = (int)va_arg(ap, int);
    
    buf = va_arg(ap, void *);
    if (buf == NULL) {
      /* we reached the end */
      break;
    }
    
    /* do we need to strlen? */
    if (many < 0) {
      if (type != CONN_TYPE_CHAR && type != CONN_TYPE_UNSIGNED_CHAR) {
	/* NOTE: this _is_ a many arg failure here */
	return CONN_ERROR_MANY_ARG;
      }
      many = strlen((char *)buf) + 1;
    }
    
    /* add in the type */
    ret = put_decimal(conn_p, io_timeout, (long)type);
    if (ret != CONN_ERROR_NONE) {
      return ret;
    }
    /* add in many */
    ret = put_decimal(conn_p, io_timeout, (long)many);
    if (ret != CONN_ERROR_NONE) {
      return ret;
    }
    
    /* now send the type */
    if (BIT_IS_SET(conn_p->co_flags, CONN_NET_ORDER)) {
      ret = (type_send[type])(conn_p, io_timeout, buf, many);
    }
    else {
      /*
       * If not in network order the character send routine by just
       * sending the data as a block of characters
       */
      len = many * type_sizes[type];
      ret = void_send(conn_p, io_timeout, buf, len);
    }
    
    if (ret != CONN_ERROR_NONE) {
      return ret;
    }
  }
  va_end(ap);
  
  return CONN_ERROR_NONE;
}

/*
 * int conn_flush
 *
 * DESCRIPTION:
 *
 * Flush any output buffered in the connection structure to the remote
 * host.  This is only necessary after a conn_send.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * conn_p - Connection structure that we will are flushing.
 *
 * io_timeout - Millisecond timeout value for input/output with the
 * remote host.
 */
int	conn_flush(conn_t *conn_p, const int io_timeout)
{
  if (conn_p == NULL) {
    return CONN_ERROR_ARG_NULL;
  }
  if (conn_p->co_magic != CONN_T_MAGIC) {
    return CONN_ERROR_PNT;
  }
  if (BIT_IS_SET(conn_p->co_flags, CONN_SERVER)) {
    return CONN_ERROR_SERVER;
  }
  
  /*
   * the reson why we do this is so we can have all of the buffer
   * adjusting routines above
   */
  return write_chars(conn_p, io_timeout);
}

/*
 * int conn_receive_data
 *
 * DESCRIPTION:
 *
 * Remove from the buffer a list of (const int data_type, const char
 * data_alloc_b, const int data_many, void *data) fivesomes, advancing
 * the connection buffer pointers so they are always at the end of the
 * data.
 *
 * WARNING: This is very dependent on the same data alignment passed
 * to conn_send_data as is passed here.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code.
 *
 * ARGUMENTS:
 *
 * conn_p - Connection structure that we will be using to send the
 * data.
 *
 * io_timeout - Millisecond timeout value for input/output with the
 * remote host.
 *
 * ... - Variable arguments list of (const int data_type, const char
 * data_alloc_b, const int data_many, const int *data_many_p, void
 * *data) fivesomes.  The data_type should be one of the defined
 * connection types in the conn.h file.  The list of items is
 * terminated if the data_type is CONN_TYPE_LAST.  If data_alloc_b is
 * 1 then the routine will assume that the data type is a pointer to a
 * pointer of the type.  Space for the data is malloc'ed and the
 * pointer is pointed to this new space.  If the data_many is < 0 then
 * you are not limiting the receive of this type to any size so the
 * data_allow_b flag must be set to 1.  If data_many >= 0 it is the
 * number of the type that we are receiving.  If data_many_p is not
 * null then it will be set with the number of the type that we got
 * from the stream.  The routine will then add a string including the
 * '\0' to the buffer.
 */
int	conn_receive_data(conn_t *conn_p, const int io_timeout, ...)
{
  void		*buf, *data_p;
  va_list	ap;
  int		ret, many;
  int		alloc_b = 0;
  unsigned long	val;
  unsigned int	type, *many_p, stream_type, stream_many, len = 0;
  
  if (conn_p == NULL) {
    return CONN_ERROR_ARG_NULL;
  }
  if (conn_p->co_magic != CONN_T_MAGIC) {
    return CONN_ERROR_PNT;
  }
  if (BIT_IS_SET(conn_p->co_flags, CONN_SERVER)) {
    return CONN_ERROR_SERVER;
  }
  
  va_start(ap, io_timeout);
  while (1) {
    
    /* get the data type */
    type = (unsigned int)va_arg(ap, unsigned int);
    if (type == CONN_TYPE_LAST) {
      break;
    }
    if (! CONN_TYPE_IS_VALID(type)) {
      return CONN_ERROR_TYPE_ARG;
    }
    /* are we allowed to transfer the type? */
    if (! BIT_IS_SET(conn_p->co_types, type_flags[type])) {
      return CONN_ERROR_TYPE_ALLOW;
    }
    
    /* get our alloc flag */
    alloc_b = va_arg(ap, int);
    
    /* get the data length */
    many = (int)va_arg(ap, int);
    
    /* if we have a < 0 length args then we must also have the alloc flag */
    if (many < 0 && (! alloc_b)) {
      return CONN_ERROR_MANY_UNKNOWN;
    }
    
    /* get the many-pointer arg */
    many_p = (unsigned int *)va_arg(ap, unsigned int *);
    
    /* now we get the data pointer from the arguments */
    buf = va_arg(ap, void *);
    /* if buf is NULL then we just skip this stream entry */
    
    /* if the buf is NULL we can't be allocating memory for it */
    if (buf == NULL && alloc_b) {
      return CONN_ERROR_TYPE_ARG;
    }
    
    /* get the type off the stream */
    ret = get_decimal(conn_p, io_timeout, &val);
    if (ret != CONN_ERROR_NONE) {
      return ret;
    }
    stream_type = val;
    if (! CONN_TYPE_IS_VALID(stream_type)) {
      return CONN_ERROR_TYPE_ARG;
    }
    
    /* get the length off the stream */
    ret = get_decimal(conn_p, io_timeout, &val);
    if (ret != CONN_ERROR_NONE) {
      return ret;
    }
    stream_many = val;
    
    /* verify many arguments -- do we have enough space to receive data? */
    if (many > 0 && many < (int)stream_many) {
      return CONN_ERROR_MANY_SYNC;
    }
    
    /* pass back the correct many */
    SET_POINTER(many_p, stream_many);
    
    if (stream_many == 0) {
      data_p = NULL;
    }
    else if (alloc_b) {
      /* get our length */
      len = stream_many * type_sizes[stream_type];
      
      data_p = malloc(len);
      if (data_p == NULL) {
	/* NOTE: we will leave some other arguments leaked here */
	return CONN_ERROR_ALLOC;
      }
      *(void **)buf = data_p;
    }
    else {
      /* NOTE: buf could be null */
      data_p = buf;
    }
    
    /* now receive the type */
    if (BIT_IS_SET(conn_p->co_flags, CONN_NET_ORDER)) {
      ret = (type_receive[type])(conn_p, io_timeout, data_p, stream_many);
    }
    else {
      /*
       * If not in network order the character send routine by just
       * sending the data as a block of characters
       */
      len = stream_many * type_sizes[stream_type];
      ret = void_receive(conn_p, io_timeout, data_p, len);
    }
  }
  va_end(ap);
  
  return CONN_ERROR_NONE;
}

/*
 * int conn_check_magic
 *
 * DESCRIPTION:
 *
 * This routine is used to exchange a magic number with the remote
 * host to verify that both sides are running the same software.
 *
 * RETURNS:
 *
 * Success - CONN_ERROR_NONE
 *
 * Failure - Connection error code
 *
 * ARGUMENTS:
 *
 * conn_p - Connection structure that we will be using to send the
 * data.
 *
 * magic - Data that will will exchange with the remote system and we
 * will verify that we get back from the remote.
 *
 * magic_len - Length of the magic data.  If < 0 then the routine will
 * do an internal strlen.
 *
 * io_timeout - Millisecond timeout value for input/output with the
 * remote host.
 */
int	conn_check_magic(conn_t *conn_p, const void *magic,
			 const int magic_len, const int io_timeout)
{
  char		*remote_magic;
  int		ret, len, remote_len;
  
  if (conn_p == NULL) {
    return CONN_ERROR_ARG_NULL;
  }
  if (conn_p->co_magic != CONN_T_MAGIC) {
    return CONN_ERROR_PNT;
  }
  if (magic == NULL) {
    return CONN_ERROR_ARG_NULL;
  }
  if (magic_len == 0) {
    return CONN_ERROR_ARG_INVALID;
  }
  
  if (magic_len >= 0) {
    len = magic_len;
  }
  else {
    len = strlen((char *)magic);
  }
  
  /* send our magic number */
  ret = conn_send_data(conn_p, io_timeout,
		       CONN_TYPE_CHAR, len, magic,
		       CONN_TYPE_LAST);
  if (ret != CONN_ERROR_NONE) {
    return ret;
  }
  
  /* now flush our send */
  ret = conn_flush(conn_p, io_timeout);
  if (ret != CONN_ERROR_NONE) {
    return ret;
  }
  
  /* read in the magic value */
  ret = conn_receive_data(conn_p, io_timeout,
			  CONN_TYPE_CHAR, 1, 0, &remote_len, &remote_magic,
			  CONN_TYPE_LAST);
  if (ret != CONN_ERROR_NONE) {
    return ret;
  }
  
  /* is the length different or the block different? */
  if (remote_len != len
      || memcmp(magic, remote_magic, len) != 0) {
    free(remote_magic);
    return CONN_ERROR_MAGIC;
  }
  
  free(remote_magic);
  return CONN_ERROR_NONE;
}

/*
 * const char *conn_strerror
 *
 * DESCRIPTION:
 *
 * Return the string equivalent of a connection error value.
 *
 * RETURNS:
 *
 * Success - String equivalent of the error.
 *
 * Failure - String "invalid error code".
 *
 * ARGUMENTS:
 *
 * error - Error value to translate into a string.
 */
const char	*conn_strerror(const int error)
{
  error_str_t	*err_p;
  
  for (err_p = errors; err_p->es_error != 0; err_p++) {
    if (err_p->es_error == error) {
      return err_p->es_string;
    }
  }
  
  return INVALID_ERROR;
}
