/*
 * gcc_crt.c  --  minimal C runtime for GCC + wlink builds of WarpNav
 *
 * kLIBC (libcn0.lib) is the normal provider of these symbols on ArcaOS,
 * but if it is not present on the build machine this file covers the small
 * subset of C library calls WarpNav actually uses.  It is compiled with
 * -fno-leading-underscore so every exported symbol matches the bare name
 * that GCC generates for its callers.
 *
 * sprintf supports: %s  %d  %u  %lu  %ld  %c  %%
 */

#include <stddef.h>
#include <stdarg.h>

/* ── memory / string ──────────────────────────────────────────────────────── */

size_t strlen(const char *s)
{
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

void *memset(void *dst, int c, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    while (n--) *d++ = (unsigned char)c;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char       *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s || d >= s + n) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n)
{
    char *d = dst;
    while (n && *src) { *d++ = *src++; n--; }
    while (n--)        *d++ = '\0';
    return dst;
}

char *strncat(char *dst, const char *src, size_t n)
{
    char *d = dst;
    while (*d) d++;
    while (n-- && *src) *d++ = *src++;
    *d = '\0';
    return dst;
}

int strcmp(const char *a, const char *b)
{
    for (; *a && (*a == *b); a++, b++) {}
    return (unsigned char)*a - (unsigned char)*b;
}

char *strrchr(const char *s, int c)
{
    const char *found = NULL;
    for (; *s; s++)
        if (*s == (char)c)
            found = s;
    return (c == '\0') ? (char *)s : (char *)found;
}

/* ── sprintf ──────────────────────────────────────────────────────────────── */

static void sp_char(char **pp, char c) { **pp = c; (*pp)++; }

static void sp_ulong(char **pp, unsigned long v)
{
    char tmp[20];
    int  n = 0;
    if (v == 0) { sp_char(pp, '0'); return; }
    while (v) { tmp[n++] = (char)('0' + (int)(v % 10)); v /= 10; }
    while (n > 0) sp_char(pp, tmp[--n]);
}

int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    char   *p = buf;

    va_start(ap, fmt);
    while (*fmt) {
        if (*fmt != '%') { sp_char(&p, *fmt++); continue; }
        fmt++;                          /* skip '%' */
        switch (*fmt++) {               /* read specifier, advance fmt */
        case 's': {
            const char *sv = va_arg(ap, const char *);
            if (!sv) sv = "";
            while (*sv) sp_char(&p, *sv++);
            break;
        }
        case 'd': {
            int v = va_arg(ap, int);
            if (v < 0) { sp_char(&p, '-'); v = -v; }
            sp_ulong(&p, (unsigned long)v);
            break;
        }
        case 'u':
            sp_ulong(&p, (unsigned long)va_arg(ap, unsigned int));
            break;
        case 'l':
            /* handle %ld and %lu */
            if (*fmt == 'd' || *fmt == 'u') {
                int unsign = (*fmt++ == 'u');
                if (unsign) {
                    sp_ulong(&p, va_arg(ap, unsigned long));
                } else {
                    long v = va_arg(ap, long);
                    if (v < 0) { sp_char(&p, '-'); v = -v; }
                    sp_ulong(&p, (unsigned long)v);
                }
            }
            break;
        case 'c':
            sp_char(&p, (char)va_arg(ap, int));
            break;
        case '%':
            sp_char(&p, '%');
            break;
        default:
            break;
        }
    }
    va_end(ap);
    sp_char(&p, '\0');
    return (int)(p - buf - 1);
}
