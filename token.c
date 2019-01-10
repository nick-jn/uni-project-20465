/*The token of our assembly language.*/

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "bool.h"
#include "token.h"

static bool is_number(char *str);
static bool is_valid_identifier(char *str);

#define MAX_OPS 16
#define MAX_DATA_DIRS 5
#define MAX_REGS 10

/*data directive strings*/
static const char *STR_DATA_DIRS[MAX_DATA_DIRS] = {
    "data",
    "string",
    "struct",
    "entry",
    "extern"
};

/*register strings*/
static const char *STR_REGS[MAX_REGS] = {
    "r0",
    "r1",
    "r2",
    "r3",
    "r4",
    "r5",
    "r6",
    "r7",
    "r8",
    "r9"
};

/*Downcasts the token_type in the passed token *tok to a more generic one.*/
token_type downcast_toktype(token_type toktype) {
    if (toktype >= toktype_operator_mov &&
        toktype <= toktype_operator_stop) {
        return toktype_operator;
    }
    
    if ((toktype >= toktype_register_0 &&
        toktype <= toktype_register_9)) {
        return toktype_operand_register;
    }
    
    if (toktype >= toktype_datadir_data &&
        toktype <= toktype_datadir_extern) {
        return toktype_datadir;
    }
    
    return toktype;
}

/*Note that input is *NOT* malloc'd. That is, token->tokstr merely
  contains a pointer to the said string, create_token is not responsible
  for creating it.*/
token *create_token(int starting_index, int length, char *input) {
    int i;
    token *new_token;
    char *new_tokstr;
    
    new_token = malloc(sizeof(token));
    new_tokstr = malloc(sizeof(char)*(length+1));
    if (new_token == NULL || new_tokstr == NULL) {
        puts("Error, malloc failure in create_token.");
        exit(1);
    }
    
    for (i = 0; i < length; i++, starting_index++) {
        new_tokstr[i] = input[starting_index];
    }
    new_tokstr[i] = '\0';
    
    new_token->starting_index = starting_index-length;
    new_token->length         = length;
    new_token->tokstr         = new_tokstr;
    
    return new_token;
}

/*Frees the token. Not for use with clist (see destroy_clist_token).*/
void destroy_token(token *tok) {
    if (tok != NULL) {
        free(tok->tokstr);
        free(tok);
    }
}

/*Mallocs a new token. Note that we create a new tokstr. The function is
  called extract to suggest the we *extract* a token from tokstream. That
  is, it becomes independent of it.*/
token *extract_token(token *tok) {
    char *string;
    token *new_token;
    
    if (tok == NULL) {
        return NULL;
    }
    
    new_token = malloc(sizeof(token));
    string = malloc(sizeof(char)*(tok->length+1));
    if (new_token == NULL || string == NULL) {
        puts("Error, malloc failure in get_next_toktype: tok or string.");
        exit(1);
    }
    
    strcpy(string, tok->tokstr);
    
    new_token->starting_index = tok->starting_index;
    new_token->length         = tok->length;
    new_token->toktype        = tok->toktype;
    new_token->tokstr         = string;
    
    return new_token;
}

/*Figures out what toktype to give to the passed token.*/
token_type get_toktype(token *tok) {
    int i;
    char *tokstr = tok->tokstr;
    token_type toktype = toktype_unknown;
    
    if (is_number(tokstr)) {
        return toktype_number;
    }
    
    if (strlen(tokstr) == 1) {
        if (*tokstr == ':') {
            toktype = toktype_colon;
        } else if (*tokstr == ',') {
            toktype = toktype_comma;
        } else if (*tokstr == '.') {
            toktype = toktype_dot;
        } else if (*tokstr == '"') {
            toktype = toktype_quote;
        } else if (*tokstr == '#') {
            toktype = toktype_hash;
        }
    } else {
        for (i = 0; i < MAX_OPS; i++) { /*operator*/
            if (strcmp(OPS[i].opname, tokstr) == 0) {
                return toktype_operator + (i+1);
            }
        }
        for (i = 0; i < MAX_REGS; i++) { /*register, will also detect r8-r9*/
            if (strcmp(STR_REGS[i], tokstr) == 0) {
                return toktype_register_0 + i;
            }
        }
        for (i = 0; i < MAX_DATA_DIRS; i++) { /*data directive*/
            if (strcmp(STR_DATA_DIRS[i], tokstr) == 0) {
                return toktype_datadir + (i+1);
            }
        }
    }
    
    if (is_valid_identifier(tokstr)) {
        return toktype_identifier;
    }

    return toktype;
}

/*Checks if the passed string is a number.*/
static bool is_number(char *str) {
    if (*str == '-' || *str == '+') {
        str++;
    }
    
    while (*str != '\0') {
        if (!isdigit(*str)) {
            return false;
        }
        str++;
    }
    
    return true;
}

/*Checks if the passed string is a valid identifier.*/
static bool is_valid_identifier(char *str) {
    if (!isalpha(*str)) {
        return false;
    }
    
    str++;
    while (*str != '\0' && *str != '\n') {
        if (!isalnum(*str)) {
            return false;
        }
        str++;
    }

    return true;
}

/*Returns the string that desribes the passed token type.*/
const char *get_toktype_string(token_type toktype) {
    static const char *toktype_strings[] = {
        /*0*/   "dot",                    /*toktype_dot*/
        /*1*/   "comma",                  /*toktype_comma*/
        /*2*/   "colon",                  /*toktype_colon*/
        /*3*/   "hash",                   /*toktype_hash*/
        /*4*/   "quote",                  /*toktype_quote*/
        /*5*/   "number",                 /*toktype_number*/
        /*6*/   "label",                  /*toktype_label*/
        /*7*/   "data directive",         /*toktype_datadir*/
        /*8*/   "data directive data",    /*toktype_datadir_data*/
        /*9*/   "data directive string",  /*toktype_datadir_string*/
        /*10*/  "data directive struct",  /*toktype_datadir_struct*/
        /*11*/  "data directive entry",   /*toktype_datadir_entry*/
        /*12*/  "data directive extern",  /*toktype_datadir_extern*/
        /*13*/  "operator",               /*toktype_operator*/
        /*14*/  "operator mov",           /*toktype_operator_mov*/
        /*15*/  "operator cmp",           /*toktype_operator_cmp*/
        /*16*/  "operator add",           /*toktype_operator_add*/
        /*17*/  "operator sub",           /*toktype_operator_sub*/
        /*18*/  "operator not‬‬",           /*toktype_operator_not‬‬*/
        /*19*/  "operator clr‬‬",           /*toktype_operator_clr‬‬*/
        /*20*/  "operator lea‬‬",           /*toktype_operator_lea*/
        /*21*/  "operator inc",           /*toktype_operator_inc*/
        /*22*/  "operator dec",           /*toktype_operator_dec*/
        /*23*/  "operator jmp",           /*toktype_operator_jmp*/
        /*24*/  "operator bne",           /*toktype_operator_bne*/
        /*25*/  "operator red",           /*toktype_operator_red*/
        /*26*/  "operator prn",           /*toktype_operator_prn*/
        /*27*/  "operator jsr",           /*toktype_operator_jsr*/
        /*28*/  "operator rts‬",           /*toktype_operator_rts‬*/
        /*29*/  "operator stop",          /*toktype_operator_stop*/
        /*30*/  "register",               /*toktype_operand_register*/
        /*31*/  "register 0",             /*toktype_register_0*/
        /*32*/  "register 1",             /*toktype_register_1*/
        /*33*/  "register 2",             /*toktype_register_2*/
        /*34*/  "register 3",             /*toktype_register_3*/
        /*35*/  "register 4",             /*toktype_register_4*/
        /*36*/  "register 5",             /*toktype_register_5*/
        /*37*/  "register 6",             /*toktype_register_6*/
        /*38*/  "register 7",             /*toktype_register_7*/
        /*39*/  "register 8",             /*toktype_register_8*/
        /*40*/  "register 9",             /*toktype_register_9*/
        /*41*/  "identifier",             /*toktype_identifier*/
        /*42*/  "unknown",                /*toktype_unknown*/
        /*43*/  "string literal",         /*toktype_string*/
        /*44*/  "end of line"             /*toktype_EOL*/
    };
    
    return toktype_strings[toktype];
}

/*DEBUG*/
void print_token(token *tok) {
    if (tok == NULL) {
        printf("TOK IS NULL\n");
        return;
    }
    
    printf("Token: %s\t%s\n", tok->tokstr, get_toktype_string(tok->toktype));
}

/*Destroyer for tokens in clist.*/
void destroy_clist_token(void *tok) {
    token *p_tok = tok;
    if (p_tok == NULL) {
        return;
    }
    
    destroy_token(p_tok);
}

/*DEBUG*/
void print_clist_token(void *tok) {
    token *p_tok = tok;
    if (tok == NULL) {
        return;
    }
    
    printf("Token: %s\t%s\n", p_tok->tokstr,
                              get_toktype_string(p_tok->toktype));
}
