/*Contains all the relevant data on the file itself.*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bool.h"
#include "token.h"
#include "clist.h"
#include "filedata.h"

extern unsigned int ERRORS;

/*Note that we create a copy of the passed token tok.*/
item_label *create_item_label(token *tok, int address, int linenum,
                              stat_type stype) {
    item_label *new_label;
    
    new_label = malloc(sizeof(item_label));
    if (new_label == NULL) {
        fprintf(stderr, "Malloc failure in create_item_label.");
        exit(1);
    }
    
    new_label->tok     = extract_token(tok);
    new_label->linenum = linenum;
    new_label->IC      = address;
    new_label->stype   = stype;
    
    return new_label;
}

/*Finder for use with clist.*/
void *find_item_label(void *item, char *str) {
    if (item == NULL) {
        return NULL;
    }
    
    if (strcmp(((item_label*)item)->tok->tokstr, str) == 0) {
        return item;
    }
    
    return NULL;
}

/*Destroyer for use with clist.*/
void destroy_item_label(void *item) {
    if (item == NULL) {
        return;
    }
    
    destroy_token(((item_label*)item)->tok);
    free(item);
}

/*Note that we create a copy of the passed token tok.*/
item_entry *create_item_entry(token *tok, int linenum) {
    item_entry *new_entry;
    
    new_entry = malloc(sizeof(item_entry));
    if (new_entry == NULL) {
        fprintf(stderr, "Malloc failure in create_ent_ext.");
        exit(1);
    }
    
    new_entry->tok = extract_token(tok);
    new_entry->linenum = linenum;
    
    return new_entry;
}

/*Finder for use with clist.*/
void *find_item_entry(void *item, char *str) {
    if (item == NULL) {
        return NULL;
    }
    
    if (strcmp(((item_entry*)item)->tok->tokstr, str) == 0) {
        return item;
    }
    
    return NULL;
}

/*Destroyer for use with clist.*/
void destroy_item_entry(void *item) {
    if (item == NULL) {
        return;
    }
    
    destroy_token(((item_entry*)item)->tok);
    free(item);
}

/*Note that we create a copy of the passed token tok.*/
item_extern *create_item_extern(token *tok, int linenum) {
    item_extern *new_item_extern;
    
    new_item_extern = malloc(sizeof(item_extern));
    if (new_item_extern == NULL) {
        fprintf(stderr, "Malloc failure in create_item_extern.");
        exit(1);
    }
    
    new_item_extern->tok      = extract_token(tok);
    new_item_extern->linenum  = linenum;
    new_item_extern->was_used = false;
    
    return new_item_extern;
}

/*Finder for use with clist.*/
void *find_item_extern(void *item, char *str) {
    if (item == NULL) {
        return NULL;
    }
    
    if (strcmp(((item_extern*)item)->tok->tokstr, str) == 0) {
        return item;
    }
    
    return NULL;
}

/*Destroyer for use with clist.*/
void destroy_item_extern(void *item) {
    if (item == NULL) {
        return;
    }
    
    destroy_token(((item_extern*)item)->tok);
    free(item);
}

/*Reads a line from p_file and writes at most length chars into *line.
  Returns line_EOF if the very first read char in the file is EOF. Returns
  line_ok if line was no longer than length chars. Returns line_too_long if
  we've read length+1 chars and the length+1 char was not EOF or \n. In this
  case we also advance the p_file pointer until \n or EOF.*/
line_ret get_line(FILE *p_file, int length, char *line) {
    int i;
    int ch;
    
    if (feof(p_file)) {
        return line_EOF;
    }
    
    /*the first length chars*/
    for (i = 0; i < length-1; i++) {
        ch = fgetc(p_file);
        if (ch == '\n') {
            return line_ok;
        } else if (ch == EOF) {
            return line_ok;
        }
        
        line[i] = ch;
    }
    
    /*so, what is the length+1 character?*/
    ch = fgetc(p_file);
    if (ch == '\n') {
        return line_ok;
    } else if (ch == EOF) {
        return line_ok;
    }
    
    /*fast forward until end of line or end of file*/
    while ((ch = fgetc(p_file)) != '\n' && ch != EOF) {
        ;;
    }
    
    return line_too_long;
}

/*Does literally what it says. Skips the initial whitespace and if the
  first char after that whitespace is ; or \0 or \n - returns true.
  Otherwise false is returned.*/
bool is_comment_or_empty_line(char *input) {
    while (*input == ' ' || *input == '\t') {
        input++;
    }
    
    if (*input == ';' || *input == '\0' || *input == '\n') {
        return true;
    }
    
    return false;
}

/*Prints an error that is relevant to the passed token tok.*/
void print_tok_error(token *tok, file_data *filedat, char *message) {
    int i = 0;
    char *line = filedat->current_line;
    
    ERRORS++;

    filedat->error = true;
    
    while (*line == ' ' || *line == '\t') {
        line++; i++;
    }
    
    fprintf(stderr, "\nLine %d: %s\n", filedat->linenum, message);
    while (*line != '\n' && *line != '\0') {
        if (*line == '\t') {
            fprintf(stderr, " ");
        } else {
            fprintf(stderr, "%c", *line);
        }
       line++;
    }
    fprintf(stderr, "\n");
    
    for (; i < tok->starting_index; i++) {
        fprintf(stderr, " ");
    }
    fprintf(stderr, "^");
    i++;
    for (; i < (tok->length)+(tok->starting_index); i++) {
        fprintf(stderr, "~");
    }

    fprintf(stderr, "\n");
}

/*DEBUG*/
void print_item_label(void *item) {
    item_label *p_label = (item_label*)item;
    
    if (item == NULL) {
        return;
    }
    
    printf("%s\tIC: %d\t| linenum: %d\t| stype: %d\n",
           p_label->tok->tokstr, p_label->IC, p_label->linenum,
           p_label->stype);
}

/*DEBUG*/
void print_item_entry(void *item) {
    item_entry *p_entry = item;
    
    if (item == NULL) {
        return;
    }
    
    printf("%d\t%s\n", p_entry->linenum, p_entry->tok->tokstr);
}

/*DEBUG*/
void print_item_extern(void *item) {
    item_extern *p_extern = item;
    
    if (item == NULL) {
        return;
    }
    
    printf("%d\t%s\tused: %d\n", p_extern->linenum, p_extern->tok->tokstr,
                                 p_extern->was_used);
}

