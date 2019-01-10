/*The parser module.*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "bool.h"
#include "clist.h"
#include "token.h"
#include "statement.h"
#include "filedata.h"
#include "tokstream.h"
#include "parser.h"

#define MAX_LABEL_LENGTH 30

/*used to check the limits of integers*/
typedef enum {
    numt_eightbit,
    numt_tenbit
} numtype;

#define MAX_RWORDS 31 /*reserved words*/

extern unsigned int ERRORS;

static char *addmode_strings[MAX_ADD_MODES] = {
    "IMM",
    "DIR",
    "STRUCT",
    "REG"
};

static void get_label(line_data *lindat, file_data *filedat);
static void *get_stat(line_data *lindat, file_data *filedat);

static stat_instr_t *get_stat_instr(file_data *filedat);
static stat_ddir_t *get_stat_ddir(file_data *filedat);

static ddir_data_t *get_ddir_data(file_data *filedat);
static ddir_struct_t *get_ddir_struct(file_data *filedat);
static token *get_ddir_entry(file_data *filedat);
static token *get_ddir_extern(file_data *filedat);

static operand_t *get_operand_next(file_data *filedat);
static operand_t *get_operand_imm(file_data *filedat);
static operand_t *get_operand_dir();
static operand_t *get_operand_struct(file_data *filedat);
static operand_t *get_operand_reg(file_data *filedat);

static bool expect_single(file_data *filedat, token_type toktype);
static bool expect(file_data *filedat, int numvar, token_type toktype, ...);
static bool stat_inst_proper_ending(int opcode, file_data *filedat);
static bool is_ident_length_ok(char *str);
static bool is_int_within_bounds(int num, numtype num_t);
static bool is_valid_label(c_list *last_label, file_data *filedat, token *tok);

static void print_operand_error(int starting_index, int length,
                         file_data *filedat, char *message);
static void print_prevdef(int linenum);
static void print_valid_addmodes(const int *valid_modes);



/*Driver for the entire parsing routine. The goal is to parse the whole
  line and to acquire all the relevant information - statement type,
  operator, operands, etc..*/
void *parse_line(line_data *lindat, file_data *filedat) {
    void *statement = NULL;
    
    #ifdef DEBUG_PARSER
        puts("___parse_line___");
    #endif
    
    init_tokstream(filedat->last_token);
    
    #ifdef DEBUG_PARSER
        print_clist(*last_token, &print_clist_token);
        printf("\n");
    #endif
    
    /*label*/
    get_label(lindat, filedat);
    if (lindat->label_error) { /*no point in continuing*/
        lindat->label_token = NULL;
        return NULL;
    }
    
    /*statement*/
    statement = get_stat(lindat, filedat);
    
    #ifdef DEBUG_PARSER
        putchar('\n');
        print_statement(statement, lindat->stype);
        putchar('\n');
    #endif /*DEBUG*/
    
    return statement; /*success*/
}

/*Tries to acquire the label from tokstream. Label is defined as
  (token), (colon). If the token's type is not toktype_identifier,
  the label is classified as invalid. The first token (i.e., the one
  before the colon) is stored in filedat, as is the information
  concerning the label's validity (as valid_label).
  
  Identifier token that isn't followed by a colon results in a parsing
  error, and lindat->label_error is set to true.*/
static void get_label(line_data *lindat, file_data *filedat) {
    bool valid_label = false;
    #ifdef DEBUG_PARSER
        puts("___get_label___");
    #endif /*DEBUG*/
    
    tstream_savepos();
   
    /*in case of .entry or .extern label may be invalid*/
    if (probe_toktype(toktype_identifier)) {
        valid_label = true;
        advance_tokstream();
    }
    
    /*could be a single operator or datadir, let the get_stat
      function deal with it*/
    if (is_EOL_token()) { 
        tstream_loadpos();
        return;
    /*but it also could be an invalid label if the next token is a colon*/
    } else {
        if (valid_label == false) {
        /*remember that the tokstream got advanced only
          if the first token was a valid identifier*/
            advance_tokstream();
        }
    }
    
    /*success, create label*/
    if (probe_toktype(toktype_colon)) {
        lindat->label_token = get_prev_token();
        lindat->valid_label = valid_label;
        
        advance_tokstream();
        #ifdef DEBUG_PARSER
            puts("LABEL ACQUIRED");
        #endif /*DEBUG*/
    } else {
        if (valid_label == true) {
            lindat->label_error = true;
            print_tok_error(get_prev_token(), filedat, 
                        "Error, expected label definition "
                        "or operator or data directive.");
        } else {
            /*statement has no label*/
            tstream_loadpos();
        }
    }
}

/*Attempts to acquire the statement.*/
static void *get_stat(line_data *lindat, file_data *filedat) {
    void *statement = NULL;
    
    #ifdef DEBUG_PARSER
        puts("\n___get_stat___");
    #endif /*DEBUG*/
    
    if (is_EOL_token()) {
        print_tok_error(get_cur_token(), filedat,
                    "Error, statement has no operator or data directive.");
    /*data statement*/
    } else if (probe_toktype(toktype_dot)) {
        advance_tokstream();
        
        if (expect_single(filedat, toktype_datadir)) {
            /*not entry or extern? better check the label*/
            if (get_cur_token()->toktype != toktype_datadir_entry &&
                get_cur_token()->toktype != toktype_datadir_extern) {
                /*if we have one, that is*/
                if (lindat->label_token != NULL) {
                    if (!is_valid_label(filedat->last_label, filedat,
                                        lindat->label_token)) {
                        return NULL;
                    /*a more generic error, triggers when toktype
                      of the label is unknown*/
                    } else if (lindat->valid_label == false) {
                        print_tok_error(lindat->label_token, filedat,
                                    "Error, invalid label.");
                        return NULL;
                    }
                }
            /*otherwise completely ignore the label*/
            } else {
                lindat->label_token = NULL;
            }
            
            statement = get_stat_ddir(filedat);
            if (statement != NULL) {
                lindat->stype = stype_datadir;
            }
        }
    /*instruction statement*/
    } else if (probe_toktype(toktype_operator)) {
        /*check the label*/
        if (lindat->label_token != NULL) {
            if (!is_valid_label(filedat->last_label, filedat,
                                lindat->label_token)) {
                return NULL;
            /*a more generic error, triggers when toktype
              of the label is unknown*/
            } else if (lindat->valid_label == false) {
                print_tok_error(lindat->label_token, filedat,
                            "Error, invalid label.");
                return NULL;
            }
        }
        
        statement = get_stat_instr(filedat);
        if (statement != NULL) {
            lindat->stype = stype_instruction;
        }
    /*invalid statement*/
    } else {
        print_tok_error(get_cur_token(), filedat,
                        "Error, expected operator or data directive.");
    }
    
    #ifdef DEBUG_PARSER
        puts("---END OF GET STATEMENT---");
    #endif /*DEBUG*/
    
    return statement;
}

/*Instruction statement*/
static stat_instr_t *get_stat_instr(file_data *filedat) {
    int i;
    int opcode = get_cur_token()->toktype-toktype_operator_mov;
    /*flag for when there's a problem with the operand*/
    bool operand_error = false;
    token *operator_token = get_cur_token();
    
    stat_instr_t *stat_inst = NULL; /*the return*/
    operand_t *operand_src  = NULL;
    operand_t *operand_dst  = NULL;
    
    const int *allowed_addmodes; /*will point to the relevant table in OPS*/
    operand_t **target_opd; /*will point at operand_src or dst*/
    
    advance_tokstream();
    
    #ifdef DEBUG_PARSER
        puts("\n   --- get_stat_instr ---");
        printf("get_stat_instr init feed: %s\n",
               get_cur_token()->tokstr);
    #endif /*DEBUG*/
    
    /*do we have any tokens after the operator?*/
    if (is_EOL_token()) {
        if (opcode != OP_RTS && opcode != OP_STOP) {
            print_tok_error(operator_token, filedat,
                        "Error, operator has no operands.");
            return NULL;
        }
    } else { /*let's deal with rts and stop here*/
        if (opcode == OP_RTS || opcode == OP_STOP) {
            print_tok_error(get_cur_token(), filedat,
                        "Error, extraneous token. "
                        "Operator expects zero operands.");
            return NULL;
        }
    }
    
    /*set our target operand and permitted addressing modes
      for the given operand and operator*/
    if (OPS[opcode].opds == 2) {
        target_opd = &operand_src;
        allowed_addmodes = OPS[opcode].src_mode;
    } else {
        target_opd = &operand_dst;
        allowed_addmodes = OPS[opcode].dst_mode;
    }
    
    /*acquire the operands, check if there are *too few* operands,
      check if addressing modes are valid*/
    for (i = 0; i < OPS[opcode].opds; i++) {
        if (is_EOL_token()) {
            print_operand_error(operator_token->starting_index,
                                (get_cur_token()->starting_index -
                                 operator_token->starting_index)-1,
                                filedat,
                                "Error, not enough operands.");
            fprintf(stderr,
                    "The number of operands that %s accepts is %d.\n",
                    get_toktype_string(opcode+toktype_operator_mov),
                    OPS[opcode].opds);
            operand_error = true;
            break;
        }
        
        *target_opd = get_operand_next(filedat);
        if (*target_opd == NULL) {
            operand_error = true;
        } else if (allowed_addmodes[(*target_opd)->addmode] != 1) {
            print_operand_error((*target_opd)->starting_index,
                                (*target_opd)->length,
                                filedat,
                                "Error, invalid addressing mode.");
            print_valid_addmodes(allowed_addmodes);
            destroy_operand(*target_opd);
            *target_opd = NULL;
        }
        
        /*change to dest operand; a bit ugly, but since we're
          only working with src and dst, it's not too bad*/
        target_opd = &operand_dst;
        allowed_addmodes = OPS[opcode].dst_mode;
    }
    
    /*tokstream has to be at EOL now for success*/
    if (i == OPS[opcode].opds && /*won't complain about extraneous comma*/
        stat_inst_proper_ending(opcode, filedat) && 
        operand_error == false) {
        /*success, create instruction statement*/
        stat_inst = create_stat_inst(opcode, operand_src, operand_dst);
    } else { /*cleanup*/
        destroy_operand(operand_src);
        destroy_operand(operand_dst);
    }

    #ifdef DEBUG_PARSER
        puts("--- end of get_instr_statement---");
    #endif /*DEBUG*/
    
    return stat_inst;
}

/*Helper function for get_stat_instr, determines if the statement's
  ending is correct.*/
static bool stat_inst_proper_ending(int opcode, file_data *filedat) {
     if (probe_prev_toktype(toktype_comma)) {
        if (!is_EOL_token()) {
            print_operand_error(get_prev_token()->starting_index,
                                (get_cur_token()->starting_index -
                                 get_prev_token()->starting_index +
                                 get_cur_token()->length),
                                filedat,
                                "Error, erroneous attempt "
                                "at operand assignment.");
            fprintf(stderr,
                    "The number of operands that %s accepts is %d.\n",
                    get_toktype_string(opcode+toktype_operator_mov),
                    OPS[opcode].opds);
        } else {
            print_operand_error(get_prev_token()->starting_index, 0,
                                filedat,
                                "Error, extraneous comma at "
                                "the end of the statement.");
        }
        
        return false;
    } else if (!is_EOL_token()) {
        print_operand_error(get_prev_token()->starting_index,
                            get_cur_token()->starting_index +
                            get_cur_token()->length,
                            filedat,
                            "Error, extraneous token at "
                            "the end of the statement.");
        
        return false;
    }
    
    return true;
}

/*Acquires the immediate operand.*/
static operand_t *get_operand_imm(file_data *filedat) {
    int length, starting_index, num;
    operand_t *operand = NULL;
    operand_data *opd_data = NULL;
    
    starting_index = get_cur_token()->starting_index;
    length = get_cur_token()->length;
    
    advance_tokstream(); /*we know that we got # as the first token*/
    length += get_cur_token()->length;
    
    if (probe_toktype(toktype_number)) {
        num = atoi(get_cur_token()->tokstr);
        
        if (!is_int_within_bounds(num, numt_eightbit)) {
            /*offset of 1 for the # operator*/
            print_operand_error(starting_index+1, length-1, filedat,
                                "Error, number out of bounds.");
            fprintf(stderr, "Expected bounds (inclusive): "
                             "%d, %d.\n", EIGHTBIT_MIN, EIGHTBIT_MAX);
        } else {
            /*get the complement*/
            if (num < 0) {
                /*plus 512*/
                num += (EIGHTBIT_MAX+1)*2;
            }
            
            opd_data = malloc(sizeof(operand_data));
            if (opd_data == NULL) {
                fprintf(stderr, "Malloc failure in get_operand_imm.");
                exit(1);
            }
            
            opd_data->number = num;
            
            operand = create_operand(starting_index, length,
                                     addmode_imm, opd_data);
        }
    } else {
        print_operand_error(starting_index, length, filedat, 
                            "Error, # operator expects a number.");
    }
    
    advance_tokstream();
    
    return operand;
}

/*Acquires the direct operand.*/
static operand_t *get_operand_dir() {
    int length, starting_index;
    operand_data *opd_data;
    operand_t *operand = NULL;
    
    starting_index = get_cur_token()->starting_index;
    length = get_cur_token()->length;
    
    opd_data = malloc(sizeof(operand_data));
    if (opd_data == NULL) {
        fprintf(stderr, "Malloc failure in get_operand_dir.");
        exit(1);
    }
    
    opd_data->identifier = get_cur_token();
    
    operand = create_operand(starting_index, length,
                             addmode_dir, opd_data);
                             
    advance_tokstream();
    
    return operand;
}

/*Acquires the struct operand. It assumes that the first two tokens in
  the tokstream are known to be of the type identifier and dot.*/
static operand_t *get_operand_struct(file_data *filedat) {
    int length, starting_index;
    int struct_field;
    token *ident_token = get_cur_token();
    operand_t *operand      = NULL;
    struct_opd_t *structure = NULL;
    operand_data *opd_data  = NULL;
    
    starting_index = get_cur_token()->starting_index;
    length = get_cur_token()->length+1; /*one extra for the dot*/
    
    advance_tokstream(); /*this one is the dot*/
    advance_tokstream(); /*and now this one should be a number*/
    length += get_cur_token()->length;
    
    if (probe_toktype(toktype_number)) {
        struct_field = atoi(get_cur_token()->tokstr);
        if (struct_field != 1 && struct_field != 2) {
            print_operand_error(starting_index, length, filedat,
                            "Error, struct field must be 1 or 2.");
        } else {
            opd_data = malloc(sizeof(operand_data));
            structure = malloc(sizeof(struct_opd_t));
            if (opd_data == NULL || structure == NULL) {
                fprintf(stderr, "Malloc failure");
                exit(1);
            }
            
            structure->field = struct_field;
            structure->identifier = ident_token;
            
            opd_data->structure = structure;
            
            operand = create_operand(starting_index, length,
                                     addmode_struct, opd_data);
        }
    } else {
        print_operand_error(starting_index, length, filedat,
                            "Error, invalid struct field access.");
    }

    advance_tokstream();
    
    return operand;
}

/*Acquires the register operand.*/
static operand_t *get_operand_reg(file_data *filedat) {
    int length, starting_index;
    operand_t *operand     = NULL;
    operand_data *opd_data = NULL;
    
    starting_index = get_cur_token()->starting_index;
    length = get_cur_token()->length;
    
    if (get_cur_token()->toktype == toktype_register_8 ||
        get_cur_token()->toktype == toktype_register_9) {
        print_operand_error(starting_index, length, filedat,
                            "Error, invalid register. Valid registers are "
                            "0 through 7 (inclusive).");
    } else {
        opd_data = malloc(sizeof(operand_data));
        if (opd_data == NULL) {
            fprintf(stderr, "Malloc failure");
            exit(1);
        }
        
        opd_data->reg_num = (get_cur_token()->toktype)-toktype_register_0;
        operand = create_operand(starting_index, length,
                                 addmode_reg, opd_data);  
    }
    
    advance_tokstream();
    
    return operand;
}

/*Acquires an operand. This function will call the appropriate function
  that deals with the relevant operand type (immediate, direct, struct,
  register).*/
static operand_t *get_operand_next(file_data *filedat) {
    operand_t *operand = NULL;
    
    #ifdef DEBUG_PARSER
        puts("\n\n   --- get_operand_next ---");
        printf("GNO init feed: %s\n", get_cur_token()->tokstr);
    #endif /*DEBUG*/
    
    tstream_savepos(); /*for struct operand*/
    
    /*immediate operand*/
    if (probe_toktype(toktype_hash)) {
        operand = get_operand_imm(filedat);
    } else if (probe_toktype(toktype_identifier)) {
        /*is the identifier's length ok?*/
        if (is_ident_length_ok(get_cur_token()->tokstr)) {
            advance_tokstream();
            /*struct operand*/
            if (probe_toktype(toktype_dot)) {
                tstream_loadpos();
                operand = get_operand_struct(filedat);
            /*direct operand*/
            } else {
                tstream_loadpos();
                /*filedat not needed for errors*/
                operand = get_operand_dir();
            }
        } else {
            print_tok_error(get_cur_token(), filedat,
                            "Error, identifier is too long.");
            fprintf(stderr, "Max. identifier length allowed: %d.\n",
                    MAX_LABEL_LENGTH);
            advance_tokstream();
        }
    /*register operand*/
    } else if (probe_toktype(toktype_operand_register)) {
        operand = get_operand_reg(filedat);
    /*invalid operand*/
    } else {
        print_tok_error(get_cur_token(), filedat, "Error, invalid operand.");
        advance_tokstream();
    }
    
    /*check if operand ends in comma or end of line*/
    if (!probe_toktype(toktype_EOL) && !probe_toktype(toktype_comma)) {
        print_operand_error(get_cur_token()->starting_index,
                            get_cur_token()->length,
                            filedat,
                            "Error, erroneous operand delimiter. "
                            "Expected comma or end of line.");
        destroy_operand(operand);
        operand = NULL;
        
        /*skip until next comma or eol*/
        while (!probe_toktype(toktype_EOL) && !probe_toktype(toktype_comma)) {
            advance_tokstream();
        }
    }
    
    advance_tokstream();
    
    #ifdef DEBUG_PARSER
        puts("   --- get_operand_next END ---");
    #endif
    
    return operand;
}

/*Acquires the data directive statement.*/
static stat_ddir_t *get_stat_ddir(file_data *filedat) {
    stat_ddir_t *stat_data = NULL;
    /*since toktype_datadir_data is the first datadir toktype, this will
      get in the scope of enum data_dir*/
    data_dir datadir = get_cur_token()->toktype-toktype_datadir_data;
    void *data = NULL; /*for create_stat_ddir*/
    
    advance_tokstream();

    #ifdef DEBUG_PARSER
        puts("\n\t---get_stat_ddir---");
        printf("FEED: %s\n", get_cur_token()->tokstr);
    #endif /*DEBUG*/
    
    /*data*/
    if (datadir == datadir_data) {
        data = get_ddir_data(filedat);
    /*struct*/
    } else if (datadir == datadir_struct) { 
        data = get_ddir_struct(filedat);
    } else {
        tstream_savepos();
        
        /*string*/
        if (datadir == datadir_string) { 
            if (!expect(filedat, 2, toktype_string, toktype_EOL)) {
                return NULL;
            }
            
            tstream_loadpos();
            
            data = malloc(sizeof(char)*(strlen(get_cur_token()->tokstr))+1);
            if (data == NULL) {
                fprintf(stderr, "Malloc failure in get_stat_ddir.");
                exit(1);
            }
        
            strcpy(data, get_cur_token()->tokstr);
        /*entry, extern*/
        } else {
            if (!expect(filedat, 2, toktype_identifier, toktype_EOL)) {
                return NULL;
            }
            
            tstream_loadpos();
            
            if (is_ident_length_ok(get_cur_token()->tokstr)) {
                if (datadir == datadir_entry) { /*entry*/
                    data = get_ddir_entry(filedat);
                } else if (datadir == datadir_extern) { /*extern*/
                    data = get_ddir_extern(filedat);
                }
            } else {
                print_tok_error(get_cur_token(), filedat,
                                "Error, erroneous token length.");
                fprintf(stderr, "Max. length allowed: %d.\n",
                        MAX_LABEL_LENGTH);
            }
        }
    }
    
    if (data != NULL) {
        stat_data = create_stat_ddir(datadir, data);
    }

    #ifdef DEBUG_PARSER
        puts("\n\t@@@ get_stat_ddir END @@@");
    #endif /*DEBUG*/
    
    return stat_data;
}

/*Acquires the data from the .data statement.*/
static ddir_data_t *get_ddir_data(file_data *filedat) {
    int buffer;
    int count = 0;
    unsigned int *new_num; /*to be put in data_list*/
    c_list *data_list = NULL; /*the list with the numbers*/
    
    /*trickier and messier if we put a condition*/
    while (!0) {
        if (!expect_single(filedat, toktype_number)) {
            destroy_clist(&data_list, &free);
            return NULL;
        /*acquire the number, add to the list*/
        } else {
            buffer = atoi(get_cur_token()->tokstr);
            count++;
            if (!is_int_within_bounds(buffer, numt_tenbit)) {
                print_tok_error(get_cur_token(), filedat,
                            "Error, number out of bounds.");
                fprintf(stderr, "Expected bounds (inclusive): "
                                "%d, %d.\n", TENBIT_MIN, TENBIT_MAX);
                destroy_clist(&data_list, &free);
                return NULL;
            }
            
            /*get 2s complement*/
            if (buffer < 0) {
                buffer = buffer + (TENBIT_MAX+1)*2;
            }
            
            new_num = malloc(sizeof(unsigned int));
            if (new_num == NULL) {
                fprintf(stderr, "Malloc failure in get_ddir_data");
                exit(1);
            }
            
            /*is alright since buffer will be > 0*/
            *new_num = buffer;
            add_clist(&data_list, new_num);
        }
        
        advance_tokstream();
        
        if (is_EOL_token()) {
            break; /*finished, this exits out of the loop*/
        } else if (probe_toktype(toktype_comma)) {
            advance_tokstream();
            if (is_EOL_token()) {
                print_tok_error(get_prev_token(), filedat,
                            "Error, erroneous comma at the "
                            "end of a data statement.");
                destroy_clist(&data_list, &free);
                return NULL;
            }
        } else {
            print_tok_error(get_cur_token(), filedat,
                            "Error, data entry is comma "
                            "delimited and accepts only number literals.");
            destroy_clist(&data_list, &free);
            return NULL;
        }
    }
    
    return create_ddir_data(count, data_list);
}

/*Acquires the struct information from the .struct statement.*/
static ddir_struct_t *get_ddir_struct(file_data *filedat) {
    int num;      /*the struct's field number*/
    char *string; /*the struct's identifier*/
    
    tstream_savepos();
    
    if (!expect(filedat, 4, toktype_number, toktype_comma,
                            toktype_string, toktype_EOL)) {
        return NULL;
    }
    
    tstream_loadpos();
    
    num = atoi(get_cur_token()->tokstr);
    
    /*check bounds*/
    if (!is_int_within_bounds(num, numt_tenbit)) {
        print_tok_error(get_cur_token(), filedat,
                    "Error, number out of bounds.");
        fprintf(stderr, "Expected bounds (inclusive): "
                        "%d, %d.\n", TENBIT_MIN, TENBIT_MAX);
        return NULL;
    }
            
    /*get 2s complement*/
    if (num < 0) {
        num = num + (TENBIT_MAX+1)*2;
    }
    
    advance_tokstream(); /*at comma*/
    advance_tokstream(); /*at string literal*/
    
    string = get_cur_token()->tokstr;
    
    return create_ddir_struct(num, string);
}

/*Acquires the .entry directive.*/
static token *get_ddir_entry(file_data *filedat) {
    bool error = filedat->error;
    item_entry *p_entry = NULL;
    item_extern *p_extern = NULL;
    
    /*entry list lookup for muldef*/
    if ((p_entry = find_clist_str(filedat->last_entry,
                                   &find_item_entry,
                                   get_cur_token()->tokstr)) != NULL) {
        print_tok_error(get_cur_token(), filedat,
                        "Warning, multiple definitions of entry.");
        print_prevdef(p_entry->linenum);
        filedat->error = error;
    /*extern list lookup*/
    } else if ((p_extern = find_clist_str(filedat->last_extern,
                                          &find_item_extern,
                                          get_cur_token()->tokstr)) != NULL) {
        print_tok_error(get_cur_token(), filedat,
                        "Warning, previously defined as extern.");
        print_prevdef(p_extern->linenum);
    }
    
    if (p_entry == NULL && p_extern == NULL) {
        return get_cur_token();
    } else {
        return NULL;
    }
}

/*Acquires the .extern directive.*/
static token *get_ddir_extern(file_data *filedat) {
    bool error = filedat->error; /*for warnings*/
    item_label *p_label = NULL;
    item_entry *p_entry = NULL;
    item_extern *p_extern = NULL;
    
    /*extern list lookup for muldef*/
    if ((p_extern = find_clist_str(filedat->last_extern,
                                   &find_item_extern,
                                   get_cur_token()->tokstr)) != NULL) {
        print_tok_error(get_cur_token(), filedat,
                        "Warning, multiple definitions of extern.");
        print_prevdef(p_extern->linenum);
        filedat->error = error;
    /*entry list lookup*/
    } else if ((p_entry = find_clist_str(filedat->last_entry,
                                          &find_item_entry,
                                          get_cur_token()->tokstr)) != NULL) {
        print_tok_error(get_cur_token(), filedat,
                        "Error, previously defined as extern.");
        print_prevdef(p_entry->linenum);
    /*label list lookup*/
    } else if ((p_label = find_clist_str(filedat->last_label,
                                         &find_item_label,
                                         get_cur_token()->tokstr)) != NULL) {
        print_tok_error(get_cur_token(), filedat,
                        "Error, previously defined as label.");
        print_prevdef(p_label->linenum);
    }
    
    /*if none of the errors were triggered, that is*/
    if (p_entry == NULL && p_label == NULL && p_extern == NULL) {
        return get_cur_token();
    } else {
        return NULL;
    }
}

/*Expects a certain toktype from the current tokstream token. If the
  current tokstream token is not of the downcasted toktype, an error is
  printed and false is returned.
  
  Note that this function does not advance the tokstream.*/
static bool expect_single(file_data *filedat, token_type toktype) {
    if (!probe_toktype(toktype)) {
        print_tok_error(get_cur_token(), filedat,
                    "Error, unexpected token.");
        fprintf(stderr, "Expected %s.\n", get_toktype_string(toktype));
        
        return false;
    }
    
    return true;
}

/*Expects a sequence of consecutive tokens in tokstream to conform to
  the passed va_list of downcasted toktypes. Tokstream is advanced with
  each token. Upon discovering an unexpected token, we print an error
  and return false.
  
  Note that in case of an error, tokstream remains at the offending
  token. In case of success, tokstream advances to next token.*/
static bool expect(file_data *filedat, int numvar, token_type toktype, ...) {
    int i;
    va_list args;
    
    va_start(args, toktype);
    for (i = 0; i < numvar; i++) {
       if (!probe_toktype(toktype)) {
            print_tok_error(get_cur_token(), filedat,
                    "Error, unexpected token.");
            fprintf(stderr, "Expected %s.\n", get_toktype_string(toktype));
            
            va_end(args);
            return false;
       }
       
       advance_tokstream();
       toktype = va_arg(args, token_type);
    }
    
    va_end(args);
    
    return true;
}

/*Prints previous definition at Line linenum to stderr.*/
static void print_prevdef(int linenum) {
    fprintf(stderr, "Previously defined at Line %d.\n", linenum);
}

/*Prints an error that is relevant to the operand. The starting_index and
  length determine the location and the length of the fancy line.*/
static void print_operand_error(int starting_index, int length,
                         file_data *filedat, char *message) {
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
    
    /*fancy line*/
    for (; i < starting_index; i++) {
        fprintf(stderr, " ");
    }
    fprintf(stderr, "^");
    i++;
    for (; i < (length+starting_index); i++) {
        fprintf(stderr, "~");
    }

    fprintf(stderr, "\n");
}

/*If the length of str is strictly greater than MAX_LABEL_LENGTH,
  true is returned. Otherwise, false is returned.*/
static bool is_ident_length_ok(char *str) {
    if (strlen(str) > MAX_LABEL_LENGTH) {
        return false;
    }
    
    return true;
}

/*Checks if the label is valid. Prints error if not, and returns false.
  Otherwise returns true.
  
  The pointer last_label isn't really required since it's in filedat,
  but it's consistent with other functions in the project.*/
static bool is_valid_label(c_list *last_label, file_data *filedat,
                           token *tok) {
    item_label *label;
    item_extern *p_extern;
    
    /*reserved word check*/
    if (downcast_toktype(tok->toktype) == toktype_operator ||
        downcast_toktype(tok->toktype) == toktype_operand_register ||
        downcast_toktype(tok->toktype) == toktype_datadir) {
        print_tok_error(tok, filedat, "Error, this token is a reserved word.");
        
        return false;
    /*length check*/
    } else if (strlen(tok->tokstr) > MAX_LABEL_LENGTH) {
        print_tok_error(tok, filedat, "Error, label is too long.");
        fprintf(stderr, "Max. allowed label length is %d.\n",
                MAX_LABEL_LENGTH);
        
        return false;
    /*label list lookup*/
    } else if ((label = find_clist_str(last_label, &find_item_label,
                                       tok->tokstr)) != NULL) {
        print_tok_error(tok, filedat, "Error, multiple definitions of label.");
        print_prevdef(label->linenum);
        
        return false;
    /*extern list lookup*/
    } else if ((p_extern = 
                find_clist_str(filedat->last_extern, &find_item_extern,
                               tok->tokstr)) != NULL) {
        print_tok_error(tok, filedat,
                        "Error, previously defined as extern.");
        print_prevdef(p_extern->linenum);
        return false;
    }
    
    return true;
}

/*Check the boundaries of the passed int num of the type numtype. Boundaries
  are defined as macros.*/
static bool is_int_within_bounds(int num, numtype num_t) {
    switch (num_t) {
        case numt_eightbit:
            if (num > EIGHTBIT_MAX ||
                num < EIGHTBIT_MIN) {
                return false;
            }
            break;
        case numt_tenbit:
            if (num > TENBIT_MAX ||
                num < TENBIT_MIN) {
                return false;
            }
            break;
    }
    
    return true;
}

/*Prints the valid addressing modes that are looked up in the passed
  pointer to OPS' relevant table.*/
static void print_valid_addmodes(const int *valid_modes) {
    int i;
    
    fprintf(stderr,
            "Valid addressing modes for this operand are:\n");
            
    for (i = 0; i < MAX_ADD_MODES; i++) {
        if (valid_modes[i] == 1) {
            fprintf(stderr, "%s ", addmode_strings[i]);
        }
    }
    
    fprintf(stderr, "\n");
}
