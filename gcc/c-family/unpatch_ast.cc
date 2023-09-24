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
#include "c-family/initstruct.h"

tree check_usage(tree *tp, int *check_subtree, void *data) {
    SubContext *ctx = (SubContext *)(data);
    tree t = *tp;
    tree z;
    subu_node *use = NULL;
    location_t loc = EXPR_LOCATION(t);
    source_range rng = EXPR_LOCATION_RANGE(t);

    if (ctx->active == 0 || ctx->mods->count == 0) {
        /* DEBUGF("substitutions complete\n"); */
        *check_subtree = 0;
        return *tp;
    }

    if (LOCATION_AFTER2(loc, rng.m_start)) {
        loc = rng.m_start;
    } else {
        rng.m_start = loc;
    }

    if (ctx->prev && LOCATION_BEFORE2(ctx->mods->start, rng.m_start)) {
        auto vloc = DECL_SOURCE_LOCATION(DECL_EXPR_DECL(*(ctx->prev)));
        /* below inequality holds inside this if condition:
         *    vloc <= ctx->mods->start <= rng.m_start
         * this means that there was a macro substitution
         * between vloc and rng.m_start, which was not
         * eliminated when we went through the other parts
         * of the parse tree earlier. thus, the decl_expr
         * that we have stored in ctx->prev needs to be
         * checked for possible macro substitutions */
        DEBUGF(
            "did we miss a decl? vloc=%u,%u, loc=%u,%u, rng.mstart=%u,%u, "
            "start=%u,%u\n",
            LOCATION_LINE(vloc), LOCATION_COLUMN(vloc),  //
            LOCATION_LINE(loc), LOCATION_COLUMN(loc),    //
            LOCATION_LINE(rng.m_start), LOCATION_COLUMN(rng.m_start),
            LOCATION_LINE(ctx->mods->start), LOCATION_COLUMN(ctx->mods->start));
        auto z = ctx->initcount;
        build_modded_declaration(ctx->prev, ctx, rng.m_start);
        if (z != ctx->initcount) {
            ctx->prev = NULL;
            check_context_clear(ctx, loc);
        }
    }

    if (TREE_CODE(t) == DECL_EXPR && TREE_STATIC(DECL_EXPR_DECL(t))) {
        INFORM(loc, "should we mod this?\n");
        ctx->prev = tp;
    }

    if (TREE_CODE(t) == SWITCH_STMT) {
        rng = get_switch_bounds(t);
        if (valid_subu_bounds(ctx->mods, rng.m_start, rng.m_finish) &&
            count_mods_in_switch(t, ctx->mods) > 0) {
            /* this is one of the switch statements
             * where we modified a case label */
            DEBUGF("modding the switch \n");
            *tp = build_modded_switch_stmt(t, ctx);
            DEBUGF("we modded it??\n");
            walk_tree_without_duplicates(tp, check_usage, ctx);
            /* due to the above call, I don't need to check
             * any subtrees from this current location */
            *check_subtree = 0;
            ctx->switchcount += 1;
            return NULL_TREE;
        }
    }

    return NULL_TREE;
}

void handle_pre_genericize(void *gcc_data, void *user_data) {
    tree t = (tree)gcc_data;
    SubContext *ctx = (SubContext *)user_data;
    tree t2;
    if (ctx->active && TREE_CODE(t) == FUNCTION_DECL &&
        DECL_INITIAL(t) != NULL && TREE_STATIC(t)) {
        /* this function is defined within the file I'm processing */
        if (ctx->mods->count == 0) {
            // DEBUGF("no substitutions were made in %s\n", IDENTIFIER_NAME(t));
            return;
        }
        t2 = DECL_SAVED_TREE(t);
        ctx->prev = NULL;
        walk_tree_without_duplicates(&t2, check_usage, ctx);
        /* now at this stage, all uses of our macros have been
         * fixed, INCLUDING case labels. Let's confirm that: */
        check_context_clear(ctx, MAX_LOCATION_T);
    }
}

void portcosmo_pre_genericize(void *gcc_data) {
    handle_pre_genericize(gcc_data, (void *)(&cosmo_ctx));
}
