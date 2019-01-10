#ifndef STATEMENT_H
#define STATEMENT_H

/*Contains info on label and on the statement type.*/
typedef struct line_data {
    bool valid_label;        /*see get_label in parser.c*/
    bool label_error;        /*see get_label in parser.c*/
    bool has_initial_wspace; /*if the line had whitespace at the start*/
    token *label_token;
    stat_type stype;
} line_data;

/*instruction statement operands*/
            typedef struct struct_opd_t {
                int field;
                token *identifier;
            } struct_opd_t;

        typedef union operand_data {
            int number;
            int reg_num; /*registry number*/
            token *identifier;
            struct_opd_t *structure;
        } operand_data;

    typedef struct operand_t {
        int starting_index; /*of the operand itself, not of the token*/
        int length;         /*of the operand itself, not of the token*/
        add_mode addmode;
        operand_data *data;
    } operand_t;

/*instruction statement*/
typedef struct stat_instr_t {
    int opcode;
    operand_t *operand_src; /*NULL if there isn't one*/
    operand_t *operand_dst; /*NULL if there isn't one*/
} stat_instr_t;


/*data statement operands*/
    typedef struct ddir_struct_t {
        int num;
        char *string;
    } ddir_struct_t;
    
    typedef struct ddir_data_t {
        int num_of_items;
        c_list *last_data;
    } ddir_data_t;
    
/*data statement*/
typedef struct stat_ddir_t {
    data_dir datadir;
    void *data; /*can be ddir_data_t, ddir_struct_t or a token*/
} stat_ddir_t;


stat_instr_t *create_stat_inst(int opcode, operand_t *src, operand_t *dst);
stat_ddir_t *create_stat_ddir(data_dir datadir, void *data);

operand_t *create_operand(int starting_index, int length,
                          add_mode addmode, operand_data *data);
ddir_struct_t *create_ddir_struct(int num, char *string);
ddir_data_t *create_ddir_data(int num_of_items, c_list *last_data);

void destroy_operand(operand_t *operand); /*does not free the tokens!*/
void destroy_statement(void *stat, stat_type stype);
void print_operand(operand_t *operand);
void print_statement(void *statement, stat_type stype);

#endif /*STATEMENT_H*/
