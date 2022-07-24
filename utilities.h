#ifndef UTILITIES_H
#define UTILITIES_H

#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

char *string_format(const char *format, ...);
uint16_t *utf8_to_utf16(const char *string, int count, int *utf16len);
char *utf16_to_utf8(const uint16_t *string, int count, int *utf8len);
unsigned char *read_file_to_bytes(const char *path, int *length);
int get_current_time_string(char target[20]);
#endif