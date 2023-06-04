#ifndef PORTCOSMO_H
#define PORTCOSMO_H
#include <tree.h>

void portcosmo_setup();
void portcosmo_teardown();
void portcosmo_pre_genericize(void*);
void portcosmo_finish_decl(void*);
void portcosmo_show_tree(location_t, tree);
tree patch_case_nonconst(location_t, tree);
tree patch_init_nonconst(location_t, tree);

#endif /* PORTCOSMO_H */
