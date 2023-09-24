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
#include "c-family/ifswitch.h"

static tree get_switch_body(tree swexpr) {
  auto body = SWITCH_STMT_BODY(swexpr);
  if (TREE_CODE(body) == BIND_EXPR) {
    body = BIND_EXPR_BODY(body);
  }
  return body;
}

source_range get_switch_bounds(tree sws) {
  auto body = get_switch_body(sws);
  source_range rng;
  rng.m_start = MAX_LOCATION_T;
  rng.m_finish = MAX_LOCATION_T;
  if (STATEMENT_LIST_HEAD(body) && STATEMENT_LIST_TAIL(body)) {
    /* otherwise this is an empty switch statement */
    auto rng1 = EXPR_LOCATION_RANGE(STATEMENT_LIST_HEAD(body)->stmt);
    auto rng2 = EXPR_LOCATION_RANGE(STATEMENT_LIST_TAIL(body)->stmt);
    rng.m_start = rng1.m_start;
    rng.m_finish = rng2.m_finish;
  }
  return rng;
}

unsigned int count_mods_in_switch(tree swexpr, subu_list *list) {
  tree body = get_switch_body(swexpr);
  tree t = NULL_TREE;
  tree replacement = NULL_TREE;
  subu_node *use = NULL;
  unsigned int count = 0;
  for (auto i = tsi_start(body); !tsi_end_p(i); tsi_next(&i)) {
    t = tsi_stmt(i);
    if (TREE_CODE(t) == CASE_LABEL_EXPR) {
      if (get_subu_elem(list, EXPR_LOCATION(t),
                        &use)          /* on a line we substituted */
          && CASE_LOW(t) != NULL_TREE  /* not a x..y range */
          && CASE_HIGH(t) == NULL_TREE /* not a default */
          && arg_should_be_unpatched(CASE_LOW(t), use, &replacement)
          /* the case is the one we substituted */) {
        DEBUGF("we substituted a case label at %u,%u\n", EXPR_LOC_LINE(t),
               EXPR_LOC_COL(t));
        count += 1;
      }
    }
  }
  return count;
}

tree build_modded_label(unsigned int swcount, const char *case_str,
                        location_t loc = UNKNOWN_LOCATION) {
  char dest[STRING_BUFFER_SIZE] = {0};
  snprintf(dest, sizeof(dest), "__tmpcosmo_%u_%s", swcount, case_str);
  tree lab = build_decl(loc, LABEL_DECL, get_identifier(dest), void_type_node);
  /* gcc's GIMPLE needs to know that this label
   * is within the current function declaration */
  DECL_CONTEXT(lab) = current_function_decl;
  return build1(LABEL_EXPR, void_type_node, lab);
}

tree build_modded_exit_label(unsigned int swcount) {
  return build_modded_label(swcount, "__end");
}

static inline tree build_modded_if_stmt(tree condition, tree then_clause,
                                        tree else_clause = NULL_TREE) {
  return build3(COND_EXPR, void_type_node, condition, then_clause, else_clause);
}

tree modded_case_label(tree t, unsigned int i, tree swcond, vec<tree> *&ifs,
                       SubContext *ctx, tree *default_label) {
  tree result;
  tree replacement = NULL_TREE;
  subu_node *use = NULL;
  char case_str[STRING_BUFFER_SIZE] = {0};

  if (CASE_LOW(t) == NULL_TREE) {
    DEBUGF("default case\n");
    /* default label of the switch case, needs to be last */
    result = build_modded_label(ctx->switchcount, "__dflt", EXPR_LOCATION(t));
    *default_label = result;
  } else if (CASE_LOW(t) != NULL_TREE && CASE_HIGH(t) == NULL_TREE) {
    /* a case label */
    if (get_subu_elem(ctx->mods, EXPR_LOCATION(t), &use)
        /* the case is on a line we substituted */
        && arg_should_be_unpatched(CASE_LOW(t), use, &replacement)
        /* the case value is the one we substituted */) {
      DEBUGF("modded case\n");
      result =
          build_modded_label(ctx->switchcount, use->name, EXPR_LOCATION(t));
      ifs->safe_push(build_modded_if_stmt(
          build2(EQ_EXPR, integer_type_node, swcond, replacement),
          build1(GOTO_EXPR, void_type_node, LABEL_EXPR_LABEL(result))));
      remove_subu_elem(ctx->mods, use);
      replacement = NULL_TREE;
    } else {
      /* a case label that we didn't substitute */
      DEBUGF("unmodded case\n");
      snprintf(case_str, sizeof(case_str), "%x_", i);
      result = build_modded_label(ctx->switchcount, case_str, EXPR_LOCATION(t));
      ifs->safe_push(build_modded_if_stmt(
          build2(EQ_EXPR, integer_type_node, swcond, CASE_LOW(t)),
          build1(GOTO_EXPR, void_type_node, LABEL_EXPR_LABEL(result))));
    }
  } else {
    DEBUGF("unmodded case range\n");
    /* CASE_LOW(t) != NULL_TREE && CASE_HIGH(t) != NULL_TREE */
    /* this is a case x .. y sort of range */
    snprintf(case_str, sizeof(case_str), "%x_", i);
    result = build_modded_label(ctx->switchcount, case_str, EXPR_LOCATION(t));
    ifs->safe_push(build_modded_if_stmt(
        build2(TRUTH_ANDIF_EXPR, integer_type_node,
               build2(GE_EXPR, integer_type_node, swcond, CASE_LOW(t)),
               build2(LE_EXPR, integer_type_node, swcond, CASE_HIGH(t))),
        build1(GOTO_EXPR, void_type_node, LABEL_EXPR_LABEL(result))));
  }
  return result;
}

struct swmod_ctx {
    int case_count, break_count, has_default;
    tree swcond, swbody;
    tree exit_label;
    tree default_label;
    vec<tree> *ifs;
    SubContext *ctx;
};

tree internal_case_labels(tree *tp, int *check_subtree, void *data) {
  struct swmod_ctx *swt = (struct swmod_ctx *)data;
  
  if (TREE_CODE(*tp) == CASE_LABEL_EXPR) {
      swt->case_count += 1;
      swt->has_default = swt->has_default || (CASE_LOW(*tp) == NULL_TREE);
      /* replace the case statement with a goto */
      *tp = modded_case_label(*tp, swt->case_count, swt->swcond,
                  swt->ifs, swt->ctx, &(swt->default_label));
      *check_subtree = 0;
  } else if (TREE_CODE(*tp) == SWITCH_STMT) {
      /* we don't to mod the cases of another switch,
       * that will happen in a another call */
      *check_subtree = 0;
  }
  return NULL_TREE;
}

tree internal_break_stmts(tree *tp, int *check_subtree, void *data) {
  struct swmod_ctx *swt = (struct swmod_ctx *)data;
 
  switch(TREE_CODE(*tp)) {
      case BREAK_STMT:
        swt->break_count += 1;
        /* replace the break statement with a goto to the end */
        *tp = build1(GOTO_EXPR, void_type_node, LABEL_EXPR_LABEL(swt->exit_label));
        break;
      case BIND_EXPR:
      case STATEMENT_LIST:
      case COND_EXPR:
        /* we want to check the subtrees, because 
         * they may contain break statements */
        break;
      default:
        *check_subtree = 0;
  }

  return NULL_TREE;
}

tree build_modded_switch_stmt(tree swexpr, SubContext *ctx) {
  struct swmod_ctx mod;
  tree *tp = NULL;
  char dest[STRING_BUFFER_SIZE] = {0};

  mod.case_count = 0;
  mod.break_count = 0;
  mod.has_default = 0;
  mod.swcond = save_expr(SWITCH_STMT_COND(swexpr));
  mod.swbody = get_switch_body(swexpr);
  vec_alloc(mod.ifs, 0);
  mod.exit_label = build_modded_exit_label(ctx->switchcount);
  mod.default_label = NULL_TREE;
  mod.ctx = ctx;

  /* to perform the replacements, we now have two functions,
   * one traversing the tree fixing the case labels. this is
   * slow because a case label can be anywhere in a switch,
   * and so we cannot just do a surface level check.
   *
   * the other function traverses the tree fixing the break
   * statements. this function ought to be faster because we
   * only need to worry about top-level break statements.
   */
  walk_tree_without_duplicates(&(mod.swbody), internal_case_labels, &mod);
  walk_tree_without_duplicates(&(mod.swbody), internal_break_stmts, &mod);

  /* add all the if statements to the start of the switch body */
  /* TODO: do we have to combine them via COND_EXPR_ELSE? why,
   * is it not possible to have them as a list one after the other? */
  tree res;
  unsigned int zz = 0;
  if (mod.ifs->length() > 0) {
    res = (*(mod.ifs))[0];
    for (zz = 1; zz < mod.ifs->length(); ++zz) {
      COND_EXPR_ELSE(res) = (*(mod.ifs))[zz];
      res = (*(mod.ifs))[zz];
    }
    /* if we have a valid default for the switch,
     * it should be the final else branch */
    if (mod.default_label && mod.default_label != NULL_TREE) {
      COND_EXPR_ELSE(res) =
          build1(GOTO_EXPR, void_type_node, LABEL_EXPR_LABEL(mod.default_label));
    } else {
      /* if we don't have a default, then the final else branch
       * should just jump to after the switch */
      COND_EXPR_ELSE(res) =
          build1(GOTO_EXPR, void_type_node, LABEL_EXPR_LABEL(mod.exit_label));
    }
    /* reset to the start of the if-else tree */
    res = (*(mod.ifs))[0];
  } else if (mod.has_default && mod.default_label != NULL_TREE) {
    /* this switch has only a default? ok... */
    res = build1(GOTO_EXPR, void_type_node, LABEL_EXPR_LABEL(mod.default_label));
  } else {
    /* this switch has no cases, and no default?! */
    warning_at(EXPR_LOCATION(mod.swcond), 0, "switch without cases or default?");
    res = build1(GOTO_EXPR, void_type_node, LABEL_EXPR_LABEL(mod.exit_label));
  }
  auto it = tsi_start(mod.swbody);
  tsi_link_before(&it, res, TSI_SAME_STMT);
  tsi_link_before(&it, build_empty_stmt(UNKNOWN_LOCATION), TSI_SAME_STMT);

  /* add the 'outside' of the switch, ie the 'finally'
   * aka the target of the break statements, the 'exit_label',
   * to the end of the switch body */
  append_to_statement_list(build_empty_stmt(UNKNOWN_LOCATION), &(mod.swbody));
  append_to_statement_list(mod.exit_label, &(mod.swbody));
  append_to_statement_list(build_empty_stmt(UNKNOWN_LOCATION), &(mod.swbody));

  /* we are returning SWITCH_STMT_BODY(swexpr),
   * instead of just swbody, because sometimes,
   * SWITCH_STMT_BODY(swexpr) may be a BIND_EXPR
   * that has some scoping-related information. */
  return SWITCH_STMT_BODY(swexpr);
}
