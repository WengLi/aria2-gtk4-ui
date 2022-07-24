#include "utilities.h"

// 0xC0: Start of a 2-byte sequence
static const uint8_t utf8Limits[] = {0xC0, 0xE0, 0xF0, 0xF8, 0xFC};

//字符串格式化
char *string_format(const char *format, ...)
{
    va_list arg_list;
    char *string;
    int string_len;

    va_start(arg_list, format);
    string_len = vsnprintf(NULL, 0, format, arg_list);
    va_end(arg_list);

    string = (char *)malloc(string_len + 1);

    va_start(arg_list, format);
    vsprintf(string, format, arg_list);
    va_end(arg_list);

    return string;
}

uint16_t *utf8_to_utf16(const char *string, int count, int *utf16len)
{
    uint8_t ch;
    uint16_t *result;
    uint16_t *dest;
    uint32_t value;
    const char *end;

    if (count == -1)
        count = strlen(string);
    end = string + count;
    result = malloc(sizeof(uint16_t) * (count + 1));
    dest = result;

    while (string < end)
    {
        ch = *string++;

        if (ch < 0x80)
        { // 0-127, US-ASCII (single byte)
            *dest++ = (uint16_t)ch;
            continue;
        }

        if (ch < 0xC0) // The first octet for each code point should within 0-191
            break;

        for (count = 1; count < 5; count++)
            if (ch < utf8Limits[count])
                break;
        value = ch - utf8Limits[count - 1];

        do
        {
            uint8_t ch2;

            if (string >= end) //  || string[0] == 0
                break;
            ch2 = *string++;
            if (ch2 == 0)
                break;
            if (ch2 < 0x80 || ch2 >= 0xC0)
                break;
            value <<= 6;
            value |= (ch2 - 0x80);
        } while (--count != 0);

        if (value < 0x10000)
        {
            *dest++ = (uint16_t)value;
        }
        else
        {
            value -= 0x10000;
            if (value >= 0x100000)
                break;
            *dest++ = (uint16_t)(0xD800 + (value >> 10));
            *dest++ = (uint16_t)(0xDC00 + (value & 0x3FF));
        }
    }

    if (utf16len)
        *utf16len = dest - result;
    *dest++ = 0;
    return result;
}

char *utf16_to_utf8(const uint16_t *string, int count, int *utf8len)
{
    uint16_t ch;
    uint8_t *result;
    uint8_t *dest;
    const uint16_t *end;

    if (count == -1)
        count = wcslen((wchar_t *)string);
    end = string + count;
    result = malloc(sizeof(uint8_t) * (count + 1) * 3);
    dest = result;

    while (string < end)
    {
        ch = *string++;
        if (ch >= 1 && ch <= 0x7F)
            *dest++ = (uint8_t)ch;
        else if (ch > 0x7FF)
        {
            *dest++ = (uint8_t)(0xE0 | ((ch >> 12) & 0x0F));
            *dest++ = (uint8_t)(0x80 | ((ch >> 6) & 0x3F));
            *dest++ = (uint8_t)(0x80 | ((ch >> 0) & 0x3F));
        }
        else
        {
            *dest++ = (uint8_t)(0xC0 | ((ch >> 6) & 0x1F));
            *dest++ = (uint8_t)(0x80 | ((ch >> 0) & 0x3F));
        }
    }

    if (utf8len)
        *utf8len = dest - result;
    *dest++ = 0;
    return (char *)result;
}

unsigned char *read_file_to_bytes(const char *path, int *length)
{
    FILE *fl = fopen(path, "r");
    fseek(fl, 0, SEEK_END);
    long len = ftell(fl);
    unsigned char *ret = malloc(len);
    fseek(fl, 0, SEEK_SET);
    fread(ret, 1, len, fl);
    fclose(fl);
    if(length){
        *length=len;
    }
    return ret;
}

int get_current_time_string(char target[20])
{
    for (int i = 0; i < 20; i++)
    {
        target[i] = '0';
    }
    time_t timep;
    time(&timep);

    struct tm *p;
    p = gmtime(&timep);

    snprintf(target, 20, "%04d-%02d-%02d %02d:%02d:%02d", 1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday, 8 + p->tm_hour, p->tm_min, p->tm_sec);
    return 1;
}