#ifndef ASSM_H
#define ASSM_H

#define MAX_MACHINE_MEM 256

#define MAX_FILE_LENGTH 1024 /*ought to be enough?*/
#define MAX_EXT_LENGTH  4 /*length of the extension*/

/*initial values for IC and DC*/
#define IC_INIT 100
#define DC_INIT 0

/*ARE values, 0 is implied to be absolute,
  but we don't need add 0 to machine codes
  for obvious reasons (x + 0 = x)*/
#define ARE_EXTERN 1
#define ARE_RELOC  2

/*offsets for moving the bits to their
  relevant locations*/
#define SHIFT_OPC  6 /*opcode*/
#define SHIFT_SRC  4 /*source operand addresing mode*/
#define SHIFT_DST  2 /*destination operand addresing mode*/

#define SHIFT_8BIT 2 /*shifts an eight bit number two bits to the left*/
#define SHIFT_REG1 6 /*shifts the code of the source register operand*/
#define SHIFT_REG2 2 /*shifts the code of the destination register operand*/
/*9   8   7   6  5  4  3 2 1 0*/
/*512 256 128 64 32 16 8 4 2 1*/

    /*container for the currently undefined identifiers*/
    typedef struct item_undefid {
        int IC;      /*IC of the identifier*/
        int linenum; /*line number of the identifier*/
        token *tok;  /*the token itself*/
    } item_undefid;
    
    /*container for the output of the addresses of
      entries and externs*/
    typedef struct item_out_ent_ext {
        int address; /*the final address*/
        char *str;   /*the identifier*/
    } item_out_ent_ext;
    
typedef struct assm_t {
    /*the instruction machine code, stored as an unsigned decimal int*/
    /*stores unsigned int* in void *item in the c_list*/
    c_list *last_instr;
    
    /*the data machine code, stored as an unsigned decimal int*/
    /*stores unsigned int* in void *item in the c_list*/
    c_list *last_data;
    
    /*this list contains all the operands whose addresses
      were not known during their assembly in the first pass*/
    /*stores item_undefid*/
    c_list *last_undefid;
    
    /*entry output file contents*/
    /*stores item_out_ent_ext*/
    c_list *last_out_ent;
    
    /*extern output file contents*/
    /*stores item_out_ent_ext*/
    c_list *last_out_ext;
} assm_t;


void assemble_line(assm_t *assm, void *stat,
                   line_data *lindat, file_data *filedat);

char *init_string(char *str, int length);

item_out_ent_ext *create_item_out_ent_ext(int address, char *str);

void output_machine_code(assm_t *assm, file_data *filedat, char *filename);
void output_weird(int dec, FILE *f_out);
void output_dec_as_word(int dec_inst, FILE *f_out);

void print_tok_error_assm(token *tok, int linenum, char *filename,
                          file_data *filedat, char *message);
                      
void destroy_assm(assm_t *assm);

/*DEBUG*/
void print_dec_as_word(int dec_inst);
void print_voidbin_as_word(void *bincode);
void print_dec_as_b32(int dec_inst);

void print_item_undefid(void *undefid);
void print_item_out_ent_ext(void *item);

#endif /*ASSM_H*/
