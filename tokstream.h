#ifndef TOKSTREAM_H
#define TOKSTREAM_H

void init_tokstream(c_list *last_token);

bool probe_toktype(token_type toktype);
bool probe_prev_toktype(token_type toktype);
bool is_EOL_token();

token *get_cur_token();
token *get_prev_token();

void advance_tokstream();
void tstream_savepos();
void tstream_loadpos();

#endif /*TOKSTREAM_H*/
