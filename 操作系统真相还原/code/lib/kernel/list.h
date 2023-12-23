#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H
#include"global.h"

#define offset(struct_type, member) (long int)(&(((struct_type*)0)->member))

#define elem2entry(struct_type, struct_member_name, elem_ptr) \
  (struct_type*)((long int)elem_ptr - offset(struct_type, struct_member_name))
struct list_node
{
    struct list_node* prev;
    struct list_node* next;

};
struct list
{
    struct list_node head;
    struct list_node tail;
};
typedef bool (function)(struct list_node*, int arg);

void list_init (struct list*);
void list_insert_before(struct list_node* before, struct list_node* elem);
void list_push(struct list* plist, struct list_node* elem);
void list_iterate(struct list* plist);
void list_append(struct list* plist, struct list_node* elem);  
void list_remove(struct list_node* pelem);
struct list_node* list_pop(struct list* plist);
bool list_empty(struct list* plist);
uint32_t list_len(struct list* plist);
struct list_node* list_traversal(struct list* plist, function func, int arg);
bool elem_find(struct list* plist, struct list_node* obj_elem);
#endif