#ifndef FILEDATA_H
#define FILEDATA_H

#define MAX_LINE 81 /*plus one for the terminator*/

/*return from get_line*/
typedef enum {
    line_EOF,
    line_ok,
    line_too_long
} line_ret;

    /*container for the label definitions.*/
    typedef struct item_label {
        token *tok;
        unsigned int IC;      /*IC of the start of the statement*/
        int linenum; /*line number*/
        stat_type stype;
    } item_label;
    
    /*container for the entry definitions*/
    typedef struct item_entry {
        token *tok;
        int linenum; /*line number*/
    } item_entry;
    
    /*container for the extern definitions*/
    typedef struct item_extern {
        token *tok;
        int linenum; /*line number*/
        
        /*used in second pass to determine if the declared extern
          was used as an operand at least once in the code, seems
          fairly efficient and reasonable to do it this way*/
        bool was_used;
    } item_extern;
    
/*Contains relevant data on the current assembly file.*/
typedef struct file_data {
    unsigned int IC, DC;
    bool error;         /*any call to print_tok_error* will set this to true*/
    int linenum;        /*current line number in the file*/
    char *current_line; /*contents of the current line in file*/
    
    /*tokens of the current line*/
    /*stores tokens int void *item*/
    c_list *last_token;
    
    /*definitions of labels*/
    /*stores item_label*/
    c_list *last_label;
    
    /*definitions of entries*/
    /*stores item_entry*/
    c_list *last_entry;
    
    /*definitions of externs*/
    /*stores item_extern*/
    c_list *last_extern;
} file_data;


/*item_label*/
item_label *create_item_label(token *tok, int address,
                              int linenum, stat_type stype);
void *find_item_label(void *item, char *str);
void destroy_item_label(void *item);
void print_item_label(void *item);

/*item_entry*/
item_entry *create_item_entry(token *tok, int linenum);
void *find_item_entry(void *item, char *str);
void destroy_item_entry(void *item);
void print_item_entry(void *item);

/*item_extern*/
item_extern *create_item_extern(token *tok, int linenum);
void *find_item_extern(void *item, char *str);
void destroy_item_extern(void *item);
void print_item_extern(void *item);

line_ret get_line(FILE *p_file, int length, char *line);
bool is_comment_or_empty_line(char *input);

void print_tok_error(token *tok, file_data *filedat, char *message);

#endif /*FILEDATA_H*/
