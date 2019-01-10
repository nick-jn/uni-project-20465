#ifndef C_LIST_H
#define C_LIST_H

typedef struct c_list {
    void *item; /*can be anything - both structs and single variables*/
    struct c_list *next;
} c_list;


void add_clist(c_list **last_node, void *item);
void destroy_clist(c_list **last_node, void(*item_destroyer)(void *));

void *find_clist_str(c_list *last_node, void*(*item_finder)(void *, char*),
                     char *str);
                     
void print_clist(c_list *last_node, void(*item_printer)(void *));
void print_clist_range(c_list *last_node, int start, int length,
                       void(*item_printer)(void *));

#endif /*C_LIST_H*/
