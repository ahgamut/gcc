# `-fportcosmo`: patching `gcc` to build C software with Cosmopolitan Libc

This `gcc` patch simplifies the compilation of C software with
[Cosmopolitan Libc][cosmo], specifically solving the issues related to system
constants: it avoids the `switch case is not constant` and `initializer element
is not constant` errors by rewriting the AST at the necessary locations. 

The code is based on [my experimental GCC plugin][plugin], with simplifications
and extra features from being able to patch gcc itself. All my code in this
patch is under the [ISC License][isc].

## How to use this feature?

First, you would need to obtain a patched `gcc` built with this patch included:

* You can use the `gcc` binaries built with this patch on Github Actions:
  https://github.com/ahgamut/superconfigure/releases
* You can use the `gcc` binaries [bundled in the Cosmopolitan Libc
  monorepo][gcc-3p] which have been built with a recent version of this patch,
  along with the [`cosmocc` wrapper script][cosmocc].
* You can build via the usual `./configure` from this repo
* You can build `gcc` 11.2.0 on Linux [from my fork of
  `musl-cross-make`][mcross], where this patch is available as a single diff, or

Once you have a `gcc` built with this patch, just pass `-fportcosmo` as a compiler
flag when building your C software. If you'd prefer specific temporary integer
values to be used during the AST patching process, you can `#define` them with
the `__tmpcosmo_` prefix, like `#define __tmpcosmo_SIGTERM 15782233`.

## How does it work?

`-fportcosmo` works by rewriting the AST. Think of it as a "context-sensitive
LISP-y `defmacro` error handler", that activates before `gcc` can trigger a
`switch case is not constant` or `initializer element is not constant` error.

* the error is triggered because `gcc` expects the case or initializer to be a
  compile-time constant, which it isn't
* this patch activates and avoids the "error" by providing a temporary `int32_t`
  constant to the AST, which you can provide via a `#define __tmpcosmo_SIGTERM`
  in your code, or let the patch choose it automatically with values starting
  from 15840000
* after the complete AST has been constructed, this patch walks through the AST
  rewriting subtrees wherever the patch was activated earlier: `switch`
  statements are rewritten into `if-else` statements (with appropriate `goto`s
  to handle fallthroughs/`break` statements, and `struct` initializers are
  appended with one-time initializations (via `__attribute__((constructor))` for
  globals or an `if` for locals). No other part of the code being compiled is
  affected.

For an extended explanation, refer to the three READMEs [with my
plugin][plugin].  Note that this patch currently does not work with: 

* `g++` with `constexpr` initializations
* `enum`s (rewrite to `#define`s instead), or 
* wacky situtations where `SIGTERM` is used as an array index within an
  initializer (hello pls `busybox` why). 

Finally, remember this patch is just a convenience -- you could always rewrite
the `switch` statements and `struct` initializers manually.

[cosmo]: https://github.com/jart/cosmopolitan
[isc]: https://www.gnu.org/licenses/license-list.html#ISC
[plugin]: https://github.com/ahgamut/cosmo-gcc-plugin
[mcross]: https://github.com/ahgamut/musl-cross-make/tree/portcosmo
[gcc-3p]: https://github.com/jart/cosmopolitan/tree/48b2afb192ec18eca40c0b25603c02a2e3b578e9/third_party/gcc
[cosmocc]: https://github.com/jart/cosmopolitan/blob/48b2afb192ec18eca40c0b25603c02a2e3b578e9/tool/scripts/cosmocc
