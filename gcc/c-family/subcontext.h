#ifndef SUBCONTEXT_H
#define SUBCONTEXT_H
#include "c-family/portcosmo.internal.h"
#include "hash-map-traits.h"
#include "hash-map.h"
#include "hash-traits.h"

enum SubstType { 
    PORTCOSMO_UNKNOWN = 0, 
    PORTCOSMO_SWCASE = 1, 
    PORTCOSMO_INITVAL = 2 
};

struct subu_node {
  /* a node indicating that an ifswitch substitution has occurred.
   *
   * Details include:
   *
   * - location_t of the substitution
   * - char* of name of the macro that was substituted (alloc'd)
   * - whether the substitution was inside a switch statement
   * - _subu_node* pointer to the next element in the list (NULL if last)
   *
   * the idea is that every time one of our modified macros is used,
   * we record the substitution, and then we delete this record if
   * we find the appropriate location_t during pre-genericize and
   * construct the necessary parse trees at that point.
   *
   * at the end of compilation (ie PLUGIN_FINISH), there should be
   * no subu_nodes allocated.
   */
  location_t loc;
  SubstType tp;
  char *name;
  struct subu_node *next;
};

typedef struct subu_node subu_node;

struct subu_list {
  subu_node *head;
  /* inclusive bounds, range containing all recorded substitutions */
  location_t start, end;
  /* number of substitutions */
  int count;
};
typedef struct subu_list subu_list;

int check_loc_in_bound(subu_list *, location_t);
int valid_subu_bounds(subu_list *, location_t, location_t);
int get_subu_elem(subu_list *, location_t, subu_node **);
int get_subu_elem2(subu_list *, source_range, subu_node **);
void remove_subu_elem(subu_list *, subu_node *);

struct tmpconst {
    long long raw;
    tree t;
};
typedef struct tmpconst tmpconst;

struct free_string_hash : pointer_hash<char>, typed_free_remove <char> {
  static inline hashval_t hash (char *id) {
    return htab_hash_string (id);
  };
  static inline bool equal (char *a, char *b) {
    return strcmp(a, b) == 0;
  };
};
using tmpmap_traits = simple_hashmap_traits<free_string_hash, tmpconst>;
using tmpmap = hash_map<char, tmpconst, tmpmap_traits>;

/* Substitution Context */
struct SubContext {
  /* record all macro uses */
  subu_list *mods;
  /* address of the previous statement we walked through,
   * in case we missed modding it and have to retry */
  tree *prev;
  /* count number of switch statements rewritten */
  unsigned int switchcount;
  /* count number of initializations rewritten */
  unsigned int initcount;
  /* count number of other substitutions rewritten */
  unsigned int subcount;
  /* if zero, it means we haven't started or something
   * went wrong somewhere */
  int active;
  /* store values of all temporary constants */
  tmpmap *map;
};

void add_context_subu(SubContext *, const location_t, const char *,
                      unsigned int, SubstType);
void construct_context(SubContext *);
void check_context_clear(SubContext *, location_t);
void cleanup_context(SubContext *);

int arg_should_be_unpatched(tree, const subu_node *, tree *);

/* declaring cosmo_ctx here so initstruct knows it exists */
extern struct SubContext cosmo_ctx;

#endif /* SUBCONTEXT_H */
