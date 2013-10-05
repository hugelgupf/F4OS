/*
 * COLLECTION - Abstraction for a group of objs - Michael Yenik
 *
 * Much like a Linux kset, ollections allow objs to be grouped together and
 * have nice common API for simple things like "add an obj to this group"
 * and "iterate through all objs in this collection".
 *
 * They are more than a kset though, since they also abstract away the
 * underlying datastructure and provide an API for iterating through it,
 * with the proper serialization/mutual exclusion, allowing the underlying
 * data structure to be changed later.
 *
 * Collections are kind of like an iterator baked in with add/delete functions
 * for your data structure.
 *
 * They are currently, like Linux kset, backed by a linked list with individual
 * objs parents pointing back to the containing obj.
 *
 * COLLECTION API
 *
 * struct obj *collection_iter(struct collection *)
 * ~ returns the first object in the collection (NULL if empty) and does
 *   whatever locking is needed to ensure sane iteration through all objs
 *
 * void collection_stop(struct collection *)
 * ~ Indicates iteration is done, usually needed to release locks taken in
 *   collection_iter().  Must be called if iteration exited early.
 *
 * struct obj* collection_next(struct collection *)
 * ~ returns the next item in a collection, or NULL if there are none left.
 *   If none are left, automatically stops iteration.
 *
 * void collection_add(struct collection *, struct obj *)
 * ~ Adds an obj to this collection
 *
 * void collection_del(struct collection *, struct obj *)
 * ~ Removes an obj from this collection
 */

#ifndef KERNEL_COLLECTION_H_INCLUDED
#define KERNEL_COLLECTION_H_INCLUDED

#include <list.h>
#include <kernel/obj.h>
#include <kernel/semaphore.h>

struct collection {
    struct semaphore lock;
    struct list list;
    struct list *curr;
};

#define INIT_COLLECTION(c) { .lock = INIT_SEMAPHORE, \
                             .list = INIT_LIST((c).list), \
                             .curr = NULL, \
                           }

struct obj *collection_iter(struct collection *c);
void collection_stop(struct collection *c);
struct obj *collection_next(struct collection *c);
void collection_add(struct collection *c, struct obj *o);
void collection_del(struct collection *c, struct obj *o);
struct obj *get_by_name(char *name, struct collection *c);

#endif
