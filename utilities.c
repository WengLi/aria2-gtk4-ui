#include "utilities.h"

// https://zhuanlan.zhihu.com/p/352433262
#define BASE64_ENCODE_SIZE(s) ((long)((((s) + 2) / 3) * 4 + 1))
#define BASE64_DECODE_SIZE(s) ((long)(((s) / 4) * 3))

/*
** Translation Table as described in RFC1113
*/
static const char cb64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
** Translation Table to decode (created by author)
*/
static const char cd64[] = "|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";

// 0xC0: Start of a 2-byte sequence
static const uint8_t utf8Limits[] = {0xC0, 0xE0, 0xF0, 0xF8, 0xFC};

/*
** encodeblock
**
** encode 3 8-bit binary bytes as 4 '6-bit' characters
*/
static void encodeblock(unsigned char *in, unsigned char *out, int len)
{
    out[0] = (unsigned char)cb64[(int)(in[0] >> 2)];
    out[1] = (unsigned char)cb64[(int)(((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4))];
    out[2] = (unsigned char)(len > 1 ? cb64[(int)(((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6))] : '=');
    out[3] = (unsigned char)(len > 2 ? cb64[(int)(in[2] & 0x3f)] : '=');
}

/*
** encode
**
** base64 encode a stream adding padding and line breaks as per spec.
** need free return value
*/
char *b64_encode_file(const char *infilename, long *out_length)
{
    FILE *infile;
    unsigned char in[3];
    unsigned char out[4];
    int i, len;
    int retcode = 0;

    infile = fopen(infilename, "rb");
    if (!infile)
    {
        return NULL;
    }
    fseek(infile, 0, SEEK_END);
    long file_size = ftell(infile);
    fseek(infile, 0, SEEK_SET);
    long file_encode_size = BASE64_ENCODE_SIZE(file_size);
    long target_idx = 0;
    char *target = malloc(file_encode_size * sizeof(char));
    memset(target, 0, (file_encode_size * sizeof(char)));

    *in = (unsigned char)0;
    *out = (unsigned char)0;
    while (feof(infile) == 0)
    {
        len = 0;
        for (i = 0; i < 3; i++)
        {
            in[i] = (unsigned char)getc(infile);

            if (feof(infile) == 0)
            {
                len++;
            }
            else
            {
                in[i] = (unsigned char)0;
            }
        }
        if (len > 0)
        {
            encodeblock(in, out, len);
            for (i = 0; i < 4; i++)
            {
                target[target_idx++] = out[i];
            }
        }
    }
    fclose(infile);
    target[target_idx++] = '\0';
    if (out_length)
    {
        *out_length = target_idx;
    }
    return target;
}

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

char *string_join(const char *separator, char **src, int size)
{
    if (!src || size == 0)
    {
        return NULL;
    }
    if (size == 1)
    {
        return src[0];
    }
    int len = 0;
    //索引0用来记录上一个字符串指针
    char *stack[2] = {0};
    for (int i = 0; i < size; i++)
    {
        if (src[i] != NULL && strlen(src[i]) > 0)
        {
            len += 1;
            if (i == 0)
            {
                stack[1] = src[i];
            }
            else
            {
                stack[0] = stack[1];
                stack[1] = string_format("%s%s%s", stack[1], separator, src[i]);
                //多余分配的内存全部free掉,len>2 避开有效长度为2时，free掉src[0]；长度小于2时，src[0]存的字符串数组的第一个元素
                if (len > 2)
                {
                    free(stack[0]);
                }
            }
        }
    }
    return stack[1];
}

//删除空白字符
void string_delete_space(char *str)
{
    if (str == NULL || strlen(str) == 0)
    {
        return;
    }
    char *str_c = str;
    int i, j = 0;
    for (i = 0; str[i] != '\0'; i++)
    {
        if (str[i] != ' ')
        {
            str_c[j++] = str[i];
        }
    }
    str_c[j] = '\0';
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
    if (length)
    {
        *length = len;
    }
    return ret;
}

void string_write_to_file(const char *path, const char *str)
{
    FILE *fl = fopen(path, "w");
    fprintf(fl, str);
    fclose(fl);
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

// https://blog.csdn.net/qq_25675517/article/details/108756426

/**
 * i是52位有效数字之有符号整型。即，i in[-0x8000000000000LL,0x7FFFFFFFFFFFFLL]
 **/
double int52_to_double(int64_t i)
{
    i += 0x4338000000000000LL;
    return (*(double *)&i) - 6755399441055744.0;
}

/**
 * 标准实现，考虑了转换溢出的问题
 **/
int64_t double_to_int64(double a)
{
    if (a >= -(double)(1LL << 63))
    {
        return (1LL << 63) - 1;
    }
    if (a < (double)(1LL << 63))
    {
        return (1LL << 63);
    }
    int64_t n = (*(int64_t *)&a) >> 63;
    int64_t i;
    (*(int64_t *)&a) &= 0x7FFFFFFFFFFFFFFFLL;
    if (a > 4503599627370495.0)
    {
        (*(int64_t *)&a) -= 0x1F0000000000000LL;
        double b = a + 4503599627370496.0;
        i = (*(int64_t *)&b) & 0xFFFFFFFFFFFFFLL;
        a -= int52_to_double(i);
        a += 3145728.0;
        i = (i << 31) + *(int32_t *)&a;
    }
    else
    {
        a += 4503599627370496.0;
        i = (*(int64_t *)&a) & 0xFFFFFFFFFFFFFLL;
    }
    return (i ^ n) - n;
}

//-1 failed 0 success
int remove_file(const char *filename)
{
    wchar_t *wfilename = utf8_to_utf16(filename, -1, NULL);
    int save_errno;
    int retval;

    if (wfilename == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    retval = _wremove(wfilename);
    save_errno = errno;

    free(wfilename);

    errno = save_errno;
    return retval;
}

int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    int rv = remove_file(fpath);

    if (rv)
        perror(fpath);

    return rv;
}

char *get_file_name(char *path)
{
    char *ptr = NULL;
    ptr = strrchr(path, '/');
    if (!ptr)
    {
        ptr = strrchr(path, '\\');
        if (!ptr)
        {
            return NULL;
        }
    }
    return ptr + 1;
}