#ifndef PORTCOSMO_H
#define PORTCOSMO_H
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

void portcosmo_setup();
void portcosmo_teardown();
void portcosmo_show_tree(location_t, tree);
tree replace_case_nonconst(location_t, tree);

#endif /* PORTCOSMO_H */
