/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */

/*
 * winclip.c
 *
 * Routines for Win32 clipboard handling.
 * Also used by Cygwin, using os_unix.c.
 */

#include "vim.h"
#include "vimio.h"

/*
 * Compile only the clipboard handling features when compiling for cygwin
 * posix environment.
 */
#ifdef FEAT_CYGWIN_WIN32_CLIPBOARD
#define WIN32_LEAN_AND_MEAN
#include "winclip.pro"
#include <windows.h>
#endif

/*
 * When generating prototypes for Win32 on Unix, these lines make the syntax
 * errors disappear.  They do not need to be correct.
 */
#ifdef PROTO
#define WINAPI
#define WINBASEAPI
typedef int DWORD;
typedef int LPBOOL;
typedef int LPCSTR;
typedef int LPCWSTR;
typedef int LPSTR;
typedef int LPWSTR;
typedef int UINT;
#endif

/*
 * Convert an UTF-8 string to UTF-16.
 * "instr[inlen]" is the input.  "inlen" is in bytes.
 * When "outstr" is NULL only return the number of UTF-16 words produced.
 * Otherwise "outstr" must be a buffer of sufficient size.
 * Returns the number of UTF-16 words produced.
 */
int utf8_to_utf16(char_u *instr, int inlen, short_u *outstr, int *unconvlenp)
{
  int outlen = 0;
  char_u *p = instr;
  int todo = inlen;
  int l;
  int ch;

  while (todo > 0)
  {
    /* Only convert if we have a complete sequence. */
    l = utf_ptr2len_len(p, todo);
    if (l > todo)
    {
      /* Return length of incomplete sequence. */
      if (unconvlenp != NULL)
        *unconvlenp = todo;
      break;
    }

    ch = utf_ptr2char(p);
    if (ch >= 0x10000)
    {
      /* non-BMP character, encoding with surrogate pairs */
      ++outlen;
      if (outstr != NULL)
      {
        *outstr++ = (0xD800 - (0x10000 >> 10)) + (ch >> 10);
        *outstr++ = 0xDC00 | (ch & 0x3FF);
      }
    }
    else if (outstr != NULL)
      *outstr++ = ch;
    ++outlen;
    p += l;
    todo -= l;
  }

  return outlen;
}

/*
 * Convert an UTF-16 string to UTF-8.
 * The input is "instr[inlen]" with "inlen" in number of UTF-16 words.
 * When "outstr" is NULL only return the required number of bytes.
 * Otherwise "outstr" must be a buffer of sufficient size.
 * Return the number of bytes produced.
 */
int utf16_to_utf8(short_u *instr, int inlen, char_u *outstr)
{
  int outlen = 0;
  int todo = inlen;
  short_u *p = instr;
  int l;
  int ch, ch2;

  while (todo > 0)
  {
    ch = *p;
    if (ch >= 0xD800 && ch <= 0xDBFF && todo > 1)
    {
      /* surrogate pairs handling */
      ch2 = p[1];
      if (ch2 >= 0xDC00 && ch2 <= 0xDFFF)
      {
        ch = ((ch - 0xD800) << 10) + (ch2 & 0x3FF) + 0x10000;
        ++p;
        --todo;
      }
    }
    if (outstr != NULL)
    {
      l = utf_char2bytes(ch, outstr);
      outstr += l;
    }
    else
      l = utf_char2len(ch);
    ++p;
    outlen += l;
    --todo;
  }

  return outlen;
}

/*
 * Call MultiByteToWideChar() and allocate memory for the result.
 * Returns the result in "*out[*outlen]" with an extra zero appended.
 * "outlen" is in words.
 */
void MultiByteToWideChar_alloc(UINT cp, DWORD flags,
                               LPCSTR in, int inlen,
                               LPWSTR *out, int *outlen)
{
  *outlen = MultiByteToWideChar(cp, flags, in, inlen, 0, 0);
  /* Add one one word to avoid a zero-length alloc(). */
  *out = ALLOC_MULT(WCHAR, *outlen + 1);
  if (*out != NULL)
  {
    MultiByteToWideChar(cp, flags, in, inlen, *out, *outlen);
    (*out)[*outlen] = 0;
  }
}

/*
 * Call WideCharToMultiByte() and allocate memory for the result.
 * Returns the result in "*out[*outlen]" with an extra NUL appended.
 */
void WideCharToMultiByte_alloc(UINT cp, DWORD flags,
                               LPCWSTR in, int inlen,
                               LPSTR *out, int *outlen,
                               LPCSTR def, LPBOOL useddef)
{
  *outlen = WideCharToMultiByte(cp, flags, in, inlen, NULL, 0, def, useddef);
  /* Add one one byte to avoid a zero-length alloc(). */
  *out = alloc(*outlen + 1);
  if (*out != NULL)
  {
    WideCharToMultiByte(cp, flags, in, inlen, *out, *outlen, def, useddef);
    (*out)[*outlen] = 0;
  }
}

/*
 * Note: the following two functions are only guaranteed to work when using
 * valid MS-Windows codepages or when iconv() is available.
 */

/*
 * Convert "str" from 'encoding' to UTF-16.
 * Input in "str" with length "*lenp".  When "lenp" is NULL, use strlen().
 * Output is returned as an allocated string.  "*lenp" is set to the length of
 * the result.  A trailing NUL is always added.
 * Returns NULL when out of memory.
 */
short_u *
enc_to_utf16(char_u *str, int *lenp)
{
  vimconv_T conv;
  WCHAR *ret;
  char_u *allocbuf = NULL;
  int len_loc;
  int length;

  if (lenp == NULL)
  {
    len_loc = (int)STRLEN(str) + 1;
    lenp = &len_loc;
  }

  if (enc_codepage > 0)
  {
    /* We can do any CP### -> UTF-16 in one pass, and we can do it
	 * without iconv() (convert_* may need iconv). */
    MultiByteToWideChar_alloc(enc_codepage, 0, (LPCSTR)str, *lenp,
                              &ret, &length);
  }
  else
  {
    /* Use "latin1" by default, we might be called before we have p_enc
	 * set up.  Convert to utf-8 first, works better with iconv().  Does
	 * nothing if 'encoding' is "utf-8". */
    conv.vc_type = CONV_NONE;
    if (convert_setup(&conv, p_enc ? p_enc : (char_u *)"latin1",
                      (char_u *)"utf-8") == FAIL)
      return NULL;
    if (conv.vc_type != CONV_NONE)
    {
      str = allocbuf = string_convert(&conv, str, lenp);
      if (str == NULL)
        return NULL;
    }
    convert_setup(&conv, NULL, NULL);

    length = utf8_to_utf16(str, *lenp, NULL, NULL);
    ret = ALLOC_MULT(WCHAR, length + 1);
    if (ret != NULL)
    {
      utf8_to_utf16(str, *lenp, (short_u *)ret, NULL);
      ret[length] = 0;
    }

    vim_free(allocbuf);
  }

  *lenp = length;
  return (short_u *)ret;
}

/*
 * Convert an UTF-16 string to 'encoding'.
 * Input in "str" with length (counted in wide characters) "*lenp".  When
 * "lenp" is NULL, use wcslen().
 * Output is returned as an allocated string.  If "*lenp" is not NULL it is
 * set to the length of the result.
 * Returns NULL when out of memory.
 */
char_u *
utf16_to_enc(short_u *str, int *lenp)
{
  vimconv_T conv;
  char_u *utf8_str = NULL, *enc_str = NULL;
  int len_loc;

  if (lenp == NULL)
  {
    len_loc = (int)wcslen(str) + 1;
    lenp = &len_loc;
  }

  if (enc_codepage > 0)
  {
    /* We can do any UTF-16 -> CP### in one pass. */
    int length;

    WideCharToMultiByte_alloc(enc_codepage, 0, str, *lenp,
                              (LPSTR *)&enc_str, &length, 0, 0);
    *lenp = length;
    return enc_str;
  }

  /* Avoid allocating zero bytes, it generates an error message. */
  utf8_str = alloc(utf16_to_utf8(str, *lenp == 0 ? 1 : *lenp, NULL));
  if (utf8_str != NULL)
  {
    *lenp = utf16_to_utf8(str, *lenp, utf8_str);

    /* We might be called before we have p_enc set up. */
    conv.vc_type = CONV_NONE;
    convert_setup(&conv, (char_u *)"utf-8",
                  p_enc ? p_enc : (char_u *)"latin1");
    if (conv.vc_type == CONV_NONE)
    {
      /* p_enc is utf-8, so we're done. */
      enc_str = utf8_str;
    }
    else
    {
      enc_str = string_convert(&conv, utf8_str, lenp);
      vim_free(utf8_str);
    }

    convert_setup(&conv, NULL, NULL);
  }

  return enc_str;
}

/*
 * Convert from the active codepage to 'encoding'.
 * Input is "str[str_size]".
 * The result is in allocated memory: "out[outlen]".  With terminating NUL.
 */
void acp_to_enc(
    char_u *str,
    int str_size,
    char_u **out,
    int *outlen)

{
  LPWSTR widestr;

  MultiByteToWideChar_alloc(GetACP(), 0, (LPCSTR)str, str_size,
                            &widestr, outlen);
  if (widestr != NULL)
  {
    ++*outlen; /* Include the 0 after the string */
    *out = utf16_to_enc((short_u *)widestr, outlen);
    vim_free(widestr);
  }
}

/*
 * Convert from 'encoding' to the active codepage.
 * Input is "str[str_size]".
 * The result is in allocated memory: "out[outlen]".  With terminating NUL.
 */
void enc_to_acp(
    char_u *str,
    int str_size,
    char_u **out,
    int *outlen)

{
  LPWSTR widestr;
  int len = str_size;

  widestr = (WCHAR *)enc_to_utf16(str, &len);
  if (widestr != NULL)
  {
    WideCharToMultiByte_alloc(GetACP(), 0, widestr, len,
                              (LPSTR *)out, outlen, 0, 0);
    vim_free(widestr);
  }
}
