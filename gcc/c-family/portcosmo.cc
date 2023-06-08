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
#include "c-family/ifswitch.h"
#include "c-family/initstruct.h"

static tree patch_int_nonconst(location_t, tree, const char **);

struct SubContext cosmo_ctx;
static int ctx_inited = 0;
static void (*other_define) (cpp_reader *, location_t, cpp_hashnode *) = NULL;

void check_macro_define(cpp_reader *reader, location_t loc,
                        cpp_hashnode *node) {
  const char *defn = (const char *)cpp_macro_definition(reader, node);
  if (cosmo_ctx.active && strstr(defn, "__tmpcosmo_") == defn)
  {
    const char *arg_start = defn + strlen("__tmpcosmo_");
    const char *arg_end = strstr(arg_start, " ");
    if (!arg_start || !arg_end || arg_end - arg_start < 1) return;
    char* name = xstrndup(arg_start, arg_end - arg_start);
    char* val = xstrdup(arg_end);
    long long val2 = strtoll(arg_end, NULL, 0);
    if (val2 == 0) {
        cpp_error_at(parse_in, CPP_DL_ERROR, loc,
                "cannot parse portcosmo temporary constant\n");
        cosmo_ctx.active = 0;
    } else {
        if (cosmo_ctx.map->get(name)) {
            cpp_error_at(parse_in, CPP_DL_ERROR, loc,
                    "duplicate portcosmo temporary constant\n");
            cosmo_ctx.active = 0;
        } else {
            tmpconst z;
            z.raw = val2; 
            z.t = build_int_cst(long_long_integer_type_node, val2);
            cosmo_ctx.map->put(name, z);
            // INFORM(loc, "added temporary for %s\n", name);
        }
    }
    /* not freeing name because it's in the hashmap */
    free(val);
  }
  else if(other_define) {
     DEBUGF("we should just let this be %s\n", defn);
     other_define(reader, loc, node);
  }
}

void portcosmo_setup() {
    cpp_callbacks *cbs = cpp_get_callbacks(parse_in);
    if (flag_portcosmo && 0 == ctx_inited) {
        construct_context(&cosmo_ctx);
        ctx_inited = 1;
        if (cbs) {
            if (cbs->define) {
                other_define = cbs->define;
                /* TODO: do we need for cbs->undef as well? */
            }
            cbs->define = check_macro_define;
        }
    }
}

void portcosmo_teardown() {
    if (flag_portcosmo && 1 == ctx_inited) {
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
    tree subs = NULL_TREE;
    const char *name = NULL;
    if (cosmo_ctx.active) {
        subs = patch_int_nonconst(loc, t, &name);
        if (subs != NULL_TREE) {
            DEBUGF("folding...\n");
            subs = c_fully_fold(subs, false, NULL, false);
            /* this substitution was successful, so record
             * the location for rewriting the thing later */
            add_context_subu(&cosmo_ctx, loc, name, strlen(name),
                             PORTCOSMO_SWCASE);
        }
    }
    if (subs == NULL_TREE) {
        inform(loc, "unable to find __tmpcosmo_ temporary");
    }
    return subs;
}

tree patch_init_nonconst(location_t loc, tree t) {
    INFORM(loc, "attempting init substitution at: line %u, col %u\n",
           LOCATION_LINE(loc), LOCATION_COLUMN(loc));
    tree subs = NULL_TREE;
    const char *name = NULL;
    if (cosmo_ctx.active) {
        subs = patch_int_nonconst(loc, t, &name);
        if (subs != NULL_TREE) {
            DEBUGF("folding...\n");
            subs = c_fully_fold(subs, false, NULL, false);
            /* this substitution was successful, so record
             * the location for rewriting the thing later */
            add_context_subu(&cosmo_ctx, loc, name, strlen(name),
                             PORTCOSMO_INITVAL);
            DEBUGF("done\n");
        }
    }
    if (subs == NULL_TREE) {
        inform(loc, "unable to find __tmpcosmo_ temporary");
    }
    return subs;
}

/* internal functions */

static tree patch_int_nonconst(location_t loc, tree t, const char **res) {
    /* t may be an integer inside a case label, or
     * t may be an integer inside an initializer */
    tree subs = NULL_TREE;
    switch (TREE_CODE(t)) {
        case VAR_DECL:
            subs = maybe_get_tmpconst_value(IDENTIFIER_NAME(t));
            if (subs != NULL_TREE) {
                *res = IDENTIFIER_NAME(t);
                DEBUGF("substitution exists %s\n", *res);
            }
            break;
        case CONVERT_EXPR:
            subs = patch_int_nonconst(loc, TREE_OPERAND(t, 0), res);
            if (subs != NULL_TREE) {
                subs = build1(CONVERT_EXPR, integer_type_node, subs);
            }
            break;
        case NOP_EXPR:
            subs = patch_int_nonconst(loc, TREE_OPERAND(t, 0), res);
            if (subs != NULL_TREE) {
                subs = build1(NOP_EXPR, integer_type_node, subs);
            }
            break;
        case NEGATE_EXPR:
            subs = patch_int_nonconst(loc, TREE_OPERAND(t, 0), res);
            if (subs != NULL_TREE) {
                subs = build1(NEGATE_EXPR, integer_type_node, subs);
            }
            break;
        case BIT_NOT_EXPR:
            subs = patch_int_nonconst(loc, TREE_OPERAND(t, 0), res);
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

tree maybe_get_tmpconst_value(const char *s) {
    char *result = xstrdup(s);
    tmpconst *z = cosmo_ctx.map->get(result);
    free(result);
    return z ? z->t : NULL_TREE;
}

int check_magic_equal(tree value, char *varname) {
    tree vx = maybe_get_tmpconst_value(varname);
    return vx != NULL_TREE && tree_int_cst_equal(value, vx);
}
