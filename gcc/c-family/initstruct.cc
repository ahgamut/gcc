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
#include "c-family/initstruct.h"

void portcosmo_finish_decl(void *gcc_data) {
    handle_decl(gcc_data, (void *)(&cosmo_ctx));
}

void set_values_based_on_ctor(tree ctor, subu_list *list, tree body, tree lhs,
                              location_t bound) {
  subu_node *use = NULL;
  unsigned int iprev = 0;
  bool started = true;
  tree replacement = NULL_TREE;

  while (list->count > 0 && LOCATION_BEFORE2(list->start, bound)) {
    tree index = NULL_TREE;
    tree val = NULL_TREE;
    unsigned int i = 0;
    int found = 0;
    FOR_EACH_CONSTRUCTOR_ELT(CONSTRUCTOR_ELTS(ctor), i, index, val) {
      DEBUGF("value %u is %s\n", i, get_tree_code_str(val));
      if (!started && i <= iprev) continue;
      if (TREE_CODE(val) == INTEGER_CST) {
        for (use = list->head; use; use = use->next) {
          found = arg_should_be_unpatched(val, use, &replacement);
          if (found) break;
        }
        if (found) {
          iprev = i;
          started = false;
          break;
        }
      } else if (TREE_CODE(val) == CONSTRUCTOR) {
        auto sub = access_at(lhs, index);
        set_values_based_on_ctor(val, list, body, sub, bound);
        use = NULL; /* might've gotten stomped */
        if (list->count == 0) return;
        get_subu_elem(list, list->start, &use);
      }
    }
    if (found) {
      auto modexpr = build2(MODIFY_EXPR, TREE_TYPE(index),
                            access_at(lhs, index), replacement);
      append_to_statement_list(modexpr, &body);
      remove_subu_elem(list, use);
      replacement = NULL_TREE;
      DEBUGF("found; %d left\n", list->count);
    } else {
      /* we did not find any (more) substitutions to fix */
      DEBUGF("exiting; %d left\n", list->count);
      break;
    }
  }
}

/* initstruct/global.cc */

void update_global_decls(tree dcl, SubContext *ctx) {
  tree body = alloc_stmt_list();
  tree replacement = NULL_TREE;
  subu_node *use = NULL;
  char chk[STRING_BUFFER_SIZE];

  /* dcl, the global declaration we have is like these:
   *
   * static int foo = __tmpcosmo_VAR;
   * static struct toy myvalue = {.x=1, .y=__tmpcosmo_VAR};
   *
   * we're going to add functions as follows:
   *
   * static int foo = __tmpcosmo_VAR;
   * __attribute__((constructor)) __hidden_ctor1() {
   *   foo = VAR_OR_EXPR;
   * }
   * static struct toy myvalue = {.x=1, .y=__tmpcosmo_VAR};
   * __attribute__((constructor)) __hidden_ctor2() {
   *    myvalue.y = VAR_OR_EXPR;
   * }
   *
   * the modifier functions have the constructor attribute,
   * so it they run before anything uses the static. it
   * works recursively too: you can have a struct of structs,
   * an array of structs, whatever, and it will figure out
   * where the substitutions are and attempt to mod them.
   *
   * a unique constructor for each declaration. probably
   * we could have a common constructor for the entire
   * file, but that's left as an exercise for the reader. */
  if (INTEGRAL_TYPE_P(TREE_TYPE(dcl)) &&
      get_subu_elem(ctx->mods, ctx->mods->start, &use) &&
      /* use is non-NULL if get_subu_elem succeeds */
      check_magic_equal(DECL_INITIAL(dcl), use->name) && 
      arg_should_be_unpatched(DECL_INITIAL(dcl), use, &replacement)) {
    if (TREE_READONLY(dcl)) {
      error_at(EXPR_LOCATION(dcl), "cannot substitute this constant\n");
      /* actually I can, but the issue is if one of gcc's optimizations
       * perform constant folding(and they do), I don't know all the spots
       * where this variable has been folded, so I can't substitute there */
      ctx->active = 0;
      return;
    }
    append_to_statement_list(
        build2(MODIFY_EXPR, void_type_node, dcl, replacement),
        &body);
    remove_subu_elem(ctx->mods, use);
    cgraph_build_static_cdtor('I', body, 0);
  } else if ((RECORD_TYPE == TREE_CODE(TREE_TYPE(dcl)) ||
              ARRAY_TYPE == TREE_CODE(TREE_TYPE(dcl))) &&
             DECL_INITIAL(dcl) != NULL_TREE) {
    if (TREE_READONLY(dcl)) {
      warning_at(DECL_SOURCE_LOCATION(dcl), 0,
                 "not sure if modding const structs is good\n");
      TREE_READONLY(dcl) = 0;
    }
    if (LOCATION_BEFORE2(ctx->mods->end, input_location)) {
      set_values_based_on_ctor(DECL_INITIAL(dcl), ctx->mods, body, dcl,
                               input_location);
    } else {
      set_values_based_on_ctor(DECL_INITIAL(dcl), ctx->mods, body, dcl,
                               ctx->mods->end);
    }
    cgraph_build_static_cdtor('I', body, 0);
    DEBUGF("uploaded ctor\n");
  }
}

void handle_decl(void *gcc_data, void *user_data) {
  tree t = (tree)gcc_data;
  SubContext *ctx = (SubContext *)user_data;
  if (TREE_CODE(t) != VAR_DECL) return;
  if (ctx->active && ctx->mods->count > 0 && DECL_INITIAL(t) != NULL &&
      DECL_CONTEXT(t) == NULL_TREE) {
    int internal_use =
        !strncmp(IDENTIFIER_NAME(t), "__tmpcosmo_", strlen("__tmpcosmo_"));
    if (internal_use || DECL_EXTERNAL(t)) {
      error_at(input_location, "the ACTUALLY is before the declaration!\n");
      ctx->active = 0;
      return;
    }
    auto rng = EXPR_LOCATION_RANGE(t);
    rng.m_start = DECL_SOURCE_LOCATION(t);
    rng.m_finish = input_location;

    DEBUGF("handle_decl with %s %u,%u - %u-%u\n", IDENTIFIER_NAME(t),
           LOCATION_LINE(rng.m_start), LOCATION_COLUMN(rng.m_start),
           LOCATION_LINE(rng.m_finish), LOCATION_COLUMN(rng.m_finish));
    ctx->initcount += ctx->mods->count;
    update_global_decls(t, ctx);
    /* now at this stage, all uses of our macros have been
     * fixed, INCLUDING case labels. Let's confirm that: */
    check_context_clear(ctx, MAX_LOCATION_T);
  }
}

/* initstruct/local.cc */

static inline tree build_modded_if_stmt(tree condition, tree then_clause,
                                        tree else_clause = NULL_TREE) {
  return build3(COND_EXPR, void_type_node, condition, then_clause, else_clause);
}

int build_modded_int_declaration(tree *dxpr, SubContext *ctx, subu_node *use) {
  char chk[STRING_BUFFER_SIZE];
  tree dcl = DECL_EXPR_DECL(*dxpr);
  tree replacement = NULL_TREE;

  if (INTEGRAL_TYPE_P(TREE_TYPE(dcl)) &&
      arg_should_be_unpatched(DECL_INITIAL(dcl), use, &replacement)) {
    if (TREE_READONLY(dcl)) {
      error_at(EXPR_LOCATION(dcl), "cannot substitute this constant\n");
      /* actually I can, but the issue is if one of gcc's optimizations
       * perform constant folding(and they do), I don't know all the spots
       * where this variable has been folded, so I can't substitute there */
      ctx->active = 0;
      return 0;
    }

    if (!TREE_STATIC(dcl)) {
      DECL_INITIAL(dcl) = replacement;
      remove_subu_elem(ctx->mods, use);
      replacement = NULL_TREE;
      return 1;
    }

    DEBUGF("fixing decl for a static integer\n");
    /* (*dxpr), the statement we have is this:
     *
     * static int myvalue = __tmpcosmo_VAR;
     *
     * we're going to modify it to this:
     *
     * static int myvalue = __tmpcosmo_VAR;
     * static uint8 __chk_ifs_myvalue = 0;
     * if(__chk_ifs_myvalue != 1) {
     *   __chk_ifs_myvalue = 1;
     *   myvalue = VAR;
     * }
     *
     * so the modified statement runs exactly once,
     * whenever the function is first called, right
     * after the initialization of the variable we
     * wanted to modify. */

    /* build __chk_ifs_myvalue */
    snprintf(chk, sizeof(chk), "__chk_ifs_%s", IDENTIFIER_NAME(dcl));
    tree chknode = build_decl(DECL_SOURCE_LOCATION(dcl), VAR_DECL,
                              get_identifier(chk), uint8_type_node);
    DECL_INITIAL(chknode) = build_int_cst(uint8_type_node, 0);
    TREE_STATIC(chknode) = TREE_STATIC(dcl);
    TREE_USED(chknode) = TREE_USED(dcl);
    DECL_READ_P(chknode) = DECL_READ_P(dcl);
    DECL_CONTEXT(chknode) = DECL_CONTEXT(dcl);
    DECL_CHAIN(chknode) = DECL_CHAIN(dcl);
    DECL_CHAIN(dcl) = chknode;

    /* create the then clause of the if statement */
    tree then_clause = alloc_stmt_list();
    append_to_statement_list(build2(MODIFY_EXPR, void_type_node, chknode,
                                    build_int_cst(uint8_type_node, 1)),
                             &then_clause);
    append_to_statement_list(
        build2(MODIFY_EXPR, void_type_node, dcl, replacement),
        &then_clause);

    /* create the if statement into the overall result mentioned above */
    tree res = alloc_stmt_list();
    append_to_statement_list(*dxpr, &res);
    append_to_statement_list(build1(DECL_EXPR, void_type_node, chknode), &res);
    append_to_statement_list(
        build_modded_if_stmt(build2(NE_EXPR, void_type_node, chknode,
                                    build_int_cst(uint8_type_node, 1)),
                             then_clause),
        &res);
    /* overwrite the input tree with our new statements */
    *dxpr = res;
    remove_subu_elem(ctx->mods, use);
    replacement = NULL_TREE;
    return 1;
  }
  return 0;
}

void modify_local_struct_ctor(tree ctor, subu_list *list, location_t bound) {
  subu_node *use = NULL;
  unsigned int iprev = 0;
  bool started = true;
  tree replacement = NULL_TREE;

  while (list->count > 0 && LOCATION_BEFORE2(list->start, bound)) {
    tree val = NULL_TREE;
    unsigned int i = 0;
    int found = 0;
    FOR_EACH_CONSTRUCTOR_VALUE(CONSTRUCTOR_ELTS(ctor), i, val) {
      DEBUGF("value %u is %s\n", i, get_tree_code_str(val));
      if (TREE_CODE(val) == INTEGER_CST) {
        for (use = list->head; use; use = use->next) {
          found = arg_should_be_unpatched(val, use, &replacement);
          if (found) break;
        }
        if (found) {
          iprev = i;
          started = false;
          break;
        }
      } else if (TREE_CODE(val) == CONSTRUCTOR) {
        modify_local_struct_ctor(val, list, bound);
        use = NULL; /* might've gotten stomped */
        if (list->count == 0 || LOCATION_AFTER2(list->start, bound)) return;
      }
    }
    if (found) {
      DEBUGF("found\n");
      CONSTRUCTOR_ELT(ctor, i)->value = replacement;
      remove_subu_elem(list, use);
      replacement = NULL_TREE;
    } else {
      /* we did not find any (more) substitutions to fix */
      break;
    }
  }
}

void build_modded_declaration(tree *dxpr, SubContext *ctx, location_t bound) {
  char chk[STRING_BUFFER_SIZE];
  tree dcl = DECL_EXPR_DECL(*dxpr);
  subu_node *use = NULL;
  subu_list *list = ctx->mods;
  unsigned int oldcount = list->count;

  if (INTEGRAL_TYPE_P(TREE_TYPE(dcl))) {
    get_subu_elem(list, list->start, &use);
    if (build_modded_int_declaration(dxpr, ctx, use)) {
      use = NULL;
      ctx->initcount += 1;
    }
    return;
  }

  if ((RECORD_TYPE == TREE_CODE(TREE_TYPE(dcl)) ||
       ARRAY_TYPE == TREE_CODE(TREE_TYPE(dcl))) &&
      DECL_INITIAL(dcl) != NULL_TREE) {
    if (TREE_READONLY(dcl)) {
      warning_at(EXPR_LOCATION(*dxpr), 0,
                 "not sure if modding const structs is good\n");
      TREE_READONLY(dcl) = 0;
      build_modded_declaration(dxpr, ctx, bound);
      return;
    } else if (TREE_STATIC(dcl)) {
      DEBUGF("fixing decl for a static struct\n");
      /* (*dxpr), the statement we have is this:
       *
       * static struct toy myvalue = {.x=1, .y=__tmpcosmo_VAR};
       *
       * we're going to modify it to this:
       *
       * static struct toy myvalue = {.x=1, .y=__tmpcosmo_VAR};
       * static uint8 __chk_ifs_myvalue = 0;
       * if(__chk_ifs_myvalue != 1) {
       *   __chk_ifs_myvalue = 1;
       *   myvalue.y = VAR;
       * }
       *
       * so the modified statement runs exactly once,
       * whenever the function is first called, right
       * after the initialization of the variable we
       * wanted to modify. */

      /* build __chk_ifs_myvalue */
      snprintf(chk, sizeof(chk), "__chk_ifs_%s", IDENTIFIER_NAME(dcl));
      tree chknode = build_decl(DECL_SOURCE_LOCATION(dcl), VAR_DECL,
                                get_identifier(chk), uint8_type_node);
      DECL_INITIAL(chknode) = build_int_cst(uint8_type_node, 0);
      TREE_STATIC(chknode) = TREE_STATIC(dcl);
      TREE_USED(chknode) = TREE_USED(dcl);
      DECL_READ_P(chknode) = DECL_READ_P(dcl);
      DECL_CONTEXT(chknode) = DECL_CONTEXT(dcl);
      DECL_CHAIN(chknode) = DECL_CHAIN(dcl);
      DECL_CHAIN(dcl) = chknode;

      /* build a scope block for the temporary value */
      tree tmpscope = build0(BLOCK, void_type_node);
      BLOCK_SUPERCONTEXT(tmpscope) = TREE_BLOCK(*dxpr);
      // debug_tree(BLOCK_SUPERCONTEXT(tmpscope));

      /* create the then clause of the if statement */
      tree then_clause = alloc_stmt_list();
      append_to_statement_list(build2(MODIFY_EXPR, void_type_node, chknode,
                                      build_int_cst(uint8_type_node, 1)),
                               &then_clause);
      set_values_based_on_ctor(DECL_INITIAL(dcl), ctx->mods, then_clause, dcl,
                               bound);

      /* create the if statement into the overall result mentioned above */
      tree res = alloc_stmt_list();
      append_to_statement_list(*dxpr, &res);
      append_to_statement_list(build1(DECL_EXPR, void_type_node, chknode),
                               &res);
      append_to_statement_list(
          build_modded_if_stmt(build2(NE_EXPR, void_type_node, chknode,
                                      build_int_cst(uint8_type_node, 1)),
                               then_clause),
          &res);
      /* overwrite the input tree with our new statements */
      *dxpr = res;
    } else {
      /* if it's a local struct, we can
       * just mod the constructor itself */
      auto ctor = DECL_INITIAL(dcl);
      modify_local_struct_ctor(ctor, list, bound);
    }
  }
  ctx->initcount += (oldcount - list->count);
}
