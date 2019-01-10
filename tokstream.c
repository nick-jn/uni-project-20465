/*Token stream interface.

  Usage: pass the c_list of tokens to init_tokstream. Then, see the
  comments to the functions below.*/

#include <stdio.h>

#include "bool.h"
#include "token.h"
#include "clist.h"
#include "tokstream.h"

/*holds the relevant nodes of the token list*/
typedef struct tokstream_state {
    c_list *prev_node;
    c_list *current_node; /*is never NULL*/
} tokstream_state;

/*Seems reasonable to implement it this way. We never need
  more than one token stream, so making a function that spits
  out a struct of tokstream states in an oop function seems
  somewhat useless. This is good enough and it's less of a hassle.*/
static tokstream_state state_current;
static tokstream_state state_saved;

/*Initializer. Note that both state_current and state_saved are
  initialized to the beginning of the list of tokens.*/
void init_tokstream(c_list *last_token) {
    if (last_token == NULL) {
        return;
    }
    
    state_current.current_node = last_token->next;
    state_current.prev_node    = last_token->next;
    state_saved.current_node   = last_token->next;
    state_saved.prev_node      = last_token->next;
}

/*The passed toktype is downcasted. Returns true if the passed downcasted
  toktype is the same as the downcasted toktype of the current token in
  the tokstream. Returns false otherwise*/
bool probe_toktype(token_type toktype) {
    if (downcast_toktype(
        ((token*)state_current.current_node->item)->toktype) ==
        downcast_toktype(toktype)) {
        
        return true;
    }
    
    return false;
}

/*See probe_toktype. This function acts on the previous token
  in the tokstream.*/
bool probe_prev_toktype(token_type toktype) {
    if (downcast_toktype(
        ((token*)state_current.prev_node->item)->toktype) ==
        downcast_toktype(toktype)) {
        
        return true;
    }
    
    return false;
}

/*True if the current token in the tokstream is of the type toktype_EOL.*/
bool is_EOL_token() {
    if (((token*)state_current.current_node->item)->toktype == toktype_EOL) {
        return true;
    }
    
    return false;
}

/*Returns a pointer to the current token in the tokstream.*/
token *get_cur_token() {
    return ((token*)state_current.current_node->item);
}

/*Returns a pointer to the previous token in the tokstream.*/
token *get_prev_token() {
    return ((token*)state_current.prev_node->item);    
}

/*Advances the tokstream by one token. If the current token is the
  end of line token, nothing is done.*/
void advance_tokstream() {
    if (!is_EOL_token()) {
        state_current.prev_node = state_current.current_node;
        state_current.current_node = state_current.current_node->next;
    }
}

/*Saves the position of the stream.*/
void tstream_savepos() {
    state_saved.current_node = state_current.current_node;
    state_saved.prev_node = state_current.prev_node;
}

/*Loads the position of the stream from the prev. saved position.*/
void tstream_loadpos() {
    state_current.current_node = state_saved.current_node;
    state_current.prev_node = state_saved.prev_node;
}
