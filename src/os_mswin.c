/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */

/*
 * os_mswin.c
 *
 * Routines for Win32.
 */

#include "vim.h"

#include <limits.h>
#include <signal.h>
#include <sys/types.h>
#ifndef PROTO
#include <process.h>
#endif

#undef chdir
#ifdef __GNUC__
#ifndef __MINGW32__
#include <dirent.h>
#endif
#else
#include <direct.h>
#endif

#ifdef __MINGW32__
#ifndef FROM_LEFT_1ST_BUTTON_PRESSED
#define FROM_LEFT_1ST_BUTTON_PRESSED 0x0001
#endif
#ifndef RIGHTMOST_BUTTON_PRESSED
#define RIGHTMOST_BUTTON_PRESSED 0x0002
#endif
#ifndef FROM_LEFT_2ND_BUTTON_PRESSED
#define FROM_LEFT_2ND_BUTTON_PRESSED 0x0004
#endif
#ifndef FROM_LEFT_3RD_BUTTON_PRESSED
#define FROM_LEFT_3RD_BUTTON_PRESSED 0x0008
#endif
#ifndef FROM_LEFT_4TH_BUTTON_PRESSED
#define FROM_LEFT_4TH_BUTTON_PRESSED 0x0010
#endif

/*
 * EventFlags
 */
#ifndef MOUSE_MOVED
#define MOUSE_MOVED 0x0001
#endif
#ifndef DOUBLE_CLICK
#define DOUBLE_CLICK 0x0002
#endif
#endif

/*
 * When generating prototypes for Win32 on Unix, these lines make the syntax
 * errors disappear.  They do not need to be correct.
 */
#ifdef PROTO
#define WINAPI
#define WINBASEAPI
typedef int BOOL;
typedef int CALLBACK;
typedef int COLORREF;
typedef int CONSOLE_CURSOR_INFO;
typedef int COORD;
typedef int DWORD;
typedef int ENUMLOGFONTW;
typedef int HANDLE;
typedef int HDC;
typedef int HFONT;
typedef int HICON;
typedef int HWND;
typedef int INPUT_RECORD;
typedef int KEY_EVENT_RECORD;
typedef int LOGFONTW;
typedef int LPARAM;
typedef int LPBOOL;
typedef int LPCSTR;
typedef int LPCWSTR;
typedef int LPDWORD;
typedef int LPSTR;
typedef int LPTSTR;
typedef int LPVOID;
typedef int LPWSTR;
typedef int LRESULT;
typedef int MOUSE_EVENT_RECORD;
typedef int NEWTEXTMETRICW;
typedef int PACL;
typedef int PRINTDLGW;
typedef int PSECURITY_DESCRIPTOR;
typedef int PSID;
typedef int SECURITY_INFORMATION;
typedef int SHORT;
typedef int SMALL_RECT;
typedef int TEXTMETRIC;
typedef int UINT;
typedef int WCHAR;
typedef int WNDENUMPROC;
typedef int WORD;
typedef int WPARAM;
typedef void VOID;
#endif

/* Record all output and all keyboard & mouse input */
/* #define MCH_WRITE_DUMP */

#ifdef MCH_WRITE_DUMP
FILE *fdDump = NULL;
#endif

extern char g_szOrigTitle[];

static HWND s_hwnd = 0; /* console window handle, set by GetConsoleHwnd() */

#ifdef FEAT_JOB_CHANNEL
int WSInitialized = FALSE; /* WinSock is initialized */
#endif

/* Don't generate prototypes here, because some systems do have these
 * functions. */
#if defined(__GNUC__) && !defined(PROTO)
#ifndef __MINGW32__
int _stricoll(char *a, char *b)
{
  // the ANSI-ish correct way is to use strxfrm():
  char a_buff[512], b_buff[512]; // file names, so this is enough on Win32
  strxfrm(a_buff, a, 512);
  strxfrm(b_buff, b, 512);
  return strcoll(a_buff, b_buff);
}

char *_fullpath(char *buf, char *fname, int len)
{
  LPTSTR toss;

  return (char *)GetFullPathName(fname, len, buf, &toss);
}
#endif

#if !defined(__MINGW32__) || (__GNUC__ < 4)
int _chdrive(int drive)
{
  char temp[3] = "-:";
  temp[0] = drive + 'A' - 1;
  return !SetCurrentDirectory(temp);
}
#endif
#endif

#ifndef PROTO
/*
 * Save the instance handle of the exe/dll.
 */
void SaveInst(HINSTANCE hInst)
{
  g_hinst = hInst;
}
#endif

#if defined(PROTO)
/*
 * GUI version of mch_exit().
 * Shut down and exit with status `r'
 * Careful: mch_exit() may be called before mch_init()!
 */
void mch_exit_g(int r)
{
  exiting = TRUE;

  display_errors();

  ml_close_all(TRUE); /* remove all memfiles */

#ifdef FEAT_OLE
  UninitOLE();
#endif
#ifdef FEAT_JOB_CHANNEL
  if (WSInitialized)
  {
    WSInitialized = FALSE;
    WSACleanup();
  }
#endif
#ifdef DYNAMIC_GETTEXT
  dyn_libintl_end();
#endif

  if (gui.in_use)
    gui_exit(r);

#ifdef EXITFREE
  free_all_mem();
#endif

  exit(r);
}
#endif

/*
 * Init the tables for toupper() and tolower().
 */
void mch_early_init(void)
{
  int i;

  PlatformId();

  /* Init the tables for toupper() and tolower() */
  for (i = 0; i < 256; ++i)
    toupper_tab[i] = tolower_tab[i] = i;
  CharUpperBuff((LPSTR)toupper_tab, 256);
  CharLowerBuff((LPSTR)tolower_tab, 256);
}

/*
 * Return TRUE if the input comes from a terminal, FALSE otherwise.
 */
int mch_input_isatty(void)
{
  if (isatty(read_cmd_fd))
    return TRUE;
  return FALSE;
}

/*
 * Get absolute file name into buffer "buf" of length "len" bytes,
 * turning all '/'s into '\\'s and getting the correct case of each component
 * of the file name.  Append a (back)slash to a directory name.
 * When 'shellslash' set do it the other way around.
 * Return OK or FAIL.
 */
int mch_FullName(
    char_u *fname,
    char_u *buf,
    int len,
    int force UNUSED)
{
  int nResult = FAIL;
  WCHAR *wname;
  WCHAR wbuf[MAX_PATH];
  char_u *cname = NULL;

  wname = enc_to_utf16(fname, NULL);
  if (wname != NULL && _wfullpath(wbuf, wname, MAX_PATH) != NULL)
  {
    cname = utf16_to_enc((short_u *)wbuf, NULL);
    if (cname != NULL)
    {
      vim_strncpy(buf, cname, len - 1);
      nResult = OK;
    }
  }
  vim_free(wname);
  vim_free(cname);

#ifdef USE_FNAME_CASE
  fname_case(buf, len);
#else
  slash_adjust(buf);
#endif

  return nResult;
}

/*
 * Return TRUE if "fname" does not depend on the current directory.
 */
int mch_isFullName(char_u *fname)
{
  /* WinNT and later can use _MAX_PATH wide characters for a pathname, which
     * means that the maximum pathname is _MAX_PATH * 3 bytes when 'enc' is
     * UTF-8. */
  char szName[_MAX_PATH * 3 + 1];

  /* A name like "d:/foo" and "//server/share" is absolute */
  if ((fname[0] && fname[1] == ':' && (fname[2] == '/' || fname[2] == '\\')) || (fname[0] == fname[1] && (fname[0] == '/' || fname[0] == '\\')))
    return TRUE;

  /* A name that can't be made absolute probably isn't absolute. */
  if (mch_FullName(fname, (char_u *)szName, sizeof(szName) - 1, FALSE) == FAIL)
    return FALSE;

  return pathcmp((const char *)fname, (const char *)szName, -1) == 0;
}

/*
 * Replace all slashes by backslashes.
 * This used to be the other way around, but MS-DOS sometimes has problems
 * with slashes (e.g. in a command name).  We can't have mixed slashes and
 * backslashes, because comparing file names will not work correctly.  The
 * commands that use a file name should try to avoid the need to type a
 * backslash twice.
 * When 'shellslash' set do it the other way around.
 * When the path looks like a URL leave it unmodified.
 */
void slash_adjust(char_u *p)
{
  if (path_with_url(p))
    return;

  if (*p == '`')
  {
    size_t len = STRLEN(p);

    /* don't replace backslash in backtick quoted strings */
    if (len > 2 && *(p + len - 1) == '`')
      return;
  }

  while (*p)
  {
    if (*p == psepcN)
      *p = psepc;
    MB_PTR_ADV(p);
  }
}

/* Use 64-bit stat functions if available. */
#ifdef HAVE_STAT64
#undef stat
#undef _stat
#undef _wstat
#undef _fstat
#define stat _stat64
#define _stat _stat64
#define _wstat _wstat64
#define _fstat _fstat64
#endif

#if (defined(_MSC_VER) && (_MSC_VER >= 1300)) || defined(__MINGW32__)
#define OPEN_OH_ARGTYPE intptr_t
#else
#define OPEN_OH_ARGTYPE long
#endif

static int
wstat_symlink_aware(const WCHAR *name, stat_T *stp)
{
#if (defined(_MSC_VER) && (_MSC_VER < 1900)) || defined(__MINGW32__)
  /* Work around for VC12 or earlier (and MinGW). _wstat() can't handle
     * symlinks properly.
     * VC9 or earlier: _wstat() doesn't support a symlink at all. It retrieves
     * status of a symlink itself.
     * VC10: _wstat() supports a symlink to a normal file, but it doesn't
     * support a symlink to a directory (always returns an error).
     * VC11 and VC12: _wstat() doesn't return an error for a symlink to a
     * directory, but it doesn't set S_IFDIR flag.
     * MinGW: Same as VC9. */
  int n;
  BOOL is_symlink = FALSE;
  HANDLE hFind, h;
  DWORD attr = 0;
  WIN32_FIND_DATAW findDataW;

  hFind = FindFirstFileW(name, &findDataW);
  if (hFind != INVALID_HANDLE_VALUE)
  {
    attr = findDataW.dwFileAttributes;
    if ((attr & FILE_ATTRIBUTE_REPARSE_POINT) && (findDataW.dwReserved0 == IO_REPARSE_TAG_SYMLINK))
      is_symlink = TRUE;
    FindClose(hFind);
  }
  if (is_symlink)
  {
    h = CreateFileW(name, FILE_READ_ATTRIBUTES,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                    OPEN_EXISTING,
                    (attr & FILE_ATTRIBUTE_DIRECTORY)
                        ? FILE_FLAG_BACKUP_SEMANTICS
                        : 0,
                    NULL);
    if (h != INVALID_HANDLE_VALUE)
    {
      int fd;

      fd = _open_osfhandle((OPEN_OH_ARGTYPE)h, _O_RDONLY);
      n = _fstat(fd, (struct _stat *)stp);
      if ((n == 0) && (attr & FILE_ATTRIBUTE_DIRECTORY))
        stp->st_mode = (stp->st_mode & ~S_IFREG) | S_IFDIR;
      _close(fd);
      return n;
    }
  }
#endif
  return _wstat(name, (struct _stat *)stp);
}

/*
 * stat() can't handle a trailing '/' or '\', remove it first.
 */
int vim_stat(const char *name, stat_T *stp)
{
  /* WinNT and later can use _MAX_PATH wide characters for a pathname, which
     * means that the maximum pathname is _MAX_PATH * 3 bytes when 'enc' is
     * UTF-8. */
  char_u buf[_MAX_PATH * 3 + 1];
  char_u *p;
  WCHAR *wp;
  int n;

  vim_strncpy((char_u *)buf, (char_u *)name, sizeof(buf) - 1);
  p = buf + STRLEN(buf);
  if (p > buf)
    MB_PTR_BACK(buf, p);

  /* Remove trailing '\\' except root path. */
  if (p > buf && (*p == '\\' || *p == '/') && p[-1] != ':')
    *p = NUL;

  if ((buf[0] == '\\' && buf[1] == '\\') || (buf[0] == '/' && buf[1] == '/'))
  {
    /* UNC root path must be followed by '\\'. */
    p = vim_strpbrk(buf + 2, (char_u *)"\\/");
    if (p != NULL)
    {
      p = vim_strpbrk(p + 1, (char_u *)"\\/");
      if (p == NULL)
        STRCAT(buf, "\\");
    }
  }

  wp = enc_to_utf16(buf, NULL);
  if (wp == NULL)
    return -1;

  n = wstat_symlink_aware(wp, stp);
  vim_free(wp);
  return n;
}

#ifdef PROTO
void mch_settmode(int tmode UNUSED)
{
  /* nothing to do */
}

int mch_get_shellsize(void)
{
  /* never used */
  return OK;
}

void mch_set_shellsize(void)
{
  /* never used */
}

/*
 * Rows and/or Columns has changed.
 */
void mch_new_shellsize(void)
{
  /* never used */
}
#endif

/*
 * We have no job control, so fake it by starting a new shell.
 */
void mch_suspend(void)
{
  suspend_shell();
}

/*
 * Return TRUE if "p" contain a wildcard that can be expanded by
 * dos_expandpath().
 */
int mch_has_exp_wildcard(char_u *p)
{
  for (; *p; MB_PTR_ADV(p))
  {
    if (vim_strchr((char_u *)"?*[", *p) != NULL || (*p == '~' && p[1] != NUL))
      return TRUE;
  }
  return FALSE;
}

/*
 * Return TRUE if "p" contain a wildcard or a "~1" kind of thing (could be a
 * shortened file name).
 */
int mch_has_wildcard(char_u *p)
{
  for (; *p; MB_PTR_ADV(p))
  {
    if (vim_strchr((char_u *)
#ifdef VIM_BACKTICK
                       "?*$[`"
#else
                       "?*$["
#endif
                   ,
                   *p) != NULL ||
        (*p == '~' && p[1] != NUL))
      return TRUE;
  }
  return FALSE;
}

/*
 * The normal _chdir() does not change the default drive.  This one does.
 * Returning 0 implies success; -1 implies failure.
 */
int mch_chdir(char *path)
{
  WCHAR *p;
  int n;

  if (path[0] == NUL) /* just checking... */
    return -1;

  if (p_verbose >= 5)
  {
    verbose_enter();
    smsg("chdir(%s)", path);
    verbose_leave();
  }
  if (isalpha(path[0]) && path[1] == ':') /* has a drive name */
  {
    /* If we can change to the drive, skip that part of the path.  If we
	 * can't then the current directory may be invalid, try using chdir()
	 * with the whole path. */
    if (_chdrive(TOLOWER_ASC(path[0]) - 'a' + 1) == 0)
      path += 2;
  }

  if (*path == NUL) /* drive name only */
    return 0;

  p = enc_to_utf16((char_u *)path, NULL);
  if (p == NULL)
    return -1;

  n = _wchdir(p);
  vim_free(p);
  return n;
}

/*
 * set screen mode, always fails.
 */
int mch_screenmode(char_u *arg UNUSED)
{
  emsg(_(e_screenmode));
  return FAIL;
}

#if defined(FEAT_LIBCALL) || defined(PROTO)
/*
 * Call a DLL routine which takes either a string or int param
 * and returns an allocated string.
 * Return OK if it worked, FAIL if not.
 */
typedef LPTSTR (*MYSTRPROCSTR)(LPTSTR);
typedef LPTSTR (*MYINTPROCSTR)(int);
typedef int (*MYSTRPROCINT)(LPTSTR);
typedef int (*MYINTPROCINT)(int);

/*
 * Check if a pointer points to a valid NUL terminated string.
 * Return the length of the string, including terminating NUL.
 * Returns 0 for an invalid pointer, 1 for an empty string.
 */
static size_t
check_str_len(char_u *str)
{
  SYSTEM_INFO si;
  MEMORY_BASIC_INFORMATION mbi;
  size_t length = 0;
  size_t i;
  const char_u *p;

  /* get page size */
  GetSystemInfo(&si);

  /* get memory information */
  if (VirtualQuery(str, &mbi, sizeof(mbi)))
  {
    /* pre cast these (typing savers) */
    long_u dwStr = (long_u)str;
    long_u dwBaseAddress = (long_u)mbi.BaseAddress;

    /* get start address of page that str is on */
    long_u strPage = dwStr - (dwStr - dwBaseAddress) % si.dwPageSize;

    /* get length from str to end of page */
    long_u pageLength = si.dwPageSize - (dwStr - strPage);

    for (p = str; !IsBadReadPtr(p, (UINT)pageLength);
         p += pageLength, pageLength = si.dwPageSize)
      for (i = 0; i < pageLength; ++i, ++length)
        if (p[i] == NUL)
          return length + 1;
  }

  return 0;
}

/*
 * Passed to do_in_runtimepath() to load a vim.ico file.
 */
static void
mch_icon_load_cb(char_u *fname, void *cookie)
{
  HANDLE *h = (HANDLE *)cookie;

  *h = LoadImage(NULL,
                 (LPSTR)fname,
                 IMAGE_ICON,
                 64,
                 64,
                 LR_LOADFROMFILE | LR_LOADMAP3DCOLORS);
}

/*
 * Try loading an icon file from 'runtimepath'.
 */
int mch_icon_load(HANDLE *iconp)
{
  return do_in_runtimepath((char_u *)"bitmaps/vim.ico",
                           0, mch_icon_load_cb, iconp);
}

int mch_libcall(
    char_u *libname,
    char_u *funcname,
    char_u *argstring, /* NULL when using a argint */
    int argint,
    char_u **string_result, /* NULL when using number_result */
    int *number_result)
{
  HINSTANCE hinstLib;
  MYSTRPROCSTR ProcAdd;
  MYINTPROCSTR ProcAddI;
  char_u *retval_str = NULL;
  int retval_int = 0;
  size_t len;

  BOOL fRunTimeLinkSuccess = FALSE;

  // Get a handle to the DLL module.
  hinstLib = vimLoadLib((char *)libname);

  // If the handle is valid, try to get the function address.
  if (hinstLib != NULL)
  {
#ifdef HAVE_TRY_EXCEPT
    __try
    {
#endif
      if (argstring != NULL)
      {
        /* Call with string argument */
        ProcAdd = (MYSTRPROCSTR)GetProcAddress(hinstLib, (LPCSTR)funcname);
        if ((fRunTimeLinkSuccess = (ProcAdd != NULL)) != 0)
        {
          if (string_result == NULL)
            retval_int = ((MYSTRPROCINT)ProcAdd)((LPSTR)argstring);
          else
            retval_str = (char_u *)(ProcAdd)((LPSTR)argstring);
        }
      }
      else
      {
        /* Call with number argument */
        ProcAddI = (MYINTPROCSTR)GetProcAddress(hinstLib, (LPCSTR)funcname);
        if ((fRunTimeLinkSuccess = (ProcAddI != NULL)) != 0)
        {
          if (string_result == NULL)
            retval_int = ((MYINTPROCINT)ProcAddI)(argint);
          else
            retval_str = (char_u *)(ProcAddI)(argint);
        }
      }

      // Save the string before we free the library.
      // Assume that a "1" result is an illegal pointer.
      if (string_result == NULL)
        *number_result = retval_int;
      else if (retval_str != NULL && (len = check_str_len(retval_str)) > 0)
      {
        *string_result = alloc(len);
        if (*string_result != NULL)
          mch_memmove(*string_result, retval_str, len);
      }

#ifdef HAVE_TRY_EXCEPT
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      if (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW)
        RESETSTKOFLW();
      fRunTimeLinkSuccess = 0;
    }
#endif

    // Free the DLL module.
    (void)FreeLibrary(hinstLib);
  }

  if (!fRunTimeLinkSuccess)
  {
    semsg(_(e_libcall), funcname);
    return FAIL;
  }

  return OK;
}
#endif

/*
 * Debugging helper: expose the MCH_WRITE_DUMP stuff to other modules
 */
void DumpPutS(const char *psz UNUSED)
{
#ifdef MCH_WRITE_DUMP
  if (fdDump)
  {
    fputs(psz, fdDump);
    if (psz[strlen(psz) - 1] != '\n')
      fputc('\n', fdDump);
    fflush(fdDump);
  }
#endif
}

#ifdef _DEBUG

void __cdecl Trace(
    char *pszFormat,
    ...)
{
  CHAR szBuff[2048];
  va_list args;

  va_start(args, pszFormat);
  vsprintf(szBuff, pszFormat, args);
  va_end(args);

  OutputDebugString(szBuff);
}

#endif //_DEBUG

/*
 * Showing the printer dialog is tricky since we have no GUI
 * window to parent it. The following routines are needed to
 * get the window parenting and Z-order to work properly.
 */
static void
GetConsoleHwnd(void)
{
  /* Skip if it's already set. */
  if (s_hwnd != 0)
    return;

  s_hwnd = GetConsoleWindow();
}

/*
 * Console implementation of ":winpos".
 */
int mch_get_winpos(int *x, int *y)
{
  RECT rect;

  GetConsoleHwnd();
  GetWindowRect(s_hwnd, &rect);
  *x = rect.left;
  *y = rect.top;
  return OK;
}

/*
 * Console implementation of ":winpos x y".
 */
void mch_set_winpos(int x, int y)
{
  GetConsoleHwnd();
  SetWindowPos(s_hwnd, NULL, x, y, 0, 0,
               SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
}

#if defined(FEAT_SHORTCUT) || defined(PROTO)
#ifndef PROTO
#include <shlobj.h>
#endif

typedef enum _FILE_INFO_BY_HANDLE_CLASS_
{
  FileBasicInfo_,
  FileStandardInfo_,
  FileNameInfo_,
  FileRenameInfo_,
  FileDispositionInfo_,
  FileAllocationInfo_,
  FileEndOfFileInfo_,
  FileStreamInfo_,
  FileCompressionInfo_,
  FileAttributeTagInfo_,
  FileIdBothDirectoryInfo_,
  FileIdBothDirectoryRestartInfo_,
  FileIoPriorityHintInfo_,
  FileRemoteProtocolInfo_,
  FileFullDirectoryInfo_,
  FileFullDirectoryRestartInfo_,
  FileStorageInfo_,
  FileAlignmentInfo_,
  FileIdInfo_,
  FileIdExtdDirectoryInfo_,
  FileIdExtdDirectoryRestartInfo_,
  FileDispositionInfoEx_,
  FileRenameInfoEx_,
  MaximumFileInfoByHandleClass_
} FILE_INFO_BY_HANDLE_CLASS_;

typedef struct _FILE_NAME_INFO_
{
  DWORD FileNameLength;
  WCHAR FileName[1];
} FILE_NAME_INFO_;

typedef BOOL(WINAPI *pfnGetFileInformationByHandleEx)(
    HANDLE hFile,
    FILE_INFO_BY_HANDLE_CLASS_ FileInformationClass,
    LPVOID lpFileInformation,
    DWORD dwBufferSize);
static pfnGetFileInformationByHandleEx pGetFileInformationByHandleEx = NULL;

typedef BOOL(WINAPI *pfnGetVolumeInformationByHandleW)(
    HANDLE hFile,
    LPWSTR lpVolumeNameBuffer,
    DWORD nVolumeNameSize,
    LPDWORD lpVolumeSerialNumber,
    LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags,
    LPWSTR lpFileSystemNameBuffer,
    DWORD nFileSystemNameSize);
static pfnGetVolumeInformationByHandleW pGetVolumeInformationByHandleW = NULL;

static char_u *
resolve_reparse_point(char_u *fname)
{
  HANDLE h = INVALID_HANDLE_VALUE;
  DWORD size;
  WCHAR *p;
  char_u *rfname = NULL;
  FILE_NAME_INFO_ *nameinfo = NULL;
  WCHAR buff[MAX_PATH], *volnames = NULL;
  HANDLE hv;
  DWORD snfile, snfind;
  static BOOL loaded = FALSE;

  if (pGetFileInformationByHandleEx == NULL ||
      pGetVolumeInformationByHandleW == NULL)
  {
    HMODULE hmod = GetModuleHandle("kernel32.dll");

    if (loaded == TRUE)
      return NULL;
    pGetFileInformationByHandleEx = (pfnGetFileInformationByHandleEx)
        GetProcAddress(hmod, "GetFileInformationByHandleEx");
    pGetVolumeInformationByHandleW = (pfnGetVolumeInformationByHandleW)
        GetProcAddress(hmod, "GetVolumeInformationByHandleW");
    loaded = TRUE;
    if (pGetFileInformationByHandleEx == NULL ||
        pGetVolumeInformationByHandleW == NULL)
      return NULL;
  }

  p = enc_to_utf16(fname, NULL);
  if (p == NULL)
    goto fail;

  h = CreateFileW(p, 0, 0, NULL, OPEN_EXISTING,
                  FILE_FLAG_BACKUP_SEMANTICS, NULL);
  vim_free(p);

  if (h == INVALID_HANDLE_VALUE)
    goto fail;

  size = sizeof(FILE_NAME_INFO_) + sizeof(WCHAR) * (MAX_PATH - 1);
  nameinfo = alloc(size + sizeof(WCHAR));
  if (nameinfo == NULL)
    goto fail;

  if (!pGetFileInformationByHandleEx(h, FileNameInfo_, nameinfo, size))
    goto fail;

  nameinfo->FileName[nameinfo->FileNameLength / sizeof(WCHAR)] = 0;

  if (!pGetVolumeInformationByHandleW(
          h, NULL, 0, &snfile, NULL, NULL, NULL, 0))
    goto fail;

  hv = FindFirstVolumeW(buff, MAX_PATH);
  if (hv == INVALID_HANDLE_VALUE)
    goto fail;

  do
  {
    GetVolumeInformationW(
        buff, NULL, 0, &snfind, NULL, NULL, NULL, 0);
    if (snfind == snfile)
      break;
  } while (FindNextVolumeW(hv, buff, MAX_PATH));

  FindVolumeClose(hv);

  if (snfind != snfile)
    goto fail;

  size = 0;
  if (!GetVolumePathNamesForVolumeNameW(buff, NULL, 0, &size) &&
      GetLastError() != ERROR_MORE_DATA)
    goto fail;

  volnames = ALLOC_MULT(WCHAR, size);
  if (!GetVolumePathNamesForVolumeNameW(buff, volnames, size,
                                        &size))
    goto fail;

  wcscpy(buff, volnames);
  if (nameinfo->FileName[0] == '\\')
    wcscat(buff, nameinfo->FileName + 1);
  else
    wcscat(buff, nameinfo->FileName);
  rfname = utf16_to_enc(buff, NULL);

fail:
  if (h != INVALID_HANDLE_VALUE)
    CloseHandle(h);
  if (nameinfo != NULL)
    vim_free(nameinfo);
  if (volnames != NULL)
    vim_free(volnames);

  return rfname;
}

/*
 * When "fname" is the name of a shortcut (*.lnk) resolve the file it points
 * to and return that name in allocated memory.
 * Otherwise NULL is returned.
 */
static char_u *
resolve_shortcut(char_u *fname)
{
  HRESULT hr;
  IShellLink *psl = NULL;
  IPersistFile *ppf = NULL;
  OLECHAR wsz[MAX_PATH];
  char_u *rfname = NULL;
  int len;
  IShellLinkW *pslw = NULL;
  WIN32_FIND_DATAW ffdw; // we get those free of charge

  /* Check if the file name ends in ".lnk". Avoid calling
     * CoCreateInstance(), it's quite slow. */
  if (fname == NULL)
    return rfname;
  len = (int)STRLEN(fname);
  if (len <= 4 || STRNICMP(fname + len - 4, ".lnk", 4) != 0)
    return rfname;

  CoInitialize(NULL);

  // create a link manager object and request its interface
  hr = CoCreateInstance(
      &CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
      &IID_IShellLinkW, (void **)&pslw);
  if (hr == S_OK)
  {
    WCHAR *p = enc_to_utf16(fname, NULL);

    if (p != NULL)
    {
      // Get a pointer to the IPersistFile interface.
      hr = pslw->lpVtbl->QueryInterface(
          pslw, &IID_IPersistFile, (void **)&ppf);
      if (hr != S_OK)
        goto shortcut_errorw;

      // "load" the name and resolve the link
      hr = ppf->lpVtbl->Load(ppf, p, STGM_READ);
      if (hr != S_OK)
        goto shortcut_errorw;
#if 0 // This makes Vim wait a long time if the target does not exist.
	    hr = pslw->lpVtbl->Resolve(pslw, NULL, SLR_NO_UI);
	    if (hr != S_OK)
		goto shortcut_errorw;
#endif

      // Get the path to the link target.
      ZeroMemory(wsz, MAX_PATH * sizeof(WCHAR));
      hr = pslw->lpVtbl->GetPath(pslw, wsz, MAX_PATH, &ffdw, 0);
      if (hr == S_OK && wsz[0] != NUL)
        rfname = utf16_to_enc(wsz, NULL);

    shortcut_errorw:
      vim_free(p);
    }
  }

  // Release all interface pointers (both belong to the same object)
  if (ppf != NULL)
    ppf->lpVtbl->Release(ppf);
  if (psl != NULL)
    psl->lpVtbl->Release(psl);
  if (pslw != NULL)
    pslw->lpVtbl->Release(pslw);

  CoUninitialize();
  return rfname;
}

char_u *
mch_resolve_path(char_u *fname, int reparse_point)
{
  char_u *path = resolve_shortcut(fname);

  if (path == NULL && reparse_point)
    path = resolve_reparse_point(fname);
  return path;
}
#endif

#if defined(FEAT_EVAL) || defined(PROTO)
/*
 * Bring ourselves to the foreground.  Does work if the OS doesn't allow it.
 */
void win32_set_foreground(void)
{
  GetConsoleHwnd(); /* get value of s_hwnd */
  if (s_hwnd != 0)
    SetForegroundWindow(s_hwnd);
}
#endif

#if (defined(FEAT_PRINTER) && !defined(FEAT_POSTSCRIPT)) || defined(PROTO)

struct charset_pair
{
  char *name;
  BYTE charset;
};

static struct charset_pair
    charset_pairs[] =
        {
            {"ANSI", ANSI_CHARSET},
            {"CHINESEBIG5", CHINESEBIG5_CHARSET},
            {"DEFAULT", DEFAULT_CHARSET},
            {"HANGEUL", HANGEUL_CHARSET},
            {"OEM", OEM_CHARSET},
            {"SHIFTJIS", SHIFTJIS_CHARSET},
            {"SYMBOL", SYMBOL_CHARSET},
            {"ARABIC", ARABIC_CHARSET},
            {"BALTIC", BALTIC_CHARSET},
            {"EASTEUROPE", EASTEUROPE_CHARSET},
            {"GB2312", GB2312_CHARSET},
            {"GREEK", GREEK_CHARSET},
            {"HEBREW", HEBREW_CHARSET},
            {"JOHAB", JOHAB_CHARSET},
            {"MAC", MAC_CHARSET},
            {"RUSSIAN", RUSSIAN_CHARSET},
            {"THAI", THAI_CHARSET},
            {"TURKISH", TURKISH_CHARSET},
#ifdef VIETNAMESE_CHARSET
            {"VIETNAMESE", VIETNAMESE_CHARSET},
#endif
            {NULL, 0}};

struct quality_pair
{
  char *name;
  DWORD quality;
};

static struct quality_pair
    quality_pairs[] = {
#ifdef CLEARTYPE_QUALITY
        {"CLEARTYPE", CLEARTYPE_QUALITY},
#endif
#ifdef ANTIALIASED_QUALITY
        {"ANTIALIASED", ANTIALIASED_QUALITY},
#endif
#ifdef NONANTIALIASED_QUALITY
        {"NONANTIALIASED", NONANTIALIASED_QUALITY},
#endif
#ifdef PROOF_QUALITY
        {"PROOF", PROOF_QUALITY},
#endif
#ifdef DRAFT_QUALITY
        {"DRAFT", DRAFT_QUALITY},
#endif
        {"DEFAULT", DEFAULT_QUALITY},
        {NULL, 0}};

/*
 * Convert a charset ID to a name.
 * Return NULL when not recognized.
 */
char *
charset_id2name(int id)
{
  struct charset_pair *cp;

  for (cp = charset_pairs; cp->name != NULL; ++cp)
    if ((BYTE)id == cp->charset)
      break;
  return cp->name;
}

/*
 * Convert a quality ID to a name.
 * Return NULL when not recognized.
 */
char *
quality_id2name(DWORD id)
{
  struct quality_pair *qp;

  for (qp = quality_pairs; qp->name != NULL; ++qp)
    if (id == qp->quality)
      break;
  return qp->name;
}

static const LOGFONTW s_lfDefault =
    {
        -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        PROOF_QUALITY, FIXED_PITCH | FF_DONTCARE,
        L"Fixedsys" /* see _ReadVimIni */
};

// Initialise the "current height" to -12 (same as s_lfDefault) just
// in case the user specifies a font in "guifont" with no size before a font
// with an explicit size has been set. This defaults the size to this value
// (-12 equates to roughly 9pt).
int current_font_height = -12; // also used in gui_w32.c

/* Convert a string representing a point size into pixels. The string should
 * be a positive decimal number, with an optional decimal point (eg, "12", or
 * "10.5"). The pixel value is returned, and a pointer to the next unconverted
 * character is stored in *end. The flag "vertical" says whether this
 * calculation is for a vertical (height) size or a horizontal (width) one.
 */
static int
points_to_pixels(WCHAR *str, WCHAR **end, int vertical, long_i pprinter_dc)
{
  int pixels;
  int points = 0;
  int divisor = 0;
  HWND hwnd = (HWND)0;
  HDC hdc;
  HDC printer_dc = (HDC)pprinter_dc;

  while (*str != NUL)
  {
    if (*str == L'.' && divisor == 0)
    {
      /* Start keeping a divisor, for later */
      divisor = 1;
    }
    else
    {
      if (!VIM_ISDIGIT(*str))
        break;

      points *= 10;
      points += *str - L'0';
      divisor *= 10;
    }
    ++str;
  }

  if (divisor == 0)
    divisor = 1;

  if (printer_dc == NULL)
  {
    hwnd = GetDesktopWindow();
    hdc = GetWindowDC(hwnd);
  }
  else
    hdc = printer_dc;

  pixels = MulDiv(points,
                  GetDeviceCaps(hdc, vertical ? LOGPIXELSY : LOGPIXELSX),
                  72 * divisor);

  if (printer_dc == NULL)
    ReleaseDC(hwnd, hdc);

  *end = str;
  return pixels;
}

static int CALLBACK
font_enumproc(
    ENUMLOGFONTW *elf,
    NEWTEXTMETRICW *ntm UNUSED,
    DWORD type UNUSED,
    LPARAM lparam)
{
  /* Return value:
     *	  0 = terminate now (monospace & ANSI)
     *	  1 = continue, still no luck...
     *	  2 = continue, but we have an acceptable LOGFONTW
     *	      (monospace, not ANSI)
     * We use these values, as EnumFontFamilies returns 1 if the
     * callback function is never called. So, we check the return as
     * 0 = perfect, 2 = OK, 1 = no good...
     * It's not pretty, but it works!
     */

  LOGFONTW *lf = (LOGFONTW *)(lparam);

#ifndef FEAT_PROPORTIONAL_FONTS
  /* Ignore non-monospace fonts without further ado */
  if ((ntm->tmPitchAndFamily & 1) != 0)
    return 1;
#endif

  /* Remember this LOGFONTW as a "possible" */
  *lf = elf->elfLogFont;

  /* Terminate the scan as soon as we find an ANSI font */
  if (lf->lfCharSet == ANSI_CHARSET || lf->lfCharSet == OEM_CHARSET || lf->lfCharSet == DEFAULT_CHARSET)
    return 0;

  /* Continue the scan - we have a non-ANSI font */
  return 2;
}

static int
init_logfont(LOGFONTW *lf)
{
  int n;
  HWND hwnd = GetDesktopWindow();
  HDC hdc = GetWindowDC(hwnd);

  n = EnumFontFamiliesW(hdc,
                        lf->lfFaceName,
                        (FONTENUMPROCW)font_enumproc,
                        (LPARAM)lf);

  ReleaseDC(hwnd, hdc);

  /* If we couldn't find a usable font, return failure */
  if (n == 1)
    return FAIL;

  /* Tidy up the rest of the LOGFONTW structure. We set to a basic
     * font - get_logfont() sets bold, italic, etc based on the user's
     * input.
     */
  lf->lfHeight = current_font_height;
  lf->lfWidth = 0;
  lf->lfItalic = FALSE;
  lf->lfUnderline = FALSE;
  lf->lfStrikeOut = FALSE;
  lf->lfWeight = FW_NORMAL;

  /* Return success */
  return OK;
}

/*
 * Compare a UTF-16 string and an ASCII string literally.
 * Only works all the code points are inside ASCII range.
 */
static int
utf16ascncmp(const WCHAR *w, const char *p, size_t n)
{
  size_t i;

  for (i = 0; i < n; i++)
  {
    if (w[i] == 0 || w[i] != p[i])
      return w[i] - p[i];
  }
  return 0;
}

/*
 * Get font info from "name" into logfont "lf".
 * Return OK for a valid name, FAIL otherwise.
 */
int get_logfont(
    LOGFONTW *lf,
    char_u *name,
    HDC printer_dc,
    int verbose)
{
  WCHAR *p;
  int i;
  int ret = FAIL;
  static LOGFONTW *lastlf = NULL;
  WCHAR *wname;

  *lf = s_lfDefault;
  if (name == NULL)
    return OK;

  wname = enc_to_utf16(name, NULL);
  if (wname == NULL)
    return FAIL;

  if (wcscmp(wname, L"*") == 0)
  {
    goto theend;
  }

  /*
     * Split name up, it could be <name>:h<height>:w<width> etc.
     */
  for (p = wname; *p && *p != L':'; p++)
  {
    if (p - wname + 1 >= LF_FACESIZE)
      goto theend; /* Name too long */
    lf->lfFaceName[p - wname] = *p;
  }
  if (p != wname)
    lf->lfFaceName[p - wname] = NUL;

  /* First set defaults */
  lf->lfHeight = -12;
  lf->lfWidth = 0;
  lf->lfWeight = FW_NORMAL;
  lf->lfItalic = FALSE;
  lf->lfUnderline = FALSE;
  lf->lfStrikeOut = FALSE;

  /*
     * If the font can't be found, try replacing '_' by ' '.
     */
  if (init_logfont(lf) == FAIL)
  {
    int did_replace = FALSE;

    for (i = 0; lf->lfFaceName[i]; ++i)
      if (lf->lfFaceName[i] == L'_')
      {
        lf->lfFaceName[i] = L' ';
        did_replace = TRUE;
      }
    if (!did_replace || init_logfont(lf) == FAIL)
      goto theend;
  }

  while (*p == L':')
    p++;

  /* Set the values found after ':' */
  while (*p)
  {
    switch (*p++)
    {
    case L'h':
      lf->lfHeight = -points_to_pixels(p, &p, TRUE, (long_i)printer_dc);
      break;
    case L'w':
      lf->lfWidth = points_to_pixels(p, &p, FALSE, (long_i)printer_dc);
      break;
    case L'W':
      lf->lfWeight = wcstol(p, &p, 10);
      break;
    case L'b':
      lf->lfWeight = FW_BOLD;
      break;
    case L'i':
      lf->lfItalic = TRUE;
      break;
    case L'u':
      lf->lfUnderline = TRUE;
      break;
    case L's':
      lf->lfStrikeOut = TRUE;
      break;
    case L'c':
    {
      struct charset_pair *cp;

      for (cp = charset_pairs; cp->name != NULL; ++cp)
        if (utf16ascncmp(p, cp->name, strlen(cp->name)) == 0)
        {
          lf->lfCharSet = cp->charset;
          p += strlen(cp->name);
          break;
        }
      if (cp->name == NULL && verbose)
      {
        char_u *s = utf16_to_enc(p, NULL);
        semsg(_("E244: Illegal charset name \"%s\" in font name \"%s\""), s, name);
        vim_free(s);
        break;
      }
      break;
    }
    case L'q':
    {
      struct quality_pair *qp;

      for (qp = quality_pairs; qp->name != NULL; ++qp)
        if (utf16ascncmp(p, qp->name, strlen(qp->name)) == 0)
        {
          lf->lfQuality = qp->quality;
          p += strlen(qp->name);
          break;
        }
      if (qp->name == NULL && verbose)
      {
        char_u *s = utf16_to_enc(p, NULL);
        semsg(_("E244: Illegal quality name \"%s\" in font name \"%s\""), s, name);
        vim_free(s);
        break;
      }
      break;
    }
    default:
      if (verbose)
        semsg(_("E245: Illegal char '%c' in font name \"%s\""), p[-1], name);
      goto theend;
    }
    while (*p == L':')
      p++;
  }
  ret = OK;

theend:
  /* ron: init lastlf */
  if (ret == OK && printer_dc == NULL)
  {
    vim_free(lastlf);
    lastlf = ALLOC_ONE(LOGFONTW);
    if (lastlf != NULL)
      mch_memmove(lastlf, lf, sizeof(LOGFONTW));
  }
  vim_free(wname);

  return ret;
}

#endif /* defined(FEAT_PRINTER) */

#if defined(FEAT_JOB_CHANNEL) || defined(PROTO)
/*
 * Initialize the Winsock dll.
 */
void channel_init_winsock(void)
{
  WSADATA wsaData;
  int wsaerr;

  if (WSInitialized)
    return;

  wsaerr = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (wsaerr == 0)
    WSInitialized = TRUE;
}
#endif
