/*Assembler for the made-up language as described in 2018a workbook
  of the 20465 course.
  
  Usage: assembler [filename1] [filename2] ... [filenameN]
  
  The assembler demands that the passed file with the name filename* has a
  .as extension, while the argument itself must not have one. That is,
  "assembler test" implies that the file test.as will be passed to the
  assembler.
  
  WARNING:  The IC counter starts with 100. Due to unclear instructions,
  --------  it was decided to implement a memory scope check - in practice,
            at most 156 words can be encoded by assembler instead of the
            maximum memory amount, which is 256. This is due to addressing
            being limited to 8bits - we can only ever address 0-255, inclusive.
            
            But then, it seems utterly illogical to just enforce the limit
            when this erroneous addressing occurs. Therefore, due to a
            general lack of clarity, we simply check the IC+DC count (where
            IC starts from 100 and DC starts from 0), and if IC+DC exceeds
            256, it is regarded as an error *REGARDLESS* if we had addressed
            a >=256 address or not.
            
            
  !!!!!!!!!!!!!!!!!!!
  LAST MINUTE ISSUES:
  !!!!!!!!!!!!!!!!!!!
  
  The only real issue I've found is the legality of delimiter and symbol
  placement in the code (that is, whitespace and symbols, as in, + ; ...).
  It wasn't at all clear which combinations are legal and which are not.
  Hopefully it's not a problem since changing it is really quite trivial.*/

#include <stdio.h>

#include "assm_driver.h"

/*General description:
  --------------------
  
  The assembler consists of three modules:
    Lexer       lexer.c    (driven by the tokenize_line function)
    Parser      parser.c   (driven by the parse_line function)
    Assembler   assm.c and assm_driver.c
    
  The driver for the assembler is assm_driver.c, which is also the
  main driver for the whole program.
  
  The first step is to read a line, deconstruct it into tokens and figure
  out the type of each token (as in, what is the purpose of this token in
  our assembly language). This is the responsibility of the lexer. Then, we
  start the parser. The parser's goal is to figure out if the tokens, based
  on their type and their ordering, actually make a legal statement in our
  language. If so, we proceed to the assembly stage. The assembler processes
  the data gathered by the parser and converts it into machine code.
  
  --------------------

  Assembly is split into two stages: first pass and second pass. Our language
  does not require us to declare entries and externs at the start of the file
  as well as to declare data statements only after all the instruction
  statements are declared. Therefore, we cannot know all the correct addresses
  of the direct access and struct access operands, as well as whether these
  operands are defined as external. Furthermore, we cannot know for sure
  if the declared entries were indeed declared labels at some point in the
  code. The solution, therefore, is to write the addresses of only those
  operands that were declared as labels of instruction statements or known
  to be externs at this particular stage of the first pass. Similar logic
  extends to entries. Otherwise, these operands will receive a dummy address
  and will await their fate during the second pass.
  
  The second pass relies on the fact that we've saved all those operands
  that we didn't know their final addresses into memory. Upon finishing the
  first pass, we know the final IC of our instructions and so we can now
  apply it as an offset to all the labels of the data statements. Having
  parsed the whole file, we also know all declared entries and externs.
  And so, we simply check our list of operands with dummy addresses against
  the list of externs and labels, and the list of entries is checked against
  the list of labels. If everything got matched and no undefined operands
  remain, we write the output object code, and the entry and extern files.*/


/*---------------------------------------------------------------------------*/


/*A more specific description:
  ----------------------------
  
  * Please see Assembler for the initial driver function of the program.
  
  Lexer:
  ------
  
  The goal of the lexer is to tokenize a line and to ensure there are no
  basic lexical errors (like missing a terminating " character). The only
  interface is the tokenize_line function which, upon success, will populate
  the c_list of tokens in file_data. The mechanism responsible for parsing
  is a simple state machine.
  
  
  Parser:
  -------
  
  The goal of the parser is to produce a statement. The interface
  for the entire parser is the parse_line function. Either stat_instr_t or
  stat_ddir_t are going to be produced upon success. The type of the
  statement (for casting the void pointer) is stored in line_data. First,
  the token list in file_data is loaded to a token stream interface provided
  by tokstream.h. Then, we determine if the statement has a label. Every
  relevant detail about the label will be stored in line_data. Then, we decide
  if the statement is an instruction statement or a data statement.
  
  
    Instruction statement:
    ----------------------
    
    Produces stat_instr_t. This is the most complex part of the whole
    assembler. The function for acquiring the desired struct is
    get_stat_instr. It, in turn, calls a function responsible for parsing
    the operands - get_operand_next. Then, get_operand_next calls the
    appropriate get_operand_* function, as determined by the initial
    parsing in get_operand_next.
    
    The goal of get_operand_next is to produce operand_t, which will be
    loaded into stat_instr_t as the appropriate (source or destination)
    operand (NULL is loaded to the appropriate operand pointer if there's
    just one operand). The stat_instr_t, therefore, upon successful parsing,
    will contain all the relevant details about the instruction statement for
    use with the assembly module.
    
    
    Data statement:
    ---------------
    
    Produces stat_ddir_t. The parsing of a data statement is, thankfully,
    extremely straightforward. The appropriate get_ddir_* function is
    called (except for string, in which case we save the string directly
    into void *data in stat_ddir_t).
    
    
  Assembler:
  ----------
    
  The assembler is divided between two .c files: assm_driver.c and assm.c.
  As was already explained, the assm_driver.c happens to hold the driver
  for the entire program - the run_assm function. The run_assm will
  receive the argc and argv from main - the file names that contain the
  assembly code that's going to be converted to our specific machine code.
  This function is the one that stores the file_data and assm_t structs
  for the entire duration of the lexing-parsing-assembly process. The
  assembler is divided into two passes:
  
  
    First pass:
    -----------
    
    The first_pass in assm_driver.c is the driver for the first pass.
    The main goal of the first pass is to populate the all the relevant
    c_lists of assm_t (see assm.h). Upon receiving a statement from
    the parser module, assemble_line is called in assm.c. Then, the
    we call either call the assm_stat_instr for instruction statements
    or assm_stat_data for data statements. The goal of assm_stat_*
    functions is to convert the parsed statement into machine code.
    
    There is, of course, one slight problem - the instruction and data
    statements may appear in a random order in the source file. Therefore,
    the proper address for a declared label may not be always known if
    there's an operand that attempts to address the said label. If the
    identifier that the operand addresses is not yet declared, we write
    a dummy instruction and add the necessary details of the aforementioned
    operand to last_undefid in assm_t, to be dealt with later during the
    second pass.
    
    We also add the definitions of labels, entries and externs into the
    appropriate lists in file_data upon successful parsing of the line. At
    the end of the first pass, we apply the IC offset to the labels that
    were declared in front of the data statements (that is, the last IC
    value is added to all such labels).
    
    
    Second pass:
    ------------
    
    The second_pass in assm_driver.c is the driver for the second pass.
    The lexer and parser modules are not called in this pass since we've
    acquired and saved all the necessary information during the first pass.
    
    First, we deal with entries - cross reference the entry definition
    list in file_data with the label definition list in the very same
    file_data. We write the matches to last_out_ent in assm_t.
    
    Then, we deal with the identifiers that weren't at the point in time
    when we were assembling the line that they appeared in. Remember that
    we've saved all such identifiers in the last_undefid list in assm_t.
    We simply cross check the last_undefid with the label (now with the
    appropriate IC offset applied to labels that were declared in front of
    data statements) and extern definition lists in file_data. We write the
    matched externs to last_out_ext.
    
    
  And that's mostly it really. All that's left is for run_assm to call the
  output_machine_code function in assm.c, which does just that - outputs
  the machine code (provided we didn't encounter any errors during the
  compilation, of course) to the relevant files. Then we go to the next
  argv (if there is one) and repeat this whole process all over again.*/


/*---------------------------------------------------------------------------*/


/*Various notes:
  --------------
  
  Circular linked lists are used due to O(1) access to both the last item
  of the list and the head, while requiring just one pointer (to the last
  item) to keep track of it.
  
  --------------
  
  The linked list stores the item_* types defined in the relevant header
  files.

  --------------

  Despite being it being suggested that the parsing of the line may be
  aborted at the first encountered error, an attempt was made to make the
  compiler's error identification quite general, and yet the subsequent
  error printing capabilities to be fairly specific in nature.
  
  --------------
  
  Error printing is not necessarily done in a logical order (i.e., printing
  all the relevant errors in the line from left to right) due to internal
  implementation of the assembler. This might be easily rectified by
  storing the messages in a buffer, but as is, there isn't really much
  point in implementing this, given that there is absolutely no real use
  for this assembler. Besides, it was not required in the description
  of the project.
  
  --------------
  
  Registers r8 and r9 are considered reserved words. It wasn't at all clear
  what to do with these, and this seemed like the most reasonable course
  of action. If used as a register operand, an error will be triggered.
  
  --------------
  
  Written in Geany with the help of valgrind, cppcheck, clang --analyze.
  
  Time to finish: ~120-150 hours. Most of it was wasted on implementing
  inefficient, clunky, and downright idiotic ideas due to a (nearly) complete
  lack of experience in software development. Though, once a decently good
  idea/interface was discovered (e.g. token stream for tokens, using a
  generic circular list for data storage, splitting the whole assembler
  into 3 major modules, etc.), the implementation was surprisingly easy
  and mostly painless.*/


int main(int argc, char **argv) {
    int i;

    if (argc == 1) {
        printf("Error, no input arguments.\n");
        return 0;
    }
    
    puts("Queued files:");
    for (i = 1; i < argc; i++) {
         printf("%d: %s\n", i, argv[i]);
    }
    
    run_assm(argc, argv); /*in assm_driver.c*/
    
    putchar('\n');
    
    return 0;
}

