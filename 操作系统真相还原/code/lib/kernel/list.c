#include "list.h"
#include "interrupt.h"
void list_init (struct list* list) {
   list->head.prev = NULL;
   list->head.next = &list->tail;
   list->tail.prev = &list->head;
   list->tail.next = NULL;
}
void list_insert_before(struct list_node* before, struct list_node* elem)
{
    enum intr_status old_status = intr_disable();//将中断关闭，防止这一步在操作时时间耗尽被切走

   before->prev->next = elem; 
   elem->prev = before->prev;
   elem->next = before;
   before->prev = elem;

   intr_set_status(old_status);
}
void list_remove(struct list_node* pelem)
{
    enum intr_status old = intr_disable();
    pelem->prev->next = pelem->next;
    pelem->next->prev = pelem->prev;
    intr_set_status(old);
}
//将元素添加到头部
void list_push(struct list* plist, struct list_node* elem)
{
    list_insert_before(plist->head.next, elem);
}
//将元素添加到尾部
void list_append(struct list* plist, struct list_node* elem)
{
    list_insert_before(&plist->tail, elem);
}
uint32_t list_len(struct list* plist)
{
    struct list_node* p = plist->head.next;
    int count=0;
    while (p != &plist->tail) {
        p = p->next;
        count++;
    }
    return count;
}
struct list_node* list_pop(struct list* plist) {
   struct list_node* elem = plist->head.next;
   list_remove(elem);
   return elem;
} 

/* 从链表中查找元素obj_elem,成功时返回true,失败时返回false */
bool elem_find(struct list* plist, struct list_node* obj_elem) {
   struct list_node* elem = plist->head.next;
   while (elem != &plist->tail) {
      if (elem == obj_elem) {
	 return true;
      }
      elem = elem->next;
   }
   return false;
}
struct list_node* list_traversal(struct list* plist, function func, int arg) {
   struct list_node* elem = plist->head.next;
/* 如果队列为空,就必然没有符合条件的结点,故直接返回NULL */
   if (list_empty(plist)) { 
      return NULL;
   }

   while (elem != &plist->tail) {
      if (func(elem, arg)) {		  // func返回ture则认为该元素在回调函数中符合条件,命中,故停止继续遍历
	 return elem;
      }					  // 若回调函数func返回true,则继续遍历
      elem = elem->next;	       
   }
   return NULL;
}
bool list_empty(struct list* plist)
{
    return (plist->head.next == &plist->tail ? true : false);
}