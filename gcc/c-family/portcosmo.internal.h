#ifndef PORTCOSMO.INTERNAL_H
#define PORTCOSMO.INTERNAL_H
#include "c-family/portcosmo.h"

const char *get_tree_code_str(tree);
int get_value_of_const(char *);
tree get_ifsw_identifier(char *);
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

#ifndef COSMO_NDEBUG
#define DEBUGF(...) fprintf(stderr, "<DEBUG> " __VA_ARGS__)
#define INFORM(...) inform(__VA_ARGS__)
#else
#define DEBUGF(...)
#define INFORM(...)
#endif

#define STRING_BUFFER_SIZE 192

#endif /* PORTCOSMO.INTERNAL_H */
