/*- mode:c++;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c++ ts=2 sts=2 sw=2 fenc=utf-8                              :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright © 2022, Gautham Venkatasubramanian                                 │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "c-family/portcosmo.internal.h"
#include "c-family/subcontext.h"

static tree maybe_get_ifsw_identifier(const char *);
static tree patch_int_nonconst(location_t, tree);

struct SubContext cosmo_ctx;
static int ctx_inited = 0;

void portcosmo_setup() {
    if (0 == ctx_inited) {
        construct_context(&cosmo_ctx);
        ctx_inited = 1;
    }
}

void portcosmo_teardown() {
    if (1 == ctx_inited) {
        cleanup_context(&cosmo_ctx);
        ctx_inited = 0;
    }
}

void portcosmo_show_tree(location_t loc, tree t) {
    INFORM(loc, "attempting case substitution at: line %u, col %u\n",
           LOCATION_LINE(loc), LOCATION_COLUMN(loc));
    debug_tree(t);
}

tree patch_case_nonconst(location_t loc, tree t) {
    INFORM(loc, "attempting case substitution at: line %u, col %u\n",
           LOCATION_LINE(loc), LOCATION_COLUMN(loc));
    debug_tree(t);
    tree subs = patch_int_nonconst(loc, t);
    if (subs != NULL_TREE) {
        DEBUGF("folding...\n");
        subs = c_fully_fold(subs, false, NULL, false);
        /* this substitution was successful, so record
         * the location for rewriting the thing later */
    }
    return subs;
}

tree unpatch_switch(location_t loc, tree t) {
    return t;
}

/* internal functions */

static tree patch_int_nonconst(location_t loc, tree t) {
    /* t may be an integer inside a case label, or
     * t may be an integer inside an initializer */
    tree subs = NULL_TREE;
    switch (TREE_CODE(t)) {
        case VAR_DECL:
            subs = maybe_get_ifsw_identifier(IDENTIFIER_NAME(t));
            if (subs != NULL_TREE && TREE_STATIC(subs) && TREE_READONLY(subs)) {
                DEBUGF("substitution exists\n");
                subs = DECL_INITIAL(subs);
                debug_tree(subs);
            }
            break;
        case NOP_EXPR:
            subs = patch_int_nonconst(loc, TREE_OPERAND(t, 0));
            if (subs != NULL_TREE) {
                subs = build1(NOP_EXPR, integer_type_node, subs);
            }
            break;
        case NEGATE_EXPR:
            subs = patch_int_nonconst(loc, TREE_OPERAND(t, 0));
            if (subs != NULL_TREE) {
                subs = build1(NEGATE_EXPR, integer_type_node, subs);
            }
            break;
        case BIT_NOT_EXPR:
            subs = patch_int_nonconst(loc, TREE_OPERAND(t, 0));
            if (subs != NULL_TREE) {
                subs = build1(BIT_NOT_EXPR, integer_type_node, subs);
            }
            break;
        default:
            subs = NULL_TREE;
    }
    return subs;
}

const char *get_tree_code_str(tree expr) {
#define END_OF_BASE_TREE_CODES
#define DEFTREECODE(a, b, c, d) \
    case a:                     \
        return b;
    switch (TREE_CODE(expr)) {
#include "all-tree.def"
        default:
            return "<unknown>";
    }
#undef DEFTREECODE
#undef END_OF_BASE_TREE_CODES
}

static tree maybe_get_ifsw_identifier(const char *s) {
    char *result = (char *)xmalloc(strlen("__tmpcosmo_") + strlen(s) + 1);
    strcpy(result, "__tmpcosmo_");
    strcat(result, s);
    tree t = maybe_get_identifier(result);
    free(result);
    if (t != NULL_TREE) {
        t = lookup_name(t);
    }
    return t;
}

tree get_ifsw_identifier(char *s) {
    char *result = (char *)xmalloc(strlen("__tmpcosmo_") + strlen(s) + 1);
    strcpy(result, "__tmpcosmo_");
    strcat(result, s);
    tree t = lookup_name(get_identifier(result));
    free(result);
    return t;
}

int get_value_of_const(char *name) {
    tree vx = get_ifsw_identifier(name);
    int z = tree_to_shwi(DECL_INITIAL(vx));
    return z;
}

int check_magic_equal(tree value, char *varname) {
    tree vx = get_ifsw_identifier(varname);
    return tree_int_cst_equal(value, DECL_INITIAL(vx));
}
