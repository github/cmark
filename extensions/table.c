#include <cmark_extension_api.h>
#include <html.h>
#include <inlines.h>
#include <parser.h>
#include <references.h>
#include <string.h>

#include "ext_scanners.h"
#include "strikethrough.h"
#include "table.h"

static cmark_node_type CMARK_NODE_TABLE, CMARK_NODE_TABLE_ROW,
    CMARK_NODE_TABLE_CELL;

typedef struct {
  uint16_t n_columns;
  cmark_llist *cells;
} table_row;

typedef struct {
  uint16_t n_columns;
  uint8_t *alignments;
} node_table;

typedef struct {
  bool is_header;
} node_table_row;

static void free_table_cell(cmark_mem *mem, void *data) {
  cmark_strbuf_free((cmark_strbuf *)data);
  mem->free(data);
}

static void free_table_row(cmark_mem *mem, table_row *row) {
  if (!row)
    return;

  cmark_llist_free_full(mem, row->cells, (cmark_free_func)free_table_cell);

  mem->free(row);
}

static void free_node_table(cmark_mem *mem, void *ptr) {
  node_table *t = (node_table *)ptr;
  mem->free(t->alignments);
  mem->free(t);
}

static void free_node_table_row(cmark_mem *mem, void *ptr) {
  mem->free(ptr);
}

static int get_n_table_columns(cmark_node *node) {
  if (!node || node->type != CMARK_NODE_TABLE)
    return -1;

  return (int)((node_table *)node->as.opaque)->n_columns;
}

static int set_n_table_columns(cmark_node *node, uint16_t n_columns) {
  if (!node || node->type != CMARK_NODE_TABLE)
    return 0;

  ((node_table *)node->as.opaque)->n_columns = n_columns;
  return 1;
}

static uint8_t *get_table_alignments(cmark_node *node) {
  if (!node || node->type != CMARK_NODE_TABLE)
    return 0;

  return ((node_table *)node->as.opaque)->alignments;
}

static int set_table_alignments(cmark_node *node, uint8_t *alignments) {
  if (!node || node->type != CMARK_NODE_TABLE)
    return 0;

  ((node_table *)node->as.opaque)->alignments = alignments;
  return 1;
}

static cmark_strbuf *unescape_pipes(cmark_mem *mem, unsigned char *string, bufsize_t len)
{
  cmark_strbuf *res = (cmark_strbuf *)mem->calloc(1, sizeof(cmark_strbuf));
  bufsize_t r, w;

  cmark_strbuf_init(mem, res, len + 1);
  cmark_strbuf_put(res, string, len);
  cmark_strbuf_putc(res, '\0');

  for (r = 0, w = 0; r < len; ++r) {
    if (res->ptr[r] == '\\' && res->ptr[r + 1] == '|')
      r++;

    res->ptr[w++] = res->ptr[r];
  }

  cmark_strbuf_truncate(res, w);

  return res;
}

static table_row *row_from_string(cmark_syntax_extension *self,
                                  cmark_parser *parser, unsigned char *string,
                                  int len) {
  table_row *row = NULL;
  bufsize_t cell_matched = 0, got_cell;
  bufsize_t cell_offset = 0;
  bool got_any_cell = false;

  row = (table_row *)parser->mem->calloc(1, sizeof(table_row));
  row->n_columns = 0;
  row->cells = NULL;

  if (len > 0 && string[0] == '|')
    ++cell_offset;

  do {
    cell_matched = got_cell = scan_table_cell(string, len, cell_offset);
    if (cell_matched) {
      got_any_cell = true;
      cmark_strbuf *cell_buf = unescape_pipes(parser->mem, string + cell_offset,
          cell_matched);
      cmark_strbuf_trim(cell_buf);
      row->n_columns += 1;
      row->cells = cmark_llist_append(parser->mem, row->cells, cell_buf);
      cell_offset += cell_matched;
    }

    cell_matched = scan_table_row_end(string, len, cell_offset);
    cell_offset += cell_matched;

    if (!got_cell && cell_matched) {
      row->n_columns += 1;
      cmark_strbuf *buf = (cmark_strbuf *)parser->mem->calloc(1, sizeof(cmark_strbuf));;
      cmark_strbuf_init(parser->mem, buf, 0);
      row->cells = cmark_llist_append(parser->mem, row->cells, buf);
    }
  } while (cell_matched && cell_offset < len);

  if (cell_offset != len || (!got_any_cell && row->n_columns < 2)) {
    free_table_row(parser->mem, row);
    row = NULL;
  }

  return row;
}

static cmark_node *try_opening_table_header(cmark_syntax_extension *self,
                                            cmark_parser *parser,
                                            cmark_node *parent_container,
                                            unsigned char *input, int len) {
  bufsize_t matched =
      scan_table_start(input, len, cmark_parser_get_first_nonspace(parser));
  cmark_node *table_header;
  table_row *header_row = NULL;
  table_row *marker_row = NULL;
  node_table_row *ntr;
  const char *parent_string;
  uint16_t i;

  if (!matched)
    return parent_container;

  parent_string = cmark_node_get_string_content(parent_container);

  cmark_arena_push();

  header_row = row_from_string(self, parser, (unsigned char *)parent_string,
                               (int)strlen(parent_string));

  if (!header_row) {
    free_table_row(parser->mem, header_row);
    cmark_arena_pop();
    return parent_container;
  }

  marker_row = row_from_string(self, parser,
                               input + cmark_parser_get_first_nonspace(parser),
                               len - cmark_parser_get_first_nonspace(parser));

  assert(marker_row);

  if (header_row->n_columns != marker_row->n_columns) {
    free_table_row(parser->mem, header_row);
    free_table_row(parser->mem, marker_row);
    cmark_arena_pop();
    return parent_container;
  }

  if (cmark_arena_pop()) {
    header_row = row_from_string(self, parser, (unsigned char *)parent_string,
                                 (int)strlen(parent_string));
    marker_row = row_from_string(self, parser,
                                 input + cmark_parser_get_first_nonspace(parser),
                                 len - cmark_parser_get_first_nonspace(parser));
  }

  if (!cmark_node_set_type(parent_container, CMARK_NODE_TABLE)) {
    free_table_row(parser->mem, header_row);
    free_table_row(parser->mem, marker_row);
    return parent_container;
  }

  cmark_node_set_syntax_extension(parent_container, self);

  parent_container->as.opaque = parser->mem->calloc(1, sizeof(node_table));


  set_n_table_columns(parent_container, header_row->n_columns);

  uint8_t *alignments =
      (uint8_t *)parser->mem->calloc(header_row->n_columns, sizeof(uint8_t));
  cmark_llist *it = marker_row->cells;
  for (i = 0; it; it = it->next, ++i) {
    cmark_strbuf *node = (cmark_strbuf *)it->data;
    bool left = node->ptr[0] == ':', right = node->ptr[node->size - 1] == ':';

    if (left && right)
      alignments[i] = 'c';
    else if (left)
      alignments[i] = 'l';
    else if (right)
      alignments[i] = 'r';
  }
  set_table_alignments(parent_container, alignments);

  table_header =
      cmark_parser_add_child(parser, parent_container, CMARK_NODE_TABLE_ROW,
                             cmark_parser_get_offset(parser));
  cmark_node_set_syntax_extension(table_header, self);

  table_header->as.opaque = ntr = (node_table_row *)parser->mem->calloc(1, sizeof(node_table_row));
  ntr->is_header = true;

  {
    cmark_llist *tmp;

    for (tmp = header_row->cells; tmp; tmp = tmp->next) {
      cmark_strbuf *cell_buf = (cmark_strbuf *) tmp->data;
      cmark_node *header_cell = cmark_parser_add_child(parser, table_header,
          CMARK_NODE_TABLE_CELL, cmark_parser_get_offset(parser));
      cmark_node_set_string_content(header_cell, (char *) cell_buf->ptr);
      cmark_node_set_syntax_extension(header_cell, self);
    }
  }

  cmark_parser_advance_offset(
      parser, (char *)input,
      (int)strlen((char *)input) - 1 - cmark_parser_get_offset(parser), false);

  free_table_row(parser->mem, header_row);
  free_table_row(parser->mem, marker_row);
  return parent_container;
}

static cmark_node *try_opening_table_row(cmark_syntax_extension *self,
                                         cmark_parser *parser,
                                         cmark_node *parent_container,
                                         unsigned char *input, int len) {
  cmark_node *table_row_block;
  table_row *row;

  if (cmark_parser_is_blank(parser))
    return NULL;

  table_row_block =
      cmark_parser_add_child(parser, parent_container, CMARK_NODE_TABLE_ROW,
                             cmark_parser_get_offset(parser));

  cmark_node_set_syntax_extension(table_row_block, self);
  table_row_block->as.opaque = parser->mem->calloc(1, sizeof(node_table_row));

  row = row_from_string(self, parser, input + cmark_parser_get_first_nonspace(parser),
      len - cmark_parser_get_first_nonspace(parser));

  {
    cmark_llist *tmp;
    int i, table_columns = get_n_table_columns(parent_container);

    for (tmp = row->cells, i = 0; tmp && i < table_columns; tmp = tmp->next, ++i) {
      cmark_strbuf *cell_buf = (cmark_strbuf *) tmp->data;
      cmark_node *cell = cmark_parser_add_child(parser, table_row_block,
          CMARK_NODE_TABLE_CELL, cmark_parser_get_offset(parser));
      cmark_node_set_string_content(cell, (char *) cell_buf->ptr);
      cmark_node_set_syntax_extension(cell, self);
    }

    for (; i < table_columns; ++i) {
      cmark_node *cell = cmark_parser_add_child(
          parser, table_row_block, CMARK_NODE_TABLE_CELL, cmark_parser_get_offset(parser));
      cmark_node_set_syntax_extension(cell, self);
    }
  }

  free_table_row(parser->mem, row);

  cmark_parser_advance_offset(parser, (char *)input,
                              len - 1 - cmark_parser_get_offset(parser), false);

  return table_row_block;
}

static cmark_node *try_opening_table_block(cmark_syntax_extension *self,
                                           int indented, cmark_parser *parser,
                                           cmark_node *parent_container,
                                           unsigned char *input, int len) {
  cmark_node_type parent_type = cmark_node_get_type(parent_container);

  if (!indented && parent_type == CMARK_NODE_PARAGRAPH) {
    return try_opening_table_header(self, parser, parent_container, input, len);
  } else if (!indented && parent_type == CMARK_NODE_TABLE) {
    return try_opening_table_row(self, parser, parent_container, input, len);
  }

  return NULL;
}

static int matches(cmark_syntax_extension *self, cmark_parser *parser,
                   unsigned char *input, int len,
                   cmark_node *parent_container) {
  int res = 0;

  if (cmark_node_get_type(parent_container) == CMARK_NODE_TABLE) {
    cmark_arena_push();
    table_row *new_row = row_from_string(
        self, parser, input + cmark_parser_get_first_nonspace(parser),
        len - cmark_parser_get_first_nonspace(parser));
    if (new_row && new_row->n_columns)
      res = 1;
    free_table_row(parser->mem, new_row);
    cmark_arena_pop();
  }

  return res;
}

static const char *get_type_string(cmark_syntax_extension *self,
                                   cmark_node *node) {
  if (node->type == CMARK_NODE_TABLE) {
    return "table";
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    if (((node_table_row *)node->as.opaque)->is_header)
      return "table_header";
    else
      return "table_row";
  } else if (node->type == CMARK_NODE_TABLE_CELL) {
    return "table_cell";
  }

  return "<unknown>";
}

static int can_contain(cmark_syntax_extension *extension, cmark_node *node,
                       cmark_node_type child_type) {
  if (node->type == CMARK_NODE_TABLE) {
    return child_type == CMARK_NODE_TABLE_ROW;
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    return child_type == CMARK_NODE_TABLE_CELL;
  } else if (node->type == CMARK_NODE_TABLE_CELL) {
    return child_type == CMARK_NODE_TEXT || child_type == CMARK_NODE_CODE ||
           child_type == CMARK_NODE_EMPH || child_type == CMARK_NODE_STRONG ||
           child_type == CMARK_NODE_LINK || child_type == CMARK_NODE_IMAGE ||
           child_type == CMARK_NODE_STRIKETHROUGH ||
           child_type == CMARK_NODE_HTML_INLINE;
  }
  return false;
}

static int contains_inlines(cmark_syntax_extension *extension,
                            cmark_node *node) {
  return node->type == CMARK_NODE_TABLE_CELL;
}

static void commonmark_render(cmark_syntax_extension *extension,
                              cmark_renderer *renderer, cmark_node *node,
                              cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);

  if (node->type == CMARK_NODE_TABLE) {
    renderer->blankline(renderer);
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    if (entering) {
      renderer->cr(renderer);
      renderer->out(renderer, node, "|", false, LITERAL);
    }
  } else if (node->type == CMARK_NODE_TABLE_CELL) {
    if (entering) {
      renderer->out(renderer, node, " ", false, LITERAL);
    } else {
      renderer->out(renderer, node, " |", false, LITERAL);
      if (((node_table_row *)node->parent->as.opaque)->is_header &&
          !node->next) {
        int i;
        uint8_t *alignments = get_table_alignments(node->parent->parent);
        uint16_t n_cols =
            ((node_table *)node->parent->parent->as.opaque)->n_columns;
        renderer->cr(renderer);
        renderer->out(renderer, node, "|", false, LITERAL);
        for (i = 0; i < n_cols; i++) {
          switch (alignments[i]) {
          case 0:   renderer->out(renderer, node, " --- |", false, LITERAL); break;
          case 'l': renderer->out(renderer, node, " :-- |", false, LITERAL); break;
          case 'c': renderer->out(renderer, node, " :-: |", false, LITERAL); break;
          case 'r': renderer->out(renderer, node, " --: |", false, LITERAL); break;
          }
        }
        renderer->cr(renderer);
      }
    }
  } else {
    assert(false);
  }
}

static void latex_render(cmark_syntax_extension *extension,
                         cmark_renderer *renderer, cmark_node *node,
                         cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);

  if (node->type == CMARK_NODE_TABLE) {
    if (entering) {
      int i;
      uint16_t n_cols;
      uint8_t *alignments = get_table_alignments(node);

      renderer->cr(renderer);
      renderer->out(renderer, node, "\\begin{table}", false, LITERAL);
      renderer->cr(renderer);
      renderer->out(renderer, node, "\\begin{tabular}{", false, LITERAL);

      n_cols = ((node_table *)node->as.opaque)->n_columns;
      for (i = 0; i < n_cols; i++) {
        switch(alignments[i]) {
        case 0:
        case 'l':
          renderer->out(renderer, node, "l", false, LITERAL);
          break;
        case 'c':
          renderer->out(renderer, node, "c", false, LITERAL);
          break;
        case 'r':
          renderer->out(renderer, node, "r", false, LITERAL);
          break;
        }
      }
      renderer->out(renderer, node, "}", false, LITERAL);
      renderer->cr(renderer);
    } else {
      renderer->out(renderer, node, "\\end{tabular}", false, LITERAL);
      renderer->cr(renderer);
      renderer->out(renderer, node, "\\end{table}", false, LITERAL);
      renderer->cr(renderer);
    }
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    if (!entering) {
      renderer->cr(renderer);
    }
  } else if (node->type == CMARK_NODE_TABLE_CELL) {
    if (!entering) {
      if (node->next) {
        renderer->out(renderer, node, " & ", false, LITERAL);
      } else {
        renderer->out(renderer, node, " \\\\", false, LITERAL);
      }
    }
  } else {
    assert(false);
  }
}

static void man_render(cmark_syntax_extension *extension,
                       cmark_renderer *renderer, cmark_node *node,
                       cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);

  if (node->type == CMARK_NODE_TABLE) {
    if (entering) {
      int i;
      uint16_t n_cols;
      uint8_t *alignments = get_table_alignments(node);

      renderer->cr(renderer);
      renderer->out(renderer, node, ".TS", false, LITERAL);
      renderer->cr(renderer);
      renderer->out(renderer, node, "tab(@);", false, LITERAL);
      renderer->cr(renderer);

      n_cols = ((node_table *)node->as.opaque)->n_columns;

      for (i = 0; i < n_cols; i++) {
        switch (alignments[i]) {
        case 'l':
          renderer->out(renderer, node, "l", false, LITERAL);
          break;
        case 0:
        case 'c':
          renderer->out(renderer, node, "c", false, LITERAL);
          break;
        case 'r':
          renderer->out(renderer, node, "r", false, LITERAL);
          break;
        }
      }

      if (n_cols) {
        renderer->out(renderer, node, ".", false, LITERAL);
        renderer->cr(renderer);
      }
    } else {
      renderer->out(renderer, node, ".TE", false, LITERAL);
      renderer->cr(renderer);
    }
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    if (!entering) {
      renderer->cr(renderer);
    }
  } else if (node->type == CMARK_NODE_TABLE_CELL) {
    if (!entering && node->next) {
      renderer->out(renderer, node, "@", false, LITERAL);
    }
  } else {
    assert(false);
  }
}

struct html_table_state {
  unsigned need_closing_table_body : 1;
  unsigned in_table_header : 1;
};

static void html_render(cmark_syntax_extension *extension,
                        cmark_html_renderer *renderer, cmark_node *node,
                        cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);
  cmark_strbuf *html = renderer->html;
  cmark_node *n;

  // XXX: we just monopolise renderer->opaque.
  struct html_table_state *table_state =
      (struct html_table_state *)&renderer->opaque;

  if (node->type == CMARK_NODE_TABLE) {
    if (entering) {
      cmark_html_render_cr(html);
      cmark_strbuf_puts(html, "<table");
      cmark_html_render_sourcepos(node, html, options);
      cmark_strbuf_putc(html, '>');
      table_state->need_closing_table_body = false;
    } else {
      if (table_state->need_closing_table_body)
        cmark_strbuf_puts(html, "</tbody>");
      table_state->need_closing_table_body = false;
      cmark_strbuf_puts(html, "</table>\n");
    }
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    if (entering) {
      cmark_html_render_cr(html);
      if (((node_table_row *)node->as.opaque)->is_header) {
        table_state->in_table_header = 1;
        cmark_strbuf_puts(html, "<thead>");
        cmark_html_render_cr(html);
      }
      cmark_strbuf_puts(html, "<tr");
      cmark_html_render_sourcepos(node, html, options);
      cmark_strbuf_putc(html, '>');
    } else {
      cmark_html_render_cr(html);
      cmark_strbuf_puts(html, "</tr>");
      if (((node_table_row *)node->as.opaque)->is_header) {
        cmark_html_render_cr(html);
        cmark_strbuf_puts(html, "</thead>");
        cmark_html_render_cr(html);
        cmark_strbuf_puts(html, "<tbody>");
        table_state->need_closing_table_body = 1;
        table_state->in_table_header = false;
      }
    }
  } else if (node->type == CMARK_NODE_TABLE_CELL) {
    uint8_t *alignments = get_table_alignments(node->parent->parent);
    if (entering) {
      cmark_html_render_cr(html);
      if (table_state->in_table_header) {
        cmark_strbuf_puts(html, "<th");
      } else {
        cmark_strbuf_puts(html, "<td");
      }

      int i = 0;
      for (n = node->parent->first_child; n; n = n->next, ++i)
        if (n == node)
          break;

      switch (alignments[i]) {
      case 'l': cmark_strbuf_puts(html, " align=\"left\""); break;
      case 'c': cmark_strbuf_puts(html, " align=\"center\""); break;
      case 'r': cmark_strbuf_puts(html, " align=\"right\""); break;
      }

      cmark_html_render_sourcepos(node, html, options);
      cmark_strbuf_putc(html, '>');
    } else {
      if (table_state->in_table_header) {
        cmark_strbuf_puts(html, "</th>");
      } else {
        cmark_strbuf_puts(html, "</td>");
      }
    }
  } else {
    assert(false);
  }
}

static void opaque_free(cmark_syntax_extension *self, cmark_mem *mem, cmark_node *node) {
  if (node->type == CMARK_NODE_TABLE) {
    free_node_table(mem, node->as.opaque);
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    free_node_table_row(mem, node->as.opaque);
  }
}

static int escape(cmark_syntax_extension *self, cmark_node *node, int c) {
  return
    node->type != CMARK_NODE_TABLE &&
    node->type != CMARK_NODE_TABLE_ROW &&
    node->type != CMARK_NODE_TABLE_CELL &&
    c == '|';
}

cmark_syntax_extension *create_table_extension(void) {
  cmark_syntax_extension *self = cmark_syntax_extension_new("table");

  cmark_syntax_extension_set_match_block_func(self, matches);
  cmark_syntax_extension_set_open_block_func(self, try_opening_table_block);
  cmark_syntax_extension_set_get_type_string_func(self, get_type_string);
  cmark_syntax_extension_set_can_contain_func(self, can_contain);
  cmark_syntax_extension_set_contains_inlines_func(self, contains_inlines);
  cmark_syntax_extension_set_commonmark_render_func(self, commonmark_render);
  cmark_syntax_extension_set_latex_render_func(self, latex_render);
  cmark_syntax_extension_set_man_render_func(self, man_render);
  cmark_syntax_extension_set_html_render_func(self, html_render);
  cmark_syntax_extension_set_opaque_free_func(self, opaque_free);
  cmark_syntax_extension_set_commonmark_escape_func(self, escape);
  CMARK_NODE_TABLE = cmark_syntax_extension_add_node(0);
  CMARK_NODE_TABLE_ROW = cmark_syntax_extension_add_node(0);
  CMARK_NODE_TABLE_CELL = cmark_syntax_extension_add_node(0);

  return self;
}
