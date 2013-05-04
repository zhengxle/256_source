/*
 * Local defines for the skip-list modules
 *
 * Copyright 1996 by Gray Watson.
 *
 * $Id: skip_loc.h,v 1.2 1998/03/25 01:07:02 gray Exp $
 */

/*
 * Based on the skip list algorithms described in the June 1990 issue
 * of CACM and invented by William Pugh in 1987.
 */

#ifndef __SKIP_LOC_H__
#define __SKIP_LOC_H__

/*
 * for random_level function:
 */
/*
 * define the following implement the hack described in the CACM
 * paper: if a random level is generated that is more than the current
 * maximum level, the current maximum level plus one is used instead.
 */
#define CACM_HACK		1
#define INCREASE_LEVEL(rand)	(((rand) % 10000) < 5000)

#define SKIP_MAGIC		0x10DD1DEA
/* NOTE: lev-1 is used because we already have 1 entry in skip_node_t */
#define NODE_SIZE(lev)		(sizeof(skip_node_t) + (lev - 1) * \
				 sizeof(skip_node_t *))

#ifdef NO_MMAP

#define SKIP_POINTER(skip_p, type, pnt)		pnt

#else

#define SKIP_POINTER(skip_p, type, pnt)	\
     (skip_p->sk_mmap == NULL ? pnt : (pnt == NULL ? NULL : \
				       (type)((char *)pnt + \
					      (long)skip_p->sk_mmap)))

#endif

/*
 * type of entry's key or data.
 *
 * NOTE: we should not do the da_data[0] here because datum_t is part of
 * the node_st below and so we don't save anything.
 */
typedef struct {
  unsigned int		da_size;		/* size of data */
  void			*da_data;		/* pointer to data */
} datum_t;

/*
 * one element in the skip list.  this is a dynamically sized node
 * depending on the number of forward pointers involved.
 */
struct skip_node_st {
  datum_t		sn_key;			/* search key for data */
  datum_t		sn_data;		/* actual data to be stored */
  unsigned int		sn_forward_n;		/* number of forward entries */
  struct skip_node_st	*sn_forward_p[1];	/* list of forward pointers */
};
typedef struct skip_node_st	skip_node_t;
typedef skip_node_t		skip_node_ext_t;

/*
 * NOTE: since we would need to realloc the head forwardp list, but
 * don't want the user's pointer to change, we need to have the node_p
 * pointer to the head node in our tree.
 *
 * Also, we could just encorporate a pointer to a forward-list and the
 * forward_n in this struct but it simplifies the code to make node_p
 * be a node-pointer.
 */
typedef struct skip_st {
  unsigned int		sk_magic;		/* magic goodie */
  unsigned int		sk_entry_n;		/* number of entries */
  skip_compare_t	sk_compare;		/* compare function */
  skip_node_t		*sk_top_p;		/* pointer to head node */
  skip_node_t		*sk_this_p;		/* linear function */
  struct skip_st	*sk_mmap;		/* mmaped skip list */
  long			sk_file_size;		/* size of on-disk space */
} skip_t;
typedef skip_t		skip_ext_t;

/*
 * list of power of two numbers to normalize a list
 */
static int	power_two[] = {
  1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768,
  65536, 131072, 262144, 524288, 1048576, 2097152, 4194304, 8388608, 16777216,
  33554432, 67108864, 134217728, 268435456, 536870912, 1073741824,
  /* NOTE: this is 2^31 - 1 so not exceed signed int */
  2147483647, 0
};

/*
 * to map error to string
 */
typedef struct {
  int		es_error;		/* error number */
  char		*es_string;		/* assocaited string */
} error_str_t;

static	error_str_t	errors[] = {
  { SKIP_ERROR_NONE,		"no error" },
  { SKIP_ERROR_PNT,		"invalid skip pointer" },
  { SKIP_ERROR_ARG_NULL,	"buffer argument is null" },
  { SKIP_ERROR_SIZE,		"incorrect size argument" },
  { SKIP_ERROR_OVERWRITE,	"key exists and no overwrite" },
  { SKIP_ERROR_NOT_FOUND,	"key does not exist" },
  { SKIP_ERROR_ALLOC,		"error allocating memory" },
  { SKIP_ERROR_LINEAR,		"linear access not in progress" },
  { SKIP_ERROR_FILE,		"could not open file" },
  { SKIP_ERROR_INTERNAL,	"found inconsistancy with list" },
  { SKIP_ERROR_MMAP_NONE,	"no mmap support compiled in library" },
  { SKIP_ERROR_MMAP,		"could not mmap the file" },
  { SKIP_ERROR_MMAP_OP,		"operation not valid on mmap files" },
  { 0 }
};

#define INVALID_ERROR	"invalid error code"

/*
 * mapping of memory pointer to size
 */
typedef struct {
  skip_node_t	*ps_node_p;		/* pointer to a node */
  long		ps_size;		/* its corresponding size */
} pnt_size_t;

#endif /* ! __SKIP_LOC_H__ */
