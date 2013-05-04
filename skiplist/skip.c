/*
 * skip lists database routines
 *
 * Copyright 1996 by Gray Watson.
 *
 * $Id: skip.c,v 1.3 1998/03/25 01:32:12 gray Exp $
 */

/*
 * Based on the skip list algorithms described in the June 1990 issue
 * of CACM and invented by William Pugh in 1987.
 */

#ifdef sparc
#include <alloca.h>
#endif
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef NO_MMAP

#include <sys/mman.h>
#include <sys/stat.h>

#ifndef MAP_FAILED
#define MAP_FAILED	(caddr_t)0L
#endif

#endif

#define SKIP_MAIN

#include "skip.h"
#include "skip_loc.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

static	char	*rcs_id =
  "$Id: skip.c,v 1.3 1998/03/25 01:32:12 gray Exp $";

/***************************** utility routines ******************************/

/*
 * return a random level in [1..max_level+>0] for the skip list
 * hierarchy for LIST_P.
 */
static	int	random_level(const skip_node_t *list_p)
{
  int	level;
  
  for (level = 1;
#if CACM_HACK
       /* stop when we have grown bigger than the max-level */
       level <= list_p->sn_forward_n;
#else
       /* continue until the random stops */;
#endif
       level++) {
    if (! INCREASE_LEVEL(random()))
      break;
  }
  
  return level;
}

/*
 * return the log base 2 of the number.
 */
static	int	good_height(const int number)
{
  int	work = number, two_c = 0;
  
  /* quick out: if odd number then height == 1 */
  if (number % 2 != 0)
    return 1;
  
  while (work > 0) {
    for (two_c = 0; two_c < 32; two_c++)
      if (power_two[two_c + 1] > work || power_two[two_c + 1] == 0)
	break;
    work -= power_two[two_c];
  }
  
  return two_c + 1;
}

/*
 * find in SKIP_P, KEY_P, returning its node or null if not found.  this
 * will set the UPDATE_P slots to the nodes we passed through for
 * possible back-tracking
 */
static	skip_node_t	*find_node(skip_t *skip_p, const datum_t *key_p,
				   skip_node_t **update_p)
{
  int		level, cmp;
  skip_node_t 	*top_p, *node_p, *next_p, *found_p = NULL;
  
  top_p = SKIP_POINTER(skip_p, skip_node_t *, skip_p->sk_top_p);
  
  level = top_p->sn_forward_n - 1;
  node_p = top_p;
  
  /* traverse list to smallest entry */
  while (level >= 0) {
    
    next_p = SKIP_POINTER(skip_p, skip_node_t *, node_p->sn_forward_p[level]);
    
    /* are we are at the end of a row? */
    if (next_p == NULL)
      cmp = 1;
    else if (next_p == found_p) {
      /* did we find it again? */
      cmp = 0;
    }
    else if (next_p->sn_forward_n > node_p->sn_forward_n) {
      /*
       * Is next taller then move down.  We do this because we know
       * that if the next is taller, we would have seen it already and
       * done the cmp already.
       */
      cmp = 1;
    }
    else {
      if (skip_p->sk_compare == NULL) {
	int	size;
	
	/* find the common size */
	size = key_p->da_size;
	if (next_p->sn_key.da_size < size)
	  size = next_p->sn_key.da_size;
	cmp = memcmp(SKIP_POINTER(skip_p, void *, next_p->sn_key.da_data),
		     SKIP_POINTER(skip_p, void *, key_p->da_data),
		     size);
	/* if common-size equal, then if next more bytes, it is larger */
	if (cmp == 0)
	  cmp = next_p->sn_key.da_size - key_p->da_size;
      }
      else
	cmp = skip_p->sk_compare(SKIP_POINTER(skip_p, void *,
					      next_p->sn_key.da_data),
				 next_p->sn_key.da_size, key_p->da_data,
				 key_p->da_size);
    }
    
    if (cmp < 0) {
      /* next node is less, go right */
      node_p = next_p;
      continue;
    }
    else if (cmp > 0) {
      /* next node is more, go down */
      if (update_p != NULL)
	update_p[level] = node_p;
      level--;
      continue;
    }
    
    /* we found it, cmp == 0 */
    
    /* no need to keep going down if we're not tracking predecessors */
    if (update_p == NULL)
      return next_p;
    
    update_p[level] = node_p;
    
    /* if we have reached the bottom, we are done */
    if (level == 0)
      return next_p;
    
    found_p = next_p;
    level--;
  }
  
  /* ASSERT(found_p == NULL); */
  return NULL;
}

/***************************** external routines *****************************/

/*
 * alloc and return a new skip list.  the COMPARE function is used to
 * locate keys -- set to null to use memcmp.  COMPARE should return
 * effectively (*key1) - (*key2) (<0,==0,>0 for (*key1) <,==,>
 * (*key2)).  returns null on error and passes back skip error codes
 * in ERROR_P.
 */
skip_t	*skip_alloc(const skip_compare_t compare, int *error_p)
{
  skip_t	*new_p;
  
  new_p = (skip_t *)malloc(sizeof(skip_t));
  if (new_p == NULL) {
    if (error_p != NULL)
      *error_p = SKIP_ERROR_ALLOC;
    return NULL;
  }
  
  new_p->sk_magic = SKIP_MAGIC;
  new_p->sk_entry_n = 0;
  new_p->sk_compare = compare;
  new_p->sk_this_p = NULL;
  new_p->sk_mmap = NULL;
  new_p->sk_file_size = 0;
  
  /* we start with a blank node with no data or forward pointers */
  new_p->sk_top_p = (skip_node_t *)calloc(1, sizeof(skip_node_t));
  if (new_p->sk_top_p == NULL) {
    if (error_p != NULL)
      *error_p = SKIP_ERROR_ALLOC;
    return NULL;
  }
  
  new_p->sk_top_p->sn_forward_n = 0;
  
  if (error_p != NULL)
    *error_p = SKIP_ERROR_NONE;
  return new_p;
}

/*
 * clear the SKIP_P structure.  returns skip error codes.
 */
int	skip_clear(skip_t *skip_p)
{
  skip_node_t	*top_p, *node_p;
  
  if (skip_p == NULL)
    return SKIP_ERROR_ARG_NULL;
  if (skip_p->sk_magic != SKIP_MAGIC)
    return SKIP_ERROR_PNT;
  
#ifndef NO_MMAP
  /* no mmap support so immediate error */
  if (skip_p->sk_mmap != NULL)
    return SKIP_ERROR_MMAP_OP;
#endif
  
  top_p = skip_p->sk_top_p;
  
  if (top_p->sn_forward_n == 0)
    return SKIP_ERROR_NONE;
  
  /*
   * traversing via the first element in each node, as this path will
   * visit each and every node.
   */
  for (node_p = top_p->sn_forward_p[0]; node_p != NULL;) {
    skip_node_t	*this_p = node_p;
      
    /* NOTE: we assume no nodes have a 0 forward_n */
    
    node_p = node_p->sn_forward_p[0];
    
    free(this_p->sn_key.da_data);
    free(this_p->sn_data.da_data);
    free(this_p);
  }
  
  skip_p->sk_entry_n = 0;
  /* now we realloc the top node to have 0 forward pointers */
  top_p = (skip_node_t *)realloc(top_p, sizeof(skip_node_t));
  if (top_p == NULL)
    return SKIP_ERROR_ALLOC;
  top_p->sn_forward_n = 0;
  skip_p->sk_top_p = top_p;
  
  return SKIP_ERROR_NONE;
}

/*
 * clear and free the SKIP_P structure.  returns skip error codes.
 */
int	skip_free(skip_t *skip_p)
{
  int		ret;
  
  if (skip_p == NULL)
    return SKIP_ERROR_ARG_NULL;
  if (skip_p->sk_magic != SKIP_MAGIC)
    return SKIP_ERROR_PNT;
  
#ifndef NO_MMAP
  /* no mmap support so immediate error */
  if (skip_p->sk_mmap != NULL)
    return SKIP_ERROR_MMAP_OP;
#endif
  
  ret = skip_clear(skip_p);
  if (ret != SKIP_ERROR_NONE)
    return ret;
  
  skip_p->sk_magic = 0;
  free(skip_p->sk_top_p);
  free(skip_p);
  
  return SKIP_ERROR_NONE;
}

/*
 * Add a key(KEY_BUF, KEY_SIZE) data(DATA_BUF, DATA_SIZE) pair to
 * SKIP_P.  OVERWRITE, if not 0, will allow the overwriting of data
 * already in the list.  The data and key data will be copied in from
 * the _BUF pointers and stored in skip buffers.  If you pass in a int
 * as data, the int will be copied into the skip entry.  Set either
 * _SIZE < 0 to do internal strlen.
 *
 * If DATA_BUF is null, it will allocate and and store a random block of
 * bytes of DATA_SIZE.  If DATA_BUF is null and DATA_SIZE is 0 then it
 * will store a null pointer as the data.  If BUF_P is not null then
 * it will pass back the block that it allocated for the data.  If
 * OVERWRITE is 0, and the key exists, BUF_P will contain the data
 * associated with that key and a overwrite error will be returned.
 * It returns skip error codes.
 */
int	skip_insert(skip_t *skip_p,
		    const void *key_buf, const int key_size,
		    const void *data_buf, const int data_size,
		    void **buf_p, const char overwrite)
{
  int		level, level_c, new_size;
  datum_t	key_info;
  skip_node_t	**update_p = NULL, *node_p, *new_p, *top_p;
  
  if (skip_p == NULL)
    return SKIP_ERROR_ARG_NULL;
  if (skip_p->sk_magic != SKIP_MAGIC)
    return SKIP_ERROR_PNT;
  /* data_buf can be null but must have valid size */
  if (data_buf == NULL && data_size < 0)
    return SKIP_ERROR_SIZE;
  
#ifndef NO_MMAP
  /* no mmap support so immediate error */
  if (skip_p->sk_mmap != NULL)
    return SKIP_ERROR_MMAP_OP;
#endif
  
  top_p = skip_p->sk_top_p;
  
  /* setup back-trace list */
  if (top_p->sn_forward_n > 0) {
    update_p = (skip_node_t **)alloca(sizeof(skip_node_t *) *
				      top_p->sn_forward_n);
    if (update_p == NULL)
      return SKIP_ERROR_ALLOC;
  }
  
  key_info.da_data = (void *)key_buf;
  if (key_size < 0)
    key_info.da_size = strlen(key_buf) + 1;
  else
    key_info.da_size = key_size;
  
  /* try and find the node while tracking the predecessors */
  node_p = find_node(skip_p, &key_info, update_p);
  if (node_p != NULL) {
    
    /* if we found it then we are in overwrite mode */
    if (! overwrite) {
      if (buf_p != NULL)
	*buf_p = SKIP_POINTER(skip_p, void *, node_p->sn_data.da_data);
      return SKIP_ERROR_OVERWRITE;
    }
    
    /* figure out data size */
    if (data_size < 0)
      new_size = strlen(data_buf) + 1;
    else
      new_size = data_size;
    
    /* do we have the space already?  maybe this should be > only */
    if (new_size != node_p->sn_data.da_size) {
      if (node_p->sn_data.da_data != NULL)
	free(node_p->sn_data.da_data);
      if (new_size == 0)
	node_p->sn_data.da_data = NULL;
      else {
	node_p->sn_data.da_data = malloc(new_size);
	if (node_p->sn_data.da_data == NULL)
	  return SKIP_ERROR_ALLOC;
      }
      node_p->sn_data.da_size = new_size;
    }
    
    if (new_size > 0 && data_buf != NULL)
      memcpy(node_p->sn_data.da_data, data_buf, new_size);
    if (buf_p != NULL)
      *buf_p = node_p->sn_data.da_data;
    
    return SKIP_ERROR_NONE;
  }
  
  level = random_level(top_p);
  
  /* create new node */
  new_p = (skip_node_t *)malloc(NODE_SIZE(level));
  if (new_p == NULL) {
    free(node_p);
    return SKIP_ERROR_ALLOC;
  }
  
  /* create the key */
  new_p->sn_key.da_size = key_info.da_size;
  new_p->sn_key.da_data = malloc(new_p->sn_key.da_size);
  if (new_p->sn_key.da_data == NULL) {
    free(node_p);
    free(new_p);
    return SKIP_ERROR_ALLOC;
  }
  
  memcpy(new_p->sn_key.da_data, key_buf, new_p->sn_key.da_size);
  
  if (data_size == 0) {
    new_p->sn_data.da_size = 0;
    new_p->sn_data.da_data = NULL;
  }
  else {
    if (data_size < 0)
      new_p->sn_data.da_size = strlen(data_buf) + 1;
    else
      new_p->sn_data.da_size = data_size;
    new_p->sn_data.da_data = malloc(new_p->sn_data.da_size);
    if (new_p->sn_data.da_data == NULL) {
      free(node_p);
      free(new_p->sn_key.da_data);
      free(new_p);
      return SKIP_ERROR_ALLOC;
    }
    if (buf_p != NULL)
      *buf_p = new_p->sn_data.da_data;
    if (data_buf != NULL)
      memcpy(new_p->sn_data.da_data, data_buf, new_p->sn_data.da_size);
  }
  
  /* update all necessary forward info */
  new_p->sn_forward_n = level;
  for (level_c = 0; level_c < level; level_c++) {
    if (top_p->sn_forward_n == 0 || level_c > top_p->sn_forward_n - 1) {
      new_p->sn_forward_p[level_c] = NULL;
      continue;
    }
    
    node_p = update_p[level_c];
    /* NOTE: we assume here that the current node's height is >= level_c); */
    
    new_p->sn_forward_p[level_c] = node_p->sn_forward_p[level_c];
    node_p->sn_forward_p[level_c] = new_p;
  }
  
  /* update root node */
  if (level > top_p->sn_forward_n) {
    top_p = (skip_node_t *)realloc(top_p, NODE_SIZE(level));
    if (top_p == NULL) {
      free(node_p);
      free(new_p->sn_key.da_data);
      free(new_p->sn_data.da_data);
      free(new_p);
      /* whole bunch of nodes not freed */
      return SKIP_ERROR_ALLOC;
    }
    
    skip_p->sk_top_p = top_p;
    
    /* update new root forward pointers */
    for (; top_p->sn_forward_n < level; (top_p->sn_forward_n)++)
      top_p->sn_forward_p[top_p->sn_forward_n] = new_p;
  }
  
  skip_p->sk_entry_n++;
  
  return SKIP_ERROR_NONE;
}

/*
 * find in SKIP_P, KEY of size KEY_SIZE returning pointer to data in
 * DATA_P and size in DATA_SIZE_P (if either not null).  any size arg
 * can be < 0 meaning that it will do a strlen on them.  returns skip
 * error codes.
 */
int	skip_retrieve(skip_t *skip_p,
		      const void *key_buf, const int key_size,
		      void **data_p, int *data_size_p)
  
{
  datum_t	key_info;
  skip_node_t	*node_p;
  
  if (skip_p == NULL)
    return SKIP_ERROR_ARG_NULL;
  if (skip_p->sk_magic != SKIP_MAGIC)
    return SKIP_ERROR_PNT;
  
  key_info.da_data = (void *)key_buf;
  if (key_size < 0)
    key_info.da_size = strlen(key_buf) + 1;
  else
    key_info.da_size = key_size;
  
  /* try and find the node */
  node_p = find_node(skip_p, &key_info, NULL);
  if (node_p == NULL)
    return SKIP_ERROR_NOT_FOUND;
  
  if (data_p != NULL)
    *data_p = SKIP_POINTER(skip_p, void *, node_p->sn_data.da_data);
  if (data_size_p != NULL)
    *data_size_p = node_p->sn_data.da_size;
  
  return SKIP_ERROR_NONE;
}

/*
 * Delete from SKIP_P, KEY of size KEY_SIZE with its associated data.
 * any size arg can be < 0 meaning that it will do a strlen on them.
 * If DATA_BUF is not null, it will get the allocated space of the
 * data which must be freed.  If DATA_SIZE is not null, it will get
 * the data's size.  Returns skip error codes.
 */
int	skip_delete(skip_t *skip_p, const void *key_buf, const int key_size,
		    void **data_buf, int *data_size_p)
{
  int		level_c;
  datum_t	key_info;
  skip_node_t	**update_p, *found_p, *top_p;
  
  if (skip_p == NULL)
    return SKIP_ERROR_ARG_NULL;
  if (skip_p->sk_magic != SKIP_MAGIC)
    return SKIP_ERROR_PNT;
  
#ifndef NO_MMAP
  /* no mmap support so immediate error */
  if (skip_p->sk_mmap != NULL)
    return SKIP_ERROR_MMAP_OP;
#endif
  
  top_p = skip_p->sk_top_p;
  
  /* setup back-trace list */
  if (top_p->sn_forward_n == 0)
    return SKIP_ERROR_NOT_FOUND;
  
  update_p = (skip_node_t **)alloca(sizeof(skip_node_t *) *
				    top_p->sn_forward_n);
  if (update_p == NULL)
    return SKIP_ERROR_ALLOC;
  
  key_info.da_data = (void *)key_buf;
  if (key_size < 0)
    key_info.da_size = strlen(key_buf) + 1;
  else
    key_info.da_size = key_size;
  
  /* try and find the node while tracking the predecessors */
  found_p = find_node(skip_p, &key_info, update_p);
  if (found_p == NULL)
    return SKIP_ERROR_NOT_FOUND;
  
  for (level_c = 0; level_c < found_p->sn_forward_n; level_c++)
    update_p[level_c]->sn_forward_p[level_c] = found_p->sn_forward_p[level_c];
  
  /* if we are deleting the one we are on then go to next */
  if (skip_p->sk_this_p == found_p)
    skip_p->sk_this_p = skip_p->sk_this_p->sn_forward_p[0];
  
  /* if we now have a null list, do some clean-up */
  if (top_p->sn_forward_p[0] == NULL) {
    /* now we realloc the top node to have 0 forward pointers */
    top_p = (skip_node_t *)realloc(top_p, sizeof(skip_node_t));
    if (top_p == NULL)
      return SKIP_ERROR_ALLOC;
    top_p->sn_forward_n = 0;
    skip_p->sk_top_p = top_p;
  }
  
  free(found_p->sn_key.da_data);
  if (data_buf == NULL)
    free(found_p->sn_data.da_data);
  else
    *data_buf = found_p->sn_data.da_data;
  found_p->sn_data.da_data = NULL;
  if (data_size_p != NULL)
    *data_size_p = found_p->sn_data.da_size;
  free(found_p);
  
  skip_p->sk_entry_n--;
  
  return SKIP_ERROR_NONE;
}

/*
 * Delete the first entry from SKIP_P.  It passes back the KEY_P
 * (which will need to be freed later) and size KEY_SIZE_P with its
 * DATA_P (which also must then be freed) and size DATA_SIZE_P, if any
 * non-null.  Returns skip error codes.
 */
int	skip_delete_first(skip_t *skip_p,
			  void **key_p, int *key_size_p,
			  void **data_p, int *data_size_p)

{
  int		level_c;
  skip_node_t	*node_p, *top_p;
  
  if (skip_p == NULL)
    return SKIP_ERROR_ARG_NULL;
  if (skip_p->sk_magic != SKIP_MAGIC)
    return SKIP_ERROR_PNT;
  
#ifndef NO_MMAP
  /* no mmap support so immediate error */
  if (skip_p->sk_mmap != NULL)
    return SKIP_ERROR_MMAP_OP;
#endif
  
  top_p = skip_p->sk_top_p;
  if (top_p->sn_forward_n == 0)
    return SKIP_ERROR_NOT_FOUND;
  
  node_p = top_p->sn_forward_p[0];
  
  /* update the top nodes which point to it */
  for (level_c = 0; level_c < node_p->sn_forward_n; level_c++)
    top_p->sn_forward_p[level_c] = node_p->sn_forward_p[level_c];
  
  /* if we are deleting the one we are on, then go to next */
  if (skip_p->sk_this_p == node_p)
    skip_p->sk_this_p = skip_p->sk_this_p->sn_forward_p[0];
  
  /* if we now have a null list, do some clean-up */
  if (top_p->sn_forward_p[0] == NULL) {
    /* now we realloc the top node to have 0 forward pointers */
    top_p = (skip_node_t *)realloc(top_p, sizeof(skip_node_t));
    if (top_p == NULL)
      return SKIP_ERROR_ALLOC;
    top_p->sn_forward_n = 0;
    skip_p->sk_top_p = top_p;
  }
  
  if (key_p == NULL)
    free(node_p->sn_key.da_data);
  else
    *key_p = node_p->sn_key.da_data;
  if (key_size_p != NULL)
    *key_size_p = node_p->sn_key.da_size;
  if (data_p == NULL)
    free(node_p->sn_data.da_data);
  else
    *data_p = node_p->sn_data.da_data;
  if (data_size_p != NULL)
    *data_size_p = node_p->sn_data.da_size;
  free(node_p);
  
  skip_p->sk_entry_n--;
  
  return SKIP_ERROR_NONE;
}

/*
 * Get some information about SKIP_P.  It passes back the height of
 * the current skip-node in HEIGHT_P.  Returns skip error codes.
 */
int	skip_info(skip_t *skip_p, int *height_p)
{
  if (skip_p == NULL)
    return SKIP_ERROR_ARG_NULL;
  if (skip_p->sk_magic != SKIP_MAGIC)
    return SKIP_ERROR_PNT;
  if (skip_p->sk_this_p == NULL)
    return SKIP_ERROR_LINEAR;
  
  if (height_p != NULL)
    *height_p = skip_p->sk_this_p->sn_forward_n;
  
  return SKIP_ERROR_NONE;
}

/*
 * redo the height of all entries in SKIP_P to be of optimal height.
 * returns skip error codes.
 */
int	skip_normalize(skip_t *skip_p)
{
  skip_node_t	*top_p, **update_p, *node_p;
  int		ele_c, height_c, root_height, best_height;
  
  if (skip_p == NULL)
    return SKIP_ERROR_ARG_NULL;
  if (skip_p->sk_magic != SKIP_MAGIC)
    return SKIP_ERROR_PNT;
  
#ifndef NO_MMAP
  /* no mmap support so immediate error */
  if (skip_p->sk_mmap != NULL)
    return SKIP_ERROR_MMAP_OP;
#endif
  
  top_p = skip_p->sk_top_p;
  
  /* do we have anything to do? */
  if (top_p->sn_forward_n == 0)
    return SKIP_ERROR_NONE;
  
  /* find the max height for the whole tree */
  for (best_height = 0;; best_height++)
    if (power_two[best_height] > skip_p->sk_entry_n
	|| power_two[best_height + 1] == 0)
      break;
  /* NOTE: forward_n may be larger than height */
  if (top_p->sn_forward_n > best_height)
    root_height = top_p->sn_forward_n;
  else
    root_height = best_height;
  
  update_p = (skip_node_t **)alloca(sizeof(skip_node_t *) * root_height);
  if (update_p == NULL)
    return SKIP_ERROR_ALLOC;
  
  /* initialize the list of forward pointers to point back to the root */
  for (height_c = 0; height_c < root_height; height_c++)
    update_p[height_c] = top_p;
  
  for (node_p = top_p->sn_forward_p[0], ele_c = 1; node_p != NULL;
       node_p = node_p->sn_forward_p[0], ele_c++) {
    int		height;
    
    /* get a good height for this ele */
    height = good_height(ele_c);
    
    if (height != node_p->sn_forward_n) {
      node_p = (skip_node_t *)realloc(node_p, NODE_SIZE(height));
      if (node_p == NULL)
	return SKIP_ERROR_ALLOC;
    }
    
    /*
     * Now update the back pointers.  Reasonably complete so careful
     * here.  If we are extending this node's forward pointer list,
     * then it needs to encorporate the new entries from the
     * corresponding node in the update-list's forward pointer.  For
     * all of the levels, we need to modify the update-list.
     */
    for (height_c = 0; height_c < height; height_c++) {
      if (height_c >= node_p->sn_forward_n)
	node_p->sn_forward_p[height_c] =
	  update_p[height_c]->sn_forward_p[height_c];
      update_p[height_c]->sn_forward_p[height_c] = node_p;
      update_p[height_c] = node_p;
    }
    /* reset the nodes height */
    node_p->sn_forward_n = height;
  }
  
  /*
   * Now update the null pointers which are the nodes left in the
   * update pointers.  We can also figure out here where to trim the
   * root node's height (if necessary) by seeing how many of the
   * update pointers reference the root node making them unnecessary.
   */
  for (height_c = 0; height_c < root_height; height_c++) {
    if (update_p[height_c]->sn_forward_p[height_c] == top_p) {
      if (height_c > 0)
	height_c--;
      break;
    }
    update_p[height_c]->sn_forward_p[height_c] = NULL;
  }
  
  /* do we need to change the height of the root node */
  if (height_c < root_height - 1) {
    top_p = (skip_node_t *)realloc(top_p, NODE_SIZE(height_c));
    if (top_p == NULL)
      return SKIP_ERROR_ALLOC;
    top_p->sn_forward_n = height_c;
    skip_p->sk_top_p = top_p;
  }
  
  return SKIP_ERROR_NONE;
}

/*
 * return the string equivalent of skip ERROR
 */
const char	*skip_strerror(const int error)
{
  error_str_t	*err_p;
  
  for (err_p = errors; err_p->es_error != 0; err_p++)
    if (err_p->es_error == error)
      return err_p->es_string;
  
  return INVALID_ERROR;
}

/*
 * Return the size of the skip_t type.
 */
int	skip_type_size(void)
{
  return sizeof(skip_t);
}

/************************** linear access functions **************************/

/*
 * get from SKIP_P, the first KEY_P and size KEY_SIZE_P with its DATA_P
 * and size DATA_SIZE_P (if any non-null).  returns skip error codes.
 */
int	skip_first(skip_t *skip_p,
		   void **key_p, int *key_size_p,
		   void **data_p, int *data_size_p)
{
  skip_node_t	*node_p;
  
  if (skip_p == NULL)
    return SKIP_ERROR_ARG_NULL;
  if (skip_p->sk_magic != SKIP_MAGIC)
    return SKIP_ERROR_PNT;
  
  node_p = SKIP_POINTER(skip_p, skip_node_t *, skip_p->sk_top_p);
  if (node_p->sn_forward_n == 0)
    return SKIP_ERROR_NOT_FOUND;
  
  node_p = SKIP_POINTER(skip_p, skip_node_t *, node_p->sn_forward_p[0]);
  skip_p->sk_this_p = node_p;
  
  if (key_p != NULL)
    *key_p = SKIP_POINTER(skip_p, void *, node_p->sn_key.da_data);
  if (key_size_p != NULL)
    *key_size_p = node_p->sn_key.da_size;
  if (data_p != NULL)
    *data_p = SKIP_POINTER(skip_p, void *, node_p->sn_data.da_data);
  if (data_size_p != NULL)
    *data_size_p = node_p->sn_data.da_size;
  
  return SKIP_ERROR_NONE;
}

/*
 * get from SKIP_P, the next KEY_P and size KEY_SIZE_P with its DATA_P and
 * size DATA_SIZE_P (if any non-null). returns skip error codes.
 */
int	skip_next(skip_t * skip_p,
		  void **key_p, int *key_size_p,
		  void **data_p, int *data_size_p)
{
  skip_node_t	*node_p;
  
  if (skip_p == NULL)
    return SKIP_ERROR_ARG_NULL;
  if (skip_p->sk_magic != SKIP_MAGIC)
    return SKIP_ERROR_PNT;
  
  node_p = skip_p->sk_this_p;
  if (node_p == NULL)
    return SKIP_ERROR_LINEAR;
  
  node_p = SKIP_POINTER(skip_p, skip_node_t *, node_p->sn_forward_p[0]);
  if (node_p == NULL)
    return SKIP_ERROR_NOT_FOUND;
  
  skip_p->sk_this_p = node_p;

  if (key_p != NULL)
    *key_p = SKIP_POINTER(skip_p, void *, node_p->sn_key.da_data);
  if (key_size_p != NULL)
    *key_size_p = node_p->sn_key.da_size;
  if (data_p != NULL)
    *data_p = SKIP_POINTER(skip_p, void *, node_p->sn_data.da_data);
  if (data_size_p != NULL)
    *data_size_p = node_p->sn_data.da_size;
  
  return SKIP_ERROR_NONE;
}

/*
 * get from SKIP_P, the current KEY_P and size KEY_SIZE_P with its DATA_P
 * and size DATA_SIZE_P (if any non-null). returns skip error codes.
 */
int	skip_this(skip_t *skip_p,
		  void **key_p, int *key_size_p,
		  void **data_p, int *data_size_p)
{
  skip_node_t	*node_p;
  
  if (skip_p == NULL)
    return SKIP_ERROR_ARG_NULL;
  if (skip_p->sk_magic != SKIP_MAGIC)
    return SKIP_ERROR_PNT;
  
  node_p = skip_p->sk_this_p;
  if (node_p == NULL)
    return SKIP_ERROR_LINEAR;
  
  if (key_p != NULL)
    *key_p = SKIP_POINTER(skip_p, void *, node_p->sn_key.da_data);
  if (key_size_p != NULL)
    *key_size_p = node_p->sn_key.da_size;
  if (data_p != NULL)
    *data_p = SKIP_POINTER(skip_p, void *, node_p->sn_data.da_data);
  if (data_size_p != NULL)
    *data_size_p = node_p->sn_data.da_size;
  
  return SKIP_ERROR_NONE;
}

/******************************* mmap routines *******************************/

/*
 * mmap a skip from disk at PATH.  The COMPARE function is used to
 * locate keys and should match (exactly) the routine used to build
 * the list that was written -- set to null to use memcmp.  Returns a
 * skip pointer or NULL on error.  Passes back skip errors in ERROR_P.
 */
skip_t	*skip_mmap(const char *path, const skip_compare_t compare,
		   int *error_p)
{
#ifdef NO_MMAP
  
  /* no mmap support so immediate error */
  if (error_p != NULL)
    *error_p = SKIP_ERROR_MMAP_NONE;
  return NULL;
  
#else
  
  skip_t	*skip_p;
  struct stat	sbuf;
  int		fd, state;
  
  /* open the mmap file */
  fd = open(path, O_RDONLY, 0);
  if (fd < 0) {
    if (error_p != NULL)
      *error_p = SKIP_ERROR_FILE;
    return NULL;
  }
  
  /* get the file size */
  if (fstat(fd, &sbuf) != 0) {
    if (error_p != NULL)
      *error_p = SKIP_ERROR_FILE;
    return NULL;
  }
  
  skip_p = (skip_t *)malloc(sizeof(skip_t));
  if (skip_p == NULL) {
    if (error_p != NULL)
      *error_p = SKIP_ERROR_ALLOC;
    return NULL;
  }
  
  /* mmap the space and close the file */
#ifdef __alpha
  state = (MAP_SHARED | MAP_FILE | MAP_VARIABLE);
#else
  state = MAP_SHARED;
#endif
  
  skip_p->sk_mmap = (skip_t *)mmap((caddr_t)0, sbuf.st_size, PROT_READ, state,
				   fd, 0);
  (void)close(fd);
  
  if (skip_p->sk_mmap == (skip_t *)MAP_FAILED) {
    if (error_p != NULL)
      *error_p = SKIP_ERROR_MMAP;
    return NULL;
  }  
  
  /* is the mmap file contain bad info or maybe another system type? */
  if (skip_p->sk_mmap->sk_magic != SKIP_MAGIC) {
    if (error_p != NULL)
      *error_p = SKIP_ERROR_PNT;
    return NULL;
  }
  
  /* sanity check on the file size */
  if (skip_p->sk_mmap->sk_file_size != sbuf.st_size) {
    if (error_p != NULL)
      *error_p = SKIP_ERROR_SIZE;
    return NULL;
  }
  
  skip_p->sk_magic = SKIP_MAGIC;
  skip_p->sk_entry_n = skip_p->sk_mmap->sk_entry_n;
  skip_p->sk_compare = compare;
  skip_p->sk_top_p = skip_p->sk_mmap->sk_top_p;
  skip_p->sk_this_p = NULL;
  /* mmap is already set */
  skip_p->sk_file_size = skip_p->sk_mmap->sk_file_size;
  
  if (error_p != NULL)
    *error_p = SKIP_ERROR_NONE;
  return skip_p;
  
#endif
}

/*
 * unmap a skip SKIP_P from memory.  returns skip error codes.
 */
int	skip_munmap(skip_t *skip_p)
{
#ifdef NO_MMAP
  
  /* no mmap support so immediate error */
  return SKIP_ERROR_MMAP_NONE;
  
#else
  
  (void)munmap((caddr_t)skip_p->sk_mmap, skip_p->sk_file_size);
  skip_p->sk_magic = 0;
  free(skip_p);
  return SKIP_ERROR_NONE;
  
#endif
}

/******************************* file routines *******************************/

/*
 * Reads and returns a skip from PATH.  The COMPARE function is used
 * to locate keys and should match (exactly) the routine used to build
 * the list that was written -- set to null to use memcmp.  Passes
 * back skip errors in ERROR_P.
 */
skip_t	*skip_read(const char *path, const skip_compare_t compare,
		   int *error_p)
{
  long		next, data_pos;
  int		fd, node_size, forw_c, entry_c, size;
  FILE		*infile;
  pnt_size_t	*entries, *entry_p;
  skip_node_t	*node_p, *last_p = NULL;
  skip_t	*skip_p;
  
  /* open the file */
  fd = open(path, O_RDONLY, 0);
  if (fd < 0) {
    if (error_p != NULL)
      *error_p = SKIP_ERROR_FILE;
    return NULL;
  }
  
  /* allocate a skip structure */
  skip_p = malloc(sizeof(skip_t));
  if (skip_p == NULL) {
    if (error_p != NULL)
      *error_p = SKIP_ERROR_ALLOC;
    return NULL;
  }
  
  /* now open the fd to get buffered i/o */
  infile = fdopen(fd, "r");
  if (infile == NULL) {
    if (error_p != NULL)
      *error_p = SKIP_ERROR_FILE;
    return NULL;
  }
  
  /* track the file pos */
  size = 0;
  
  /* read the main skip struct */
  if (fread(skip_p, sizeof(skip_t), 1, infile) != 1) {
    if (error_p != NULL)
      *error_p = SKIP_ERROR_FILE;
    free(skip_p);
    return NULL;
  }
  
  skip_p->sk_file_size = 0;
  skip_p->sk_compare = compare;
  
  size += sizeof(skip_t);
  
  /* is the mmap file contain bad info or maybe another system type? */
  if (skip_p->sk_magic != SKIP_MAGIC) {
    if (error_p != NULL)
      *error_p = SKIP_ERROR_PNT;
    return NULL;
  }
  
  /* allocate a block of sizes for each entry + head-entry */
  entries = (pnt_size_t *)alloca(sizeof(pnt_size_t) *
				 (skip_p->sk_entry_n + 1));
  if (entries == NULL) {
    if (error_p != NULL)
      *error_p = SKIP_ERROR_ALLOC;
    free(skip_p);
    return NULL;
  }
  
  /* read in the entries */
  next = sizeof(skip_t);
  for (entry_c = 0; entry_c < skip_p->sk_entry_n + 1; entry_c++) {
    
    /* make a new entry */
    node_p = (skip_node_t *)malloc(sizeof(skip_node_t));
    if (node_p == NULL) {
      if (error_p != NULL)
	*error_p = SKIP_ERROR_ALLOC;
      free(skip_p);
      /* the other skip elements will not be freed */
      return NULL;
    }
    entries[entry_c].ps_node_p = node_p;
    entries[entry_c].ps_size = next;
    
    /* read in the start of the node */
    node_size = sizeof(skip_node_t);
    if (fseek(infile, next, SEEK_SET) != 0
	|| fread(node_p, node_size, 1, infile) != 1) {
      if (error_p != NULL)
	*error_p = SKIP_ERROR_FILE;
      free(skip_p);
      free(node_p);
      /* the other skip elements will not be freed */
      return NULL;
    }
    
    /* NOTE: now that we have the number of forward entries we maybe re-read */
    if (node_p->sn_forward_n > 1) {
      /* make a new entry */
      node_size = NODE_SIZE(node_p->sn_forward_n);
      node_p = (skip_node_t *)realloc(node_p, node_size);
      if (node_p == NULL) {
	if (error_p != NULL)
	  *error_p = SKIP_ERROR_ALLOC;
	free(skip_p);
	/* the other skip elements will not be freed */
	return NULL;
      }
      
      /* we now read the true size */
      if (fseek(infile, next, SEEK_SET) != 0
	  || fread(node_p, node_size, 1, infile) != 1) {
	if (error_p != NULL)
	  *error_p = SKIP_ERROR_FILE;
	free(skip_p);
	free(node_p);
	/* the other skip elements will not be freed */
	return NULL;
      }
    }
    
    /* maintain the bottom row linked list */
    if (last_p == NULL)
      skip_p->sk_top_p = node_p;
    else
      last_p->sn_forward_p[0] = node_p;
    
    size += node_size;
    size += node_p->sn_key.da_size;
    size += node_p->sn_data.da_size;
    
    /* now read in the key */
    if (node_p->sn_key.da_size == 0)
      node_p->sn_key.da_data = NULL;
    else {
      data_pos = (long)node_p->sn_key.da_data;
      node_p->sn_key.da_data = malloc(node_p->sn_key.da_size);
      if (node_p->sn_key.da_data == NULL) {
	if (error_p != NULL)
	  *error_p = SKIP_ERROR_ALLOC;
	free(skip_p);
	free(node_p);
	/* the other list elements will not be freed */
	return NULL;
      }
      
      if (fseek(infile, data_pos, SEEK_SET) != 0
	  || fread(node_p->sn_key.da_data, node_p->sn_key.da_size, 1,
		   infile) != 1) {
	if (error_p != NULL)
	  *error_p = SKIP_ERROR_FILE;
	free(skip_p);
	free(node_p);
	/* the other list elements will not be freed */
	return NULL;
      }
    }
    
    /* read in the entry's data */
    if (node_p->sn_data.da_size == 0)
      node_p->sn_data.da_data = NULL;
    else {
      data_pos = (long)node_p->sn_data.da_data;
      node_p->sn_data.da_data = malloc(node_p->sn_data.da_size);
      if (node_p->sn_data.da_data == NULL) {
	if (error_p != NULL)
	  *error_p = SKIP_ERROR_ALLOC;
	free(skip_p);
	free(node_p);
	/* the other list elements will not be freed */
	return NULL;
      }
      if (fseek(infile, data_pos, SEEK_SET) != 0
	  || fread(node_p->sn_data.da_data, node_p->sn_data.da_size, 1,
		   infile) != 1) {
	if (error_p != NULL)
	  *error_p = SKIP_ERROR_FILE;
	free(skip_p);
	free(node_p);
	/* the other list elements will not be freed */
	return NULL;
      }
    }
    
    next = (long)node_p->sn_forward_p[0];
    last_p = node_p;
  }
  (void)fclose(infile);
  
  if (last_p != NULL)
    last_p->sn_forward_p[0] = NULL;
  
  /* now we need to post process all the forward links */
  for (node_p = skip_p->sk_top_p; node_p != NULL;
       node_p = node_p->sn_forward_p[0]) {
    
    /* set the forward pointer sizes */
    entry_p = entries;
    /* we start at 1 because 0 was taken care of above */
    for (forw_c = 1; forw_c < node_p->sn_forward_n; forw_c++) {
      
      /* no need to convert the nulls */
      if (node_p->sn_forward_p[forw_c] == NULL)
	continue;
      
      /*
       * NOTE: we always increase entry_p because we are searching
       * bottom -> up in the list which always increases
       */
      for (; entry_p < entries + skip_p->sk_entry_n + 1; entry_p++)
	if ((long)node_p->sn_forward_p[forw_c] == entry_p->ps_size)
	  break;
      /* we better have found it */
      if (entry_p >= entries + skip_p->sk_entry_n + 1) {
	if (error_p != NULL)
	  *error_p = SKIP_ERROR_INTERNAL;
	free(skip_p);
	/* the list elements will not be freed */
	return NULL;
      }
      
      node_p->sn_forward_p[forw_c] = entry_p->ps_node_p;
    }
  }
  
  if (error_p != NULL)
    *error_p = SKIP_ERROR_NONE;
  return skip_p;
}

/*
 * write skip in SKIP_P to file PATH with file perms MODE
 */
int	skip_write(const skip_t *skip_p, const char *path, const int mode)
{
  int		fd, entry_c, forw_c, rem, node_size;
  long		size;
  pnt_size_t	*entries, *entry_p;
  skip_t	main;
  skip_node_t	*node_p, *copy_p;
  FILE		*outfile;
  
  if (skip_p == NULL)
    return SKIP_ERROR_ARG_NULL;
  if (skip_p->sk_magic != SKIP_MAGIC)
    return SKIP_ERROR_PNT;
  
#ifndef NO_MMAP
  /* no mmap support so immediate error */
  if (skip_p->sk_mmap != NULL)
    return SKIP_ERROR_MMAP_OP;
#endif
  
  fd = open(path, O_WRONLY | O_CREAT, mode);
  if (fd < 0)
    return SKIP_ERROR_FILE;
  
  outfile = fdopen(fd, "w");
  if (outfile == NULL)
    return SKIP_ERROR_FILE;
  
  /* allocate a block of sizes for each entry + head-entry */
  entries = (pnt_size_t *)alloca(sizeof(pnt_size_t) *
				 (skip_p->sk_entry_n + 1));
  if (entries == NULL)
    return SKIP_ERROR_ALLOC;
  
  /* make a temporary copy -- max height is the height of the head node */
  copy_p = (skip_node_t *)malloc(NODE_SIZE(skip_p->sk_top_p->sn_forward_n));
  if (copy_p == NULL) {
    free(entries);
    return SKIP_ERROR_ALLOC;
  }
  
  /* make a copy of the main struct */
  main = *skip_p;
  
  /* start counting the bytes */
  size = 0;
  
  /* head node goes right after main struct */
  node_p = main.sk_top_p;
  size += sizeof(skip_t);
  
  /* run through and count the entry sizes */
  for (entry_c = 0; entry_c < skip_p->sk_entry_n + 1; entry_c++) {
    entries[entry_c].ps_node_p = node_p;
    entries[entry_c].ps_size = size;
    size += NODE_SIZE(node_p->sn_forward_n);
    size += node_p->sn_key.da_size;
    size += node_p->sn_data.da_size;
    /*
     * We now have to round the file to the nearest long so the
     * mmaping of the longs in the entry structs will work.
     *
     * We could also run through the skip and write all the entries structs
     * out with the modified data offsets, and then write all the
     * keys and data at the end.  We could do this because we know
     * the number of entries and their size beforehand.  But this
     * may increase page-faults if the entry and data aren't near.
     */
    rem = size % sizeof(long);
    if (rem > 0)
      size += sizeof(long) - rem;
    node_p = node_p->sn_forward_p[0];
  }
  
  /* set the main fields */
  main.sk_compare = NULL;
  main.sk_this_p = NULL;
  main.sk_mmap = NULL;
  main.sk_file_size = size;
  
  /*
   * now we can start the writing because we got the bucket offsets
   */
  
  /* write the main skip struct */
  size = 0;
  size += sizeof(skip_t);
  node_p = main.sk_top_p;
  main.sk_top_p = (skip_node_t *)size;
  if (fwrite(&main, sizeof(skip_t), 1, outfile) != 1)
    return SKIP_ERROR_FILE;
  
  /* write out the entries */
  for (entry_c = 0; entry_c < skip_p->sk_entry_n + 1; entry_c++) {
    
    node_size = NODE_SIZE(node_p->sn_forward_n);
    /* copy the node into the temporary buffer */
    memcpy(copy_p, node_p, node_size);
    size += node_size;
    
    /* set the forward pointer sizes */
    entry_p = entries;
    for (forw_c = 0; forw_c < node_p->sn_forward_n; forw_c++) {
      
      /* no need to convert the nulls */
      if (copy_p->sn_forward_p[forw_c] == NULL)
	continue;
      
      /*
       * NOTE: we always increase entry_p because we are searching
       * bottom -> up in the list which always increases
       */
      for (; entry_p < entries + skip_p->sk_entry_n + 1; entry_p++)
	if (node_p->sn_forward_p[forw_c] == entry_p->ps_node_p)
	  break;
      /* we better have found it */
      if (entry_p >= entries + skip_p->sk_entry_n + 1) {
	free(entries);
	free(copy_p);
	return SKIP_ERROR_INTERNAL;
      }
      
      copy_p->sn_forward_p[forw_c] = (skip_node_t *)entry_p->ps_size;
    }
    
    if (node_p->sn_key.da_size > 0) {
      copy_p->sn_key.da_data = (void *)size;
      size += node_p->sn_key.da_size;
    }
    if (node_p->sn_data.da_size > 0) {
      copy_p->sn_data.da_data = (void *)size;
      size += node_p->sn_data.da_size;
    }
    
    /* write out the node */
    if (fwrite(copy_p, node_size, 1, outfile) != 1)
      return SKIP_ERROR_FILE;
    
    /* now write out its data */
    if (node_p->sn_key.da_size > 0) {
      if (fwrite(node_p->sn_key.da_data, node_p->sn_key.da_size, 1,
		 outfile) != 1)
	return SKIP_ERROR_FILE;
    }
    if (node_p->sn_data.da_size > 0) {
      if (fwrite(node_p->sn_data.da_data, node_p->sn_data.da_size, 1,
		 outfile) != 1)
	return SKIP_ERROR_FILE;
    }
    
    /* duplicate rounding function defined above */
    rem = size % sizeof(long);
    if (rem > 0) {
      rem = sizeof(long) - rem;
      if (fseek(outfile, rem, SEEK_CUR) != 0)
	return SKIP_ERROR_FILE;
      size += rem;
    }
    
    node_p = node_p->sn_forward_p[0];
  }
  
  (void)fclose(outfile);
  /* in case we are overwriting another list */
  (void)truncate(path, size);
  
  return SKIP_ERROR_NONE;
}
