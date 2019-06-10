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

#include <sys/types.h>
#include <signal.h>
#include <limits.h>
#ifndef PROTO
# include <process.h>
#endif

#undef chdir
#ifdef __GNUC__
# ifndef __MINGW32__
#  include <dirent.h>
# endif
#else
# include <direct.h>
#endif

#ifndef PROTO
# if defined(FEAT_TITLE) && !defined(FEAT_GUI_MSWIN)
#  include <shellapi.h>
# endif

#endif /* PROTO */

#ifdef __MINGW32__
# ifndef FROM_LEFT_1ST_BUTTON_PRESSED
#  define FROM_LEFT_1ST_BUTTON_PRESSED    0x0001
# endif
# ifndef RIGHTMOST_BUTTON_PRESSED
#  define RIGHTMOST_BUTTON_PRESSED	  0x0002
# endif
# ifndef FROM_LEFT_2ND_BUTTON_PRESSED
#  define FROM_LEFT_2ND_BUTTON_PRESSED    0x0004
# endif
# ifndef FROM_LEFT_3RD_BUTTON_PRESSED
#  define FROM_LEFT_3RD_BUTTON_PRESSED    0x0008
# endif
# ifndef FROM_LEFT_4TH_BUTTON_PRESSED
#  define FROM_LEFT_4TH_BUTTON_PRESSED    0x0010
# endif

/*
 * EventFlags
 */
# ifndef MOUSE_MOVED
#  define MOUSE_MOVED   0x0001
# endif
# ifndef DOUBLE_CLICK
#  define DOUBLE_CLICK  0x0002
# endif
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
FILE* fdDump = NULL;
#endif

#if !defined(FEAT_GUI_MSWIN) || defined(VIMDLL)
extern char g_szOrigTitle[];
#endif

#ifdef FEAT_GUI
extern HWND s_hwnd;
#else
static HWND s_hwnd = 0;	    /* console window handle, set by GetConsoleHwnd() */
#endif

#ifdef FEAT_JOB_CHANNEL
int WSInitialized = FALSE; /* WinSock is initialized */
#endif

/* Don't generate prototypes here, because some systems do have these
 * functions. */
#if defined(__GNUC__) && !defined(PROTO)
# ifndef __MINGW32__
int _stricoll(char *a, char *b)
{
    // the ANSI-ish correct way is to use strxfrm():
    char a_buff[512], b_buff[512];  // file names, so this is enough on Win32
    strxfrm(a_buff, a, 512);
    strxfrm(b_buff, b, 512);
    return strcoll(a_buff, b_buff);
}

char * _fullpath(char *buf, char *fname, int len)
{
    LPTSTR toss;

    return (char *)GetFullPathName(fname, len, buf, &toss);
}
# endif

# if !defined(__MINGW32__) || (__GNUC__ < 4)
int _chdrive(int drive)
{
    char temp [3] = "-:";
    temp[0] = drive + 'A' - 1;
    return !SetCurrentDirectory(temp);
}
# endif
#endif


#ifndef PROTO
/*
 * Save the instance handle of the exe/dll.
 */
    void
SaveInst(HINSTANCE hInst)
{
    g_hinst = hInst;
}
#endif

#if defined(FEAT_GUI_MSWIN) || defined(PROTO)
/*
 * GUI version of mch_exit().
 * Shut down and exit with status `r'
 * Careful: mch_exit() may be called before mch_init()!
 */
    void
mch_exit_g(int r)
{
    exiting = TRUE;

    display_errors();

    ml_close_all(TRUE);		/* remove all memfiles */

# ifdef FEAT_OLE
    UninitOLE();
# endif
# ifdef FEAT_JOB_CHANNEL
    if (WSInitialized)
    {
	WSInitialized = FALSE;
	WSACleanup();
    }
# endif
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

#endif /* FEAT_GUI_MSWIN */


/*
 * Init the tables for toupper() and tolower().
 */
    void
mch_early_init(void)
{
    int		i;

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
    int
mch_input_isatty(void)
{
#ifdef FEAT_GUI_MSWIN
# ifdef VIMDLL
    if (gui.in_use)
# endif
	return TRUE;	    /* GUI always has a tty */
#endif
#if !defined(FEAT_GUI_MSWIN) || defined(VIMDLL)
    if (isatty(read_cmd_fd))
	return TRUE;
    return FALSE;
#endif
}

#ifdef FEAT_TITLE
/*
 * mch_settitle(): set titlebar of our window
 */
    void
mch_settitle(
    char_u *title,
    char_u *icon)
{
# ifdef FEAT_GUI_MSWIN
#  ifdef VIMDLL
    if (gui.in_use)
#  endif
    {
	gui_mch_settitle(title, icon);
	return;
    }
# endif
# if !defined(FEAT_GUI_MSWIN) || defined(VIMDLL)
    if (title != NULL)
    {
	WCHAR	*wp = enc_to_utf16(title, NULL);

	if (wp == NULL)
	    return;

	SetConsoleTitleW(wp);
	vim_free(wp);
	return;
    }
# endif
}


/*
 * Restore the window/icon title.
 * which is one of:
 *  SAVE_RESTORE_TITLE: Just restore title
 *  SAVE_RESTORE_ICON:  Just restore icon (which we don't have)
 *  SAVE_RESTORE_BOTH:  Restore title and icon (which we don't have)
 */
    void
mch_restore_title(int which UNUSED)
{
#if !defined(FEAT_GUI_MSWIN) || defined(VIMDLL)
# ifdef VIMDLL
    if (!gui.in_use)
# endif
	SetConsoleTitle(g_szOrigTitle);
#endif
}


/*
 * Return TRUE if we can restore the title (we can)
 */
    int
mch_can_restore_title(void)
{
    return TRUE;
}


/*
 * Return TRUE if we can restore the icon title (we can't)
 */
    int
mch_can_restore_icon(void)
{
    return FALSE;
}
#endif /* FEAT_TITLE */


/*
 * Get absolute file name into buffer "buf" of length "len" bytes,
 * turning all '/'s into '\\'s and getting the correct case of each component
 * of the file name.  Append a (back)slash to a directory name.
 * When 'shellslash' set do it the other way around.
 * Return OK or FAIL.
 */
    int
mch_FullName(
    char_u	*fname,
    char_u	*buf,
    int		len,
    int		force UNUSED)
{
    int		nResult = FAIL;
    WCHAR	*wname;
    WCHAR	wbuf[MAX_PATH];
    char_u	*cname = NULL;

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
    int
mch_isFullName(char_u *fname)
{
    /* WinNT and later can use _MAX_PATH wide characters for a pathname, which
     * means that the maximum pathname is _MAX_PATH * 3 bytes when 'enc' is
     * UTF-8. */
    char szName[_MAX_PATH * 3 + 1];

    /* A name like "d:/foo" and "//server/share" is absolute */
    if ((fname[0] && fname[1] == ':' && (fname[2] == '/' || fname[2] == '\\'))
	    || (fname[0] == fname[1] && (fname[0] == '/' || fname[0] == '\\')))
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
    void
slash_adjust(char_u *p)
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
# undef stat
# undef _stat
# undef _wstat
# undef _fstat
# define stat _stat64
# define _stat _stat64
# define _wstat _wstat64
# define _fstat _fstat64
#endif

#if (defined(_MSC_VER) && (_MSC_VER >= 1300)) || defined(__MINGW32__)
# define OPEN_OH_ARGTYPE intptr_t
#else
# define OPEN_OH_ARGTYPE long
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
    int			n;
    BOOL		is_symlink = FALSE;
    HANDLE		hFind, h;
    DWORD		attr = 0;
    WIN32_FIND_DATAW	findDataW;

    hFind = FindFirstFileW(name, &findDataW);
    if (hFind != INVALID_HANDLE_VALUE)
    {
	attr = findDataW.dwFileAttributes;
	if ((attr & FILE_ATTRIBUTE_REPARSE_POINT)
		&& (findDataW.dwReserved0 == IO_REPARSE_TAG_SYMLINK))
	    is_symlink = TRUE;
	FindClose(hFind);
    }
    if (is_symlink)
    {
	h = CreateFileW(name, FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		OPEN_EXISTING,
		(attr & FILE_ATTRIBUTE_DIRECTORY)
					    ? FILE_FLAG_BACKUP_SEMANTICS : 0,
		NULL);
	if (h != INVALID_HANDLE_VALUE)
	{
	    int	    fd;

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
    int
vim_stat(const char *name, stat_T *stp)
{
    /* WinNT and later can use _MAX_PATH wide characters for a pathname, which
     * means that the maximum pathname is _MAX_PATH * 3 bytes when 'enc' is
     * UTF-8. */
    char_u	buf[_MAX_PATH * 3 + 1];
    char_u	*p;
    WCHAR	*wp;
    int		n;

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

#if (defined(FEAT_GUI_MSWIN) && !defined(VIMDLL)) || defined(PROTO)
    void
mch_settmode(int tmode UNUSED)
{
    /* nothing to do */
}

    int
mch_get_shellsize(void)
{
    /* never used */
    return OK;
}

    void
mch_set_shellsize(void)
{
    /* never used */
}

/*
 * Rows and/or Columns has changed.
 */
    void
mch_new_shellsize(void)
{
    /* never used */
}

#endif

/*
 * We have no job control, so fake it by starting a new shell.
 */
    void
mch_suspend(void)
{
    suspend_shell();
}

#if defined(USE_MCH_ERRMSG) || defined(PROTO)

# ifdef display_errors
#  undef display_errors
# endif

/*
 * Display the saved error message(s).
 */
    void
display_errors(void)
{
# ifdef FEAT_GUI
    char *p;

#  ifdef VIMDLL
    if (gui.in_use || gui.starting)
#  endif
    {
	if (error_ga.ga_data != NULL)
	{
	    /* avoid putting up a message box with blanks only */
	    for (p = (char *)error_ga.ga_data; *p; ++p)
		if (!isspace(*p))
		{
		    (void)gui_mch_dialog(
				     gui.starting ? VIM_INFO :
					     VIM_ERROR,
				     gui.starting ? (char_u *)_("Message") :
					     (char_u *)_("Error"),
				     (char_u *)p, (char_u *)_("&Ok"),
					1, NULL, FALSE);
		    break;
		}
	    ga_clear(&error_ga);
	}
	return;
    }
# endif
# if !defined(FEAT_GUI) || defined(VIMDLL)
    FlushFileBuffers(GetStdHandle(STD_ERROR_HANDLE));
# endif
}
#endif


/*
 * Return TRUE if "p" contain a wildcard that can be expanded by
 * dos_expandpath().
 */
    int
mch_has_exp_wildcard(char_u *p)
{
    for ( ; *p; MB_PTR_ADV(p))
    {
	if (vim_strchr((char_u *)"?*[", *p) != NULL
		|| (*p == '~' && p[1] != NUL))
	    return TRUE;
    }
    return FALSE;
}

/*
 * Return TRUE if "p" contain a wildcard or a "~1" kind of thing (could be a
 * shortened file name).
 */
    int
mch_has_wildcard(char_u *p)
{
    for ( ; *p; MB_PTR_ADV(p))
    {
	if (vim_strchr((char_u *)
#  ifdef VIM_BACKTICK
				    "?*$[`"
#  else
				    "?*$["
#  endif
						, *p) != NULL
		|| (*p == '~' && p[1] != NUL))
	    return TRUE;
    }
    return FALSE;
}


/*
 * The normal _chdir() does not change the default drive.  This one does.
 * Returning 0 implies success; -1 implies failure.
 */
    int
mch_chdir(char *path)
{
    WCHAR   *p;
    int	    n;

    if (path[0] == NUL)		/* just checking... */
	return -1;

    if (p_verbose >= 5)
    {
	verbose_enter();
	smsg("chdir(%s)", path);
	verbose_leave();
    }
    if (isalpha(path[0]) && path[1] == ':')	/* has a drive name */
    {
	/* If we can change to the drive, skip that part of the path.  If we
	 * can't then the current directory may be invalid, try using chdir()
	 * with the whole path. */
	if (_chdrive(TOLOWER_ASC(path[0]) - 'a' + 1) == 0)
	    path += 2;
    }

    if (*path == NUL)		/* drive name only */
	return 0;

    p = enc_to_utf16((char_u *)path, NULL);
    if (p == NULL)
	return -1;

    n = _wchdir(p);
    vim_free(p);
    return n;
}


#if defined(FEAT_GUI_MSWIN) && !defined(VIMDLL)
/*
 * return non-zero if a character is available
 */
    int
mch_char_avail(void)
{
    /* never used */
    return TRUE;
}

# if defined(FEAT_TERMINAL) || defined(PROTO)
/*
 * Check for any pending input or messages.
 */
    int
mch_check_messages(void)
{
    /* TODO: check for messages */
    return TRUE;
}
# endif
#endif


/*
 * set screen mode, always fails.
 */
    int
mch_screenmode(char_u *arg UNUSED)
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
    SYSTEM_INFO			si;
    MEMORY_BASIC_INFORMATION	mbi;
    size_t			length = 0;
    size_t			i;
    const char_u		*p;

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
    int
mch_icon_load(HANDLE *iconp)
{
    return do_in_runtimepath((char_u *)"bitmaps/vim.ico",
						  0, mch_icon_load_cb, iconp);
}

    int
mch_libcall(
    char_u	*libname,
    char_u	*funcname,
    char_u	*argstring,	/* NULL when using a argint */
    int		argint,
    char_u	**string_result,/* NULL when using number_result */
    int		*number_result)
{
    HINSTANCE		hinstLib;
    MYSTRPROCSTR	ProcAdd;
    MYINTPROCSTR	ProcAddI;
    char_u		*retval_str = NULL;
    int			retval_int = 0;
    size_t		len;

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
	    ProcAddI = (MYINTPROCSTR) GetProcAddress(hinstLib, (LPCSTR)funcname);
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
	else if (retval_str != NULL
		&& (len = check_str_len(retval_str)) > 0)
	{
	    *string_result = alloc(len);
	    if (*string_result != NULL)
		mch_memmove(*string_result, retval_str, len);
	}

#ifdef HAVE_TRY_EXCEPT
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
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
    void
DumpPutS(const char *psz UNUSED)
{
# ifdef MCH_WRITE_DUMP
    if (fdDump)
    {
	fputs(psz, fdDump);
	if (psz[strlen(psz) - 1] != '\n')
	    fputc('\n', fdDump);
	fflush(fdDump);
    }
# endif
}

#ifdef _DEBUG

void __cdecl
Trace(
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

#if !defined(FEAT_GUI) || defined(VIMDLL) || defined(PROTO)
# ifdef FEAT_TITLE
extern HWND g_hWnd;	/* This is in os_win32.c. */
# endif

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

# ifdef FEAT_TITLE
    /* Window handle may have been found by init code (Windows NT only) */
    if (g_hWnd != 0)
    {
	s_hwnd = g_hWnd;
	return;
    }
# endif

    s_hwnd = GetConsoleWindow();
}

/*
 * Console implementation of ":winpos".
 */
    int
mch_get_winpos(int *x, int *y)
{
    RECT  rect;

    GetConsoleHwnd();
    GetWindowRect(s_hwnd, &rect);
    *x = rect.left;
    *y = rect.top;
    return OK;
}

/*
 * Console implementation of ":winpos x y".
 */
    void
mch_set_winpos(int x, int y)
{
    GetConsoleHwnd();
    SetWindowPos(s_hwnd, NULL, x, y, 0, 0,
		 SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
}
#endif

#if defined(FEAT_SHORTCUT) || defined(PROTO)
# ifndef PROTO
#  include <shlobj.h>
# endif

typedef enum _FILE_INFO_BY_HANDLE_CLASS_ {
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

typedef struct _FILE_NAME_INFO_ {
  DWORD FileNameLength;
  WCHAR FileName[1];
} FILE_NAME_INFO_;

typedef BOOL (WINAPI *pfnGetFileInformationByHandleEx)(
	HANDLE				hFile,
	FILE_INFO_BY_HANDLE_CLASS_	FileInformationClass,
	LPVOID				lpFileInformation,
	DWORD				dwBufferSize);
static pfnGetFileInformationByHandleEx pGetFileInformationByHandleEx = NULL;

typedef BOOL (WINAPI *pfnGetVolumeInformationByHandleW)(
	HANDLE	hFile,
	LPWSTR	lpVolumeNameBuffer,
	DWORD	nVolumeNameSize,
	LPDWORD	lpVolumeSerialNumber,
	LPDWORD	lpMaximumComponentLength,
	LPDWORD	lpFileSystemFlags,
	LPWSTR	lpFileSystemNameBuffer,
	DWORD	nFileSystemNameSize);
static pfnGetVolumeInformationByHandleW pGetVolumeInformationByHandleW = NULL;

    static char_u *
resolve_reparse_point(char_u *fname)
{
    HANDLE	    h = INVALID_HANDLE_VALUE;
    DWORD	    size;
    WCHAR	    *p;
    char_u	    *rfname = NULL;
    FILE_NAME_INFO_ *nameinfo = NULL;
    WCHAR	    buff[MAX_PATH], *volnames = NULL;
    HANDLE	    hv;
    DWORD	    snfile, snfind;
    static BOOL	    loaded = FALSE;

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

    do {
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
    HRESULT		hr;
    IShellLink		*psl = NULL;
    IPersistFile	*ppf = NULL;
    OLECHAR		wsz[MAX_PATH];
    char_u		*rfname = NULL;
    int			len;
    IShellLinkW		*pslw = NULL;
    WIN32_FIND_DATAW	ffdw; // we get those free of charge

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
	    &IID_IShellLinkW, (void**)&pslw);
    if (hr == S_OK)
    {
	WCHAR	*p = enc_to_utf16(fname, NULL);

	if (p != NULL)
	{
	    // Get a pointer to the IPersistFile interface.
	    hr = pslw->lpVtbl->QueryInterface(
		    pslw, &IID_IPersistFile, (void**)&ppf);
	    if (hr != S_OK)
		goto shortcut_errorw;

	    // "load" the name and resolve the link
	    hr = ppf->lpVtbl->Load(ppf, p, STGM_READ);
	    if (hr != S_OK)
		goto shortcut_errorw;
# if 0  // This makes Vim wait a long time if the target does not exist.
	    hr = pslw->lpVtbl->Resolve(pslw, NULL, SLR_NO_UI);
	    if (hr != S_OK)
		goto shortcut_errorw;
# endif

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
    char_u  *path = resolve_shortcut(fname);

    if (path == NULL && reparse_point)
	path = resolve_reparse_point(fname);
    return path;
}
#endif

#if (defined(FEAT_EVAL) && (!defined(FEAT_GUI) || defined(VIMDLL))) || defined(PROTO)
/*
 * Bring ourselves to the foreground.  Does work if the OS doesn't allow it.
 */
    void
win32_set_foreground(void)
{
    GetConsoleHwnd();	    /* get value of s_hwnd */
    if (s_hwnd != 0)
	SetForegroundWindow(s_hwnd);
}
#endif

#if defined(FEAT_CLIENTSERVER) || defined(PROTO)
/*
 * Client-server code for Vim
 *
 * Originally written by Paul Moore
 */

/* In order to handle inter-process messages, we need to have a window. But
 * the functions in this module can be called before the main GUI window is
 * created (and may also be called in the console version, where there is no
 * GUI window at all).
 *
 * So we create a hidden window, and arrange to destroy it on exit.
 */
HWND message_window = 0;	    /* window that's handling messages */

#define VIM_CLASSNAME      "VIM_MESSAGES"
#define VIM_CLASSNAME_LEN  (sizeof(VIM_CLASSNAME) - 1)

/* Communication is via WM_COPYDATA messages. The message type is send in
 * the dwData parameter. Types are defined here. */
#define COPYDATA_KEYS		0
#define COPYDATA_REPLY		1
#define COPYDATA_EXPR		10
#define COPYDATA_RESULT		11
#define COPYDATA_ERROR_RESULT	12
#define COPYDATA_ENCODING	20

/* This is a structure containing a server HWND and its name. */
struct server_id
{
    HWND hwnd;
    char_u *name;
};

/* Last received 'encoding' that the client uses. */
static char_u	*client_enc = NULL;

/*
 * Tell the other side what encoding we are using.
 * Errors are ignored.
 */
    static void
serverSendEnc(HWND target)
{
    COPYDATASTRUCT data;

    data.dwData = COPYDATA_ENCODING;
    data.cbData = (DWORD)STRLEN(p_enc) + 1;
    data.lpData = p_enc;
    (void)SendMessage(target, WM_COPYDATA, (WPARAM)message_window,
							     (LPARAM)(&data));
}

/*
 * Clean up on exit. This destroys the hidden message window.
 */
    static void
CleanUpMessaging(void)
{
    if (message_window != 0)
    {
	DestroyWindow(message_window);
	message_window = 0;
    }
}

static int save_reply(HWND server, char_u *reply, int expr);

/*
 * The window procedure for the hidden message window.
 * It handles callback messages and notifications from servers.
 * In order to process these messages, it is necessary to run a
 * message loop. Code which may run before the main message loop
 * is started (in the GUI) is careful to pump messages when it needs
 * to. Features which require message delivery during normal use will
 * not work in the console version - this basically means those
 * features which allow Vim to act as a server, rather than a client.
 */
    static LRESULT CALLBACK
Messaging_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_COPYDATA)
    {
	/* This is a message from another Vim. The dwData member of the
	 * COPYDATASTRUCT determines the type of message:
	 *   COPYDATA_ENCODING:
	 *	The encoding that the client uses. Following messages will
	 *	use this encoding, convert if needed.
	 *   COPYDATA_KEYS:
	 *	A key sequence. We are a server, and a client wants these keys
	 *	adding to the input queue.
	 *   COPYDATA_REPLY:
	 *	A reply. We are a client, and a server has sent this message
	 *	in response to a request.  (server2client())
	 *   COPYDATA_EXPR:
	 *	An expression. We are a server, and a client wants us to
	 *	evaluate this expression.
	 *   COPYDATA_RESULT:
	 *	A reply. We are a client, and a server has sent this message
	 *	in response to a COPYDATA_EXPR.
	 *   COPYDATA_ERROR_RESULT:
	 *	A reply. We are a client, and a server has sent this message
	 *	in response to a COPYDATA_EXPR that failed to evaluate.
	 */
	COPYDATASTRUCT	*data = (COPYDATASTRUCT*)lParam;
	HWND		sender = (HWND)wParam;
	COPYDATASTRUCT	reply;
	char_u		*res;
	int		retval;
	char_u		*str;
	char_u		*tofree;

	switch (data->dwData)
	{
	case COPYDATA_ENCODING:
	    /* Remember the encoding that the client uses. */
	    vim_free(client_enc);
	    client_enc = enc_canonize((char_u *)data->lpData);
	    return 1;

	case COPYDATA_KEYS:
	    /* Remember who sent this, for <client> */
	    clientWindow = sender;

	    /* Add the received keys to the input buffer.  The loop waiting
	     * for the user to do something should check the input buffer. */
	    str = serverConvert(client_enc, (char_u *)data->lpData, &tofree);
	    server_to_input_buf(str);
	    vim_free(tofree);

# ifdef FEAT_GUI
	    /* Wake up the main GUI loop. */
#  ifdef VIMDLL
	    if (gui.in_use)
#  endif
		if (s_hwnd != 0)
		    PostMessage(s_hwnd, WM_NULL, 0, 0);
# endif
	    return 1;

	case COPYDATA_EXPR:
	    /* Remember who sent this, for <client> */
	    clientWindow = sender;

	    str = serverConvert(client_enc, (char_u *)data->lpData, &tofree);
	    res = eval_client_expr_to_string(str);

	    if (res == NULL)
	    {
		char	*err = _(e_invexprmsg);
		size_t	len = STRLEN(str) + STRLEN(err) + 5;

		res = alloc(len);
		if (res != NULL)
		    vim_snprintf((char *)res, len, "%s: \"%s\"", err, str);
		reply.dwData = COPYDATA_ERROR_RESULT;
	    }
	    else
		reply.dwData = COPYDATA_RESULT;
	    reply.lpData = res;
	    reply.cbData = (DWORD)STRLEN(res) + 1;

	    serverSendEnc(sender);
	    retval = (int)SendMessage(sender, WM_COPYDATA,
				    (WPARAM)message_window, (LPARAM)(&reply));
	    vim_free(tofree);
	    vim_free(res);
	    return retval;

	case COPYDATA_REPLY:
	case COPYDATA_RESULT:
	case COPYDATA_ERROR_RESULT:
	    if (data->lpData != NULL)
	    {
		str = serverConvert(client_enc, (char_u *)data->lpData,
								     &tofree);
		if (tofree == NULL)
		    str = vim_strsave(str);
		if (save_reply(sender, str,
			   (data->dwData == COPYDATA_REPLY ?  0 :
			   (data->dwData == COPYDATA_RESULT ? 1 :
							      2))) == FAIL)
		    vim_free(str);
		else if (data->dwData == COPYDATA_REPLY)
		{
		    char_u	winstr[30];

		    sprintf((char *)winstr, PRINTF_HEX_LONG_U, (long_u)sender);
		    apply_autocmds(EVENT_REMOTEREPLY, winstr, str,
								TRUE, curbuf);
		}
	    }
	    return 1;
	}

	return 0;
    }

    else if (msg == WM_ACTIVATE && wParam == WA_ACTIVE)
    {
	/* When the message window is activated (brought to the foreground),
	 * this actually applies to the text window. */
#if !defined(FEAT_GUI) || defined(VIMDLL)
# ifdef VIMDLL
	if (!gui.in_use)
# endif
	    GetConsoleHwnd();	    /* get value of s_hwnd */
#endif
	if (s_hwnd != 0)
	{
	    SetForegroundWindow(s_hwnd);
	    return 0;
	}
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/*
 * Initialise the message handling process.  This involves creating a window
 * to handle messages - the window will not be visible.
 */
    void
serverInitMessaging(void)
{
    WNDCLASS wndclass;

    /* Clean up on exit */
    atexit(CleanUpMessaging);

    /* Register a window class - we only really care
     * about the window procedure
     */
    wndclass.style = 0;
    wndclass.lpfnWndProc = Messaging_WndProc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = 0;
    wndclass.hInstance = g_hinst;
    wndclass.hIcon = NULL;
    wndclass.hCursor = NULL;
    wndclass.hbrBackground = NULL;
    wndclass.lpszMenuName = NULL;
    wndclass.lpszClassName = VIM_CLASSNAME;
    RegisterClass(&wndclass);

    /* Create the message window. It will be hidden, so the details don't
     * matter.  Don't use WS_OVERLAPPEDWINDOW, it will make a shortcut remove
     * focus from gvim. */
    message_window = CreateWindow(VIM_CLASSNAME, "",
			 WS_POPUPWINDOW | WS_CAPTION,
			 CW_USEDEFAULT, CW_USEDEFAULT,
			 100, 100, NULL, NULL,
			 g_hinst, NULL);
}

/* Used by serverSendToVim() to find an alternate server name. */
static char_u *altname_buf_ptr = NULL;

/*
 * Get the title of the window "hwnd", which is the Vim server name, in
 * "name[namelen]" and return the length.
 * Returns zero if window "hwnd" is not a Vim server.
 */
    static int
getVimServerName(HWND hwnd, char *name, int namelen)
{
    int		len;
    char	buffer[VIM_CLASSNAME_LEN + 1];

    /* Ignore windows which aren't Vim message windows */
    len = GetClassName(hwnd, buffer, sizeof(buffer));
    if (len != VIM_CLASSNAME_LEN || STRCMP(buffer, VIM_CLASSNAME) != 0)
	return 0;

    /* Get the title of the window */
    return GetWindowText(hwnd, name, namelen);
}

    static BOOL CALLBACK
enumWindowsGetServer(HWND hwnd, LPARAM lparam)
{
    struct	server_id *id = (struct server_id *)lparam;
    char	server[MAX_PATH];

    /* Get the title of the window */
    if (getVimServerName(hwnd, server, sizeof(server)) == 0)
	return TRUE;

    /* If this is the server we're looking for, return its HWND */
    if (STRICMP(server, id->name) == 0)
    {
	id->hwnd = hwnd;
	return FALSE;
    }

    /* If we are looking for an alternate server, remember this name. */
    if (altname_buf_ptr != NULL
	    && STRNICMP(server, id->name, STRLEN(id->name)) == 0
	    && vim_isdigit(server[STRLEN(id->name)]))
    {
	STRCPY(altname_buf_ptr, server);
	altname_buf_ptr = NULL;	    /* don't use another name */
    }

    /* Otherwise, keep looking */
    return TRUE;
}

    static BOOL CALLBACK
enumWindowsGetNames(HWND hwnd, LPARAM lparam)
{
    garray_T	*ga = (garray_T *)lparam;
    char	server[MAX_PATH];

    /* Get the title of the window */
    if (getVimServerName(hwnd, server, sizeof(server)) == 0)
	return TRUE;

    /* Add the name to the list */
    ga_concat(ga, (char_u *)server);
    ga_concat(ga, (char_u *)"\n");
    return TRUE;
}

struct enum_windows_s
{
    WNDENUMPROC lpEnumFunc;
    LPARAM      lParam;
};

    static BOOL CALLBACK
enum_windows_child(HWND hwnd, LPARAM lParam)
{
    struct enum_windows_s *ew = (struct enum_windows_s *)lParam;

    return (ew->lpEnumFunc)(hwnd, ew->lParam);
}

    static BOOL CALLBACK
enum_windows_toplevel(HWND hwnd, LPARAM lParam)
{
    struct enum_windows_s *ew = (struct enum_windows_s *)lParam;

    if ((ew->lpEnumFunc)(hwnd, ew->lParam))
	return TRUE;
    return EnumChildWindows(hwnd, enum_windows_child, lParam);
}

/* Enumerate all windows including children. */
    static BOOL
enum_windows(WNDENUMPROC lpEnumFunc, LPARAM lParam)
{
    struct enum_windows_s ew;

    ew.lpEnumFunc = lpEnumFunc;
    ew.lParam = lParam;
    return EnumWindows(enum_windows_toplevel, (LPARAM)&ew);
}

    static HWND
findServer(char_u *name)
{
    struct server_id id;

    id.name = name;
    id.hwnd = 0;

    enum_windows(enumWindowsGetServer, (LPARAM)(&id));

    return id.hwnd;
}

    void
serverSetName(char_u *name)
{
    char_u	*ok_name;
    HWND	hwnd = 0;
    int		i = 0;
    char_u	*p;

    /* Leave enough space for a 9-digit suffix to ensure uniqueness! */
    ok_name = alloc(STRLEN(name) + 10);

    STRCPY(ok_name, name);
    p = ok_name + STRLEN(name);

    for (;;)
    {
	/* This is inefficient - we're doing an EnumWindows loop for each
	 * possible name. It would be better to grab all names in one go,
	 * and scan the list each time...
	 */
	hwnd = findServer(ok_name);
	if (hwnd == 0)
	    break;

	++i;
	if (i >= 1000)
	    break;

	sprintf((char *)p, "%d", i);
    }

    if (hwnd != 0)
	vim_free(ok_name);
    else
    {
	/* Remember the name */
	serverName = ok_name;
#ifdef FEAT_TITLE
	need_maketitle = TRUE;	/* update Vim window title later */
#endif

	/* Update the message window title */
	SetWindowText(message_window, (LPCSTR)ok_name);

#ifdef FEAT_EVAL
	/* Set the servername variable */
	set_vim_var_string(VV_SEND_SERVER, serverName, -1);
#endif
    }
}

    char_u *
serverGetVimNames(void)
{
    garray_T ga;

    ga_init2(&ga, 1, 100);

    enum_windows(enumWindowsGetNames, (LPARAM)(&ga));
    ga_append(&ga, NUL);

    return ga.ga_data;
}

    int
serverSendReply(
    char_u	*name,		/* Where to send. */
    char_u	*reply)		/* What to send. */
{
    HWND	target;
    COPYDATASTRUCT data;
    long_u	n = 0;

    /* The "name" argument is a magic cookie obtained from expand("<client>").
     * It should be of the form 0xXXXXX - i.e. a C hex literal, which is the
     * value of the client's message window HWND.
     */
    sscanf((char *)name, SCANF_HEX_LONG_U, &n);
    if (n == 0)
	return -1;

    target = (HWND)n;
    if (!IsWindow(target))
	return -1;

    data.dwData = COPYDATA_REPLY;
    data.cbData = (DWORD)STRLEN(reply) + 1;
    data.lpData = reply;

    serverSendEnc(target);
    if (SendMessage(target, WM_COPYDATA, (WPARAM)message_window,
							     (LPARAM)(&data)))
	return 0;

    return -1;
}

    int
serverSendToVim(
    char_u	 *name,			/* Where to send. */
    char_u	 *cmd,			/* What to send. */
    char_u	 **result,		/* Result of eval'ed expression */
    void	 *ptarget,		/* HWND of server */
    int		 asExpr,		/* Expression or keys? */
    int		 timeout,		/* timeout in seconds or zero */
    int		 silent)		/* don't complain about no server */
{
    HWND	target;
    COPYDATASTRUCT data;
    char_u	*retval = NULL;
    int		retcode = 0;
    char_u	altname_buf[MAX_PATH];

    /* Execute locally if no display or target is ourselves */
    if (serverName != NULL && STRICMP(name, serverName) == 0)
	return sendToLocalVim(cmd, asExpr, result);

    /* If the server name does not end in a digit then we look for an
     * alternate name.  e.g. when "name" is GVIM the we may find GVIM2. */
    if (STRLEN(name) > 1 && !vim_isdigit(name[STRLEN(name) - 1]))
	altname_buf_ptr = altname_buf;
    altname_buf[0] = NUL;
    target = findServer(name);
    altname_buf_ptr = NULL;
    if (target == 0 && altname_buf[0] != NUL)
	/* Use another server name we found. */
	target = findServer(altname_buf);

    if (target == 0)
    {
	if (!silent)
	    semsg(_(e_noserver), name);
	return -1;
    }

    if (ptarget)
	*(HWND *)ptarget = target;

    data.dwData = asExpr ? COPYDATA_EXPR : COPYDATA_KEYS;
    data.cbData = (DWORD)STRLEN(cmd) + 1;
    data.lpData = cmd;

    serverSendEnc(target);
    if (SendMessage(target, WM_COPYDATA, (WPARAM)message_window,
							(LPARAM)(&data)) == 0)
	return -1;

    if (asExpr)
	retval = serverGetReply(target, &retcode, TRUE, TRUE, timeout);

    if (result == NULL)
	vim_free(retval);
    else
	*result = retval; /* Caller assumes responsibility for freeing */

    return retcode;
}

/*
 * Bring the server to the foreground.
 */
    void
serverForeground(char_u *name)
{
    HWND	target = findServer(name);

    if (target != 0)
	SetForegroundWindow(target);
}

/* Replies from server need to be stored until the client picks them up via
 * remote_read(). So we maintain a list of server-id/reply pairs.
 * Note that there could be multiple replies from one server pending if the
 * client is slow picking them up.
 * We just store the replies in a simple list. When we remove an entry, we
 * move list entries down to fill the gap.
 * The server ID is simply the HWND.
 */
typedef struct
{
    HWND	server;		/* server window */
    char_u	*reply;		/* reply string */
    int		expr_result;	/* 0 for REPLY, 1 for RESULT 2 for error */
} reply_T;

static garray_T reply_list = {0, 0, sizeof(reply_T), 5, 0};

#define REPLY_ITEM(i) ((reply_T *)(reply_list.ga_data) + (i))
#define REPLY_COUNT (reply_list.ga_len)

/* Flag which is used to wait for a reply */
static int reply_received = 0;

/*
 * Store a reply.  "reply" must be allocated memory (or NULL).
 */
    static int
save_reply(HWND server, char_u *reply, int expr)
{
    reply_T *rep;

    if (ga_grow(&reply_list, 1) == FAIL)
	return FAIL;

    rep = REPLY_ITEM(REPLY_COUNT);
    rep->server = server;
    rep->reply = reply;
    rep->expr_result = expr;
    if (rep->reply == NULL)
	return FAIL;

    ++REPLY_COUNT;
    reply_received = 1;
    return OK;
}

/*
 * Get a reply from server "server".
 * When "expr_res" is non NULL, get the result of an expression, otherwise a
 * server2client() message.
 * When non NULL, point to return code. 0 => OK, -1 => ERROR
 * If "remove" is TRUE, consume the message, the caller must free it then.
 * if "wait" is TRUE block until a message arrives (or the server exits).
 */
    char_u *
serverGetReply(HWND server, int *expr_res, int remove, int wait, int timeout)
{
    int		i;
    char_u	*reply;
    reply_T	*rep;
    int		did_process = FALSE;
    time_t	start;
    time_t	now;

    /* When waiting, loop until the message waiting for is received. */
    time(&start);
    for (;;)
    {
	/* Reset this here, in case a message arrives while we are going
	 * through the already received messages. */
	reply_received = 0;

	for (i = 0; i < REPLY_COUNT; ++i)
	{
	    rep = REPLY_ITEM(i);
	    if (rep->server == server
		    && ((rep->expr_result != 0) == (expr_res != NULL)))
	    {
		/* Save the values we've found for later */
		reply = rep->reply;
		if (expr_res != NULL)
		    *expr_res = rep->expr_result == 1 ? 0 : -1;

		if (remove)
		{
		    /* Move the rest of the list down to fill the gap */
		    mch_memmove(rep, rep + 1,
				     (REPLY_COUNT - i - 1) * sizeof(reply_T));
		    --REPLY_COUNT;
		}

		/* Return the reply to the caller, who takes on responsibility
		 * for freeing it if "remove" is TRUE. */
		return reply;
	    }
	}

	/* If we got here, we didn't find a reply. Return immediately if the
	 * "wait" parameter isn't set.  */
	if (!wait)
	{
	    /* Process pending messages once. Without this, looping on
	     * remote_peek() would never get the reply. */
	    if (!did_process)
	    {
		did_process = TRUE;
		serverProcessPendingMessages();
		continue;
	    }
	    break;
	}

	/* We need to wait for a reply. Enter a message loop until the
	 * "reply_received" flag gets set. */

	/* Loop until we receive a reply */
	while (reply_received == 0)
	{
#ifdef FEAT_TIMERS
	    /* TODO: use the return value to decide how long to wait. */
	    check_due_timer();
#endif
	    time(&now);
	    if (timeout > 0 && (now - start) >= timeout)
		break;

	    /* Wait for a SendMessage() call to us.  This could be the reply
	     * we are waiting for.  Use a timeout of a second, to catch the
	     * situation that the server died unexpectedly. */
	    MsgWaitForMultipleObjects(0, NULL, TRUE, 1000, QS_ALLINPUT);

	    /* If the server has died, give up */
	    if (!IsWindow(server))
		return NULL;

	    serverProcessPendingMessages();
	}
    }

    return NULL;
}

/*
 * Process any messages in the Windows message queue.
 */
    void
serverProcessPendingMessages(void)
{
    MSG msg;

    while (pPeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
	TranslateMessage(&msg);
	pDispatchMessage(&msg);
    }
}

#endif /* FEAT_CLIENTSERVER */

#if defined(FEAT_GUI) || (defined(FEAT_PRINTER) && !defined(FEAT_POSTSCRIPT)) \
	|| defined(PROTO)

struct charset_pair
{
    char	*name;
    BYTE	charset;
};

static struct charset_pair
charset_pairs[] =
{
    {"ANSI",		ANSI_CHARSET},
    {"CHINESEBIG5",	CHINESEBIG5_CHARSET},
    {"DEFAULT",		DEFAULT_CHARSET},
    {"HANGEUL",		HANGEUL_CHARSET},
    {"OEM",		OEM_CHARSET},
    {"SHIFTJIS",	SHIFTJIS_CHARSET},
    {"SYMBOL",		SYMBOL_CHARSET},
    {"ARABIC",		ARABIC_CHARSET},
    {"BALTIC",		BALTIC_CHARSET},
    {"EASTEUROPE",	EASTEUROPE_CHARSET},
    {"GB2312",		GB2312_CHARSET},
    {"GREEK",		GREEK_CHARSET},
    {"HEBREW",		HEBREW_CHARSET},
    {"JOHAB",		JOHAB_CHARSET},
    {"MAC",		MAC_CHARSET},
    {"RUSSIAN",		RUSSIAN_CHARSET},
    {"THAI",		THAI_CHARSET},
    {"TURKISH",		TURKISH_CHARSET},
#ifdef VIETNAMESE_CHARSET
    {"VIETNAMESE",	VIETNAMESE_CHARSET},
#endif
    {NULL,		0}
};

struct quality_pair
{
    char	*name;
    DWORD	quality;
};

static struct quality_pair
quality_pairs[] = {
#ifdef CLEARTYPE_QUALITY
    {"CLEARTYPE",	CLEARTYPE_QUALITY},
#endif
#ifdef ANTIALIASED_QUALITY
    {"ANTIALIASED",	ANTIALIASED_QUALITY},
#endif
#ifdef NONANTIALIASED_QUALITY
    {"NONANTIALIASED",	NONANTIALIASED_QUALITY},
#endif
#ifdef PROOF_QUALITY
    {"PROOF",		PROOF_QUALITY},
#endif
#ifdef DRAFT_QUALITY
    {"DRAFT",		DRAFT_QUALITY},
#endif
    {"DEFAULT",		DEFAULT_QUALITY},
    {NULL,		0}
};

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
    L"Fixedsys"	/* see _ReadVimIni */
};

// Initialise the "current height" to -12 (same as s_lfDefault) just
// in case the user specifies a font in "guifont" with no size before a font
// with an explicit size has been set. This defaults the size to this value
// (-12 equates to roughly 9pt).
int current_font_height = -12;		// also used in gui_w32.c

/* Convert a string representing a point size into pixels. The string should
 * be a positive decimal number, with an optional decimal point (eg, "12", or
 * "10.5"). The pixel value is returned, and a pointer to the next unconverted
 * character is stored in *end. The flag "vertical" says whether this
 * calculation is for a vertical (height) size or a horizontal (width) one.
 */
    static int
points_to_pixels(WCHAR *str, WCHAR **end, int vertical, long_i pprinter_dc)
{
    int		pixels;
    int		points = 0;
    int		divisor = 0;
    HWND	hwnd = (HWND)0;
    HDC		hdc;
    HDC		printer_dc = (HDC)pprinter_dc;

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
    ENUMLOGFONTW    *elf,
    NEWTEXTMETRICW  *ntm UNUSED,
    DWORD	    type UNUSED,
    LPARAM	    lparam)
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
    if (lf->lfCharSet == ANSI_CHARSET
	    || lf->lfCharSet == OEM_CHARSET
	    || lf->lfCharSet == DEFAULT_CHARSET)
	return 0;

    /* Continue the scan - we have a non-ANSI font */
    return 2;
}

    static int
init_logfont(LOGFONTW *lf)
{
    int		n;
    HWND	hwnd = GetDesktopWindow();
    HDC		hdc = GetWindowDC(hwnd);

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
    int
get_logfont(
    LOGFONTW	*lf,
    char_u	*name,
    HDC		printer_dc,
    int		verbose)
{
    WCHAR	*p;
    int		i;
    int		ret = FAIL;
    static LOGFONTW *lastlf = NULL;
    WCHAR	*wname;

    *lf = s_lfDefault;
    if (name == NULL)
	return OK;

    wname = enc_to_utf16(name, NULL);
    if (wname == NULL)
	return FAIL;

    if (wcscmp(wname, L"*") == 0)
    {
#if defined(FEAT_GUI_MSWIN)
	CHOOSEFONTW	cf;
	/* if name is "*", bring up std font dialog: */
	vim_memset(&cf, 0, sizeof(cf));
	cf.lStructSize = sizeof(cf);
	cf.hwndOwner = s_hwnd;
	cf.Flags = CF_SCREENFONTS | CF_FIXEDPITCHONLY | CF_INITTOLOGFONTSTRUCT;
	if (lastlf != NULL)
	    *lf = *lastlf;
	cf.lpLogFont = lf;
	cf.nFontType = 0 ; //REGULAR_FONTTYPE;
	if (ChooseFontW(&cf))
	    ret = OK;
#endif
	goto theend;
    }

    /*
     * Split name up, it could be <name>:h<height>:w<width> etc.
     */
    for (p = wname; *p && *p != L':'; p++)
    {
	if (p - wname + 1 >= LF_FACESIZE)
	    goto theend;			/* Name too long */
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
	int	did_replace = FALSE;

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
		lf->lfHeight = - points_to_pixels(p, &p, TRUE, (long_i)printer_dc);
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

#endif /* defined(FEAT_GUI) || defined(FEAT_PRINTER) */

#if defined(FEAT_JOB_CHANNEL) || defined(PROTO)
/*
 * Initialize the Winsock dll.
 */
    void
channel_init_winsock(void)
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
