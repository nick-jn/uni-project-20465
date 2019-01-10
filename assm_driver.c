/*Driver for the assembler and the entire program.*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bool.h"
#include "clist.h"
#include "token.h"
#include "statement.h"
#include "filedata.h"
#include "lexer.h"
#include "parser.h"
#include "assm.h"
#include "assm_driver.h"

#define EXTENSION_AS ".as"

/*Debug options:
  --------------
    #define DEBUG_FPASS - first pass debugger
    #define DEBUG_SPASS - second pass debugger
*/

static void first_pass(assm_t *assm, file_data *filedat, FILE *f_input);
static void second_pass(assm_t *assm, file_data *filedat, char *filename);
static void second_pass_entry(assm_t *assm, file_data *filedat,
                              char *filename);
static void second_pass_undefid(assm_t *assm, file_data *filedat,
                                char *filename);

static void apply_IC_offset(c_list *last_label_def, int IC);
static void init_run_assm(file_data *filedat, assm_t *assm);
static bool has_initial_wspace(const char *line);
static void init_first_pass(line_data *lindat, file_data *filedat,
                            void **statement, char input[MAX_LINE]);
static void print_line(char *str);

unsigned int ERRORS = 0; /*for debugging*/

/*Main driver for the whole assembler.*/
void run_assm(int argc, char **argv) {
    int cur_file = 1;  /*counts the current argv*/
    assm_t assm;       /*the relevant assembly data on the current file*/
    file_data filedat; /*the relevant data on the current file*/
    FILE *f_input;     /*the input file*/
    char fname_as_ext[MAX_FILE_LENGTH]; /*filename with the .as extension*/
    
    while (argc > 1) {
        /*we should be able to fit the extensions after the filename*/
        if (strlen(argv[cur_file]) > MAX_FILE_LENGTH-MAX_EXT_LENGTH) {
            fprintf(stderr, "\nError, filename too long.\n");
            argc--; cur_file++;
            continue;
        }
        
        /*initializes filedat and assm*/
        init_run_assm(&filedat, &assm);
        
        /*open the input file*/
        init_string(fname_as_ext, MAX_FILE_LENGTH);
        sprintf(fname_as_ext, "%s%s", argv[cur_file], EXTENSION_AS);
        f_input = fopen(fname_as_ext, "r");
        if (f_input == NULL) {
            fprintf(stderr, "\nError, unknown filename: %s\n", fname_as_ext);
            argc--; cur_file++;
            continue;
        }
        
        printf("\n\nCurrent file:\n~~~~~~~~~~~~~\n%d: %s\n\n",
               cur_file, argv[cur_file]);
        
        /*First pass*/
        first_pass(&assm, &filedat, f_input);
        fclose(f_input);
        
        /*Apply the IC offset to the labels created in data
          statements (the offset is the last IC).*/
        apply_IC_offset(filedat.last_label, filedat.IC);
        
        /*Second pass*/
        /*this guy needs .as in the filename for the print_tok_error_assm*/
        second_pass(&assm, &filedat, fname_as_ext); 
        
        /*Write output to the relevant files*/
        if (filedat.error != true) {
            output_machine_code(&assm, &filedat, argv[cur_file]);
        }
        
        /*cleanup*/
        destroy_assm(&assm);
        destroy_clist(&filedat.last_label, &destroy_item_label);
        destroy_clist(&filedat.last_entry, &destroy_item_entry);
        destroy_clist(&filedat.last_extern, &destroy_item_extern);
        
        if (filedat.error == true) {
            puts("\nCompilation failed.");
            printf("\nErrors found: %u", ERRORS);
        } else {
            puts("\nCompilation finished successfully.");
        }
        
        printf("\nLines parsed: %d\n", filedat.linenum);
        
        argc--; cur_file++;
    }
}

/*The goals are to parse the line and write all the relevant information
  about it to the assmt_t assm (labels are stored it filedat though). That is,
  the instructions or data codes, and entry or extern declarations.*/
static void first_pass(assm_t *assm, file_data *filedat, FILE *f_input) {
    char input[MAX_LINE];
    line_ret lineret; /*returned from get_line*/
    bool exceed_machmem = false; /*a flag*/
    
    /*stored here because all the tokens in statement actually
      belong to filedat->last_token, we only duplicate the relevant
      ones during the assembly stage*/
    void *statement; /*stat_instr_t or stat_datadir_t*/
    line_data lindat; /*data on the current line*/
    #ifdef DEBUG_FPASS
        int LAST_IC;
        int LAST_DC;
    #endif
    
    /*Get a line from the input file. If it's a comment, skip it completely.
      If not, call the parser - it will attempt to acquire all the relevant
      data from the line. If it fails, move on to the next line. If we
      succeed, if the line had a label we add it to the label list,
      and we sent the statement to assembler in order to convert it
      to machine code (stored as decimal numbers). We continue until we reach
      EOF in the input file.*/
      
    /*note that lin_out will return line_EOF (defined as 0) upon EOF*/
    while ((lineret = get_line(f_input, MAX_LINE,
                               init_string(input, MAX_LINE)))) {
        init_first_pass(&lindat, filedat, &statement, &input[0]);
        
        if (is_comment_or_empty_line(&input[0])) {
            continue;
        }
        
        if (lineret == line_too_long) {
            fprintf(stderr, "Line %d: Error, line too long.\n",
                    filedat->linenum);
            print_line(input);
            /*minus one for the terminator*/
            fprintf(stderr, "\nMax. line length allowed: %d.\n", MAX_LINE-1);
            ERRORS++;
            filedat->error = true;
            continue;
        }
        
        #ifdef DEBUG_FPASS
            printf("\nLine %d:\n", filedat->linenum);
            print_line(input);
        #endif
        
        /*lexer*/
        tokenize_line(filedat);
        if (filedat->last_token == NULL) { /*lexer error*/
            destroy_clist(&filedat->last_token, &destroy_clist_token);
            continue;
        }
        
        #ifdef DEBUG_FPASS
            print_clist(filedat->last_token, &print_clist_token);
            putchar('\n');
        #endif
        
        /*parser*/
        statement = parse_line(&lindat, filedat);
        
        #ifdef DEBUG_FPASS
            print_statement(statement, lindat.stype);
            LAST_IC = filedat->IC;
            LAST_DC = filedat->DC;
        #endif
        
        /*assembler*/
        assemble_line(assm, statement, &lindat, filedat);
        
        /*check if we've exceeded MAX_MACHINE_MEM*/
        if (exceed_machmem == false &&
            (filedat->IC + filedat->DC) > MAX_MACHINE_MEM) {
            exceed_machmem = true;
            fprintf(stderr, "Line %d: Error, machine memory exceeded.\n",
                    filedat->linenum);
            print_line(input);
            ERRORS++;
            filedat->error = true;
        }
        
        #ifdef DEBUG_FPASS
            if (statement != NULL) {
                switch (lindat.stype) {
                    case stype_instruction:
                        print_clist_range(assm->last_instr,
                                          (LAST_IC-IC_INIT),
                                          (filedat->IC-IC_INIT),
                                          &print_voidbin_as_word);
                        break;
                    case stype_datadir:
                        if (((stat_ddir_t*)statement)->datadir != 
                            datadir_entry &&
                            ((stat_ddir_t*)statement)->datadir != 
                            datadir_extern) {
                            
                            print_clist_range(assm->last_data,
                                              (LAST_DC-DC_INIT),
                                              (filedat->DC-DC_INIT),
                                              &print_voidbin_as_word);
                        }
                        break;
                    default:
                        break;
                }
            }
        #endif /*FPASS*/
        
        destroy_statement(statement, lindat.stype);
        destroy_clist(&filedat->last_token, &destroy_clist_token);
    }
    
    #ifdef DEBUG_FPASS
        printf("\nFinal IC+DC: %d\n", filedat->IC+filedat->DC);
    #endif
}

/*Driver for the second pass routine.*/
static void second_pass(assm_t *assm, file_data *filedat, char *filename) {
    c_list *cur_extern;
    item_extern *p_extern;
    
    second_pass_entry(assm, filedat, filename);
    second_pass_undefid(assm, filedat, filename);
    
    /*check if some of the declared externs
      were never used as operands*/
    if (filedat->last_extern != NULL) {
        cur_extern = filedat->last_extern->next;
        p_extern = cur_extern->item;
        
        do {
            if (p_extern->was_used == false) {
                print_tok_error_assm(p_extern->tok, p_extern->linenum,
                                     filename, filedat, "Error, declared "
                                     "extern was never used as an operand.");
            }
            
            cur_extern = cur_extern->next;
            p_extern = cur_extern->item;
        } while (cur_extern != filedat->last_extern->next);
    }
}

/*Populates the last_out_ent_ext with entry items and their final addresses.
  If entry is not found the filedat's label list - an error is printed.*/
static void second_pass_entry(assm_t *assm, file_data *filedat,
                              char *filename) {
    c_list *cur_entry;
    item_entry *p_entry;
    item_label *p_label;
    
    if (filedat->last_entry == NULL) {
        return;
    }
    
    /*iterate through the entry definition list*/
    cur_entry = filedat->last_entry->next; /*point to head*/
    p_entry = cur_entry->item;
    do {
        if ((p_label = find_clist_str(filedat->last_label,
                                      &find_item_label,
                                      p_entry->tok->tokstr)) != NULL) {
            
            add_clist(&assm->last_out_ent,
                      create_item_out_ent_ext(p_label->IC,
                                              p_label->tok->tokstr));
        } else {
            print_tok_error_assm(p_entry->tok, p_entry->linenum,
                                 filename, filedat,
                                "Error, entry was not defined as a label.");
        }
        
        cur_entry = cur_entry->next;
        p_entry = cur_entry->item;
    } while (cur_entry != filedat->last_entry->next);
}

/*Sets the proper addresses for the instructions in assm->last_instr. All the
  relevant (yet) undefined identifiers were stored in assm->last_undefid.
  Externs are dealt with here as well.*/
static void second_pass_undefid(assm_t *assm, file_data *filedat,
                                char *filename) {
    int IC_counter = IC_INIT; /*targets the appropriate last_instr node*/
    
    c_list *instr_node; /*instruction list in assm*/
    c_list *undefid_node; /*undefined identifier list in assm*/
    
    /*item pointers for ease of use*/
    item_label *p_label;
    item_extern *p_extern;
    item_undefid *p_undefid;
    
    /*no undefined identifiers*/
    if (assm->last_undefid == NULL) {
        return;
    }
    
    /*no instructions*/
    if (assm->last_instr == NULL) {
        return;
    }
    
    #ifdef DEBUG_SPASS
        puts("Second pass feed:\n-----------------\nUndefid:");
        print_clist(assm->last_undefid, &print_item_undefid);
        puts("\nLabels:");
        print_clist(filedat->last_label, &print_item_label);
    #endif
    
    /*iterate through the undefid list*/
    instr_node = assm->last_instr->next; /*point to the head*/
    undefid_node = assm->last_undefid->next; /*point to the head*/
    p_undefid = undefid_node->item;
    do {
        /*point to the relevant instruction in the
          instruction list based on the IC values*/
        while (p_undefid->IC != IC_counter) {
            instr_node = instr_node->next;
            IC_counter++;
        }
        
        /*extern lookup*/
        if ((p_extern = find_clist_str(filedat->last_extern,
                                        &find_item_extern,
                                        p_undefid->tok->tokstr)) != NULL) {
            
            *((int*)instr_node->item) = ARE_EXTERN;
            /*note that tokstr's *pointer* is copied*/
            add_clist(&assm->last_out_ext, 
                      create_item_out_ent_ext(IC_counter, 
                                              p_undefid->tok->tokstr));
            p_extern->was_used = true;
        /*label lookup*/
        } else if ((p_label = find_clist_str(filedat->last_label,
                                            &find_item_label,
                                            p_undefid->tok->tokstr)) != NULL) {
            *((int*)instr_node->item) = (p_label->IC << SHIFT_8BIT) +
                                        ARE_RELOC;
        /*nope, this one wasn't declared at all*/
        } else {
            print_tok_error_assm(p_undefid->tok, p_undefid->linenum, filename,
                             filedat, "Error, undeclared identifier.");
        }
        
        undefid_node = undefid_node->next;
        p_undefid = undefid_node->item;
    } while (undefid_node != assm->last_undefid->next);
}

/*Applies the passed IC offset to the data statement labels.*/
static void apply_IC_offset(c_list *last_label, int IC) {
    c_list *cur_label;
    item_label *p_label;
    
    if (last_label == NULL) {
        return;
    }
   
    cur_label = last_label->next;
    p_label = (item_label*)cur_label->item;
    do {
        if (p_label->stype == stype_datadir) {
            p_label->IC += IC;
        }
        
        cur_label = cur_label->next;
        p_label = (item_label*)cur_label->item;
    } while (cur_label != last_label->next);
}

/*Initializes the run_assm. Setting the lists to NULL is done as a safety
  precaution since the destroyer should take care of it. But in any case,
  since the overhead is tiny, might as well.*/
static void init_run_assm(file_data *filedat, assm_t *assm) {
    filedat->IC          = IC_INIT;
    filedat->DC          = DC_INIT;
    filedat->error       = false;
    filedat->linenum     = 0;
    filedat->last_token  = NULL;
    filedat->last_label  = NULL;
    filedat->last_entry  = NULL;
    filedat->last_extern = NULL;
    
    assm->last_instr     = NULL;
    assm->last_data      = NULL;
    assm->last_undefid   = NULL;
    assm->last_out_ent   = NULL;
    assm->last_out_ext   = NULL;
}

/*Initializes all the relevant passed arguments for the first pass.*/
static void init_first_pass(line_data *lindat, file_data *filedat,
                            void **statement, char input[MAX_LINE]) {
    
    *statement = NULL;
    
    filedat->linenum++;
    filedat->last_token   = NULL;
    filedat->current_line = &input[0];
    
    lindat->label_token = NULL;
    lindat->label_error = false;
    lindat->valid_label = false;
    /*wasn't sure where to put this one; logically, it's
      the parser's job, but since we have this initializing
      function and the check is so simple... well, seems
      more reasonable to put it here*/
    lindat->has_initial_wspace = has_initial_wspace(&input[0]);
    lindat->stype              = stype_unknown; /*precaution*/
}

/*Dereferences line and if it's value is equal to ' ' or '\t',
  returns true.*/
static bool has_initial_wspace(const char *line) {
    if (*line == ' ' || *line == '\t') {
        return true;
    }
    
    return false;
}

/*DEBUG*/
static void print_line(char *str) {
    while (*str == ' ' || *str == '\t') {
        str++;
    }
    
    while (*str != '\n' && *str != '\0') {
        if (*str == '\t') {
            printf(" ");
        } else {
            printf("%c", *str);
        }
       str++;
    }
    printf("\n----------------------\n");
}
