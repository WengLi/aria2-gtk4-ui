#ifndef UTILITIES_H
#define UTILITIES_H

#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ftw.h>
#include <unistd.h>
#define _XOPEN_SOURCE 500


char *b64_encode_file(const char *infilename, long *out_length);
char *string_format(const char *format, ...);
char *string_join(const char *separator, char **src, int size);
void string_delete_space(char *str);
int get_real_uri(char *src, char *target);
uint16_t *utf8_to_utf16(const char *string, int count, int *utf16len);
char *utf16_to_utf8(const uint16_t *string, int count, int *utf8len);
unsigned char *read_file_to_bytes(const char *path, int *length);
int get_current_time_string(char target[20]);
double int52_to_double(int64_t i);
int64_t double_to_int64(double a);
int remove_file(const char *filename);
char *get_file_name(char *path);
int remove_dir(char *dir);
#endif