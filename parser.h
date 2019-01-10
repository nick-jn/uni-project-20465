#ifndef PARSER_H
#define PARSER_H

#define EIGHTBIT_MAX 127
#define EIGHTBIT_MIN (-128)
#define TENBIT_MAX 511
#define TENBIT_MIN (-512)

void *parse_line(line_data *lindat, file_data *filedat);

#endif /*PARSER_H*/
