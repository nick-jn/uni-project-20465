GCC = gcc -Wall -ansi -pedantic
OBJ = main.o statement.o lexer.o token.o \
      tokstream.o parser.o \
      assm.o assm_driver.o clist.o filedata.o

assembler: $(OBJ)
	$(GCC) -o assembler $(OBJ)

%.o: %.c
	$(GCC) -c $< -o $@

clean: $(OBJ)
	rm -f $(OBJ)

clang: *.c
	clang --analyze ./*.c && rm -f ./*.plist