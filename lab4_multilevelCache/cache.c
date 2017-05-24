#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "cache.h"


extern uns64 cycle_count; // You can use this as timestamp for LRU

////////////////////////////////////////////////////////////////////
// ------------- DO NOT MODIFY THE INIT FUNCTION -----------
////////////////////////////////////////////////////////////////////

Cache  *cache_new(uns64 size, uns64 assoc, uns64 linesize, uns64 repl_policy){

   Cache *c = (Cache *) calloc (1, sizeof (Cache));
   c->num_ways = assoc;
   c->repl_policy = repl_policy;

   if(c->num_ways > MAX_WAYS){
     printf("Change MAX_WAYS in cache.h to support %llu ways\n", c->num_ways);
     exit(-1);
   }

   // determine num sets, and init the cache
   c->num_sets = size/(linesize*assoc);
   c->sets  = (Cache_Set *) calloc (c->num_sets, sizeof(Cache_Set));

   return c;
}

////////////////////////////////////////////////////////////////////
// ------------- DO NOT MODIFY THE PRINT STATS FUNCTION -----------
////////////////////////////////////////////////////////////////////

void    cache_print_stats    (Cache *c, char *header){
  double read_mr =0;
  double write_mr =0;

  if(c->stat_read_access){
    read_mr=(double)(c->stat_read_miss)/(double)(c->stat_read_access);
  }

  if(c->stat_write_access){
    write_mr=(double)(c->stat_write_miss)/(double)(c->stat_write_access);
  }

  printf("\n%s_READ_ACCESS    \t\t : %10llu", header, c->stat_read_access);
  printf("\n%s_WRITE_ACCESS   \t\t : %10llu", header, c->stat_write_access);
  printf("\n%s_READ_MISS      \t\t : %10llu", header, c->stat_read_miss);
  printf("\n%s_WRITE_MISS     \t\t : %10llu", header, c->stat_write_miss);
  printf("\n%s_READ_MISSPERC  \t\t : %10.3f", header, 100*read_mr);
  printf("\n%s_WRITE_MISSPERC \t\t : %10.3f", header, 100*write_mr);
  printf("\n%s_DIRTY_EVICTS   \t\t : %10llu", header, c->stat_dirty_evicts);

  printf("\n");
}



////////////////////////////////////////////////////////////////////
// Note: the system provides the cache with the line address
// Return HIT if access hits in the cache, MISS otherwise
// Also if mark_dirty is TRUE, then mark the resident line as dirty
// Update appropriate stats
////////////////////////////////////////////////////////////////////

#include <assert.h>
#include <tgmath.h>
int get_bits(int value, int start, int end) {
  int result;
  assert(start >= end);
  result = value >> end;
  result = result % (1 << (start - end + 1));
  return result;
}

Flag cache_access(Cache *c, Addr lineaddr, uns mark_dirty){
  if (mark_dirty) {
    c->stat_write_access = c->stat_write_access + 1;
  } else {
    c->stat_read_access = c->stat_read_access + 1;
  }
  Flag outcome = MISS;
  int bit6 = get_bits(lineaddr, (log2(c->num_sets) - 1) , 0);
  for(uns64 i = 0; i < c->num_ways; i++) {
    if (lineaddr == (c->sets[bit6]).line[i].tag) {
      outcome = HIT;
      c->sets[bit6].line[i].last_access_time = cycle_count;
      if (mark_dirty){
        c->sets[bit6].line[i].dirty = TRUE;
      }
    break;
    }
  }
  if (outcome == MISS){
    if (mark_dirty) {
      c->stat_write_miss = c->stat_write_miss + 1;
    } else {
      c->stat_read_miss = c->stat_read_miss + 1;
    }
  }
  return outcome;
}


////////////////////////////////////////////////////////////////////
// Note: the system provides the cache with the line address
// Install the line: determine victim using repl policy (LRU/RAND)
// copy victim into last_evicted_line for tracking writebacks
////////////////////////////////////////////////////////////////////

void cache_install(Cache *c, Addr lineaddr, uns mark_dirty){

  int bit6 = get_bits(lineaddr, (log2(c->num_sets) - 1), 0);
  int checkSpace = FALSE;
  for (uns64 i = 0; i < c->num_ways; i++) {
    if (c->sets[bit6].line[i].tag == 0) {
      c->sets[bit6].line[i].dirty = mark_dirty;
      c->sets[bit6].line[i].last_access_time = cycle_count;
      c->sets[bit6].line[i].tag = lineaddr;
      c->sets[bit6].line[i].valid = TRUE;
      checkSpace = TRUE;
      break;
    }
  }
  if (checkSpace == FALSE) {
    if (c->repl_policy) {
      int randNum = rand() % c->num_ways;
      c->last_evicted_line = c->sets[bit6].line[randNum];
      c->sets[bit6].line[randNum].dirty = mark_dirty;
      c->sets[bit6].line[randNum].last_access_time = cycle_count;
      c->sets[bit6].line[randNum].tag = lineaddr;
      c->sets[bit6].line[randNum].valid = TRUE;
      if (c->sets[bit6].line[randNum].dirty) {
        c->stat_dirty_evicts++;
      }
    } else {
      uns64 min = 0;
      int cacheBlock = 0;
      for (uns64 i = 0; i < c->num_ways; i++) {
        if (i == 0) {
          min = c->sets[bit6].line[i].last_access_time;
          cacheBlock = i;
        }
        if (min > c->sets[bit6].line[i].last_access_time) {
          min = c->sets[bit6].line[i].last_access_time;
          cacheBlock = i;
        }
      }
      if (c->sets[bit6].line[cacheBlock].dirty){
        c->stat_dirty_evicts++;
      }
      c->last_evicted_line = c->sets[bit6].line[cacheBlock];
      c->sets[bit6].line[cacheBlock].dirty = mark_dirty;
      c->sets[bit6].line[cacheBlock].last_access_time = cycle_count;
      c->sets[bit6].line[cacheBlock].tag = lineaddr;
      c->sets[bit6].line[cacheBlock].valid = TRUE;
    }
  }
}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////


