/*
  memqueue
  (C) 2011  Hasan Alayli <halayli@gmail.com>
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  http_str.h
*/
#ifndef _HTTP_STR_H_
#define _HTTP_STR_H_
#include <stdlib.h>
#include <stdint.h>

struct http_str {
    char     *str;
    size_t    len;
};

typedef struct http_str http_str_t;


#define LF     '\n'
#define CR     '\r'
#define CRLF   "\r\n"
#define LFLF   "\n\n"

extern const char is_host_token[256];
extern const char is_sep[256];
extern const char is_cr_or_lf[256];
extern const char is_digit[256];
extern const char is_host_terminator[256];
extern const char is_hex[256];
extern const char is_al[256];
extern const char is_al_cap[256];
extern const char is_space[256];

#define HTTP_IS_HOST_TERMINATOR(x) (is_host_terminator[((uint8_t)x)])
#define HTTP_IS_CR_OR_LF(x) (is_cr_or_lf[((uint8_t)x)])
#define HTTP_IS_DIGIT(x) (is_digit[((uint8_t)x)])
#define HTTP_IS_SEP(x) (is_sep[((uint8_t)x)])
#define HTTP_IS_HOST_TOKEN(x) (is_host_token[((uint8_t)x)])
#define HTTP_IS_HEX(x) (is_hex[((uint8_t)x)])
#define HTTP_IS_AL(x) (is_al[((uint8_t)x)])
#define HTTP_IS_AL_CAP(x) (is_al_cap[((uint8_t)x)])
#define HTTP_IS_SPACE(x) (is_space[((uint8_t)x)])

char    *http_strcasestr(char *s1, char *s2, int len, int len2);
char    *http_strcasechr(char *s1, char s2, int len);
int     http_strcasecmp(http_str_t *s1, http_str_t *s2);
long    http_strtol(char *s1, int len, int base);
int     http_strtol2(const char *s, int len);
void    http_print_exact(char *s, int len);
unsigned hash_str(char *s, int hash_len);

#define http_strcmp2(s1, s2) \
    (*(uint16_t *)(s1) == (*(uint16_t*)(s2)))

#define http_strcmp3(s1, s2) \
    ((*(uint32_t *)(s1) << 8) == (*(uint32_t *)(s2) << 8))

#define http_strcmp4(s1, s2) \
    (*(uint32_t *)(s1) == (*(uint32_t*)(s2)))

#define http_strcmp5(s1, s2) \
    (http_strcmp4(s1, s2) && s1[4] == s2[4])

#define http_strcmp6(s1, s2) \
    (http_strcmp4(s1, s2) && (http_strcmp2(s1 + 4, s2 + 4)))

#define http_strcmp7(s1, s2) \
    (http_strcmp6(s1, s2) && (s1)[6] == s2[6])

#define http_strcmp8(s1, s2) \
        http_strcmp4(s1, s2) && http_strcmp4(s1 + 4, s2 + 4)

#define http_strcmp9(s1, s2) \
        (http_strcmp4(s1, s2) && http_strcmp4(s1 + 4, s2 + 4) && (s1)[8] == s2[8])

#define http_tolower(c)      ((HTTP_IS_AL_CAP(c)) ? (c | 0x20) : c)
#define http_toupper(c)      ((HTTP_IS_AL(c)) ? (c & ~0x20) : c)


#define http_strcasecmp2(s1, c0, c1) \
    (http_toupper((s1)[0]) == c0 && http_toupper((s1)[1]) == c1)

#define http_strcasecmp3(s1, c0, c1, c2) \
    (http_strcasecmp2(s1, c0, c1) && http_toupper((s1)[2]) == c2)

#define http_strcasecmp4(s1, c0, c1, c2, c3) \
    (http_strcasecmp2(s1, c0, c1) && http_strcasecmp2(s1 + 2, c2, c3))

#define http_strcasecmp5(s1, c0, c1, c2, c3, c4) \
    (http_strcasecmp2(s1, c0, c1) && http_strcasecmp2(s1 + 2, c2, c3) && \
    http_toupper((s1)[4]) == http_toupper(c4))

#define http_strcasecmp6(s1, c0, c1, c2, c3, c4, c5) \
    (http_strcasecmp4(s1, c0, c1, c2, c3) && \
    http_strcasecmp2(s1 + 4, c4, c5))

#define http_strcasecmp7(s1, c0, c1, c2, c3, c4, c5, c6) \
    (http_strcasecmp4(s1, c0, c1, c2, c3) && \
    http_strcasecmp3(s1 + 4, c4, c5, c6))

#define http_strcasecmp8(s1, c0, c1, c2, c3, c4, c5, c6, c7) \
        http_strcasecmp4(s1, c0, c1, c2, c3) && \
        http_strcasecmp4(s1 + 4, c4, c5, c6, c7)

#define http_strcasecmp9(s1, c0, c1, c2, c3, c4, c5, c6, c7, c8) \
        (http_strcasecmp4(s1, c0, c1, c2, c3) && \
        http_strcasecmp4(s1 + 4, c4, c5, c6, c7) && http_toupper((s1)[8]) == c8)

#define http_strcasecmp10(s1, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9) \
        (http_strcasecmp5(s1, c0, c1, c2, c3, c4) && \
        http_strcasecmp5(s1 + 5, c5, c6, c7, c8, c9))

#define http_strcasecmp11(s1, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10) \
        (http_strcasecmp5(s1, c0, c1, c2, c3, c4) && \
        http_strcasecmp6(s1 + 5, c5, c6, c7, c8, c9, c10))

#define http_strcasecmp13(s1, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, \
    c10, c11, c12) \
        (http_strcasecmp6(s1, c0, c1, c2, c3, c4, c5) && \
        http_strcasecmp7(s1 + 6, c6, c7, c8, c9, c10, c11, c12))

#define http_strcasecmp15(s1, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, \
    c10, c11, c12, c13, c14) \
        (http_strcasecmp7(s1, c0, c1, c2, c3, c4, c5, c6) && \
        http_strcasecmp8(s1 + 7, c7, c8, c9, c10, c11, c12, c13, c14))

#define http_strcasecmp18(s1, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, \
    c10, c11, c12, c13, c14, c15, c16, c17) \
    (http_strcasecmp9(s1, c0, c1, c2, c3, c4, c5, c6, c7, c8) && \
    http_strcasecmp9(s1 + 9, c9, c10,  c11, c12, c13, c14, c15, c16, c17))

#endif
