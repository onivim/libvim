#include "vim.h"

/* libvim.c */

/*
 * vimInit
 *
 * This must be called prior to using any other methods.
 *
 * This expects an `argc` and an `argv` parameters,
 * for the command line arguments for this vim instance.
 */
void vimInit(int argc, char **argv);

/***
 * Buffer Methods
 ***/

/*
 * vimBufferOpen
 *
 * Open a buffer and set as current.
 */

buf_T *vimBufferOpen(char_u *ffname_arg, linenr_T lnum, int flags);
/*
 * vimBufferCheckIfChanged
 *
 * Check if the contents of a buffer have been changed on the filesystem, outside of libvim.
 * Returns 1 if buffer was changed (and changes the buffer contents)
 * Returns 2 if a message was displayed
 * Returns 0 otherwise
 */
int vimBufferCheckIfChanged(buf_T *buf);
buf_T *vimBufferGetById(int id);
buf_T *vimBufferGetCurrent(void);
void vimBufferSetCurrent(buf_T *buf);

char_u *vimBufferGetFilename(buf_T *buf);
char_u *vimBufferGetFiletype(buf_T *buf);
int vimBufferGetId(buf_T *buf);
long vimBufferGetLastChangedTick(buf_T *buf);
char_u *vimBufferGetLine(buf_T *buf, linenr_T lnum);
size_t vimBufferGetLineCount(buf_T *buf);

/*
 * vimBufferSetLines
 *
 * Set a range of lines from the one-based start line to one-based end, inclusive.
 * 
 * Examples:
 * vimBufferSetLine(buf, 1, 1, ["abc"]); // Set line 1 to "abc""
 * vimBufferSetLine(buf, 1, 2, ["abc"]); // Remove line 2, set line 1 to "abc"
 * vimBufferSetLine(buf, 0, 0, ["def"]); // Insert "def" before the contents of the buffer
 */
void vimBufferSetLines(buf_T *buf, linenr_T start, linenr_T end, char_u **lines, int count);

int vimBufferGetModified(buf_T *buf);

int vimBufferGetModifiable(buf_T *buf);
void vimBufferSetModifiable(buf_T *buf, int modifiable);

int vimBufferGetFileFormat(buf_T *buf);
void vimBufferSetFileFormat(buf_T *buf, int fileformat);

int vimBufferGetReadOnly(buf_T *buf);
void vimBufferSetReadOnly(buf_T *buf, int modifiable);

void vimSetBufferUpdateCallback(BufferUpdateCallback bufferUpdate);

/***
 * Autocommands
 ***/

void vimSetAutoCommandCallback(AutoCommandCallback autoCommandDispatch);

/**
 * Commandline
 ***/

char_u vimCommandLineGetType(void);
char_u *vimCommandLineGetText(void);
int vimCommandLineGetPosition(void);
void vimCommandLineGetCompletions(char_u ***completions, int *count);

/**
* Eval
***/

/***
 * vimEval
 * 
 * Evaluate a string as vim script, and return the result as string.
 * Callee is responsible for freeing the command as well as the result.
 */
char_u *vimEval(char_u *str);

/***
 * Cursor Methods
 ***/
colnr_T vimCursorGetColumn(void);
linenr_T vimCursorGetLine(void);
pos_T vimCursorGetPosition(void);
void vimCursorSetPosition(pos_T pos);

/***
 * vimCursorGetDesiredColumn
 *
 * Get the column that we'd like to be at - used to stay in the same
 * column for up/down cursor motions.
 */
colnr_T vimCursorGetDesiredColumn(void);

/***
 * File I/O
 ***/
void vimSetFileWriteFailureCallback(FileWriteFailureCallback fileWriteFailureCallback);

/***
 * User Input
 ***/

/***
 * vimInput
 *
 * vimInput(input) passes the string, verbatim, to vim to be processed,
 * without replacing term-codes. This means strings like "<LEFT>" are 
 * handled literally. This function handles Unicode text correctly.
 */
void vimInput(char_u *input);

/***
 * vimKey
 *
 * vimKey(input) passes a string and escapes termcodes - so a 
 * a string like "<LEFT>" will first be replaced with the appropriate
 * term-code, and handled.
 */
void vimKey(char_u *key);

/***
 * vimExecute
 *
 * vimExecute(cmd) executes a command as if it was typed at the command-line.
 *
 * Example: vimExecute("echo 'hello!');
 */
void vimExecute(char_u *cmd);

/***
 * Auto-indent
 ***/

int vimSetAutoIndentCallback(AutoIndentCallback callback);

/***
 * Colorschemes
 */
void vimColorSchemeSetChangedCallback(ColorSchemeChangedCallback callback);
void vimColorSchemeSetCompletionCallback(ColorSchemeCompletionCallback callback);

/***
 * Messages
 ***/

void vimSetMessageCallback(MessageCallback messageCallback);

/**
 * Misc
 **/

void vimSetFormatCallback(FormatCallback formatCallback);
void vimSetGotoCallback(GotoCallback gotoCallback);
void vimSetTabPageCallback(TabPageCallback tabPageCallback);
void vimSetDirectoryChangedCallback(DirectoryChangedCallback callback);
void vimSetOptionSetCallback(OptionSetCallback callback);

/*
 * vimSetQuitCallback
 *
 * Called when a `:q`, `:qa`, `:q!` is called
 * 
 * It is up to the libvim consumer how to handle the 'quit' call.
 * There are two arguments passed:
 * - `buffer`: the buffer quit was requested for
 * - `force`: a boolean if the command was forced (ie, if `q!` was used)
 */
void vimSetQuitCallback(QuitCallback callback);

/*
 * vimSetUnhandledEscapeCallback
 *
 * Called when <esc> is pressed in normal mode, but there is no
 * pending operator or action.
 *
 * This is intended for UI's to pick up and handle (for example,
 * to clear messages or alerts).
 */
void vimSetUnhandledEscapeCallback(VoidCallback callback);

/***
 * Macros
 */

void vimMacroSetStartRecordCallback(MacroStartRecordCallback callback);
void vimMacroSetStopRecordCallback(MacroStopRecordCallback callback);

/***
 * Options
 **/

void vimOptionSetTabSize(int tabSize);
void vimOptionSetInsertSpaces(int insertSpaces);
void vimOptionSetLineComment(char_u *str);

int vimOptionGetInsertSpaces(void);
int vimOptionGetTabSize(void);

/***
 * Registers
 ***/

void vimRegisterGet(int reg_name, int *num_lines, char_u ***lines);

/***
 * Undo
 ***/

int vimUndoSaveCursor(void);
int vimUndoSaveRegion(linenr_T start_lnum, linenr_T end_lnum);

/*
 * vimUndoSync(force)
 *
 * Create a sync point (a new undo level) - stop adding to current
 * undo entry, and start a new one.
 */
void vimUndoSync(int force);

/***
 * Visual Mode
 ***/

int vimVisualGetType(void);
int vimVisualIsActive(void);
int vimSelectIsActive(void);

/*
 * vimVisualGetRange
 *
 * If in visual mode or select mode, returns the current range.
 * If not in visual or select mode, returns the last visual range.
 */
void vimVisualGetRange(pos_T *startPos, pos_T *endPos);

/***
 * Search
 ***/

/*
 * vimSearchGetMatchingPair
 *
 * Returns the position of a matching pair,
 * based on the current buffer and cursor position
 *
 * result is NULL if no match found.
 */
pos_T *vimSearchGetMatchingPair(int initc);

/*
 * vimSearchGetHighlights
 *
 * Get highlights for the current search
 */
void vimSearchGetHighlights(linenr_T start_lnum, linenr_T end_lnum,
                            int *num_highlights,
                            searchHighlight_T **highlights);

/*
 * vimSearchGetPattern
 *
 * Get the current search pattern
 */
char_u *vimSearchGetPattern();

void vimSetStopSearchHighlightCallback(VoidCallback callback);

/***
 * Terminal
 */

void vimSetTerminalCallback(TerminalCallback callback);

/***
 * Window
 */

int vimWindowGetWidth(void);
int vimWindowGetHeight(void);
int vimWindowGetTopLine(void);
int vimWindowGetLeftColumn(void);

void vimWindowSetWidth(int width);
void vimWindowSetHeight(int height);
void vimWindowSetTopLeft(int top, int left);

void vimSetWindowSplitCallback(WindowSplitCallback callback);
void vimSetWindowMovementCallback(WindowMovementCallback callback);

/***
 * Misc
 ***/

void vimSetClipboardGetCallback(ClipboardGetCallback callback);

int vimGetMode(void);
int vimGetPendingOperator(pendingOp_T *pendingOp);

void vimSetYankCallback(YankCallback callback);

/* Callbacks for when the `:intro` and `:version` commands are used
  
  The Vim license has some specific requirements when implementing these methods:
    
    3) A message must be added, at least in the output of the ":version"
       command and in the intro screen, such that the user of the modified Vim
       is able to see that it was modified.  When distributing as mentioned
       under 2)e) adding the message is only required for as far as this does
       not conflict with the license used for the changes.
*/
void vimSetDisplayIntroCallback(VoidCallback callback);
void vimSetDisplayVersionCallback(VoidCallback callback);

/* vim: set ft=c : */
