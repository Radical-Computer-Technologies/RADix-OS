#ifndef RAD_SYSROOT_SEARCH_H
#define RAD_SYSROOT_SEARCH_H

typedef enum {
    preorder,
    postorder,
    endorder,
    leaf
} VISIT;

typedef enum {
    FIND,
    ENTER
} ACTION;

void *tsearch(const void *key, void **rootp, int (*compar)(const void *, const void *));
void *tfind(const void *key, void *const *rootp, int (*compar)(const void *, const void *));
void *tdelete(const void *key, void **rootp, int (*compar)(const void *, const void *));
void twalk(const void *root, void (*action)(const void *, VISIT, int));
void tdestroy(void *root, void (*free_node)(void *nodep));

#endif
