/*The assembler module.*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bool.h"
#include "token.h"
#include "clist.h"
#include "statement.h"
#include "filedata.h"
#include "assm.h"
#include "parser.h"

#define EXTENSION_OB  ".ob"
#define EXTENSION_ENT ".ent"
#define EXTENSION_EXT ".ext"

#define MAX_OPDS 2       /*max operands for an operator*/
#define BASE_32_COUNT 32 /*for weird_base array*/

#define WORD_SIZE 10        /*the word size of the machine*/
#define TABSTOP "    "      /*whitespace between tokens in the final output*/
#define STRING_TERMINATOR 0 /*for .string data*/


static void add_label(line_data *lindat, file_data *filedat);
static void assm_stat_instr(assm_t *assm, stat_instr_t *stat,
                            file_data *filedat);
static void assm_stat_data(assm_t *assm, stat_ddir_t *stat,
                           file_data *filedat);

static void destroy_item_undefid(void *undefid);
static item_undefid *create_item_undefid(int IC, int linenum, token *tok);
static void add_bincode(c_list **binc_list, unsigned int bincode,
                        unsigned int *increment);
static void assm_stat_instr_opds(assm_t *assm, stat_instr_t *stat,
                                 file_data *filedat);
static void assm_opd_ident(assm_t *assm, file_data *filedat, token *ident);
static item_label *get_instr_label(c_list *last_label, char *str);

extern unsigned int ERRORS; /*for debugging*/

char weird_base[BASE_32_COUNT] = {
    /*0*/  '!',
    /*1*/  '@',
    /*2*/  '#',
    /*3*/  '$',
    /*4*/  '%',
    /*5*/  '^',
    /*6*/  '&',
    /*7*/  '*',
    /*8*/  '<',
    /*9*/  '>',
    /*10*/ 'a',
    /*11*/ 'b',
    /*12*/ 'c',
    /*13*/ 'd',
    /*14*/ 'e',
    /*15*/ 'f',
    /*16*/ 'g',
    /*17*/ 'h',
    /*18*/ 'i',
    /*19*/ 'j',
    /*20*/ 'k',
    /*21*/ 'l',
    /*22*/ 'm',
    /*23*/ 'n',
    /*24*/ 'o',
    /*25*/ 'p',
    /*26*/ 'q',
    /*27*/ 'r',
    /*28*/ 's',
    /*29*/ 't',
    /*30*/ 'u',
    /*31*/ 'v'
};

/*Driver for the assembly stage.*/
void assemble_line(assm_t *assm, void *stat,
                   line_data *lindat, file_data *filedat) {
    if (stat == NULL) {
        return;
    }
    
    add_label(lindat, filedat);
    
    if (lindat->stype == stype_instruction) {
        /*write instruction code*/
        assm_stat_instr(assm, stat, filedat);
    } else if (lindat->stype == stype_datadir) {
        /*write data code*/
        assm_stat_data(assm, stat, filedat);
    }
}

/*Creates a new item_undefid. Note that a new token is created.*/
item_undefid *create_item_undefid(int IC, int linenum, token *tok) {
    item_undefid *new_undefid = malloc(sizeof(item_undefid));
    
    if (new_undefid == NULL) {
        fprintf(stderr, "Malloc failure in create_undefid.");
        exit(1);
    }
    
    new_undefid->IC      = IC;
    new_undefid->linenum = linenum;
    new_undefid->tok     = extract_token(tok);
    
    return new_undefid;
}

/*Creates a new item_out_ent_ext. Note that a new str is created.*/
item_out_ent_ext *create_item_out_ent_ext(int address, char *str) {
    char *new_str = malloc(sizeof(char) * (strlen(str)+1));
    item_out_ent_ext *new_out_ent_ext = malloc(sizeof(item_out_ent_ext));
    
    if (new_out_ent_ext == NULL || new_str == NULL) {
        fprintf(stderr, "Malloc failure in create_item_out_ent_ext.");
        exit(1);
    }
    
    strcpy(new_str, str);
    
    new_out_ent_ext->address = address;
    new_out_ent_ext->str     = new_str;
    
    return new_out_ent_ext;
}

/*Destroyer for use with c_list.*/
void destroy_item_undefid(void *undefid) {
    if (undefid == NULL) {
        return;
    }
    
    destroy_token(((item_undefid*)undefid)->tok);
    free(undefid);
}

/*Destroyer for use with c_list.*/
void destroy_item_out_ent_ext(void *item) {
    if (item == NULL) {
        return;
    }
    
    free(((item_out_ent_ext*)item)->str);
    free(item);
}

/*Wrapper for adding binary codes to instruction and data lists. Increment
  recieves either IC or DC from filedat and increments it.*/
static void add_bincode(c_list **binc_list, unsigned int bincode,
                        unsigned int *increment) {
    unsigned int *new_bincode = malloc(sizeof(unsigned int));
    
    if (new_bincode == NULL) {
        fprintf(stderr, "Malloc failure in add_bincode.");
        exit(1);
    }
    
    *new_bincode = bincode;
    
    add_clist(binc_list, new_bincode);
    
    (*increment)++;
}

/*Assembles the instruction statement.*/
void assm_stat_instr(assm_t *assm, stat_instr_t *stat, file_data *filedat) {   
    int i;
    unsigned int inst = 0; /*the machine code instruction*/
    /*the appropriate amount of bitshift for the current operand*/
    int cur_opd_shift = SHIFT_SRC;
    int reg_opd_count = 0; /*used for the case of two reg addmodes*/
    operand_t *target_opd; /*points at the current operand*/

    /*instruction*/
    target_opd = stat->operand_src;
    inst += (stat->opcode << SHIFT_OPC);
    for (i = 0; i < MAX_OPDS; i++) {
        if (target_opd != NULL) {
            inst += (target_opd->addmode << cur_opd_shift);
            if (target_opd->addmode == addmode_reg) {
                reg_opd_count++;
            }
        }
        
        target_opd = stat->operand_dst;
        cur_opd_shift = SHIFT_DST;
    }
    
    add_bincode(&assm->last_instr, inst, &filedat->IC);
    
    /*special case - two reg operands*/
    if (reg_opd_count == 2) {
        inst = 0;
        inst += (stat->operand_src->data->reg_num << SHIFT_REG1);
        inst += (stat->operand_dst->data->reg_num << SHIFT_REG2);
        
        add_bincode(&assm->last_instr, inst, &filedat->IC);
    /*assemble the operand codes*/
    } else {
        assm_stat_instr_opds(assm, stat, filedat);
    }
}

/*Assembles the codes for the operands of the instruction statement.*/
static void assm_stat_instr_opds(assm_t *assm, stat_instr_t *stat,
                                 file_data *filedat) {
    int i;
    int cur_reg_shift = SHIFT_REG1; /*applies the appropriate REG offset*/
    operand_t *target_opd; /*targets the current operand*/
    
    /*Goes through the two operands (if an operand is null, we skip)
      and adds the relevant codes to assm->last_instr. For identifiers,
      if an identifier is a known label to an instruction statement,
      we add the code (since we know for sure that its IC is final). If
      not, we add it to assm->last_undefid to be dealt with during the
      second pass.*/
    target_opd = stat->operand_src;
    for (i = 0; i < MAX_OPDS; i++) {
        if (target_opd != NULL) {
            switch (target_opd->addmode) {
                /*immidiate*/
                case addmode_imm:
                    add_bincode(&assm->last_instr,
                                target_opd->data->number << SHIFT_8BIT,
                                &filedat->IC);
                    break;
                /*direct*/
                case addmode_dir:
                    assm_opd_ident(assm, filedat,
                                   target_opd->data->identifier);
                    break;
                /*struct*/
                case addmode_struct:
                    assm_opd_ident(assm, filedat,
                                   target_opd->data->structure->identifier);
                    
                    /*struct field*/
                    add_bincode(&assm->last_instr,
                                target_opd->data->structure->field <<
                                SHIFT_8BIT, 
                                &filedat->IC);
                    break;
                /*register*/
                case addmode_reg:
                    add_bincode(&assm->last_instr,
                                target_opd->data->reg_num << cur_reg_shift,
                                &filedat->IC);
                    break;
            }
        }
        
        target_opd = stat->operand_dst;
        cur_reg_shift = SHIFT_REG2;
    }
}

/*Assembles the identifier.*/
static void assm_opd_ident(assm_t *assm, file_data *filedat, token *ident) {
    item_label *p_label; /*pointers for ease of use*/
    item_extern *p_extern;
    
    /*label list lookup, we want instruction labels only*/
    if ((p_label = get_instr_label(filedat->last_label,
                                   ident->tokstr)) != NULL) {
                                       
        add_bincode(&assm->last_instr,
                   (p_label->IC << SHIFT_8BIT) + ARE_RELOC,
                    &filedat->IC);
    /*extern list lookup*/
    } else if ((p_extern =
               find_clist_str(filedat->last_extern,
                              &find_item_extern,
                              ident->tokstr)) != NULL) {
        /*remember that add_bincode increments the IC!*/
        p_extern->was_used = true;
        add_clist(&assm->last_out_ext,
                  create_item_out_ent_ext(filedat->IC, ident->tokstr));
        
        add_bincode(&assm->last_instr, ARE_EXTERN, &filedat->IC);
    /*add dummy instruction*/
    } else {
        /*remember that add_bincode increments the IC!*/
        add_clist(&assm->last_undefid,
                  create_item_undefid(filedat->IC, filedat->linenum, ident));
                  
        add_bincode(&assm->last_instr, ARE_RELOC, &filedat->IC);
    }
}

/*Attempts to find a label of an instruction statement that is
  lexicographically equivalent to str. If found, pointer to item_label
  in the label list is returned. Otherwise we return NULL.*/
static item_label *get_instr_label(c_list *last_label, char *str) {
    item_label *p_label;
    
    if ((p_label = find_clist_str(last_label,
                                  &find_item_label, str)) != NULL) {
        
        if (p_label->stype == stype_instruction) {
            return p_label;
        }
    }
    
    return NULL;
}

/*Assembles the data statement.*/
static void assm_stat_data(assm_t *assm, stat_ddir_t *stat,
                           file_data *filedat) {
    char *p_str; /*pointer to a string, for identifiers*/
    ddir_data_t *p_data; /*pointer to .data data, for ease of use*/
    c_list *p_assm_data_head;
    
    /*data*/
    /*a little tricky - we need to append the data list at the
      end of list of data codes*/
    if (stat->datadir == datadir_data) {
        p_data = stat->data;
        filedat->DC += p_data->num_of_items;
        
        if (assm->last_data != NULL) {
            p_assm_data_head = assm->last_data->next;
            
            assm->last_data->next = p_data->last_data->next;
            p_data->last_data->next = p_assm_data_head;
        }
        
        assm->last_data = p_data->last_data;
    /*string*/
    } else if (stat->datadir == datadir_string) {
        p_str = (char*)stat->data;
        p_str++;
        
        while (*p_str != '"') {
            add_bincode(&assm->last_data, *p_str, &filedat->DC);
            p_str++;
        }
        
        add_bincode(&assm->last_data, STRING_TERMINATOR, &filedat->DC);
    /*struct*/
    } else if (stat->datadir == datadir_struct) {
        add_bincode(&assm->last_data, ((ddir_struct_t*)stat->data)->num,
                    &filedat->DC);
        
        p_str = ((ddir_struct_t*)stat->data)->string;
        p_str++;
        while (*p_str != '"') {
            add_bincode(&assm->last_data, *p_str, &filedat->DC);
            p_str++;
        }
        
        add_bincode(&assm->last_data, STRING_TERMINATOR, &filedat->DC);
    /*entry and extern*/
    } else if (stat->datadir == datadir_entry) {
        add_clist(&filedat->last_entry,
                  create_item_entry((token*)stat->data, filedat->linenum));
    } else if (stat->datadir == datadir_extern) {
        add_clist(&filedat->last_extern,
                  create_item_extern((token*)stat->data, filedat->linenum));
    }
}

/*A wrapper function for adding a label.*/
static void add_label(line_data *lindat, file_data *filedat) {
    bool error = filedat->error;
    
    if (lindat->label_token == NULL) {
        return;
    } else if (lindat->has_initial_wspace == true) {
        print_tok_error(lindat->label_token, filedat,
                        "Warning, label has preceding whitespace.");
        
        filedat->error = error;
    }
    
    if (lindat->stype == stype_instruction) {
        add_clist(&filedat->last_label,
                  create_item_label(lindat->label_token, filedat->IC,
                                    filedat->linenum, lindat->stype));
    } else if (lindat->stype == stype_datadir) {
        add_clist(&filedat->last_label,
                  create_item_label(lindat->label_token, filedat->DC,
                                    filedat->linenum, lindat->stype)); 
    }
}

/*This is not exactly a desirable solution, but in practice, we won't
  have to deal with many errors. The alternative would be to store the
  relevant lines for undefined identifiers somewhere, but, of course,
  we won't know until the last minute which one of those will remain
  undefined. So I thought that some redundant i/o and CPU usage is
  a better choice in this case than redundant RAM usage. Again, in practice
  we'd have a couple of errors at most, but potentially hundreds of undefined
  identifiers.
  
  Of course, we might also forego the printing of the line entirely,
  since it's completely optional. But it looks nicer if we still do.*/
void print_tok_error_assm(token *tok, int linenum, char *filename,
                          file_data *filedat, char *message) {
    int i = 0;
    int lincount = 1;
    int ch;
    FILE *p_file = fopen(filename, "r");
    
    /*really shouldn't happen, but just to be pedantic*/
    if (p_file == NULL) {
        fprintf(stderr, "Error, fopen returned NULL for a known filename.\n");
        exit(1);
    }
    
    ERRORS++;
    filedat->error = true;
    
    /*get to the desired line*/
    while (lincount != linenum && !feof(p_file)) {
        ch = fgetc(p_file);
        if (ch == '\n') {
            lincount++;
            if (lincount == linenum) {
                break;
            }
        }
    }
    
    /*skip initial whitespace*/
    while (((ch = fgetc(p_file)) == ' ') || ch == '\t') {
        i++;
    }
    
    fprintf(stderr, "\nAssembly error.\nLine %d: %s\n", linenum, message);
    while (ch != '\n' && !feof(p_file)) {
        if (ch == '\t') {
            fprintf(stderr, " ");
        } else {
            fprintf(stderr, "%c", ch);
        }
        
        ch = fgetc(p_file);
    }
    fclose(p_file);
    fprintf(stderr, "\n");
    
    /*fancy line*/
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

/*Cleans up the assm.*/
void destroy_assm(assm_t *assm) {   
    destroy_clist(&assm->last_undefid, &destroy_item_undefid);
    destroy_clist(&assm->last_instr, &free);
    destroy_clist(&assm->last_data, &free);
    destroy_clist(&assm->last_out_ent, &destroy_item_out_ent_ext);
    destroy_clist(&assm->last_out_ext, &destroy_item_out_ent_ext);
}

/*Initializes the input to 0.*/
char *init_string(char *str, int length) {
    int i;
    
    for (i = 0; i < length; i++) {
        str[i] = 0;
    }
    
    return str;
}

/*Driver for the output of instructions to the output files.*/
void output_machine_code(assm_t *assm, file_data *filedat, char *filename) {
    int i;
    /*initial starting address, has to be bound to IC_INIT*/
    int address = IC_INIT;
    c_list *cur_node; /*used as an iterator*/
    c_list *target_clist; /*will point at instr or data*/
    item_out_ent_ext *p_out_ent_ext;
    char fname_buf[MAX_FILE_LENGTH];
    FILE *f_out; /*pointer to the output files*/
    
    /*instructions and data*/
    init_string(fname_buf, MAX_FILE_LENGTH);
    sprintf(fname_buf, "%s%s", filename, EXTENSION_OB);
    f_out = fopen(fname_buf, "w");
    if (f_out == NULL) {
        fprintf(stderr, "Error, fopen returned NULL for \"w\" "
                        "in output_machine_code.");
        exit(1);
    }
    
    /*the amount of instructions*/
    output_weird(filedat->IC-IC_INIT, f_out);
    fprintf(f_out, "%s", TABSTOP);
    output_weird(filedat->DC-DC_INIT, f_out);
    fprintf(f_out, "\n");
    
    /*instruction and data codes*/
    target_clist = assm->last_instr;
    for (i = 0; i < 2; i++) { /*2 for instructions and data*/
        if (target_clist != NULL) {
            cur_node = target_clist->next;
            
            /*output format: "ADDRESS" TABSTOP "MACHINECODE"*/
            do {
                output_weird(address, f_out);
                fprintf(f_out, "%s", TABSTOP);
                output_weird(*(unsigned int*)(cur_node->item), f_out);
                
                #ifdef DEBUG_OUTPUT
                    fprintf(f_out, "%s%d%s", TABSTOP, address, TABSTOP);
                    output_dec_as_word(*(unsigned int*)(cur_node->item),
                                       f_out);
                    fprintf(f_out, "%sreal address: %d", TABSTOP,
                            *(unsigned int*)(cur_node->item) >> 2);
                #endif
                
                fprintf(f_out, "\n");
                cur_node = cur_node->next;
                address++;
            } while (cur_node != target_clist->next);
        }
        
        target_clist = assm->last_data;
    }
    fclose(f_out);
    
    /*these are best done separately to avoid any confusion - 
      ugly, but safer*/
    /*entries*/
    init_string(fname_buf, MAX_FILE_LENGTH);
    sprintf(fname_buf, "%s%s", filename, EXTENSION_ENT);
    if (assm->last_out_ent != NULL) {
        f_out = fopen(fname_buf, "w");
        if (f_out == NULL) {
            fprintf(stderr, "Error, fopen returned NULL for \"w\" "
                            "in output_machine_code.");
            exit(1);
        }   
    
        cur_node = assm->last_out_ent->next;
        p_out_ent_ext = cur_node->item;
        /*output format: "LABEL" TABSTOP "ADDRESS"*/
        do {
            fprintf(f_out, "%s\t", p_out_ent_ext->str);
            output_weird(p_out_ent_ext->address, f_out);
            
            #ifdef DEBUG_OUTPUT
                fprintf(f_out, "\t%d", p_out_ent_ext->address);
            #endif
            
            fprintf(f_out, "\n");
            cur_node = cur_node->next;
            p_out_ent_ext = cur_node->item;
        } while (cur_node != assm->last_out_ent->next);
        
        fclose(f_out);
    } else {
        remove(fname_buf);
    }
    
    /*externs*/
    init_string(fname_buf, MAX_FILE_LENGTH);
    sprintf(fname_buf, "%s%s", filename, EXTENSION_EXT);
    if (assm->last_out_ext != NULL) {
        f_out = fopen(fname_buf, "w");
        if (f_out == NULL) {
            fprintf(stderr, "Error, fopen returned NULL for \"w\" "
                            "in output_machine_code.");
            exit(1);
        }
    
        cur_node = assm->last_out_ext->next;
        p_out_ent_ext = cur_node->item;
        /*output format: "LABEL" TABSTOP "ADDRESS"*/
        do {
            fprintf(f_out, "%s\t", p_out_ent_ext->str);
            output_weird(p_out_ent_ext->address, f_out);
            
            #ifdef DEBUG_OUTPUT
                fprintf(f_out, "\t%d", p_out_ent_ext->address);
            #endif
            
            fprintf(f_out, "\n");
            cur_node = cur_node->next;
            p_out_ent_ext = cur_node->item;
        } while (cur_node != assm->last_out_ext->next);
        
        fclose(f_out);
    } else {
        remove(fname_buf);
    }
}

/*Outputs dec_inst as weird base into f_out. Since we know for sure
  that instructions are in the 10 bit range and that all the negative
  numbers were converted to the 2s complement, we can be lazy with
  the conversion to base 32. In case of confusion - 2^10 = 1024, so
  every single word in our machine can be represented by 2 weird base
  symbols because we have 32*32 = 1024 choices for 2 base 32 digits.*/
void output_weird(int dec_inst, FILE *f_out) {
    char buffer[2] = {0};
    
    /*leftmost digit*/
    buffer[1] = weird_base[dec_inst % 32];
    /*rightmost digit*/
    dec_inst /= 32;
    buffer[0] = weird_base[dec_inst % 32];
    fprintf(f_out, "%c%c", buffer[0], buffer[1]);
}

/*DEBUG*/
void output_dec_as_word(int dec_inst, FILE *f_out) {
    int i;
    
    for (i = WORD_SIZE-1; i >= 0; i--) {
        if (dec_inst >> i & 1) {
            fprintf(f_out, "%c", '1');
        } else {
            fprintf(f_out, "%c", '0');
        }
    }
}


/*DEBUG*/
void print_dec_as_word(int dec_inst) {
    int i;
    
    for (i = WORD_SIZE-1; i >= 0; i--) {
        (dec_inst >> i & 1) ? putchar('1') : putchar('0');
    }
    
    putchar('\n');
}

/*DEBUG*/
void print_voidbin_as_word(void *bincode) {
    int i;
    
    for (i = WORD_SIZE-1; i >= 0; i--) {
        (*((int*)bincode) >> i & 1) ? putchar('1') : putchar('0');
    }
    
    putchar('\n');
}

/*DEBUG*/
void print_dec_as_b32(int dec_inst) {
    char buffer[2];
    
    if (dec_inst < 0) {
        dec_inst = -dec_inst;
    }
    
    buffer[1] = weird_base[dec_inst % 32];
    dec_inst /= 32;
    buffer[0] = weird_base[dec_inst % 32];
    
    printf("%c%c", buffer[0], buffer[1]);
}

/*DEBUG*/
void print_item_out_ent_ext(void *item) {
    item_out_ent_ext *p_out_ent_ext = item;
    
    if (item == NULL) {
        return;
    }
    
    printf("%d\t%s\n", p_out_ent_ext->address, p_out_ent_ext->str);
}

/*DEBUG*/
void print_item_undefid(void *undefid) {
    item_undefid *p_undefid = undefid;
    
    if (undefid == NULL) {
        return;
    }

    printf("%d\t%s\t%d\t<-linenum\n", p_undefid->IC, p_undefid->tok->tokstr,
                                      p_undefid->linenum);
}
