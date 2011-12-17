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

  http_str.c
*/
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

#include "http_str.h"

const char is_host_terminator[256] = {[':'] = 1, ['/'] = 1, [' '] = 1};

const char is_cr_or_lf[256] = {[LF] = 1, [CR] = 1};

const char is_digit[256] = {
    ['0'] = 1, ['1'] = 1, ['2'] = 1, ['3'] = 1,
    ['4'] = 1, ['5'] = 1, ['6'] = 1, ['7'] = 1, ['8'] = 1, ['9'] = 1,
};

const char is_sep[256] = {[' '] = 1, ['\t'] = 1};

const char is_host_token[256] = {
    ['0' ... '9'] = 1,
    ['A' ... 'Z'] = 1,
    ['a' ... 'z'] = 1,
    ['.'] = 1,
    ['-'] = 1,
};

const char is_hex[256] = {
    ['x'] = 1, ['X'] = 1, ['0' ... '9'] = 1,
    ['a' ... 'f'] = 1, ['A' ... 'F'] = 1,
};

const char is_al[256] = {['a' ... 'z'] = 1};

const char is_al_cap[256] = {['A' ... 'Z'] = 1};

const char is_space[256] = {[' ']=1};

const char upper[256] = {
    ['\t']  = '\t',
    ['\n']  = '\n',
    ['\r']  = '\r',
    [' ']  = ' ',
    ['!']  = '!',
    ['"']  = '"',
    ['#']  = '#',
    ['$']  = '$',
    ['%']  = '%',
    ['&']  = '&',
    ['\'']  = '\'',
    ['(']  = '(',
    [')']  = ')',
    ['*']  = '*',
    ['+']  = '+',
    [',']  = ',',
    ['-']  = '-',
    ['.']  = '.',
    ['/']  = '/',
    ['0']  = '0',
    ['1']  = '1',
    ['2']  = '2',
    ['3']  = '3',
    ['4']  = '4',
    ['5']  = '5',
    ['6']  = '6',
    ['7']  = '7',
    ['8']  = '8',
    ['9']  = '9',
    [':']  = ':',
    [';']  = ';',
    ['<']  = '<',
    ['=']  = '=',
    ['>']  = '>',
    ['?']  = '?',
    ['@']  = '@',
    ['A']  = 'A',
    ['B']  = 'B',
    ['C']  = 'C',
    ['D']  = 'D',
    ['E']  = 'E',
    ['F']  = 'F',
    ['G']  = 'G',
    ['H']  = 'H',
    ['I']  = 'I',
    ['J']  = 'J',
    ['K']  = 'K',
    ['L']  = 'L',
    ['M']  = 'M',
    ['N']  = 'N',
    ['O']  = 'O',
    ['P']  = 'P',
    ['Q']  = 'Q',
    ['R']  = 'R',
    ['S']  = 'S',
    ['T']  = 'T',
    ['U']  = 'U',
    ['V']  = 'V',
    ['W']  = 'W',
    ['X']  = 'X',
    ['Y']  = 'Y',
    ['Z']  = 'Z',
    ['[']  = '[',
    ['\\']  = '\\',
    [']']  = ']',
    ['^']  = '^',
    ['_']  = '_',
    ['`']  = '`',
    ['a']  = 'A',
    ['b']  = 'B',
    ['c']  = 'C',
    ['d']  = 'D',
    ['e']  = 'E',
    ['f']  = 'F',
    ['g']  = 'G',
    ['h']  = 'H',
    ['i']  = 'I',
    ['j']  = 'J',
    ['k']  = 'K',
    ['l']  = 'L',
    ['m']  = 'M',
    ['n']  = 'N',
    ['o']  = 'O',
    ['p']  = 'P',
    ['q']  = 'Q',
    ['r']  = 'R',
    ['s']  = 'S',
    ['t']  = 'T',
    ['u']  = 'U',
    ['v']  = 'V',
    ['w']  = 'W',
    ['x']  = 'X',
    ['y']  = 'Y',
    ['z']  = 'Z',
    ['{']  = '{',
    ['|']  = '|',
    ['}']  = '}',
    ['~']  = '~',
};

#define to_upper(x) upper[(unsigned char)x]

int
http_strcasecmp(http_str_t *s1, http_str_t *s2)
{
    int i;
    if (s1->len != s2->len)
        return -1;

    i = s1->len - 1;
    while (i) {
        if (s1->str[i] != s2->str[i] && (s1->str[i] - 32) != s2->str[i])
            return -1;
        i--;
    }

    return 0;
}

char*
http_strcasestr(char *s1, char *s2, int len, int len2)
{
        int i = 0, j = 0;

        while (1) {
            /* find the first char that matches */
                while(to_upper(s1[i]) != to_upper(s2[0]) && i++ <= len);

        /* pattern cannot fit in what's remaining */
                if ((len - i) < len2)
                        return NULL;

                j = 0;

        while (to_upper(s1[i + j]) == to_upper(s2[j]) && j < len2)
            j++;

                if (j == len2) {
            return &s1[i];
        }
                i += j;
        }

        return NULL;
}

char*
http_strcasechr(char *s1, char c2, int len)
{
        int i = 0;

    while (i < len && s1[i] != c2)
        i++;

    if (i == len)
        return NULL;

        return &s1[i];
}

long
http_strtol(char *s1, int len, int base)
{
    const char *s;
    long acc = 0;
    char c;
    long cutoff;
    int neg, any, cutlim;

        /*
         * Skip white space and pick up leading +/- sign if any.
         * If base is 0, allow 0x for hex and 0 for octal, else
         * assume decimal; if base is already 16, allow 0x.
         */
    s = s1;
    //printf ("len is : %d, base is: %d\n", len, base);
    do {
        c = *s++;
    } while (HTTP_IS_SPACE(c) && &s1[len] > (s + 1));

    if (c == '-') {
        neg = 1;
        c = *s++;
    } else {
        neg = 0;
        if (c == '+')
            c = *s++;
    }

    if ((base == 0 || base == 16) && HTTP_IS_HEX(*s) && c == '0') {
        if (&s1[len] >= (s + 2))
            goto noconv;
        c = s[1];
        s += 2;
        base = 16;
    }
    if (base == 0)
        base = c == '0' ? 8 : 10;
    acc = any = 0;
    if (base < 2 || base > 36)
        goto noconv;

    cutoff = neg ? (unsigned long)-(LONG_MIN + LONG_MAX) + LONG_MAX
        : LONG_MAX;
    cutlim = cutoff % base;
    cutoff /= base;
    for ( ; &s1[len] >= s; c = *s++) {
        if (HTTP_IS_DIGIT(c))
            c -= '0';
        else if (HTTP_IS_AL_CAP(c))
            c -= 'A' - 10;
        else if (HTTP_IS_AL(c))
            c -= 'a' - 10;
        else
            break;
        if (c >= base)
            break;
        if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
            any = -1;
        else {
            any = 1;
            acc *= base;
            acc += c;
        }
    }
    if (any < 0) {
        acc = neg ? LONG_MIN : LONG_MAX;
        errno = ERANGE;
    } else if (!any) {
noconv:
        errno = EINVAL;
    } else if (neg)
        acc = -acc;

        return (acc);
}


void
http_print_exact(char *s, int len)
{
    int i;

    for (i = 0; i < len; i++)
        if (s[i] == '\0')
            printf("\\0");
        else if (s[i] == '\r')
            printf("\\r");
        else if (s[i] == ' ' || s[i] == '\t')
            printf(".");
        else if (s[i] == '\n') {
            printf("\\n");
            printf("\n");
        }
        else
            printf("%c", s[i]);
    printf("END\n");
}

unsigned
hash_str(char *s, int hash_length)
{
    unsigned hashval = 0;
    for (hashval = 0; *s != '\0'; s++)
        hashval = *s + 31 * hashval;
    return hashval % hash_length;
}

int http_strtol2(const char *s, int len)
{
    int i = 0;
    int j, k;

    if (len > 0) {
        if (*s != '-') {
            /* positive number */
            while (len-- > 0) {
                j = (*s++) - '0';
                k = i * 10;
                if (j > 9)
                    break;
                i = k + j;
            }
        } else {
            /* negative number */
            s++;
            while (--len > 0) {
                j = (*s++) - '0';
                k = i * 10;
                if (j > 9)
                    break;
                i = k - j;
            }
        }
    }
    return i;
}
