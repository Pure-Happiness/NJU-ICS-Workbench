#include "common.h"
#include <inttypes.h>
#include <string.h>

void mem_read(uintptr_t block_num, uint8_t *buf);
void mem_write(uintptr_t block_num, const uint8_t *buf);

static uint64_t cycle_cnt = 0;

void cycle_increase(int n) { cycle_cnt += n; }

struct cache_line {
  uint32_t data[BLOCK_SIZE / sizeof(uint32_t)];
  uintptr_t tag;
  bool valid;
  bool dirty;
} **cache;
static int tag_start;
static size_t group_lines;

uint32_t cache_read(uintptr_t addr) {
  uintptr_t tag = addr >> tag_start;
  size_t group_rank = (addr & mask_with_len(tag_start)) >> BLOCK_WIDTH;
  struct cache_line *empty = NULL, *group = cache[group_rank];
  for (struct cache_line *line = group + group_lines; line-- > group;)
    if (!line->valid)
      empty = line;
    else if (line->tag == tag)
      return line->data[(addr & (BLOCK_SIZE - 1)) / sizeof(uint32_t)];
  if (!empty) {
    empty = group + (rand() & (group_lines - 1));
    if (empty->dirty)
      mem_write(empty->tag << (tag_start - BLOCK_WIDTH) | group_rank,
                (uint8_t *)empty->data);
  }
  mem_read(addr >> BLOCK_WIDTH, (uint8_t *)empty->data);
  empty->tag = tag;
  empty->valid = true;
  empty->dirty = false;
  return empty->data[(addr & (BLOCK_SIZE - 1)) / sizeof(uint32_t)];
}

void cache_write(uintptr_t addr, uint32_t data, uint32_t wmask) {
  uintptr_t tag = addr >> tag_start;
  size_t group_rank = (addr & mask_with_len(tag_start)) >> BLOCK_WIDTH;
  struct cache_line *empty = NULL, *group = cache[group_rank];
  for (struct cache_line *line = group + group_lines; line-- > group;)
    if (!line->valid)
      empty = line;
    else if (line->tag == tag) {
      uint32_t *p = &line->data[(addr & (BLOCK_SIZE - 1)) / sizeof(uint32_t)];
      *p = (*p & ~wmask) | (data & wmask);
      line->dirty = true;
      return;
    }
  if (!empty) {
    empty = group + (rand() & (group_lines - 1));
    if (empty->dirty)
      mem_write(empty->tag << (tag_start - BLOCK_WIDTH) | group_rank,
                (uint8_t *)empty->data);
  }
  mem_read(addr >> BLOCK_WIDTH, (uint8_t *)empty->data);
  empty->tag = tag;
  empty->valid = true;
  uint32_t *p = &empty->data[(addr & (BLOCK_SIZE - 1)) / sizeof(uint32_t)];
  *p = (*p & ~wmask) | (data & wmask);
  empty->dirty = true;
}

void init_cache(int total_size_width, int associativity_width) {
  tag_start = total_size_width - associativity_width;
  group_lines = (size_t)1 << associativity_width;
  const size_t group_number = (size_t)1 << (tag_start - BLOCK_WIDTH);
  cache = malloc(group_number * sizeof(struct cache_line *));
  for (struct cache_line **group = cache + group_number; group-- > cache;) {
    *group = malloc(group_lines * sizeof(struct cache_line));
    for (struct cache_line *line = *group + group_lines; line-- > *group;)
      line->valid = false;
  }
}

void display_statistic(void) {}
