#ifndef TOKEN_H
#define TOKEN_H

#define MAX_ADD_MODES 4

#define MAX_OPD_MOV  2
#define MAX_OPD_CMP  2
#define MAX_OPD_ADD  2
#define MAX_OPD_SUB  2
#define MAX_OPD_NOT  1
#define MAX_OPD_CLR  1
#define MAX_OPD_LEA  2
#define MAX_OPD_INC  1
#define MAX_OPD_DEC  1
#define MAX_OPD_JMP  1
#define MAX_OPD_BNE  1
#define MAX_OPD_RED  1
#define MAX_OPD_PRN  1
#define MAX_OPD_JSR  1
#define MAX_OPD_RTS  0
#define MAX_OPD_STOP 0

typedef enum {
    OP_MOV,
    OP_CMP,
    OP_ADD,
    OP_SUB,
    OP_NOT,
    OP_CLR,
    OP_LEA,
    OP_INC,
    OP_DEC,
    OP_JMP,
    OP_BNE,
    OP_RED,
    OP_PRN,
    OP_JSR,
    OP_RTS,
    OP_STOP
} OPCODES;

/*The operators. Now this one doesn't quite belong here, but
  trouble is, both lexer and parser need it. Making a new c file
  or a header seems overkill, I think it's better left here.*/
static const struct {
    char *opname;
    int opc;
    int opds;
    int src_mode[MAX_ADD_MODES];
    int dst_mode[MAX_ADD_MODES];
                                     /*key for address modes:
                                       IMM, DIR, STRUCT, REG
                                       1 is allowed, 0 is disallowed*/
} OPS[] = {
    /*name*/ /*code*/   /*opds*/       /*source*/ /*dest*/
    {"mov",   OP_MOV,   MAX_OPD_MOV,   {1,1,1,1}, {0,1,1,1}},
    {"cmp",   OP_CMP,   MAX_OPD_CMP,   {1,1,1,1}, {1,1,1,1}},
    {"add",   OP_ADD,   MAX_OPD_ADD,   {1,1,1,1}, {0,1,1,1}},
    {"sub",   OP_SUB,   MAX_OPD_SUB,   {1,1,1,1}, {0,1,1,1}},
    {"not",   OP_NOT,   MAX_OPD_NOT,   {0,0,0,0}, {0,1,1,1}},
    {"clr",   OP_CLR,   MAX_OPD_CLR,   {0,0,0,0}, {0,1,1,1}},
    {"lea",   OP_LEA,   MAX_OPD_LEA,   {0,1,1,0}, {0,1,1,1}},
    {"inc",   OP_INC,   MAX_OPD_INC,   {0,0,0,0}, {0,1,1,1}},
    {"dec",   OP_DEC,   MAX_OPD_DEC,   {0,0,0,0}, {0,1,1,1}},
    {"jmp",   OP_JMP,   MAX_OPD_JMP,   {0,0,0,0}, {0,1,1,1}},
    {"bne",   OP_BNE,   MAX_OPD_BNE,   {0,0,0,0}, {0,1,1,1}},
    {"red",   OP_RED,   MAX_OPD_RED,   {0,0,0,0}, {0,1,1,1}},
    {"prn",   OP_PRN,   MAX_OPD_PRN,   {0,0,0,0}, {1,1,1,1}},
    {"jsr",   OP_JSR,   MAX_OPD_JSR,   {0,0,0,0}, {0,1,1,1}},
    {"rts",   OP_RTS,   MAX_OPD_RTS,   {0,0,0,0}, {0,0,0,0}},
    {"stop",  OP_STOP,  MAX_OPD_STOP,  {0,0,0,0}, {0,0,0,0}}
};

/*addressing modes*/
typedef enum {
    addmode_imm,
    addmode_dir,
    addmode_struct,
    addmode_reg
} add_mode;

/*data directives*/
typedef enum {
    datadir_data,
    datadir_string,
    datadir_struct,
    datadir_entry,
    datadir_extern
} data_dir;

/*statement types*/
typedef enum {
    stype_instruction,
    stype_datadir,
    stype_unknown
} stat_type;

/*token types*/
typedef enum {
    /*0*/   toktype_dot,
    /*1*/   toktype_comma,
    /*2*/   toktype_colon,
    /*3*/   toktype_hash,
    /*4*/   toktype_quote,
    /*5*/   toktype_number,
    /*6*/   toktype_label,
    /*7*/   toktype_datadir,
    /*8*/   toktype_datadir_data,
    /*9*/   toktype_datadir_string,
    /*10*/  toktype_datadir_struct,
    /*11*/  toktype_datadir_entry,
    /*12*/  toktype_datadir_extern,
    /*13*/  toktype_operator,
    /*14*/  toktype_operator_mov,
    /*15*/  toktype_operator_cmp,
    /*16*/  toktype_operator_add,
    /*17*/  toktype_operator_sub,
    /*18*/  toktype_operator_not,
    /*19*/  toktype_operator_clr,
    /*20*/  toktype_operator_lea,
    /*21*/  toktype_operator_inc,
    /*22*/  toktype_operator_dec,
    /*23*/  toktype_operator_jmp,
    /*24*/  toktype_operator_bne,
    /*25*/  toktype_operator_red,
    /*26*/  toktype_operator_prn,
    /*27*/  toktype_operator_jsr,
    /*28*/  toktype_operator_rts,
    /*29*/  toktype_operator_stop,
    /*30*/  toktype_operand_register,
    /*31*/  toktype_register_0,
    /*32*/  toktype_register_1,
    /*33*/  toktype_register_2,
    /*34*/  toktype_register_3,
    /*35*/  toktype_register_4,
    /*36*/  toktype_register_5,
    /*37*/  toktype_register_6,
    /*38*/  toktype_register_7,
    /*39*/  toktype_register_8,
    /*40*/  toktype_register_9,
    /*41*/  toktype_identifier,
    /*42*/  toktype_unknown,
    /*43*/  toktype_string,
    /*44*/  toktype_EOL
} token_type;

typedef struct token {
    int starting_index; /*in the line that it appears*/
    int length;         /*its length*/
    token_type toktype; /*its type*/
    char *tokstr;       /*the characters that make up the token*/
} token;


token *create_token(int starting_index, int length, char *input);
void destroy_token(token *tok);
token *extract_token(token *tok);
token_type get_toktype(token *tok);

token_type downcast_toktype(token_type toktype);
void print_token(token *tok);

const char *get_toktype_string(token_type toktype);

void destroy_clist_token(void *tok);

/*DEBUG*/
void print_clist_token(void *tok);

#endif /*TOKEN_H*/
