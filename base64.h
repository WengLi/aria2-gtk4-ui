#ifndef BASE64_H
#define BASE64_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int base64_encode(unsigned char *source, size_t sourcelen, char *target, size_t targetlen);
size_t base64_decode(char *source, unsigned char *target, size_t targetlen);
#endif