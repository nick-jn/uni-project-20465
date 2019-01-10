/*Generic circular linked list.*/

#include <stdio.h>
#include <stdlib.h>

#include "clist.h"

/*Adds a new item to the list. The last node of the list must be provided.
  If the last node is null, this, in effect, creates the first node of
  the list.*/
void add_clist(c_list **last_node, void *item) {
    c_list *new_node;
   
    new_node = malloc(sizeof(c_list));
    if (new_node == NULL) {
        fprintf(stderr, "Malloc failure in add_clist.");
        exit(1);
    }
    
    new_node->item = item;
    
    if (*last_node == NULL) {
        new_node->next = new_node;
    } else {
        new_node->next = (*last_node)->next;
        (*last_node)->next = new_node;
    }
    
    *last_node = new_node;
}

/*Frees the given list. Requires the appropriate item_destroyer function.
  The last_node pointer contents will be set to NULL at the end to prevent
  a dangling pointer.*/
void destroy_clist(c_list **last_node, void(*item_destroyer)(void *)) {
    c_list *cur_node;
    c_list *next_node;
    
    if (*last_node == NULL) {
        return;
    }
    
    /*converts to singly linked list for ease of use*/
    cur_node = (*last_node)->next;
    (*last_node)->next = NULL;

    while (cur_node != NULL) {
        next_node = cur_node->next;
        
        (*item_destroyer)(cur_node->item);
        free(cur_node);
        
        cur_node = next_node;
    }
    
    *last_node = NULL;
}

/*Finds a string in a given list. Requires the appropriate item_finder
  function.*/
void *find_clist_str(c_list *last_node, void*(*item_finder)(void *, char*),
                     char *str) {
    c_list *cur_node;
    void *found_item;
       
    if (last_node == NULL) {
        return NULL;
    }
    
    cur_node = last_node->next;
    do {
        if ((found_item = (*item_finder)(cur_node->item, str)) != NULL) {
            return found_item;
        }
        
        cur_node = cur_node->next;
    } while (cur_node != last_node->next);
    
    return NULL;
}

/*DEBUG*/
void print_clist(c_list *last_node, void(*item_printer)(void *)) {
    c_list *cur_node;
    
    if (last_node == NULL) {
        return;
    }
    
    cur_node = last_node->next;
    do {
        item_printer(cur_node->item);
        cur_node = cur_node->next;
    } while (cur_node != last_node->next);
}

/*DEBUG*/
void print_clist_range(c_list *last_node, int start, int length,
                       void(*item_printer)(void *)) {
    c_list *cur_node;
    int counter = start;
    
    if (last_node == NULL) {
        return;
    }
    
    
    cur_node = last_node->next;
    if (start != 0) {
        do {
            cur_node = cur_node->next;
            start--;
        } while (cur_node != last_node->next && start != 0);
    }
    
    if (start != 0) {
        printf("print_clist_range out of bounds: start\n");
        return;
    }
    
    do {
        printf("%d\t", counter);
        item_printer(cur_node->item);
        cur_node = cur_node->next;
        length--;
        counter++;
    } while (cur_node != last_node->next && length != 0);
    
    if (start != 0) {
        printf("print_clist_range out of bounds: length\n");
    }
}
