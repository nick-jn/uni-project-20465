/*The lexer module.*/

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "bool.h"
#include "token.h"
#include "clist.h"
#include "filedata.h"
#include "lexer.h"

extern unsigned int ERRORS;

static token *get_next_token(token *prev_token, file_data *filedat);
static char *skip_wspace(char *str);
static void print_errlex(int index, file_data *filedat, char *message);

/*Driver for the lexer module. Tokenizes the entire line. Upon lexer
  failure (i.e., get_next_token return NULL), the function returns NULL.
  Otherwise, filedat->last_token is populated with the acquired tokens.*/
void tokenize_line(file_data *filedat) {
    token *tok = NULL;
    
    while (!0) {
        tok = get_next_token(tok, filedat);
        if (tok == NULL) { /*lexer error*/
            destroy_clist(&filedat->last_token, &destroy_clist_token);
            return;
        }
        
        add_clist(&filedat->last_token, tok);
        
        if (tok->toktype == toktype_EOL) {
            break;
        }
    }
}

/*Token extractor. Returns NULL in case of severe lexing error (see
  the code). Upon reaching end of line, a special token with the type
  toktype_EOL is returned.*/
static token *get_next_token(token *prev_token, file_data *filedat) {
    int i, starting_index; /*the starting index of the new token*/
    int length = 0; /*the length of the new token*/
    char *input = filedat->current_line;
    bool got_string = false; /*flag for when we get a string literal*/
    token *next_token = NULL;
    
    /*get the starting index for the new token*/
    if (prev_token != NULL) {
        i = prev_token->starting_index + prev_token->length;
    } else {
        i = 0; /*start of the line*/
    }
    
    /*skip initial whitespace*/
    while (input[i] == ' ' || input[i] == '\t') {
        i++; /*we need the i here, so no skip_wspace*/
    }
    starting_index = i;
    
    /*essentially, it's a state machine*/
    while (input[i] != '\n' && input[i] != '\0') {
        /*dot*/
        if (input[i] == '.') {
            if (length > 0) {
                break; /*write token*/
            }
            
            length++;
            if (input[i+1] == ' ' || input[i+1] == '\t') {
                print_errlex(i, filedat, "Error, whitespace after '.'.");
            }
            break; /*write token*/
        /*whitespace*/
        } else if (input[i] == ' ' || input[i] == '\t') {
            /*look ahead for dot or colon*/
            if (*skip_wspace(&input[i]) == '.') {
                print_errlex(i, filedat,
                             "Error, '.' is preceded by whitespace.");
            }
            if (*skip_wspace(&input[i]) == ':') {
                print_errlex(i, filedat,
                             "Error, ':' is preceded by whitespace.");
            }
            break; /*write token*/
        /*hash, colon*/
        } else if (input[i] == '#' || input[i] == ':') {
            if (length > 0) {
                break; /*write token*/
            }
            
            length++;
            if (input[i] != ':' &&
                (input[i+1] == ' ' || input[i+1] == '\t')) {
                print_errlex(i, filedat,
                             "Error, erroneous whitespace.");
            }
            break; /*write token*/
        /*comma*/
        } else if (input[i] == ',') {
            if (length > 0) {
                break; /*write token*/
            }
            
            length++;
            break;
        /*string literal*/
        } else if (input[i] == '"') {
            if (length > 0) {
                print_errlex(i, filedat,
                             "Error, string literal has to be "
                             "delimited by whitespace.");
                return NULL;
            }
            
            i++; length++;
            while (input[i] != '"') {
                if (input[i] == '\n' || input[i] == '\0') {
                    print_errlex(i, filedat,
                                 "Error, missing terminating '\"' character.");
                    return NULL;
                }
                i++; length++;
            }
            length++;
            got_string = true;
            break; /*write token*/
        /*plus and minus signs*/
        } else if (input[i] == '+' || input[i] == '-') {
            if (length > 0) {
                print_errlex(i, filedat,
                             "Error, number literal has to be "
                             "delimited by whitespace.");
                return NULL;
            }
            
            if (input[i+1] == ' ' || input[i+1] == '\t') {
                print_errlex(i, filedat,
                             "Error, erroneous whitespace "
                             "between sign and number.");
                return NULL;
            } else if (!isdigit(input[i+1])) {
                print_errlex(i, filedat,
                             "Error, invalid number literal.");
                return NULL;
            }
        /*semicolon*/
        } else if (input[i] == ';') {
            print_errlex(i, filedat,
                         "Error, ';' must be placed at the end of the line.");
            return NULL;
        }
        
        length++; i++;
    }
    
    /*signifies end of line*/
    if (length == 0) {
        next_token = create_token(starting_index+1, 0, "\\n");
        next_token->toktype = toktype_EOL;
        
        return next_token;
    }
    
    /*create token*/
    next_token = create_token(starting_index, length, input);
    if (got_string) {
        next_token->toktype = toktype_string;
    } else {
        next_token->toktype = get_toktype(next_token);
    }
    
    #ifdef DEBUG_LEXER
        printf("TOKEN: %s\t%s\n",
               new_tokstr, toktype_strings[next_token->toktype]);
    #endif /*DEBUG*/
    
    return next_token;
}

/*Skips the whitespace chars in str and returns the pointer to the
  first non-whitespace char.*/
static char *skip_wspace(char *str) {
    while (*str == ' ' || *str == '\t') {
        str++;
    }
    
    return str;
}

/*Error printing for lexer.*/
static void print_errlex(int index, file_data *filedat, char *message) {
    int i = 0;
    char *line = filedat->current_line;
    
    filedat->error = true;
    
    ERRORS++;
    
    while (*line == ' ' || *line == '\t') {
        line++; i++;
    }
    
    fprintf(stderr, "\nLexer error.\nLine %d: %s\n",
            filedat->linenum, message);
    while (*line != '\n' && *line != '\0') {
        if (*line == '\t') {
            fprintf(stderr, " ");
        } else {
            fprintf(stderr, "%c", *line);
        }
       line++;
    }
    fprintf(stderr, "\n");
    
    /*fancy line*/
    for (; i < index; i++) {
        fprintf(stderr, " ");
    }
    fprintf(stderr, "^");
    i++;
    while (filedat->current_line[i] == ' ' ||
           filedat->current_line[i] == '\t') {
        fprintf(stderr, "~");
        i++;
    }

    fprintf(stderr, "\n");
}


