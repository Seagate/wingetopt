/* SPDX-License-Identifier: 0BSD AND BSD-2-Clause */
/* Modifications Copyright 2022 Seagate Technology and/or its Affiliates */

/*	$OpenBSD: getopt_long.c,v 1.32 2020/05/27 22:25:09 schwarze Exp $	*/
/*	$NetBSD: getopt_long.c,v 1.15 2002/01/31 22:43:40 tv Exp $	*/

/*
 * Copyright (c) 2002 Todd C. Miller <millert@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */
/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Dieter Baron and Thomas Klausner.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(HAVE_STD_INT) || (defined __STDC__ && defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L)
#include <stdint.h>
#elif !defined(_UINTPTR_T_DEFINED) && !defined(UINTPTR_MAX)
/* uintptr_t does not exist so need to define what type it is as best we can */
/* Checking known data model macros first and Windows predefined macros */
#if defined(__LP64__) || defined(_LP64) || defined(__ILP64__) || defined(_ILP64)
typedef unsigned long uintptr_t;
#define UINTPTR_MAX FFFFFFFFFFFFFFFFUL;
#elif defined(_WIN32)
#if defined(_WIN64)
typedef unsigned long long uintptr_t;
#define UINTPTR_MAX FFFFFFFFFFFFFFFFULL;
#else
typedef unsigned long uintptr_t;
#define UINTPTR_MAX FFFFFFFFUL;
#endif
#elif defined(__ILP32__) || defined(_ILP32) || defined(__LP32__) || defined(_LP32)
typedef unsigned long uintptr_t;
#define UINTPTR_MAX FFFFFFFFUL;
#else /* done checking macros for various known data models. Fall back to CPU macros */
#if defined(_WIN64) || defined(_M_IA64) || defined(_M_ALPHA) || defined(_M_X64) || defined(_M_AMD64) ||                \
    defined(__alpha__) || defined(__amd64__) || defined(__x86_64__) || defined(__aarch64__) || defined(__ia64__) ||    \
    defined(__IA64__) || defined(__powerpc64__) || defined(__PPC64__) || defined(__ppc64__) ||                         \
    defined(_ARCH_PPC64) // 64bit
/* 64bit and Windows already handled, so assuming unsigned long*/
typedef unsigned long uintptr_t;
#define UINTPTR_MAX FFFFFFFFFFFFFFFFUL
#else
/* 32bit, use unsigned long*/
typedef unsigned long uintptr_t;
#define UINTPTR_MAX FFFFFFFFUL;
#endif
#endif
#endif // checking for stdint
#if defined(_WIN32)
#if defined(_MSC_VER) && !defined(__clang__)
#define DISABLE_WARNING_4255 __pragma(warning(push)) __pragma(warning(disable : 4255))

#define RESTORE_WARNING_4255 __pragma(warning(pop))
#else
#define DISABLE_WARNING_4255
#define RESTORE_WARNING_4255
#endif //_MSVC && !clang workaround for Windows API headers
DISABLE_WARNING_4255
#include <windows.h>
RESTORE_WARNING_4255
#else
#include <libgen.h> /*for basename*/
#endif              /*_WIN32*/

#define REPLACE_GETOPT /* use this getopt as the system getopt(3) */

#ifdef REPLACE_GETOPT
int opterr = 1;   /* if error message should be printed */
int optind = 1;   /* index into parent argv vector */
int optopt = '?'; /* character checked for validity */
#if defined(__MINGW32__)
#undef optreset /* see getopt.h */
#define optreset __mingw_optreset
#endif          /*__MINGW32__*/
int   optreset; /* reset getopt */
char* optarg;   /* argument associated with option */
#endif          /*REPLACE_GETOPT*/

#define PRINT_ERROR ((opterr) && (*options != ':'))

#define FLAG_PERMUTE  0x01 /* permute non-options to the end of argv */
#define FLAG_ALLARGS  0x02 /* treat non-options as args to option "-1" */
#define FLAG_LONGONLY 0x04 /* operate as getopt_long_only */

/* return values */
#define BADCH   (int)'?'
#define BADARG  ((*options == ':') ? (int)':' : (int)'?')
#define INORDER (int)1

/*
 * expand this long list of definitions for systems that DO have __progname
 * create an elif defined list for systems that have something similar, but named differently or other functions
 * check UEFI first since it's cross compiled from Win or Lin which will contain other preprocessor flags
 * Try relying on what is provided by these systems instead of using our own global whenever necessary
 */
#if defined(HAS_GETPROGNAME) || defined(UEFI_C_SOURCE) || defined(__APPLE__)
/*
 * this case has the function getprogname available to use
 * note: I have found references that getprogname exists in solaris 11+, but using the getexecname instead for all
 * solaris versions at this time-TJE The BSDs also have this function. If necessary, add version checks to the previous
 * case to fall into here. I'm fairly certain those are not necessary at this point as the references I have point to
 * only needing this check for very old versions-TJE
 */
#if !defined(HAS_GETPROGNAME)
#define HAS_GETPROGNAME
#endif /*HAS_GETPROGNAME*/
#elif defined(HAS_PROGNAME) || defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) ||                   \
    defined(__NetBSD__) || defined(__DragonFly__) || defined(__QNX__) || defined(__CYGWIN__)
/*This case has the __progname available to use*/
#if defined(__CYGWIN__)
extern char __declspec(dllimport) * __progname; // NOLINT
#else
extern const char* __progname; // NOLINT
#endif
#if !defined(HAS_PROGNAME)
#define HAS_PROGNAME
#endif /*HAS_PROGNAME*/
#elif defined(HAS_ARGV0) || defined(_WIN32) && !defined(__CYGWIN__)
#if !defined(HAS_ARGV0)
#define HAS_ARGV0
#endif
#elif defined(HAS_GETEXECNAME) || defined(__sun)
#if !defined(HAS_GETEXECNAME)
#define HAS_GETEXECNAME
#endif /*HAS_GETEXECNAME*/
#else
#define NEED_PROGNAME
/*This will define our own global to store the programe name into -TJE*/
#endif /*Checking PROGNAME capabilities*/

#ifdef __CYGWIN__
static char EMSG[] = "";
#else
#define EMSG ""
#endif

static int  getopt_internal(int, char* const*, const char*, const struct option*, int*, int);
static int  parse_long_options(char* const*, const char*, const struct option*, int*, int, int);
static int  gcd(int, int);
static void permute_args(int, int, int, char* const*);

static char* place = (char*)(uintptr_t)EMSG; /* option letter processing */

/* XXX: set optreset to 1 rather than these two */
static int nonopt_start = -1; /* first non option argument (for permute) */
static int nonopt_end   = -1; /* first option after non options (for permute) */

/*
 * Due to Warning about non-const format string, using an enum since this code uses
 * it's own internal implementation of warnings/errors to select a constant string
 * This warning only happens in Clang, but rather than disabling an otherwise useful
 * warning, this enum can map to the correct warning/error output for us.
 */
typedef enum eGetoptErrorMessageEnum
{
    GETOPT_ERR_MSG_RECARGCHAR,
    GETOPT_ERR_MSG_RECARGSTRING,
    GETOPT_ERR_MSG_AMBIG,
    GETOPT_ERR_MSG_NOARG,
    GETOPT_ERR_MSG_ILLOPTCHAR,
    GETOPT_ERR_MSG_ILLOPTSTRING
} eGetoptErrorMessage;

#if defined(NEED_PROGNAME)
char* getopt_progname;
#endif /*NEED_PROGNAME*/

static char* getopt_getprogname(void)
{
    const char* progname = NULL;
#if defined(HAS_PROGNAME)
    progname = __progname;
#elif defined(HAS_ARGV0)
    /*Win32 most likely*/
#if defined(_MSC_VER)
    progname = __argv[0];
#else  /*mingw???*/
    progname = __argv[0];
#endif /*_MSC_VER*/
#elif defined(HAS_GETPROGNAME)
    progname = getprogname();
#elif defined(HAS_GETEXECNAME)
    progname = getexecname();
    if (progname != NULL)
    {
        char* execfullname = strdup(progname);
        char* execname = strdup(basename(execfullname)); /* basename can return internal pointers, modified memory, may
                                                            get changed, so dup it to return this just in case -TJE */
        free(execfullname);
        return execname;
    }
#elif defined(NEED_PROGNAME)
    /* own global declared that can be accessed -TJE */
    progname = getopt_progname;
#else /*This is the "we don't know how to get this" case. */
#endif
    if (progname == NULL)
    {
#if defined(_DEBUG)
#if defined(_MSC_VER)
        return _strdup("Unknown progname");
#else
        return strdup("Unknown progname");
#endif
#else
#if defined(_MSC_VER)
        return _strdup("");
#else
        return strdup("");
#endif
#endif
    }
    else
    {
#if defined(_MSC_VER)
        return _strdup(progname);
#else
        return strdup(progname);
#endif
    }
}

/*
 * some systems have warnx, _vwarnx from err.h, but not all have this.
 * defining our own versions here to help prevent them from coliding. -TJE
 */

static void getopt_vwarnx(eGetoptErrorMessage errmsg, va_list ap)
{
    char* progname = getopt_getprogname();
#if (defined(HAVE_FPRINTF_S) && defined(HAVE_VFPRINTF_S)) ||                                                           \
    (defined(_WIN32) && defined(_MSC_VER) && defined(__STDC_SECURE_LIB__)) ||                                          \
    (defined(__STDC_LIB_EXT1__) && defined(__STDC_WANT_LIB_EXT1__))
    /*Use MSFT's/C11 Annex K's _s functions for fprintf and vfprintf*/
    if (progname)
    {
        (void)fprintf_s(stderr, "%s: ", progname);
    }
    switch (errmsg)
    {
    case GETOPT_ERR_MSG_RECARGCHAR:
        (void)vfprintf_s(stderr, "option requires an argument -- %c", ap);
        break;
    case GETOPT_ERR_MSG_RECARGSTRING:
        (void)vfprintf_s(stderr, "option requires an argument -- %s", ap);
        break;
    case GETOPT_ERR_MSG_AMBIG:
        (void)vfprintf_s(stderr, "ambiguous option -- %.*s", ap);
        break;
    case GETOPT_ERR_MSG_NOARG:
        (void)vfprintf_s(stderr, "option doesn't take an argument -- %.*s", ap);
        break;
    case GETOPT_ERR_MSG_ILLOPTCHAR:
        (void)vfprintf_s(stderr, "unknown option -- %c", ap);
        break;
    case GETOPT_ERR_MSG_ILLOPTSTRING:
        (void)vfprintf_s(stderr, "unknown option -- %s", ap);
        break;
    }
    (void)fprintf_s(stderr, "\n");
#else  /* MSFT's/C11 Annex K's _s functions not available/used */
    if (progname)
    {
        (void)fprintf(stderr, "%s: ", progname);
    }
    switch (errmsg)
    {
    case GETOPT_ERR_MSG_RECARGCHAR:
        (void)vfprintf(stderr, "option requires an argument -- %c", ap);
        break;
    case GETOPT_ERR_MSG_RECARGSTRING:
        (void)vfprintf(stderr, "option requires an argument -- %s", ap);
        break;
    case GETOPT_ERR_MSG_AMBIG:
        (void)vfprintf(stderr, "ambiguous option -- %.*s", ap);
        break;
    case GETOPT_ERR_MSG_NOARG:
        (void)vfprintf(stderr, "option doesn't take an argument -- %.*s", ap);
        break;
    case GETOPT_ERR_MSG_ILLOPTCHAR:
        (void)vfprintf(stderr, "unknown option -- %c", ap);
        break;
    case GETOPT_ERR_MSG_ILLOPTSTRING:
        (void)vfprintf(stderr, "unknown option -- %s", ap);
        break;
    }
    (void)fprintf(stderr, "\n");
#endif // MSFT secure lib
    if (progname)
    {
        free(progname);
        progname = NULL;
    }
}

static void getopt_warnx(eGetoptErrorMessage errmsg, ...)
{
    va_list ap;
    va_start(ap, errmsg);
    getopt_vwarnx(errmsg, ap);
    va_end(ap);
}

/*
 * Compute the greatest common divisor of a and b.
 */
static int gcd(int a, int b)
{
    int c;

    c = a % b;
    while (c != 0)
    {
        a = b;
        b = c;
        c = a % b;
    }

    return (b);
}

static size_t getopt_strlen(const char* str)
{
    if (str)
    {
#if defined(RSIZE_MAX)
        const char* found = memchr(str, '\0', RSIZE_MAX);
#elif defined(SIZE_MAX)
        // Note dividing SIZE_MAX by 2 to prevent a warning about
        // stringop maximum object size.
        const char* found = memchr(str, '\0', SIZE_MAX >> 1);
#else
        // Note dividing SIZE_MAX by 2 to prevent a warning about
        // stringop maximum object size.
        const char* found = memchr(str, '\0', ((size_t)(-1)) >> 1);
#endif
        if (found != NULL)
        {
            return (size_t)((uintptr_t)found - (uintptr_t)str);
        }
        else
        {
#if defined(RSIZE_MAX)
            return RSIZE_MAX;
#elif defined(SIZE_MAX)
            return SIZE_MAX >> 1;
#else
            return ((size_t)(-1)) >> 1;
#endif
        }
    }
    else
    {
        return 0;
    }
}

/*
 * Exchange the block from nonopt_start to nonopt_end with the block
 * from nonopt_end to opt_end (keeping the same order of arguments
 * in each block).
 */
static void permute_args(int panonopt_start, int panonopt_end, int opt_end, char* const* nargv)
{
    int   cstart, cyclelen, i, j, ncycle, nnonopts, nopts, pos;
    char* swap;

    /*
     * compute lengths of blocks and number and size of cycles
     */
    nnonopts = panonopt_end - panonopt_start;
    nopts    = opt_end - panonopt_end;
    ncycle   = gcd(nnonopts, nopts);
    cyclelen = (opt_end - panonopt_start) / ncycle;

    for (i = 0; i < ncycle; i++)
    {
        cstart = panonopt_end + i;
        pos    = cstart;
        for (j = 0; j < cyclelen; j++)
        {
            if (pos >= panonopt_end)
                pos -= nnonopts;
            else
                pos += nopts;
            swap = nargv[pos];
            /* LINTED const cast */
            /* to perform a const case in C, first cast to uintptr_t, then the necessary type */
            /* -Wcast-qual will not warn about this when done this way */
            ((char**)(uintptr_t)nargv)[pos] = nargv[cstart];
            /* LINTED const cast */
            /* to perform a const case in C, first cast to uintptr_t, then the necessary type */
            /* -Wcast-qual will not warn about this when done this way */
            ((char**)(uintptr_t)nargv)[cstart] = swap;
        }
    }
}

/*
 * parse_long_options --
 *	Parse long options in argc/argv argument vector.
 * Returns -1 if short_too is set and the option does not match long_options.
 */
static int parse_long_options(char* const*         nargv,
                              const char*          options,
                              const struct option* long_options,
                              int*                 idx,
                              int                  short_too,
                              int                  flags)
{
    char * current_argv, *has_equal;
    size_t current_argv_len;
    int    i, match, exact_match, second_partial_match;

    current_argv         = place;
    match                = -1;
    exact_match          = 0;
    second_partial_match = 0;

    optind++;

    if ((has_equal = strchr(current_argv, '=')) != NULL)
    {
        /* argument found (--option=arg) */
        current_argv_len = (uintptr_t)has_equal - (uintptr_t)current_argv;
        has_equal++;
    }
    else
        current_argv_len = getopt_strlen(current_argv);

    for (i = 0; long_options[i].name; i++)
    {
        /* find matching long option */
        if (strncmp(current_argv, long_options[i].name, current_argv_len) != 0)
            continue;

        if (getopt_strlen(long_options[i].name) == current_argv_len)
        {
            /* exact match */
            match       = i;
            exact_match = 1;
            break;
        }
        /*
         * If this is a known short option, don't allow
         * a partial match of a single character.
         */
        if (short_too && current_argv_len == 1)
            continue;

        if (match == -1) /* first partial match */
            match = i;
        else if ((flags & FLAG_LONGONLY) || long_options[i].has_arg != long_options[match].has_arg ||
                 long_options[i].flag != long_options[match].flag || long_options[i].val != long_options[match].val)
            second_partial_match = 1;
    }
    if (!exact_match && second_partial_match)
    {
        /* ambiguous abbreviation */
        if (PRINT_ERROR)
            getopt_warnx(GETOPT_ERR_MSG_AMBIG, (int)current_argv_len, current_argv);
        optopt = 0;
        return (BADCH);
    }
    if (match != -1)
    { /* option found */
        if (long_options[match].has_arg == no_argument && has_equal)
        {
            if (PRINT_ERROR)
                getopt_warnx(GETOPT_ERR_MSG_NOARG, (int)current_argv_len, current_argv);
            /*
             * XXX: GNU sets optopt to val regardless of flag
             */
            if (long_options[match].flag == NULL)
                optopt = long_options[match].val;
            else
                optopt = 0;
            return (BADARG);
        }
        if (long_options[match].has_arg == required_argument || long_options[match].has_arg == optional_argument)
        {
            if (has_equal)
                optarg = has_equal;
            else if (long_options[match].has_arg == required_argument)
            {
                /*
                 * optional argument doesn't use next nargv
                 */
                optarg = nargv[optind++];
            }
        }
        if ((long_options[match].has_arg == required_argument) && (optarg == NULL))
        {
            /*
             * Missing argument; leading ':' indicates no error
             * should be generated.
             */
            if (PRINT_ERROR)
                getopt_warnx(GETOPT_ERR_MSG_RECARGSTRING, current_argv);
            /*
             * XXX: GNU sets optopt to val regardless of flag
             */
            if (long_options[match].flag == NULL)
                optopt = long_options[match].val;
            else
                optopt = 0;
            --optind;
            return (BADARG);
        }
    }
    else
    { /* unknown option */
        if (short_too)
        {
            --optind;
            return (-1);
        }
        if (PRINT_ERROR)
            getopt_warnx(GETOPT_ERR_MSG_ILLOPTSTRING, current_argv);
        optopt = 0;
        return (BADCH);
    }
    if (idx)
        *idx = match;
    if (long_options[match].flag)
    {
        *long_options[match].flag = long_options[match].val;
        return (0);
    }
    else
        return (long_options[match].val);
}

static const char* posixlycorrectenv = "POSIXLY_CORRECT";

/*
 * getopt_internal --
 *	Parse argc/argv argument vector.  Called by user level routines.
 */
static int getopt_internal(int                  nargc,
                           char* const*         nargv,
                           const char*          options,
                           const struct option* long_options,
                           int*                 idx,
                           int                  flags)
{
    const char* oli; /* option letter list index */
    int         optchar, short_too;
    static int  posixly_correct = -1;

#if defined(NEED_PROGNAME)
    /* store progam name before any other parsing is done */
    getopt_progname = nargv[0];
#endif // NEED_PROGNAME

    if (options == NULL)
        return (-1);

    /*
     * XXX Some GNU programs (like cvs) set optind to 0 instead of
     * XXX using optreset.  Work around this braindamage.
     */
    if (optind == 0)
        optind = optreset = 1;

    /*
     * Disable GNU extensions if POSIXLY_CORRECT is set or options
     * string begins with a '+'.
     *
     * CV, 2009-12-14: Check POSIXLY_CORRECT anew if optind == 0 or
     *                 optreset != 0 for GNU compatibility.
     */
    if (posixly_correct == -1 || optreset != 0)
    {
#if defined(HAVE_GETENV_S) || (defined(_WIN32) && defined(_MSC_VER) && defined(__STDC_SECURE_LIB__)) ||                \
    (defined(__STDC_LIB_EXT1__) && defined(__STDC_WANT_LIB_EXT1__))
        /* MSFT/C11 annex K adds getenv_s, so use it when available to check if this exists */
        size_t size = 0;
        if (getenv_s(&size, NULL, 0, posixlycorrectenv) == 0)
        {
            /*
             * You can allocate a buffer based off of size and call it again to read it,
             * however, this is not necessary. We just need to know if this exists or not
             * since that is how to getenv line below was set to work before this _s function was added.
             */
            posixly_correct = 1;
        }
        else
        {
            posixly_correct = 0;
        }
#elif defined(HAVE_SECURE_GETENV) && !defined(DISABLE_SECURE_GETENV)
        /*
         * Use secure_getenv, unless the DISABLE_SECURE_GETENV is defined
         * secure_getenv (when available) is used by default unless DISABLE_SECURE_GETENV is defined
         * by the person building this library.
         * See https://linux.die.net/man/3/secure_getenv for reasons to disable it.
         */
        posixly_correct = (secure_getenv(posixlycorrectenv) != NULL);
#elif defined(HAVE___SECURE_GETENV) && !defined(DISABLE_SECURE_GETENV)
        /*
         * Use secure_getenv, unless the DISABLE_SECURE_GETENV is defined
         * secure_getenv (when available) is used by default unless DISABLE_SECURE_GETENV is defined
         * by the person building this library.
         * See https://linux.die.net/man/3/secure_getenv for reasons to disable it.
         */
        posixly_correct = (__secure_getenv(posixlycorrectenv) != NULL);
#else
        posixly_correct = (getenv(posixlycorrectenv) != NULL);
#endif
    }
    if (*options == '-')
        flags |= FLAG_ALLARGS;
    else if (posixly_correct || *options == '+')
        flags &= ~FLAG_PERMUTE;
    if (*options == '+' || *options == '-')
        options++;

    optarg = NULL;
    if (optreset)
        nonopt_start = nonopt_end = -1;
start:
    if (optreset || !*place)
    { /* update scanning pointer */
        optreset = 0;
        if (optind >= nargc)
        { /* end of argument vector */
            place = (char*)(uintptr_t)EMSG;
            if (nonopt_end != -1)
            {
                /* do permutation, if we have to */
                permute_args(nonopt_start, nonopt_end, optind, nargv);
                optind -= nonopt_end - nonopt_start;
            }
            else if (nonopt_start != -1)
            {
                /*
                 * If we skipped non-options, set optind
                 * to the first of them.
                 */
                optind = nonopt_start;
            }
            nonopt_start = nonopt_end = -1;
            return (-1);
        }
        if (*(place = nargv[optind]) != '-' || (place[1] == '\0' && strchr(options, '-') == NULL))
        {
            place = (char*)(uintptr_t)EMSG; /* found non-option */
            if (flags & FLAG_ALLARGS)
            {
                /*
                 * GNU extension:
                 * return non-option as argument to option 1
                 */
                optarg = nargv[optind++];
                return (INORDER);
            }
            if (!(flags & FLAG_PERMUTE))
            {
                /*
                 * If no permutation wanted, stop parsing
                 * at first non-option.
                 */
                return (-1);
            }
            /* do permutation */
            if (nonopt_start == -1)
                nonopt_start = optind;
            else if (nonopt_end != -1)
            {
                permute_args(nonopt_start, nonopt_end, optind, nargv);
                nonopt_start = optind - (nonopt_end - nonopt_start);
                nonopt_end   = -1;
            }
            optind++;
            /* process next argument */
            goto start;
        }
        if (nonopt_start != -1 && nonopt_end == -1)
            nonopt_end = optind;

        /*
         * If we have "-" do nothing, if "--" we are done.
         */
        if (place[1] != '\0' && *++place == '-' && place[1] == '\0')
        {
            optind++;
            place = (char*)(uintptr_t)EMSG;
            /*
             * We found an option (--), so if we skipped
             * non-options, we have to permute.
             */
            if (nonopt_end != -1)
            {
                permute_args(nonopt_start, nonopt_end, optind, nargv);
                optind -= nonopt_end - nonopt_start;
            }
            nonopt_start = nonopt_end = -1;
            return (-1);
        }
    }

    /*
     * Check long options if:
     *  1) we were passed some
     *  2) the arg is not just "-"
     *  3) either the arg starts with -- we are getopt_long_only()
     */
    if (long_options != NULL && place != nargv[optind] && (*place == '-' || (flags & FLAG_LONGONLY)))
    {
        short_too = 0;
        if (*place == '-')
            place++; /* --foo long option */
        else if (*place != ':' && strchr(options, *place) != NULL)
            short_too = 1; /* could be short option too */

        optchar = parse_long_options(nargv, options, long_options, idx, short_too, flags);
        if (optchar != -1)
        {
            place = (char*)(uintptr_t)EMSG;
            return (optchar);
        }
    }

    if ((optchar = (int)*place++) == (int)':' || (optchar == (int)'-' && *place != '\0') ||
        (oli = strchr(options, optchar)) == NULL)
    {
        /*
         * If the user specified "-" and  '-' isn't listed in
         * options, return -1 (non-option) as per POSIX.
         * Otherwise, it is an unknown option character (or ':').
         */
        if (optchar == (int)'-' && *place == '\0')
            return (-1);
        if (!*place)
            ++optind;
        if (PRINT_ERROR)
            getopt_warnx(GETOPT_ERR_MSG_ILLOPTCHAR, optchar);
        optopt = optchar;
        return (BADCH);
    }
    if (long_options != NULL && optchar == 'W' && oli[1] == ';')
    {
        /* -W long-option */
        if (*place) /* no space */
            /* NOTHING */;
        else if (++optind >= nargc)
        { /* no arg */
            place = (char*)(uintptr_t)EMSG;
            if (PRINT_ERROR)
                getopt_warnx(GETOPT_ERR_MSG_RECARGCHAR, optchar);
            optopt = optchar;
            return (BADARG);
        }
        else /* white space */
            place = nargv[optind];
        optchar = parse_long_options(nargv, options, long_options, idx, 0, flags);
        place   = (char*)(uintptr_t)EMSG;
        return (optchar);
    }
    if (*++oli != ':')
    { /* doesn't take argument */
        if (!*place)
            ++optind;
    }
    else
    { /* takes (optional) argument */
        optarg = NULL;
        if (*place) /* no white space */
            optarg = place;
        else if (oli[1] != ':')
        { /* arg not optional */
            if (++optind >= nargc)
            { /* no arg */
                place = (char*)(uintptr_t)EMSG;
                if (PRINT_ERROR)
                    getopt_warnx(GETOPT_ERR_MSG_RECARGCHAR, optchar);
                optopt = optchar;
                return (BADARG);
            }
            else
                optarg = nargv[optind];
        }
        place = (char*)(uintptr_t)EMSG;
        ++optind;
    }
    /* dump back option letter */
    return (optchar);
}

#ifdef REPLACE_GETOPT
/*
 * getopt --
 *	Parse argc/argv argument vector.
 *
 * [eventually this will replace the BSD getopt]
 */
int getopt(int nargc, char* const* nargv, const char* options)
{

    /*
     * We don't pass FLAG_PERMUTE to getopt_internal() since
     * the BSD getopt(3) (unlike GNU) has never done this.
     *
     * Furthermore, since many privileged programs call getopt()
     * before dropping privileges it makes sense to keep things
     * as simple (and bug-free) as possible.
     */
    return (getopt_internal(nargc, nargv, options, NULL, NULL, 0));
}
#endif /* REPLACE_GETOPT */

/*
 * getopt_long --
 *	Parse argc/argv argument vector.
 */
int getopt_long(int nargc, char* const* nargv, const char* options, const struct option* long_options, int* idx)
{

    return (getopt_internal(nargc, nargv, options, long_options, idx, FLAG_PERMUTE));
}

/*
 * getopt_long_only --
 *	Parse argc/argv argument vector.
 */
int getopt_long_only(int nargc, char* const* nargv, const char* options, const struct option* long_options, int* idx)
{

    return (getopt_internal(nargc, nargv, options, long_options, idx, FLAG_PERMUTE | FLAG_LONGONLY));
}
