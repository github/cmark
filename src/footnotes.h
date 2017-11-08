#ifndef CMARK_FOOTNOTES_H
#define CMARK_FOOTNOTES_H

#include "memory.h"
#include "chunk.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cmark_footnote {
  struct cmark_footnote *next;
  unsigned char *label;
  cmark_node *node;
  unsigned int age;
};

typedef struct cmark_footnote cmark_footnote;

struct cmark_footnote_map {
  cmark_mem *mem;
  cmark_footnote *refs;
  cmark_footnote **sorted;
  unsigned int size;
};

typedef struct cmark_footnote_map cmark_footnote_map;

cmark_footnote_map *cmark_footnote_map_new(cmark_mem *mem);
void cmark_footnote_map_free(cmark_footnote_map *map);
cmark_footnote *cmark_footnote_lookup(cmark_footnote_map *map,
                                      cmark_chunk *label);
extern void cmark_footnote_create(cmark_footnote_map *map, cmark_node *node);

#ifdef __cplusplus
}
#endif

#endif
