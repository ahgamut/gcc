// { dg-do compile { target c++2a } }
// { dg-options "-fdiagnostics-show-caret" }

template<typename T>
  inline constexpr bool foo_v = false;

template<typename T>
  concept foo = foo_v<T> || foo_v<T&>; // { dg-message "no operand" }
/* { dg-begin-multiline-output "" }
   concept foo = foo_v<T> || foo_v<T&>;
                 ~~~~~~~~~^~~~~~~~~~~~
   { dg-end-multiline-output "" } */

template<typename T>
  requires foo<T>
  void bar();
/* { dg-begin-multiline-output "" }
   void bar();
   { dg-end-multiline-output "" } */
/* { dg-prune-output "~" } */

void
baz()
{
  bar<int>(); // { dg-error "no match" }
/* { dg-begin-multiline-output "for no match error" }
   bar<int>();
   ~~~~~~~~^~
   { dg-end-multiline-output "" } */
/* { dg-begin-multiline-output "for required from here message" }
   bar<int>();
   ~~~~~~~~^~
   { dg-end-multiline-output "" } */
}
