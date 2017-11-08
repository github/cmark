#include "cmark.h"
#include "utf8.h"
#include "parser.h"
#include "footnotes.h"
#include "inlines.h"
#include "chunk.h"

static void footnote_free(cmark_footnote_map *map, cmark_footnote *ref) {
  cmark_mem *mem = map->mem;
  if (ref != NULL) {
    mem->free(ref->label);
    if (ref->node)
      cmark_node_free(ref->node);
    mem->free(ref);
  }
}

// normalize footnote:  collapse internal whitespace to single space,
// remove leading/trailing whitespace, case fold
// Return NULL if the footnote name is actually empty (i.e. composed
// solely from whitespace)
static unsigned char *normalize_footnote(cmark_mem *mem, cmark_chunk *ref) {
  cmark_strbuf normalized = CMARK_BUF_INIT(mem);
  unsigned char *result;

  if (ref == NULL)
    return NULL;

  if (ref->len == 0)
    return NULL;

  cmark_utf8proc_case_fold(&normalized, ref->data, ref->len);
  cmark_strbuf_trim(&normalized);
  cmark_strbuf_normalize_whitespace(&normalized);

  result = cmark_strbuf_detach(&normalized);
  assert(result);

  if (result[0] == '\0') {
    mem->free(result);
    return NULL;
  }

  return result;
}

void cmark_footnote_create(cmark_footnote_map *map, cmark_node *node) {
  cmark_footnote *ref;
  unsigned char *reflabel = normalize_footnote(map->mem, &node->as.literal);

  /* empty footnote name, or composed from only whitespace */
  if (reflabel == NULL)
    return;

  assert(map->sorted == NULL);

  ref = (cmark_footnote *)map->mem->calloc(1, sizeof(*ref));
  ref->label = reflabel;
  ref->node = node;
  ref->age = map->size;
  ref->next = map->refs;

  map->refs = ref;
  map->size++;
}

static int
labelcmp(const unsigned char *a, const unsigned char *b) {
  return strcmp((const char *)a, (const char *)b);
}

static int
refcmp(const void *p1, const void *p2) {
  cmark_footnote *r1 = *(cmark_footnote **)p1;
  cmark_footnote *r2 = *(cmark_footnote **)p2;
  int res = labelcmp(r1->label, r2->label);
  return res ? res : ((int)r1->age - (int)r2->age);
}

static int
refsearch(const void *label, const void *p2) {
  cmark_footnote *ref = *(cmark_footnote **)p2;
  return labelcmp((const unsigned char *)label, ref->label);
}

static void sort_footnotes(cmark_footnote_map *map) {
  unsigned int i = 0, last = 0, size = map->size;
  cmark_footnote *r = map->refs, **sorted = NULL;

  sorted = (cmark_footnote **)map->mem->calloc(size, sizeof(cmark_footnote *));
  while (r) {
    sorted[i++] = r;
    r = r->next;
  }

  qsort(sorted, size, sizeof(cmark_footnote *), refcmp);

  for (i = 1; i < size; i++) {
    if (labelcmp(sorted[i]->label, sorted[last]->label) != 0)
      sorted[++last] = sorted[i];
  }

  map->sorted = sorted;
  map->size = last + 1;
}

// Returns footnote if refmap contains a footnote with matching
// label, otherwise NULL.
cmark_footnote *cmark_footnote_lookup(cmark_footnote_map *map,
                                      cmark_chunk *label) {
  cmark_footnote **ref = NULL;
  unsigned char *norm;

  if (label->len < 1 || label->len > MAX_LINK_LABEL_LENGTH)
    return NULL;

  if (map == NULL || !map->size)
    return NULL;

  norm = normalize_footnote(map->mem, label);
  if (norm == NULL)
    return NULL;

  if (!map->sorted)
    sort_footnotes(map);

  ref = (cmark_footnote **)bsearch(norm, map->sorted, map->size, sizeof(cmark_footnote *), refsearch);
  map->mem->free(norm);

  if (!ref)
    return NULL;

  if (!ref[0]->ix)
    ref[0]->ix = ++map->ix;
  return ref[0];
}

void cmark_footnote_map_free(cmark_footnote_map *map) {
  cmark_footnote *ref;

  if (map == NULL)
    return;

  ref = map->refs;
  while (ref) {
    cmark_footnote *next = ref->next;
    footnote_free(map, ref);
    ref = next;
  }

  map->mem->free(map->sorted);
  map->mem->free(map);
}

cmark_footnote_map *cmark_footnote_map_new(cmark_mem *mem) {
  cmark_footnote_map *map =
      (cmark_footnote_map *)mem->calloc(1, sizeof(cmark_footnote_map));
  map->mem = mem;
  return map;
}
