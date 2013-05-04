/*
 * test program for skip lists.
 *
 * Copyright 1996 by Gray Watson.
 *
 * $Id: skip_t.c,v 1.2 1998/03/25 01:07:03 gray Exp $
 */

/*
 * Based on the Skip List algorithms described in the June 1990 issue
 * of CACM and invented by William Pugh in 1987.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SKIP_T_MAIN

#include "skip.h"

#define TEST_PATH		"skip_file"
#define RANDOM_VALUE(x)		((random() % ((x) * 10)) / 10)

#define SAMPLE_SIZE	1024
#define ITERATIONS	10000
#define MAX_ENTRIES	10240

static	char	*rcs_id =
  "$Id: skip_t.c,v 1.2 1998/03/25 01:07:03 gray Exp $";

#define MODE_CLEAR	0		/* then only a 1 in 100 */
#define MODE_INSERT	1		/* store a value into table */
#define MODE_REPLACE	2		/* store with overwrite */
#define MODE_RETRIEVE	3		/* retrieve value from table */
#define MODE_DELETE	4		/* delete value */	
#define MODE_INFO	5		/* get info about current entry */
#define MODE_FIRST	6		/* return first entry in table */
#define MODE_NEXT	7		/* return next entry in table */
#define MODE_THIS	8		/* return current entry in table */
#define MODE_NORMALIZE	9		/* normalize the list */
#define MODE_MAX	10

typedef struct entry_st {
  char			en_free;		/* free flag */
  long			en_data;		/* value to store */
  struct entry_st	*en_next_p;		/* next pointer */
} entry_t;

/* local vars */
static	int		call_c = 0;

/*
 * compare two keys
 */
static	int	compare(const void *key1_p, const int key1_size,
			const void *key2_p, const int key2_size)
{
    int key1, key2;
    key1 = *(int *)key1_p;
    key2 = *(int *)key2_p;
    if (key1 > key2) 
        return 1;
    else if (key1 < key2) 
        return -1;
    else
        return 0;
  
    //return *(int *)key1_p - *(int *)key2_p; /* may overflow */
}

/*
 * dump the contents of LISTP
 */
static	void	dump_list(skip_t * list)
{
  int	*key_p, *data_p, ret, height, error;
  char	marker[] = "*********************************************************";
  
  for (ret = skip_first(list, (void **)&key_p, NULL, (void **)&data_p, NULL);
       ret == SKIP_ERROR_NONE;
       ret = skip_next(list, (void **)&key_p, NULL, (void **)&data_p, NULL)) {
    error = skip_info(list, &height);
    if (height < 0) {
      (void)printf("ERROR current-height: %s\n", skip_strerror(error));
      break;
    }
    (void)printf("%15d %15d %2d %.*s\n",
		 *key_p, *data_p, height, height, marker);
  }
  
  if (ret != SKIP_ERROR_NOT_FOUND)
    (void)printf("ERROR dump-list: %s\n", skip_strerror(ret));
}

/*
 * run through a number ITER_N of transactions on SKIP_P.
 */
static	void	stress(skip_t *skip_p, const int iter_n, const char mmaping,
		       const char verbose)
{
  long		data, *data_p, key, *key_p;
  int		mode, iter_c, size, pnt_c, free_c, ret;
  entry_t	*grid, *free_p, *grid_p, *last_p;
  char		linear = 0;
  
  (void)printf("Stressing for %d iterations\n", iter_n);
  
  grid = malloc(sizeof(entry_t) * MAX_ENTRIES);
  if (grid == NULL) {
    (void)printf("problems allocating space for %d entries.\n",
		 MAX_ENTRIES);
    exit(1);
  }
  
  /* initialize free list */
  free_p = grid;
  for (grid_p = grid; grid_p < grid + MAX_ENTRIES; grid_p++) {
    grid_p->en_free = 1;
    grid_p->en_next_p = grid_p + 1;
  }
  /* redo the last next pointer */
  (grid_p - 1)->en_next_p = NULL;
  free_c = MAX_ENTRIES;
  
  /* load the list */
  if (mmaping) {
  }
  
  for (iter_c = 0; iter_c < iter_n;) {
    int		which;
    
    /* decide what to do */
    mode = RANDOM_VALUE(MODE_MAX);
    
    switch (mode) {
      
    case MODE_CLEAR:
      if (mmaping)
	continue;
      which = RANDOM_VALUE(300);
      if (which != 1)
	continue;
      
      call_c++;
      if (skip_clear(skip_p) != SKIP_ERROR_NONE) {
	(void)fprintf(stderr, "ERROR clearning list: %s\n",
		      skip_strerror(ret));
	continue;
      }
      if (verbose)
	(void)printf("skip cleared\n");
	
      /* re-init free list */
      free_p = grid;
      for (grid_p = grid; grid_p < grid + MAX_ENTRIES; grid_p++) {
	grid_p->en_free = 1;
	grid_p->en_next_p = grid_p + 1;
      }
      /* redo the last next pointer */
      (grid_p - 1)->en_next_p = NULL;
      free_c = MAX_ENTRIES;
      linear = 0;
      iter_c++;
      if (verbose)
	(void)printf("skip cleared.\n");
      break;
      
    case MODE_INSERT:
      if (mmaping)
	continue;
      if (free_c <= 0)
	continue;
      
      which = RANDOM_VALUE(free_c);
      last_p = NULL;
      grid_p = free_p;
      for (pnt_c = 0; pnt_c < which && grid_p != NULL; pnt_c++) {
	last_p = grid_p;
	grid_p = grid_p->en_next_p;
      }
      if (grid_p == NULL) {
	(void)printf("reached end of free list prematurely\n");
	exit(1);
      }
	
      key = grid_p - grid;
      data = RANDOM_VALUE(1000000);
	
      call_c++;
      ret = skip_insert(skip_p, &key, sizeof(key), &data, sizeof(data),
			NULL, 0);
      if (ret == SKIP_ERROR_NONE) {
	if (verbose)
	  (void)printf("stored in pos %ld, data %ld\n", key, data);
	grid_p->en_free = 0;
	grid_p->en_data = data;
	  
	/* shift free list */
	if (last_p == NULL)
	  free_p = grid_p->en_next_p;
	else
	  last_p->en_next_p = grid_p->en_next_p;
	grid_p->en_next_p = NULL;
	free_c--;
	iter_c++;
      }
      else
	(void)fprintf(stderr, "ERROR storing #%ld: %s\n",
		      key, skip_strerror(ret));
      break;
      
    case MODE_REPLACE:
      if (mmaping)
	continue;
      if (free_c >= MAX_ENTRIES)
	continue;
      
      key = RANDOM_VALUE(MAX_ENTRIES);
	
      if (grid[key].en_free)
	continue;
	
      data = RANDOM_VALUE(1000000);
	
      call_c++;
      ret = skip_insert(skip_p, &key, sizeof(key), &data, sizeof(data),
			NULL, 1);
      if (ret == SKIP_ERROR_NONE) {
	if (verbose)
	  (void)printf("overwrite pos %ld with %ld\n", key, data);
	grid[key].en_free = 0;
	grid[key].en_data = data;
	grid[key].en_next_p = NULL;
	free_c--;
	iter_c++;
      }
      else
	(void)fprintf(stderr, "error overwriting #%ld: %s\n",
		      key, skip_strerror(ret));
      break;
      
    case MODE_RETRIEVE:
      if (free_c >= MAX_ENTRIES)
	continue;
      
      key = RANDOM_VALUE(MAX_ENTRIES);
      if (grid[key].en_free)
	continue;
      
      call_c++;
      ret = skip_retrieve(skip_p, &key, sizeof(key), (void **)&data_p,
			  &size);
      if (ret == SKIP_ERROR_NONE) {
	if (sizeof(grid[key].en_data) == size
	    && grid[key].en_data == *data_p) {
	  if (verbose)
	    (void)printf("retrieved key %ld, got data %ld\n", key, *data_p);
	}
	else
	  (void)fprintf(stderr,
			"ERROR: retrieve key %ld: data %ld didn't match "
			"skip %ld\n",
			key, grid[key].en_data, *data_p);
	iter_c++;
      }
      else
	(void)fprintf(stderr, "error retrieving key %ld: %s\n",
		      key, skip_strerror(ret));
      break;
      
    case MODE_DELETE:
      if (mmaping)
	continue;
      if (free_c >= MAX_ENTRIES)
	continue;
      which = RANDOM_VALUE(5);
      if (which != 1)
	continue;
      
      key = RANDOM_VALUE(MAX_ENTRIES);
	
      if (grid[key].en_free)
	continue;
	
      call_c++;
      ret = skip_delete(skip_p, &key, sizeof(key), (void **)&data_p, &size);
      if (ret == SKIP_ERROR_NONE) {
	if (sizeof(grid[key].en_data) == size
	    && grid[key].en_data == *data_p) {
	  if (verbose)
	    (void)printf("deleted key %ld, got data %ld\n", key, *data_p);
	}
	else
	  (void)fprintf(stderr,
			"ERROR deleting key %ld: data %ld didn't match "
			"skip %ld\n",
			key, grid[key].en_data, *data_p);
	grid[key].en_free = 1;
	grid[key].en_next_p = free_p;
	free_p = grid + key;
	free_c++;
	if (free_c == MAX_ENTRIES)
	  linear = 0;
	iter_c++;
      }
      else
	(void)fprintf(stderr, "error deleting key %ld: %s\n",
		      key, skip_strerror(ret));
      free(data_p);
      break;
      
    case MODE_INFO:
      {
	int	height;
	
	call_c++;
	ret = skip_info(skip_p, &height);
	if (ret == SKIP_ERROR_NONE) {
	  if (verbose)
	    (void)printf("current entry has a height of %d\n", height);
	  iter_c++;
	}
	else if (ret == SKIP_ERROR_LINEAR && (! linear)) {
	  if (verbose)
	    (void)printf("no first command run yet\n");
	}
	else
	  (void)fprintf(stderr, "ERROR: skip info: %s\n",
			skip_strerror(ret));
      }
    break;
    
    case MODE_FIRST:
      call_c++;
      ret = skip_first(skip_p, (void **)&key_p, NULL, (void **)&data_p, NULL);
      if (ret == SKIP_ERROR_NONE) {
	linear = 1;
	if (verbose)
	  (void)printf("first entry is key %ld, data %ld\n", *key_p, *data_p);
	iter_c++;
      }
      else if (free_c == MAX_ENTRIES && ret == SKIP_ERROR_NOT_FOUND) {
	if (verbose)
	  (void)printf("no first in skip\n");
      }
      else
	(void)fprintf(stderr, "ERROR: first in skip: %s\n",
		      skip_strerror(ret));
      break;
      
    case MODE_NEXT:
      call_c++;
      ret = skip_next(skip_p, (void **)&key_p, NULL, (void **)&data_p, NULL);
      if (ret == SKIP_ERROR_NONE) {
	if (verbose)
	  (void)printf("next entry is key %ld, data %ld\n", *key_p, *data_p);
	iter_c++;
      }
      else if (ret == SKIP_ERROR_LINEAR && (! linear)) {
	if (verbose)
	  (void)printf("no first command run yet\n");
      }
      else if (ret == SKIP_ERROR_NOT_FOUND) {
	if (verbose)
	  (void)printf("reached EOF with next in skip: %s\n",
		       skip_strerror(ret));
	linear = 0;
      }
      else {
	(void)fprintf(stderr, "ERROR: skip_next reports: %s\n",
		      skip_strerror(ret));
	linear = 0;
      }
      break;
      
    case MODE_THIS:
      call_c++;
      ret = skip_this(skip_p, (void **)&key_p, NULL, (void **)&data_p, NULL);
      if (ret == SKIP_ERROR_NONE) {
	if (verbose)
	  (void)printf("this entry is key %ld, data %ld\n", *key_p, *data_p);
	iter_c++;
      }
      else if (ret == SKIP_ERROR_LINEAR && (! linear)) {
	if (verbose)
	  (void)printf("no first command run yet\n");
      }
      else {
	(void)fprintf(stderr, "ERROR: this skip: %s\n", skip_strerror(ret));
	linear = 0;
      }
      break;
      
    case MODE_NORMALIZE:
      if (1)
	continue;
      if (mmaping)
	continue;
      which = RANDOM_VALUE(20);
      if (which != 1)
	continue;
      if (free_c >= MAX_ENTRIES)
	continue;
      
      call_c++;
      ret = skip_normalize(skip_p);
      if (ret == SKIP_ERROR_NONE) {
	if (verbose)
	  (void)printf("list normalized\n");
	iter_c++;
      }
      else {
	(void)fprintf(stderr, "ERROR: skip-normalize: %s\n",
		      skip_strerror(ret));
      }
      break;
      
    default:
      (void)printf("unknown mode %d\n", which);
      break;
    }
  }
  
  free(grid);
}

/*
 * test the i/o systems
 */
static	void	io_test(skip_t *skip_p, const char *path)
{
  int		error;
  skip_t	*read_p;
  
  /* write it to disk */
  error = skip_write(skip_p, path, 0600);
  if (error != SKIP_ERROR_NONE) {
    (void)printf("ERROR writing list to '%s': %s\n",
		 path, skip_strerror(error));
    exit(1);
  }
  
  /* read back from disk */
  read_p = skip_read(path, compare, &error);
  if (read_p == NULL) {
    (void)printf("ERROR reading list from '%s': %s\n",
		 path, skip_strerror(error));
    exit(1);
  }
  
  /* now stress test the read list */
  stress(read_p, 10000, 0, 0);
  
  error = skip_free(read_p);
  if (error != SKIP_ERROR_NONE)
    (void)printf("ERROR in skip free: %s\n", skip_strerror(error));
}

/*
 * test the mmap systems
 */
static	void	mmap_test(skip_t *skip_p, const char *path)
{
  int		error;
  skip_t	*mmap_p;
  
  mmap_p = skip_mmap(path, compare, &error);
  if (mmap_p == NULL) {
    (void)printf("ERROR reading list from '%s': %s\n",
		 path, skip_strerror(error));
    exit(1);
  }
  
  stress(mmap_p, 100, 1, 1);
  
  error = skip_munmap(mmap_p);
  if (error != SKIP_ERROR_NONE) {
    (void)printf("ERROR un-mapping list from '%s': %s\n",
		 path, skip_strerror(error));
    exit(1);
  }
}

int	main(int argc, char ** argv)
{
  skip_t	*skip_p;
  int		error;
  long		seed;
  
  argc--, argv++;
  
  if (argc == 1)
    seed = atol(*argv);
  else
    seed = time(NULL);
  (void)srandom(seed);
  (void)printf("Seed for random is %ld\n", seed);
  
  skip_p = skip_alloc(compare, &error);
  if (skip_p == NULL) {
    (void)printf("ERROR allocating list: %s\n", skip_strerror(error));
    exit(1);
  }
  
#if 1
  stress(skip_p, ITERATIONS, 0, 0);
#else
  {
    long	key = 1, data = 2;
    error = skip_insert(skip_p, &key, sizeof(key), &data, sizeof(data), NULL,
			0);
    if (error != SKIP_ERROR_NONE)
      (void)printf("Could not insert into list: %s\n", skip_strerror(error));
  }
#endif
  
  dump_list(skip_p);
  skip_normalize(skip_p);
  (void)printf("\n\n");
  (void)printf("Normalized:\n");
  dump_list(skip_p);
  
  (void)printf("Testing I/O:\n");
  io_test(skip_p, TEST_PATH);
  (void)printf("Testing Mmap:\n");
  mmap_test(skip_p, TEST_PATH);
  
  error = skip_free(skip_p);
  if (error != SKIP_ERROR_NONE)
    (void)printf("ERROR in skip free: %s\n", skip_strerror(error));
  
  (void)printf("Test program performed %d table calls\n", call_c);
  
  exit(0);
}
