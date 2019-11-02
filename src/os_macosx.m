/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */

/*
 * os_macosx.m -- Mac specific things for Mac OS X.
 */

/* Suppress compiler warnings to non-C89 code. */
#if defined(__clang__) && defined(__STRICT_ANSI__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wc99-extensions"
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wdeclaration-after-statement"
#endif

/* Avoid a conflict for the definition of Boolean between Mac header files and
 * X11 header files. */
#define NO_X11_INCLUDES

#include "vim.h"
#import <AppKit/AppKit.h>

/* Lift the compiler warning suppression. */
#if defined(__clang__) && defined(__STRICT_ANSI__)
# pragma clang diagnostic pop
# pragma clang diagnostic pop
#endif
