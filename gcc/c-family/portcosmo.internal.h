#ifndef PORTCOSMO_INTERNAL_H
#define PORTCOSMO_INTERNAL_H
/* first stdlib headers */
#include <stdio.h>
/* now all the plugin headers */
#include <gcc-plugin.h>
/* first gcc-plugin, then the others */
#include <c-family/c-common.h>
#include <c-family/c-pragma.h>
#include <c/c-tree.h>
#include <cgraph.h>
#include <cpplib.h>
#include <diagnostic.h>
#include <print-tree.h>
#include <stringpool.h>
#include <tree-iterator.h>
#include <tree.h>

const char *get_tree_code_str(tree);
tree maybe_get_tmpconst_value(const char *);
int check_magic_equal(tree, char *);

#define EXPR_LOC_LINE(x)      LOCATION_LINE(EXPR_LOCATION((x)))
#define EXPR_LOC_COL(x)       LOCATION_COLUMN(EXPR_LOCATION((x)))
#define LOCATION_APPROX(x, y) (LOCATION_LINE((x)) == LOCATION_LINE((y)))
#define LOCATION_BEFORE(x, y) (LOCATION_LINE((x)) <= LOCATION_LINE((y)))
#define LOCATION_AFTER(x, y)  (LOCATION_LINE((x)) >= LOCATION_LINE((y)))

#define LOCATION_BEFORE2(x, y)                  \
  (LOCATION_LINE((x)) < LOCATION_LINE((y)) ||   \
   (LOCATION_LINE((x)) == LOCATION_LINE((y)) && \
    LOCATION_COLUMN((x)) <= LOCATION_COLUMN((y))))
#define LOCATION_AFTER2(x, y)                   \
  (LOCATION_LINE((x)) > LOCATION_LINE((y)) ||   \
   (LOCATION_LINE((x)) == LOCATION_LINE((y)) && \
    LOCATION_COLUMN((x)) >= LOCATION_COLUMN((y))))

#define VAR_NAME_AS_TREE(fname)   lookup_name(get_identifier((fname)))
#define IDENTIFIER_NAME(z)        IDENTIFIER_POINTER(DECL_NAME((z)))
#define BUILD_STRING_AS_TREE(str) build_string_literal(strlen((str)) + 1, (str))

#if 0
#define DEBUGF(...) fprintf(stderr, "<DEBUG> " __VA_ARGS__)
#define INFORM(...) inform(__VA_ARGS__)
#else
#define DEBUGF(...)
#define INFORM(...)
#endif

#define STRING_BUFFER_SIZE 192

#define MAXIMUM_PATCHES 65536

void handle_pre_genericize(void *, void *);

#endif /* PORTCOSMO.INTERNAL_H */
