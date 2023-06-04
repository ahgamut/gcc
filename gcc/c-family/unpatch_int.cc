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
#include "c-family/subcontext.h"

int arg_should_be_unpatched(tree arg, const subu_node *use, tree *rep_ptr) {
  /* if we are returning 1, rep_ptr has been set.
   * if we are returning 0, rep_ptr is unchanged.
   * use is not affected! */
  if (TREE_CODE(arg) == INTEGER_CST) {
    tree vx = DECL_INITIAL(get_ifsw_identifier(use->name));
    if (tree_int_cst_equal(arg, vx)) {
      /* if this is an integer constant, AND its
       * value is equal to the macro we substituted,
       * then we replace the correct variable here */
      *rep_ptr =
          build1(NOP_EXPR, integer_type_node, VAR_NAME_AS_TREE(use->name));
      INFORM(use->loc, "unpatched an integer here with %s\n", use->name);
      return 1;
    }
    /* here you might want to handle some
     * minimal constant folding algebra,
     * like -VAR or ~VAR */
    if (tree_fits_poly_int64_p(vx) && tree_fits_poly_int64_p(arg)) {
      auto v1 = tree_to_poly_int64(vx);
      auto v2 = tree_to_poly_int64(arg);

      /* handle the -VAR case */
      if (known_eq(v1, -v2)) {
        INFORM(use->loc, "unpatched an integer here with -%s\n", use->name);
        *rep_ptr =
            build1(NEGATE_EXPR, integer_type_node, VAR_NAME_AS_TREE(use->name));
        return 1;
      }

      /* handle the ~VAR case */
      if (known_eq(v1, ~v2)) {
        INFORM(use->loc, "unpatched an integer here with ~%s\n", use->name);
        *rep_ptr = build1(BIT_NOT_EXPR, integer_type_node,
                          VAR_NAME_AS_TREE(use->name));
        return 1;
      }
    }
    return 0;
  }

  return 0;
}
