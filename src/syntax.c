/* vi:set ts=8 sts=4 sw=4 noet:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */

/*
 * syntax.c: code for syntax highlighting
 */

#include "vim.h"

/*
 * Structure that stores information about a highlight group.
 * The ID of a highlight group is also called group ID.  It is the index in
 * the highlight_ga array PLUS ONE.
 */
struct hl_group
{
  char_u *sg_name;   /* highlight group name */
  char_u *sg_name_u; /* uppercase of sg_name */
  int sg_cleared;    /* "hi clear" was used */
                     /* for normal terminals */
  int sg_term;       /* "term=" highlighting attributes */
  char_u *sg_start;  /* terminal string for start highl */
  char_u *sg_stop;   /* terminal string for stop highl */
  int sg_term_attr;  /* Screen attr for term mode */
                     /* for color terminals */
  int sg_cterm;      /* "cterm=" highlighting attr */
  int sg_cterm_bold; /* bold attr was set for light color */
  int sg_cterm_fg;   /* terminal fg color number + 1 */
  int sg_cterm_bg;   /* terminal bg color number + 1 */
  int sg_cterm_attr; /* Screen attr for color term mode */
/* for when using the GUI */
#if defined(FEAT_EVAL)
  /* Store the sp color name for the GUI or synIDattr() */
  int sg_gui;             /* "gui=" highlighting attributes */
  char_u *sg_gui_fg_name; /* GUI foreground color name */
  char_u *sg_gui_bg_name; /* GUI background color name */
  char_u *sg_gui_sp_name; /* GUI special color name */
#endif
  int sg_link; /* link to this highlight group ID */
  int sg_set;  /* combination of SG_* flags */
#ifdef FEAT_EVAL
  sctx_T sg_script_ctx; /* script in which the group was last set */
#endif
};

#define SG_TERM 1  /* term has been set */
#define SG_CTERM 2 /* cterm has been set */
#define SG_GUI 4   /* gui has been set */
#define SG_LINK 8  /* link has been set */

static garray_T highlight_ga; /* highlight groups for 'highlight' option */

#define HL_TABLE() ((struct hl_group *)((highlight_ga.ga_data)))

#define MAX_HL_ID 20000 /* maximum value for a highlight ID. */

#ifdef FEAT_CMDL_COMPL
/* Flags to indicate an additional string for highlight name completion. */
static int include_none = 0;    /* when 1 include "None" */
static int include_default = 0; /* when 1 include "default" */
static int include_link = 0;    /* when 2 include "link" and "clear" */
#endif

/*
 * The "term", "cterm" and "gui" arguments can be any combination of the
 * following names, separated by commas (but no spaces!).
 */
static char *(hl_name_table[]) =
    {"bold", "standout", "underline", "undercurl",
     "italic", "reverse", "inverse", "nocombine", "strikethrough", "NONE"};
static int hl_attr_table[] =
    {HL_BOLD, HL_STANDOUT, HL_UNDERLINE, HL_UNDERCURL, HL_ITALIC, HL_INVERSE, HL_INVERSE, HL_NOCOMBINE, HL_STRIKETHROUGH, 0};
#define ATTR_COMBINE(attr_a, attr_b) ((((attr_b)&HL_NOCOMBINE) ? attr_b : (attr_a)) | (attr_b))

static void syn_unadd_group(void);
static void set_hl_attr(int idx);
static void highlight_list_one(int id);
static int highlight_list_arg(int id, int didh, int type, int iarg, char_u *sarg, char *name);
static int syn_add_group(char_u *name);
static int syn_list_header(int did_header, int outlen, int id);
static int hl_has_settings(int idx, int check_link);
static void highlight_clear(int idx);

/*
 * An attribute number is the index in attr_table plus ATTR_OFF.
 */
#define ATTR_OFF (HL_ALL + 1)

/**************************************
 *  Highlighting stuff		      *
 **************************************/

/*
 * The default highlight groups.  These are compiled-in for fast startup and
 * they still work when the runtime files can't be found.
 * When making changes here, also change runtime/colors/default.vim!
 * The #ifdefs are needed to reduce the amount of static data.  Helps to make
 * the 16 bit DOS (museum) version compile.
 */
#if defined(FEAT_EVAL)
#define CENT(a, b) b
#else
#define CENT(a, b) a
#endif
static char *(highlight_init_both[]) = {
    CENT("ErrorMsg term=standout ctermbg=DarkRed ctermfg=White",
         "ErrorMsg term=standout ctermbg=DarkRed ctermfg=White guibg=Red guifg=White"),
    CENT("IncSearch term=reverse cterm=reverse",
         "IncSearch term=reverse cterm=reverse gui=reverse"),
    CENT("ModeMsg term=bold cterm=bold",
         "ModeMsg term=bold cterm=bold gui=bold"),
    CENT("NonText term=bold ctermfg=Blue",
         "NonText term=bold ctermfg=Blue gui=bold guifg=Blue"),
    CENT("StatusLine term=reverse,bold cterm=reverse,bold",
         "StatusLine term=reverse,bold cterm=reverse,bold gui=reverse,bold"),
    CENT("StatusLineNC term=reverse cterm=reverse",
         "StatusLineNC term=reverse cterm=reverse gui=reverse"),
    "default link EndOfBuffer NonText",
    CENT("VertSplit term=reverse cterm=reverse",
         "VertSplit term=reverse cterm=reverse gui=reverse"),
#ifdef FEAT_DIFF
    CENT("DiffText term=reverse cterm=bold ctermbg=Red",
         "DiffText term=reverse cterm=bold ctermbg=Red gui=bold guibg=Red"),
#endif
    CENT("TabLineSel term=bold cterm=bold",
         "TabLineSel term=bold cterm=bold gui=bold"),
    CENT("TabLineFill term=reverse cterm=reverse",
         "TabLineFill term=reverse cterm=reverse gui=reverse"),
    "default link QuickFixLine Search",
    CENT("Normal cterm=NONE", "Normal gui=NONE"),
    NULL};

/* Default colors only used with a light background. */
static char *(highlight_init_light[]) = {
    CENT("Directory term=bold ctermfg=DarkBlue",
         "Directory term=bold ctermfg=DarkBlue guifg=Blue"),
    CENT("LineNr term=underline ctermfg=Brown",
         "LineNr term=underline ctermfg=Brown guifg=Brown"),
    CENT("CursorLineNr term=bold ctermfg=Brown",
         "CursorLineNr term=bold ctermfg=Brown gui=bold guifg=Brown"),
    CENT("MoreMsg term=bold ctermfg=DarkGreen",
         "MoreMsg term=bold ctermfg=DarkGreen gui=bold guifg=SeaGreen"),
    CENT("Question term=standout ctermfg=DarkGreen",
         "Question term=standout ctermfg=DarkGreen gui=bold guifg=SeaGreen"),
    CENT("Search term=reverse ctermbg=Yellow ctermfg=NONE",
         "Search term=reverse ctermbg=Yellow ctermfg=NONE guibg=Yellow guifg=NONE"),
    CENT("SpecialKey term=bold ctermfg=DarkBlue",
         "SpecialKey term=bold ctermfg=DarkBlue guifg=Blue"),
    CENT("Title term=bold ctermfg=DarkMagenta",
         "Title term=bold ctermfg=DarkMagenta gui=bold guifg=Magenta"),
    CENT("WarningMsg term=standout ctermfg=DarkRed",
         "WarningMsg term=standout ctermfg=DarkRed guifg=Red"),
#ifdef FEAT_WILDMENU
    CENT("WildMenu term=standout ctermbg=Yellow ctermfg=Black",
         "WildMenu term=standout ctermbg=Yellow ctermfg=Black guibg=Yellow guifg=Black"),
#endif
#ifdef FEAT_FOLDING
    CENT("Folded term=standout ctermbg=Grey ctermfg=DarkBlue",
         "Folded term=standout ctermbg=Grey ctermfg=DarkBlue guibg=LightGrey guifg=DarkBlue"),
    CENT("FoldColumn term=standout ctermbg=Grey ctermfg=DarkBlue",
         "FoldColumn term=standout ctermbg=Grey ctermfg=DarkBlue guibg=Grey guifg=DarkBlue"),
#endif
#ifdef FEAT_SIGNS
    CENT("SignColumn term=standout ctermbg=Grey ctermfg=DarkBlue",
         "SignColumn term=standout ctermbg=Grey ctermfg=DarkBlue guibg=Grey guifg=DarkBlue"),
#endif
    CENT("Visual term=reverse",
         "Visual term=reverse guibg=LightGrey"),
#ifdef FEAT_DIFF
    CENT("DiffAdd term=bold ctermbg=LightBlue",
         "DiffAdd term=bold ctermbg=LightBlue guibg=LightBlue"),
    CENT("DiffChange term=bold ctermbg=LightMagenta",
         "DiffChange term=bold ctermbg=LightMagenta guibg=LightMagenta"),
    CENT("DiffDelete term=bold ctermfg=Blue ctermbg=LightCyan",
         "DiffDelete term=bold ctermfg=Blue ctermbg=LightCyan gui=bold guifg=Blue guibg=LightCyan"),
#endif
    CENT("TabLine term=underline cterm=underline ctermfg=black ctermbg=LightGrey",
         "TabLine term=underline cterm=underline ctermfg=black ctermbg=LightGrey gui=underline guibg=LightGrey"),
    CENT("MatchParen term=reverse ctermbg=Cyan",
         "MatchParen term=reverse ctermbg=Cyan guibg=Cyan"),
#ifdef FEAT_TERMINAL
    CENT("StatusLineTerm term=reverse,bold cterm=bold ctermfg=White ctermbg=DarkGreen",
         "StatusLineTerm term=reverse,bold cterm=bold ctermfg=White ctermbg=DarkGreen gui=bold guifg=bg guibg=DarkGreen"),
    CENT("StatusLineTermNC term=reverse ctermfg=White ctermbg=DarkGreen",
         "StatusLineTermNC term=reverse ctermfg=White ctermbg=DarkGreen guifg=bg guibg=DarkGreen"),
#endif
    NULL};

/* Default colors only used with a dark background. */
static char *(highlight_init_dark[]) = {
    CENT("Directory term=bold ctermfg=LightCyan",
         "Directory term=bold ctermfg=LightCyan guifg=Cyan"),
    CENT("LineNr term=underline ctermfg=Yellow",
         "LineNr term=underline ctermfg=Yellow guifg=Yellow"),
    CENT("CursorLineNr term=bold ctermfg=Yellow",
         "CursorLineNr term=bold ctermfg=Yellow gui=bold guifg=Yellow"),
    CENT("MoreMsg term=bold ctermfg=LightGreen",
         "MoreMsg term=bold ctermfg=LightGreen gui=bold guifg=SeaGreen"),
    CENT("Question term=standout ctermfg=LightGreen",
         "Question term=standout ctermfg=LightGreen gui=bold guifg=Green"),
    CENT("Search term=reverse ctermbg=Yellow ctermfg=Black",
         "Search term=reverse ctermbg=Yellow ctermfg=Black guibg=Yellow guifg=Black"),
    CENT("SpecialKey term=bold ctermfg=LightBlue",
         "SpecialKey term=bold ctermfg=LightBlue guifg=Cyan"),
    CENT("Title term=bold ctermfg=LightMagenta",
         "Title term=bold ctermfg=LightMagenta gui=bold guifg=Magenta"),
    CENT("WarningMsg term=standout ctermfg=LightRed",
         "WarningMsg term=standout ctermfg=LightRed guifg=Red"),
#ifdef FEAT_WILDMENU
    CENT("WildMenu term=standout ctermbg=Yellow ctermfg=Black",
         "WildMenu term=standout ctermbg=Yellow ctermfg=Black guibg=Yellow guifg=Black"),
#endif
#ifdef FEAT_FOLDING
    CENT("Folded term=standout ctermbg=DarkGrey ctermfg=Cyan",
         "Folded term=standout ctermbg=DarkGrey ctermfg=Cyan guibg=DarkGrey guifg=Cyan"),
    CENT("FoldColumn term=standout ctermbg=DarkGrey ctermfg=Cyan",
         "FoldColumn term=standout ctermbg=DarkGrey ctermfg=Cyan guibg=Grey guifg=Cyan"),
#endif
#ifdef FEAT_SIGNS
    CENT("SignColumn term=standout ctermbg=DarkGrey ctermfg=Cyan",
         "SignColumn term=standout ctermbg=DarkGrey ctermfg=Cyan guibg=Grey guifg=Cyan"),
#endif
    CENT("Visual term=reverse",
         "Visual term=reverse guibg=DarkGrey"),
#ifdef FEAT_DIFF
    CENT("DiffAdd term=bold ctermbg=DarkBlue",
         "DiffAdd term=bold ctermbg=DarkBlue guibg=DarkBlue"),
    CENT("DiffChange term=bold ctermbg=DarkMagenta",
         "DiffChange term=bold ctermbg=DarkMagenta guibg=DarkMagenta"),
    CENT("DiffDelete term=bold ctermfg=Blue ctermbg=DarkCyan",
         "DiffDelete term=bold ctermfg=Blue ctermbg=DarkCyan gui=bold guifg=Blue guibg=DarkCyan"),
#endif
    CENT("TabLine term=underline cterm=underline ctermfg=white ctermbg=DarkGrey",
         "TabLine term=underline cterm=underline ctermfg=white ctermbg=DarkGrey gui=underline guibg=DarkGrey"),
    CENT("MatchParen term=reverse ctermbg=DarkCyan",
         "MatchParen term=reverse ctermbg=DarkCyan guibg=DarkCyan"),
#ifdef FEAT_TERMINAL
    CENT("StatusLineTerm term=reverse,bold cterm=bold ctermfg=Black ctermbg=LightGreen",
         "StatusLineTerm term=reverse,bold cterm=bold ctermfg=Black ctermbg=LightGreen gui=bold guifg=bg guibg=LightGreen"),
    CENT("StatusLineTermNC term=reverse ctermfg=Black ctermbg=LightGreen",
         "StatusLineTermNC term=reverse ctermfg=Black ctermbg=LightGreen guifg=bg guibg=LightGreen"),
#endif
    NULL};

void init_highlight(
    int both,  /* include groups where 'bg' doesn't matter */
    int reset) /* clear group first */
{
  int i;
  char **pp;
  static int had_both = FALSE;
#ifdef FEAT_EVAL
  char_u *p;

  /*
     * Try finding the color scheme file.  Used when a color file was loaded
     * and 'background' or 't_Co' is changed.
     */
  p = get_var_value((char_u *)"g:colors_name");
  if (p != NULL)
  {
    /* The value of g:colors_name could be freed when sourcing the script,
	* making "p" invalid, so copy it. */
    char_u *copy_p = vim_strsave(p);
    int r;

    if (copy_p != NULL)
    {
      r = load_colors(copy_p);
      vim_free(copy_p);
      if (r == OK)
        return;
    }
  }

#endif

  /*
     * Didn't use a color file, use the compiled-in colors.
     */
  if (both)
  {
    had_both = TRUE;
    pp = highlight_init_both;
    for (i = 0; pp[i] != NULL; ++i)
      do_highlight((char_u *)pp[i], reset, TRUE);
  }
  else if (!had_both)
    /* Don't do anything before the call with both == TRUE from main().
	 * Not everything has been setup then, and that call will overrule
	 * everything anyway. */
    return;

  if (*p_bg == 'l')
    pp = highlight_init_light;
  else
    pp = highlight_init_dark;
  for (i = 0; pp[i] != NULL; ++i)
    do_highlight((char_u *)pp[i], reset, TRUE);

  /* Reverse looks ugly, but grey may not work for 8 colors.  Thus let it
     * depend on the number of colors available.
     * With 8 colors brown is equal to yellow, need to use black for Search fg
     * to avoid Statement highlighted text disappears.
     * Clear the attributes, needed when changing the t_Co value. */
  if (t_colors > 8)
    do_highlight((char_u *)(*p_bg == 'l'
                                ? "Visual cterm=NONE ctermbg=LightGrey"
                                : "Visual cterm=NONE ctermbg=DarkGrey"),
                 FALSE, TRUE);
  else
  {
    do_highlight((char_u *)"Visual cterm=reverse ctermbg=NONE",
                 FALSE, TRUE);
    if (*p_bg == 'l')
      do_highlight((char_u *)"Search ctermfg=black", FALSE, TRUE);
  }
}

/*
 * Load color file "name".
 * Return OK for success, FAIL for failure.
 */
int load_colors(char_u *name)
{
  char_u *buf;
  int retval = FAIL;
  static int recursive = FALSE;

  /* When being called recursively, this is probably because setting
     * 'background' caused the highlighting to be reloaded.  This means it is
     * working, thus we should return OK. */
  if (recursive)
    return OK;

  recursive = TRUE;
  buf = alloc(STRLEN(name) + 12);
  if (buf != NULL)
  {
    apply_autocmds(EVENT_COLORSCHEMEPRE, name,
                   curbuf->b_fname, FALSE, curbuf);
    sprintf((char *)buf, "colors/%s.vim", name);
    retval = source_runtime(buf, DIP_START + DIP_OPT);
    vim_free(buf);
    apply_autocmds(EVENT_COLORSCHEME, name, curbuf->b_fname, FALSE, curbuf);
  }
  recursive = FALSE;

  return retval;
}

static char *(color_names[28]) = {
    "Black", "DarkBlue", "DarkGreen", "DarkCyan",
    "DarkRed", "DarkMagenta", "Brown", "DarkYellow",
    "Gray", "Grey", "LightGray", "LightGrey",
    "DarkGray", "DarkGrey",
    "Blue", "LightBlue", "Green", "LightGreen",
    "Cyan", "LightCyan", "Red", "LightRed", "Magenta",
    "LightMagenta", "Yellow", "LightYellow", "White", "NONE"};
/* indices:
	     * 0, 1, 2, 3,
	     * 4, 5, 6, 7,
	     * 8, 9, 10, 11,
	     * 12, 13,
	     * 14, 15, 16, 17,
	     * 18, 19, 20, 21, 22,
	     * 23, 24, 25, 26, 27 */
static int color_numbers_16[28] = {0, 1, 2, 3,
                                   4, 5, 6, 6,
                                   7, 7, 7, 7,
                                   8, 8,
                                   9, 9, 10, 10,
                                   11, 11, 12, 12, 13,
                                   13, 14, 14, 15, -1};
/* for xterm with 88 colors... */
static int color_numbers_88[28] = {0, 4, 2, 6,
                                   1, 5, 32, 72,
                                   84, 84, 7, 7,
                                   82, 82,
                                   12, 43, 10, 61,
                                   14, 63, 9, 74, 13,
                                   75, 11, 78, 15, -1};
/* for xterm with 256 colors... */
static int color_numbers_256[28] = {0, 4, 2, 6,
                                    1, 5, 130, 130,
                                    248, 248, 7, 7,
                                    242, 242,
                                    12, 81, 10, 121,
                                    14, 159, 9, 224, 13,
                                    225, 11, 229, 15, -1};
/* for terminals with less than 16 colors... */
static int color_numbers_8[28] = {0, 4, 2, 6,
                                  1, 5, 3, 3,
                                  7, 7, 7, 7,
                                  0 + 8, 0 + 8,
                                  4 + 8, 4 + 8, 2 + 8, 2 + 8,
                                  6 + 8, 6 + 8, 1 + 8, 1 + 8, 5 + 8,
                                  5 + 8, 3 + 8, 3 + 8, 7 + 8, -1};

/*
 * Lookup the "cterm" value to be used for color with index "idx" in
 * color_names[].
 * "boldp" will be set to TRUE or FALSE for a foreground color when using 8
 * colors, otherwise it will be unchanged.
 */
int lookup_color(int idx, int foreground, int *boldp)
{
  int color = color_numbers_16[idx];
  char_u *p;

  /* Use the _16 table to check if it's a valid color name. */
  if (color < 0)
    return -1;

  if (t_colors == 8)
  {
    /* t_Co is 8: use the 8 colors table */
#if defined(__QNXNTO__)
    color = color_numbers_8_qansi[idx];
#else
    color = color_numbers_8[idx];
#endif
    if (foreground)
    {
      /* set/reset bold attribute to get light foreground
	     * colors (on some terminals, e.g. "linux") */
      if (color & 8)
        *boldp = TRUE;
      else
        *boldp = FALSE;
    }
    color &= 7; /* truncate to 8 colors */
  }
  else if (t_colors == 16 || t_colors == 88 || t_colors >= 256)
  {
    /*
	 * Guess: if the termcap entry ends in 'm', it is
	 * probably an xterm-like terminal.  Use the changed
	 * order for colors.
	 */
    if (*T_CAF != NUL)
      p = T_CAF;
    else
      p = T_CSF;
    if (*p != NUL && (t_colors > 256 || *(p + STRLEN(p) - 1) == 'm'))
    {
      if (t_colors == 88)
        color = color_numbers_88[idx];
      else if (t_colors >= 256)
        color = color_numbers_256[idx];
      else
        color = color_numbers_8[idx];
    }
  }
  return color;
}

/*
 * Handle the ":highlight .." command.
 * When using ":hi clear" this is called recursively for each group with
 * "forceit" and "init" both TRUE.
 */
void do_highlight(
    char_u *line,
    int forceit,
    int init) /* TRUE when called for initializing */
{
  char_u *name_end;
  char_u *p;
  char_u *linep;
  char_u *key_start;
  char_u *arg_start;
  char_u *key = NULL, *arg = NULL;
  long i;
  int off;
  int len;
  int attr;
  int id;
  int idx;
  struct hl_group item_before;
  int did_change = FALSE;
  int dodefault = FALSE;
  int doclear = FALSE;
  int dolink = FALSE;
  int error = FALSE;
  int color;
  int is_normal_group = FALSE; /* "Normal" group */
#ifdef FEAT_TERMINAL
  int is_terminal_group = FALSE; /* "Terminal" group */
#endif
#define is_menu_group 0
#define is_tooltip_group 0

  /*
     * If no argument, list current highlighting.
     */
  if (ends_excmd(*line))
  {
    for (i = 1; i <= highlight_ga.ga_len && !got_int; ++i)
      /* TODO: only call when the group has attributes set */
      highlight_list_one((int)i);
    return;
  }

  /*
     * Isolate the name.
     */
  name_end = skiptowhite(line);
  linep = skipwhite(name_end);

  /*
     * Check for "default" argument.
     */
  if (STRNCMP(line, "default", name_end - line) == 0)
  {
    dodefault = TRUE;
    line = linep;
    name_end = skiptowhite(line);
    linep = skipwhite(name_end);
  }

  /*
     * Check for "clear" or "link" argument.
     */
  if (STRNCMP(line, "clear", name_end - line) == 0)
    doclear = TRUE;
  if (STRNCMP(line, "link", name_end - line) == 0)
    dolink = TRUE;

  /*
     * ":highlight {group-name}": list highlighting for one group.
     */
  if (!doclear && !dolink && ends_excmd(*linep))
  {
    id = syn_namen2id(line, (int)(name_end - line));
    if (id == 0)
      semsg(_("E411: highlight group not found: %s"), line);
    else
      highlight_list_one(id);
    return;
  }

  /*
     * Handle ":highlight link {from} {to}" command.
     */
  if (dolink)
  {
    char_u *from_start = linep;
    char_u *from_end;
    char_u *to_start;
    char_u *to_end;
    int from_id;
    int to_id;

    from_end = skiptowhite(from_start);
    to_start = skipwhite(from_end);
    to_end = skiptowhite(to_start);

    if (ends_excmd(*from_start) || ends_excmd(*to_start))
    {
      semsg(_("E412: Not enough arguments: \":highlight link %s\""),
            from_start);
      return;
    }

    if (!ends_excmd(*skipwhite(to_end)))
    {
      semsg(_("E413: Too many arguments: \":highlight link %s\""), from_start);
      return;
    }

    from_id = syn_check_group(from_start, (int)(from_end - from_start));
    if (STRNCMP(to_start, "NONE", 4) == 0)
      to_id = 0;
    else
      to_id = syn_check_group(to_start, (int)(to_end - to_start));

    if (from_id > 0 && (!init || HL_TABLE()[from_id - 1].sg_set == 0))
    {
      /*
	     * Don't allow a link when there already is some highlighting
	     * for the group, unless '!' is used
	     */
      if (to_id > 0 && !forceit && !init && hl_has_settings(from_id - 1, dodefault))
      {
        if (sourcing_name == NULL && !dodefault)
          emsg(_("E414: group has settings, highlight link ignored"));
      }
      else if (HL_TABLE()[from_id - 1].sg_link != to_id
#ifdef FEAT_EVAL
               || HL_TABLE()[from_id - 1].sg_script_ctx.sc_sid != current_sctx.sc_sid
#endif
               || HL_TABLE()[from_id - 1].sg_cleared)
      {
        if (!init)
          HL_TABLE()
          [from_id - 1].sg_set |= SG_LINK;
        HL_TABLE()
        [from_id - 1].sg_link = to_id;
#ifdef FEAT_EVAL
        HL_TABLE()
        [from_id - 1].sg_script_ctx = current_sctx;
        HL_TABLE()
        [from_id - 1].sg_script_ctx.sc_lnum += sourcing_lnum;
#endif
        HL_TABLE()
        [from_id - 1].sg_cleared = FALSE;
        redraw_all_later(SOME_VALID);

        /* Only call highlight_changed() once after multiple changes. */
        need_highlight_changed = TRUE;
      }
    }

    return;
  }

  if (doclear)
  {
    /*
	 * ":highlight clear [group]" command.
	 */
    line = linep;
    if (ends_excmd(*line))
    {
#ifdef FEAT_EVAL
      do_unlet((char_u *)"colors_name", TRUE);
#endif
      restore_cterm_colors();

      /*
	     * Clear all default highlight groups and load the defaults.
	     */
      for (idx = 0; idx < highlight_ga.ga_len; ++idx)
        highlight_clear(idx);
      init_highlight(TRUE, TRUE);
      highlight_changed();
      redraw_later_clear();
      return;
    }
    name_end = skiptowhite(line);
    linep = skipwhite(name_end);
  }

  /*
     * Find the group name in the table.  If it does not exist yet, add it.
     */
  id = syn_check_group(line, (int)(name_end - line));
  if (id == 0) /* failed (out of memory) */
    return;
  idx = id - 1; /* index is ID minus one */

  /* Return if "default" was used and the group already has settings. */
  if (dodefault && hl_has_settings(idx, TRUE))
    return;

  /* Make a copy so we can check if any attribute actually changed. */
  item_before = HL_TABLE()[idx];

  if (STRCMP(HL_TABLE()[idx].sg_name_u, "NORMAL") == 0)
    is_normal_group = TRUE;
#ifdef FEAT_TERMINAL
  else if (STRCMP(HL_TABLE()[idx].sg_name_u, "TERMINAL") == 0)
    is_terminal_group = TRUE;
#endif

  /* Clear the highlighting for ":hi clear {group}" and ":hi clear". */
  if (doclear || (forceit && init))
  {
    highlight_clear(idx);
    if (!doclear)
      HL_TABLE()
      [idx].sg_set = 0;
  }

  if (!doclear)
    while (!ends_excmd(*linep))
    {
      key_start = linep;
      if (*linep == '=')
      {
        semsg(_("E415: unexpected equal sign: %s"), key_start);
        error = TRUE;
        break;
      }

      /*
	 * Isolate the key ("term", "ctermfg", "ctermbg", "font", "guifg" or
	 * "guibg").
	 */
      while (*linep && !VIM_ISWHITE(*linep) && *linep != '=')
        ++linep;
      vim_free(key);
      key = vim_strnsave_up(key_start, (int)(linep - key_start));
      if (key == NULL)
      {
        error = TRUE;
        break;
      }
      linep = skipwhite(linep);

      if (STRCMP(key, "NONE") == 0)
      {
        if (!init || HL_TABLE()[idx].sg_set == 0)
        {
          if (!init)
            HL_TABLE()
            [idx].sg_set |= SG_TERM + SG_CTERM + SG_GUI;
          highlight_clear(idx);
        }
        continue;
      }

      /*
	 * Check for the equal sign.
	 */
      if (*linep != '=')
      {
        semsg(_("E416: missing equal sign: %s"), key_start);
        error = TRUE;
        break;
      }
      ++linep;

      /*
	 * Isolate the argument.
	 */
      linep = skipwhite(linep);
      if (*linep == '\'') /* guifg='color name' */
      {
        arg_start = ++linep;
        linep = vim_strchr(linep, '\'');
        if (linep == NULL)
        {
          semsg(_(e_invarg2), key_start);
          error = TRUE;
          break;
        }
      }
      else
      {
        arg_start = linep;
        linep = skiptowhite(linep);
      }
      if (linep == arg_start)
      {
        semsg(_("E417: missing argument: %s"), key_start);
        error = TRUE;
        break;
      }
      vim_free(arg);
      arg = vim_strnsave(arg_start, (int)(linep - arg_start));
      if (arg == NULL)
      {
        error = TRUE;
        break;
      }
      if (*linep == '\'')
        ++linep;

      /*
	 * Store the argument.
	 */
      if (STRCMP(key, "TERM") == 0 || STRCMP(key, "CTERM") == 0 || STRCMP(key, "GUI") == 0)
      {
        attr = 0;
        off = 0;
        while (arg[off] != NUL)
        {
          for (i = sizeof(hl_attr_table) / sizeof(int); --i >= 0;)
          {
            len = (int)STRLEN(hl_name_table[i]);
            if (STRNICMP(arg + off, hl_name_table[i], len) == 0)
            {
              attr |= hl_attr_table[i];
              off += len;
              break;
            }
          }
          if (i < 0)
          {
            semsg(_("E418: Illegal value: %s"), arg);
            error = TRUE;
            break;
          }
          if (arg[off] == ',') /* another one follows */
            ++off;
        }
        if (error)
          break;
        if (*key == 'T')
        {
          if (!init || !(HL_TABLE()[idx].sg_set & SG_TERM))
          {
            if (!init)
              HL_TABLE()
              [idx].sg_set |= SG_TERM;
            HL_TABLE()
            [idx].sg_term = attr;
          }
        }
        else if (*key == 'C')
        {
          if (!init || !(HL_TABLE()[idx].sg_set & SG_CTERM))
          {
            if (!init)
              HL_TABLE()
              [idx].sg_set |= SG_CTERM;
            HL_TABLE()
            [idx].sg_cterm = attr;
            HL_TABLE()
            [idx].sg_cterm_bold = FALSE;
          }
        }
#if defined(FEAT_EVAL)
        else
        {
          if (!init || !(HL_TABLE()[idx].sg_set & SG_GUI))
          {
            if (!init)
              HL_TABLE()
              [idx].sg_set |= SG_GUI;
            HL_TABLE()
            [idx].sg_gui = attr;
          }
        }
#endif
      }
      else if (STRCMP(key, "FONT") == 0)
      {
        /* noop - libvim */
      }
      else if (STRCMP(key, "CTERMFG") == 0 || STRCMP(key, "CTERMBG") == 0)
      {
        if (!init || !(HL_TABLE()[idx].sg_set & SG_CTERM))
        {
          if (!init)
            HL_TABLE()
            [idx].sg_set |= SG_CTERM;

          /* When setting the foreground color, and previously the "bold"
	     * flag was set for a light color, reset it now */
          if (key[5] == 'F' && HL_TABLE()[idx].sg_cterm_bold)
          {
            HL_TABLE()
            [idx].sg_cterm &= ~HL_BOLD;
            HL_TABLE()
            [idx].sg_cterm_bold = FALSE;
          }

          if (VIM_ISDIGIT(*arg))
            color = atoi((char *)arg);
          else if (STRICMP(arg, "fg") == 0)
          {
            if (cterm_normal_fg_color)
              color = cterm_normal_fg_color - 1;
            else
            {
              emsg(_("E419: FG color unknown"));
              error = TRUE;
              break;
            }
          }
          else if (STRICMP(arg, "bg") == 0)
          {
            if (cterm_normal_bg_color > 0)
              color = cterm_normal_bg_color - 1;
            else
            {
              emsg(_("E420: BG color unknown"));
              error = TRUE;
              break;
            }
          }
          else
          {
            int bold = MAYBE;

#if defined(__QNXNTO__)
            static int *color_numbers_8_qansi = color_numbers_8;
            /* On qnx, the 8 & 16 color arrays are the same */
            if (STRNCMP(T_NAME, "qansi", 5) == 0)
              color_numbers_8_qansi = color_numbers_16;
#endif

            /* reduce calls to STRICMP a bit, it can be slow */
            off = TOUPPER_ASC(*arg);
            for (i = (sizeof(color_names) / sizeof(char *)); --i >= 0;)
              if (off == color_names[i][0] && STRICMP(arg + 1, color_names[i] + 1) == 0)
                break;
            if (i < 0)
            {
              semsg(_("E421: Color name or number not recognized: %s"), key_start);
              error = TRUE;
              break;
            }

            color = lookup_color(i, key[5] == 'F', &bold);

            /* set/reset bold attribute to get light foreground
		 * colors (on some terminals, e.g. "linux") */
            if (bold == TRUE)
            {
              HL_TABLE()
              [idx].sg_cterm |= HL_BOLD;
              HL_TABLE()
              [idx].sg_cterm_bold = TRUE;
            }
            else if (bold == FALSE)
              HL_TABLE()
              [idx].sg_cterm &= ~HL_BOLD;
          }

          /* Add one to the argument, to avoid zero.  Zero is used for
	     * "NONE", then "color" is -1. */
          if (key[5] == 'F')
          {
            HL_TABLE()
            [idx].sg_cterm_fg = color + 1;
            if (is_normal_group)
            {
              cterm_normal_fg_color = color + 1;
              cterm_normal_fg_bold = (HL_TABLE()[idx].sg_cterm & HL_BOLD);
              {
                must_redraw = CLEAR;
                if (termcap_active && color >= 0)
                  term_fg_color(color);
              }
            }
          }
          else
          {
            HL_TABLE()
            [idx].sg_cterm_bg = color + 1;
            if (is_normal_group)
            {
              cterm_normal_bg_color = color + 1;

              must_redraw = CLEAR;
              if (color >= 0)
              {
                int dark = -1;

                if (termcap_active)
                  term_bg_color(color);
                if (t_colors < 16)
                  dark = (color == 0 || color == 4);
                /* Limit the heuristic to the standard 16 colors */
                else if (color < 16)
                  dark = (color < 7 || color == 8);
                /* Set the 'background' option if the value is
			     * wrong. */
                if (dark != -1 && dark != (*p_bg == 'd') && !option_was_set((char_u *)"bg"))
                {
                  set_option_value((char_u *)"bg", 0L,
                                   (char_u *)(dark ? "dark" : "light"), 0);
                  reset_option_was_set((char_u *)"bg");
                }
              }
            }
          }
        }
      }
      else if (STRCMP(key, "GUIFG") == 0)
      {
#ifdef FEAT_EVAL
        char_u **namep = &HL_TABLE()[idx].sg_gui_fg_name;

        if (!init || !(HL_TABLE()[idx].sg_set & SG_GUI))
        {
          if (!init)
            HL_TABLE()
            [idx].sg_set |= SG_GUI;

          if (*namep == NULL || STRCMP(*namep, arg) != 0)
          {
            vim_free(*namep);
            if (STRCMP(arg, "NONE") != 0)
              *namep = vim_strsave(arg);
            else
              *namep = NULL;
            did_change = TRUE;
          }
        }
#endif
      }
      else if (STRCMP(key, "GUIBG") == 0)
      {
#ifdef FEAT_EVAL
        char_u **namep = &HL_TABLE()[idx].sg_gui_bg_name;

        if (!init || !(HL_TABLE()[idx].sg_set & SG_GUI))
        {
          if (!init)
            HL_TABLE()
            [idx].sg_set |= SG_GUI;

          if (*namep == NULL || STRCMP(*namep, arg) != 0)
          {
            vim_free(*namep);
            if (STRCMP(arg, "NONE") != 0)
              *namep = vim_strsave(arg);
            else
              *namep = NULL;
            did_change = TRUE;
          }
        }
#endif
      }
      else if (STRCMP(key, "GUISP") == 0)
      {
#ifdef FEAT_EVAL
        char_u **namep = &HL_TABLE()[idx].sg_gui_sp_name;

        if (!init || !(HL_TABLE()[idx].sg_set & SG_GUI))
        {
          if (!init)
            HL_TABLE()
            [idx].sg_set |= SG_GUI;

          if (*namep == NULL || STRCMP(*namep, arg) != 0)
          {
            vim_free(*namep);
            if (STRCMP(arg, "NONE") != 0)
              *namep = vim_strsave(arg);
            else
              *namep = NULL;
            did_change = TRUE;
          }
        }
#endif
      }
      else if (STRCMP(key, "START") == 0 || STRCMP(key, "STOP") == 0)
      {
        char_u buf[100];
        char_u *tname;

        if (!init)
          HL_TABLE()
          [idx].sg_set |= SG_TERM;

        /*
	     * The "start" and "stop"  arguments can be a literal escape
	     * sequence, or a comma separated list of terminal codes.
	     */
        if (STRNCMP(arg, "t_", 2) == 0)
        {
          off = 0;
          buf[0] = 0;
          while (arg[off] != NUL)
          {
            /* Isolate one termcap name */
            for (len = 0; arg[off + len] &&
                          arg[off + len] != ',';
                 ++len)
              ;
            tname = vim_strnsave(arg + off, len);
            if (tname == NULL) /* out of memory */
            {
              error = TRUE;
              break;
            }
            /* lookup the escape sequence for the item */
            p = get_term_code(tname);
            vim_free(tname);
            if (p == NULL) /* ignore non-existing things */
              p = (char_u *)"";

            /* Append it to the already found stuff */
            if ((int)(STRLEN(buf) + STRLEN(p)) >= 99)
            {
              semsg(_("E422: terminal code too long: %s"), arg);
              error = TRUE;
              break;
            }
            STRCAT(buf, p);

            /* Advance to the next item */
            off += len;
            if (arg[off] == ',') /* another one follows */
              ++off;
          }
        }
        else
        {
          /*
		 * Copy characters from arg[] to buf[], translating <> codes.
		 */
          for (p = arg, off = 0; off < 100 - 6 && *p;)
          {
            len = trans_special(&p, buf + off, FALSE, FALSE);
            if (len > 0) /* recognized special char */
              off += len;
            else /* copy as normal char */
              buf[off++] = *p++;
          }
          buf[off] = NUL;
        }
        if (error)
          break;

        if (STRCMP(buf, "NONE") == 0) /* resetting the value */
          p = NULL;
        else
          p = vim_strsave(buf);
        if (key[2] == 'A')
        {
          vim_free(HL_TABLE()[idx].sg_start);
          HL_TABLE()
          [idx].sg_start = p;
        }
        else
        {
          vim_free(HL_TABLE()[idx].sg_stop);
          HL_TABLE()
          [idx].sg_stop = p;
        }
      }
      else
      {
        semsg(_("E423: Illegal argument: %s"), key_start);
        error = TRUE;
        break;
      }
      HL_TABLE()
      [idx].sg_cleared = FALSE;

      /*
	 * When highlighting has been given for a group, don't link it.
	 */
      if (!init || !(HL_TABLE()[idx].sg_set & SG_LINK))
        HL_TABLE()
        [idx].sg_link = 0;

      /*
	 * Continue with next argument.
	 */
      linep = skipwhite(linep);
    }

  /*
     * If there is an error, and it's a new entry, remove it from the table.
     */
  if (error && idx == highlight_ga.ga_len)
    syn_unadd_group();
  else
  {
    if (is_normal_group)
    {
      HL_TABLE()
      [idx].sg_term_attr = 0;
      HL_TABLE()
      [idx].sg_cterm_attr = 0;
    }
#ifdef FEAT_TERMINAL
    else if (is_terminal_group)
      set_terminal_default_colors(
          HL_TABLE()[idx].sg_cterm_fg, HL_TABLE()[idx].sg_cterm_bg);
#endif
    else
      set_hl_attr(idx);
#ifdef FEAT_EVAL
    HL_TABLE()
    [idx].sg_script_ctx = current_sctx;
    HL_TABLE()
    [idx].sg_script_ctx.sc_lnum += sourcing_lnum;
#endif
  }

  vim_free(key);
  vim_free(arg);

  /* Only call highlight_changed() once, after a sequence of highlight
     * commands, and only if an attribute actually changed. */
  if ((did_change || memcmp(&HL_TABLE()[idx], &item_before, sizeof(item_before)) != 0))
  {
    /* Do not trigger a redraw when highlighting is changed while
	 * redrawing.  This may happen when evaluating 'statusline' changes the
	 * StatusLine group. */
    if (!updating_screen)
      redraw_all_later(NOT_VALID);
    need_highlight_changed = TRUE;
  }
}

#if defined(EXITFREE) || defined(PROTO)
void free_highlight(void)
{
  int i;

  for (i = 0; i < highlight_ga.ga_len; ++i)
  {
    highlight_clear(i);
    vim_free(HL_TABLE()[i].sg_name);
    vim_free(HL_TABLE()[i].sg_name_u);
  }
  ga_clear(&highlight_ga);
}
#endif

/*
 * Reset the cterm colors to what they were before Vim was started, if
 * possible.  Otherwise reset them to zero.
 */
void restore_cterm_colors(void)
{
#if defined(MSWIN)
  /* Since t_me has been set, this probably means that the user
     * wants to use this as default colors.  Need to reset default
     * background/foreground colors. */
  mch_set_normal_colors();
#else
#ifdef VIMDLL
  if (!gui.in_use)
  {
    mch_set_normal_colors();
    return;
  }
#endif
  cterm_normal_fg_color = 0;
  cterm_normal_fg_bold = 0;
  cterm_normal_bg_color = 0;
#endif
}

/*
 * Return TRUE if highlight group "idx" has any settings.
 * When "check_link" is TRUE also check for an existing link.
 */
static int
hl_has_settings(int idx, int check_link)
{
  return (HL_TABLE()[idx].sg_term_attr != 0 || HL_TABLE()[idx].sg_cterm_attr != 0 || HL_TABLE()[idx].sg_cterm_fg != 0 || HL_TABLE()[idx].sg_cterm_bg != 0 || (check_link && (HL_TABLE()[idx].sg_set & SG_LINK)));
}

/*
 * Clear highlighting for one group.
 */
static void
highlight_clear(int idx)
{
  HL_TABLE()
  [idx].sg_cleared = TRUE;

  HL_TABLE()
  [idx].sg_term = 0;
  VIM_CLEAR(HL_TABLE()[idx].sg_start);
  VIM_CLEAR(HL_TABLE()[idx].sg_stop);
  HL_TABLE()
  [idx].sg_term_attr = 0;
  HL_TABLE()
  [idx].sg_cterm = 0;
  HL_TABLE()
  [idx].sg_cterm_bold = FALSE;
  HL_TABLE()
  [idx].sg_cterm_fg = 0;
  HL_TABLE()
  [idx].sg_cterm_bg = 0;
  HL_TABLE()
  [idx].sg_cterm_attr = 0;
#if defined(FEAT_EVAL)
  HL_TABLE()
  [idx].sg_gui = 0;
  VIM_CLEAR(HL_TABLE()[idx].sg_gui_fg_name);
  VIM_CLEAR(HL_TABLE()[idx].sg_gui_bg_name);
  VIM_CLEAR(HL_TABLE()[idx].sg_gui_sp_name);
#endif
#ifdef FEAT_EVAL
  /* Clear the script ID only when there is no link, since that is not
     * cleared. */
  if (HL_TABLE()[idx].sg_link == 0)
  {
    HL_TABLE()
    [idx].sg_script_ctx.sc_sid = 0;
    HL_TABLE()
    [idx].sg_script_ctx.sc_lnum = 0;
  }
#endif
}

#if defined(PROTO)
/*
 * Set the normal foreground and background colors according to the "Normal"
 * highlighting group.  For X11 also set "Menu", "Scrollbar", and
 * "Tooltip" colors.
 */
void set_normal_colors(void)
{
  /* noop - libvim */
}
#endif

#if defined(PROTO)
/*
 * Set the colors for "Normal", "Menu", "Tooltip" or "Scrollbar".
 */
static int
set_group_colors(
    char_u *name,
    guicolor_T *fgp,
    guicolor_T *bgp,
    int do_menu,
    int use_norm,
    int do_tooltip)
{
  int idx;

  idx = syn_name2id(name) - 1;
  if (idx >= 0)
  {
    gui_do_one_color(idx, do_menu, do_tooltip);

    if (HL_TABLE()[idx].sg_gui_fg != INVALCOLOR)
      *fgp = HL_TABLE()[idx].sg_gui_fg;
    else if (use_norm)
      *fgp = gui.def_norm_pixel;
    if (HL_TABLE()[idx].sg_gui_bg != INVALCOLOR)
      *bgp = HL_TABLE()[idx].sg_gui_bg;
    else if (use_norm)
      *bgp = gui.def_back_pixel;
    return TRUE;
  }
  return FALSE;
}

/*
 * Get the font of the "Normal" group.
 * Returns "" when it's not found or not set.
 */
char_u *
hl_get_font_name(void)
{
  int id;
  char_u *s;

  id = syn_name2id((char_u *)"Normal");
  if (id > 0)
  {
    s = HL_TABLE()[id - 1].sg_font_name;
    if (s != NULL)
      return s;
  }
  return (char_u *)"";
}

/*
 * Set font for "Normal" group.  Called by gui_mch_init_font() when a font has
 * actually chosen to be used.
 */
void hl_set_font_name(char_u *font_name)
{
  int id;

  id = syn_name2id((char_u *)"Normal");
  if (id > 0)
  {
    vim_free(HL_TABLE()[id - 1].sg_font_name);
    HL_TABLE()
    [id - 1].sg_font_name = vim_strsave(font_name);
  }
}

/*
 * Set background color for "Normal" group.  Called by gui_set_bg_color()
 * when the color is known.
 */
void hl_set_bg_color_name(
    char_u *name) /* must have been allocated */
{
  int id;

  if (name != NULL)
  {
    id = syn_name2id((char_u *)"Normal");
    if (id > 0)
    {
      vim_free(HL_TABLE()[id - 1].sg_gui_bg_name);
      HL_TABLE()
      [id - 1].sg_gui_bg_name = name;
    }
  }
}

/*
 * Set foreground color for "Normal" group.  Called by gui_set_fg_color()
 * when the color is known.
 */
void hl_set_fg_color_name(
    char_u *name) /* must have been allocated */
{
  int id;

  if (name != NULL)
  {
    id = syn_name2id((char_u *)"Normal");
    if (id > 0)
    {
      vim_free(HL_TABLE()[id - 1].sg_gui_fg_name);
      HL_TABLE()
      [id - 1].sg_gui_fg_name = name;
    }
  }
}

/*
 * Return the handle for a font name.
 * Returns NOFONT when failed.
 */
static GuiFont
font_name2handle(char_u *name)
{
  if (STRCMP(name, "NONE") == 0)
    return NOFONT;

  return gui_mch_get_font(name, TRUE);
}

#ifdef FEAT_XFONTSET
/*
 * Return the handle for a fontset name.
 * Returns NOFONTSET when failed.
 */
static GuiFontset
fontset_name2handle(char_u *name, int fixed_width)
{
  if (STRCMP(name, "NONE") == 0)
    return NOFONTSET;

  return gui_mch_get_fontset(name, TRUE, fixed_width);
}
#endif

/*
 * Get the font or fontset for one highlight group.
 */
static void
hl_do_font(
    int idx,
    char_u *arg,
    int do_normal,         /* set normal font */
    int do_menu UNUSED,    /* set menu font */
    int do_tooltip UNUSED, /* set tooltip font */
    int free_font)         /* free current font/fontset */
{
#ifdef FEAT_XFONTSET
  /* If 'guifontset' is not empty, first try using the name as a
     * fontset.  If that doesn't work, use it as a font name. */
  if (*p_guifontset != NUL
#ifdef FONTSET_ALWAYS
      || do_menu
#endif
#ifdef FEAT_BEVAL_TIP
      /* In Athena & Motif, the Tooltip highlight group is always a fontset */
      || do_tooltip
#endif
  )
  {
    if (free_font)
      gui_mch_free_fontset(HL_TABLE()[idx].sg_fontset);
    HL_TABLE()
    [idx].sg_fontset = fontset_name2handle(arg, 0
#ifdef FONTSET_ALWAYS
                                                    || do_menu
#endif
#ifdef FEAT_BEVAL_TIP
                                                    || do_tooltip
#endif
    );
  }
  if (HL_TABLE()[idx].sg_fontset != NOFONTSET)
  {
    /* If it worked and it's the Normal group, use it as the normal
	 * fontset.  Same for the Menu group. */
    if (do_normal)
      gui_init_font(arg, TRUE);
  }
  else
#endif
  {
    if (free_font)
      gui_mch_free_font(HL_TABLE()[idx].sg_font);
    HL_TABLE()
    [idx].sg_font = font_name2handle(arg);
    /* If it worked and it's the Normal group, use it as the
	 * normal font.  Same for the Menu group. */
    if (HL_TABLE()[idx].sg_font != NOFONT)
    {
      if (do_normal)
        gui_init_font(arg, FALSE);
    }
  }
}

#endif

#if defined(PROTO)
/*
 * Return the handle for a color name.
 * Returns INVALCOLOR when failed.
 */
guicolor_T
color_name2handle(char_u *name)
{
  if (STRCMP(name, "NONE") == 0)
    return INVALCOLOR;

  if (STRICMP(name, "fg") == 0 || STRICMP(name, "foreground") == 0)
  {
    /* noop - libvim */
  }
  if (STRICMP(name, "bg") == 0 || STRICMP(name, "background") == 0)
  {
    /* noop - libvim */
  }

  return GUI_GET_COLOR(name);
}
#endif

/*
 * Table with the specifications for an attribute number.
 * Note that this table is used by ALL buffers.  This is required because the
 * GUI can redraw at any time for any buffer.
 */
static garray_T term_attr_table = {0, 0, 0, 0, NULL};

#define TERM_ATTR_ENTRY(idx) ((attrentry_T *)term_attr_table.ga_data)[idx]

static garray_T cterm_attr_table = {0, 0, 0, 0, NULL};

#define CTERM_ATTR_ENTRY(idx) ((attrentry_T *)cterm_attr_table.ga_data)[idx]

/*
 * Return the attr number for a set of colors and font.
 * Add a new entry to the term_attr_table, cterm_attr_table or gui_attr_table
 * if the combination is new.
 * Return 0 for error (no more room).
 */
static int
get_attr_entry(garray_T *table, attrentry_T *aep)
{
  int i;
  attrentry_T *taep;
  static int recursive = FALSE;

  /*
     * Init the table, in case it wasn't done yet.
     */
  table->ga_itemsize = sizeof(attrentry_T);
  table->ga_growsize = 7;

  /*
     * Try to find an entry with the same specifications.
     */
  for (i = 0; i < table->ga_len; ++i)
  {
    taep = &(((attrentry_T *)table->ga_data)[i]);
    if (aep->ae_attr == taep->ae_attr && ((table == &term_attr_table && (aep->ae_u.term.start == NULL) == (taep->ae_u.term.start == NULL) && (aep->ae_u.term.start == NULL || STRCMP(aep->ae_u.term.start, taep->ae_u.term.start) == 0) && (aep->ae_u.term.stop == NULL) == (taep->ae_u.term.stop == NULL) && (aep->ae_u.term.stop == NULL || STRCMP(aep->ae_u.term.stop, taep->ae_u.term.stop) == 0)) || (table == &cterm_attr_table && aep->ae_u.cterm.fg_color == taep->ae_u.cterm.fg_color && aep->ae_u.cterm.bg_color == taep->ae_u.cterm.bg_color)))

      return i + ATTR_OFF;
  }

  if (table->ga_len + ATTR_OFF > MAX_TYPENR)
  {
    /*
	 * Running out of attribute entries!  remove all attributes, and
	 * compute new ones for all groups.
	 * When called recursively, we are really out of numbers.
	 */
    if (recursive)
    {
      emsg(_("E424: Too many different highlighting attributes in use"));
      return 0;
    }
    recursive = TRUE;

    clear_hl_tables();

    must_redraw = CLEAR;

    for (i = 0; i < highlight_ga.ga_len; ++i)
      set_hl_attr(i);

    recursive = FALSE;
  }

  /*
     * This is a new combination of colors and font, add an entry.
     */
  if (ga_grow(table, 1) == FAIL)
    return 0;

  taep = &(((attrentry_T *)table->ga_data)[table->ga_len]);
  vim_memset(taep, 0, sizeof(attrentry_T));
  taep->ae_attr = aep->ae_attr;
  if (table == &term_attr_table)
  {
    if (aep->ae_u.term.start == NULL)
      taep->ae_u.term.start = NULL;
    else
      taep->ae_u.term.start = vim_strsave(aep->ae_u.term.start);
    if (aep->ae_u.term.stop == NULL)
      taep->ae_u.term.stop = NULL;
    else
      taep->ae_u.term.stop = vim_strsave(aep->ae_u.term.stop);
  }
  else if (table == &cterm_attr_table)
  {
    taep->ae_u.cterm.fg_color = aep->ae_u.cterm.fg_color;
    taep->ae_u.cterm.bg_color = aep->ae_u.cterm.bg_color;
  }
  ++table->ga_len;
  return (table->ga_len - 1 + ATTR_OFF);
}

#if defined(FEAT_TERMINAL) || defined(PROTO)
/*
 * Get an attribute index for a cterm entry.
 * Uses an existing entry when possible or adds one when needed.
 */
int get_cterm_attr_idx(int attr, int fg, int bg)
{
  attrentry_T at_en;

  vim_memset(&at_en, 0, sizeof(attrentry_T));
  at_en.ae_attr = attr;
  at_en.ae_u.cterm.fg_color = fg;
  at_en.ae_u.cterm.bg_color = bg;
  return get_attr_entry(&cterm_attr_table, &at_en);
}
#endif

#if defined(PROTO)
/*
 * Get an attribute index for a cterm entry.
 * Uses an existing entry when possible or adds one when needed.
 */
int get_gui_attr_idx(int attr, guicolor_T fg, guicolor_T bg)
{
  attrentry_T at_en;

  vim_memset(&at_en, 0, sizeof(attrentry_T));
  at_en.ae_attr = attr;
  at_en.ae_u.gui.fg_color = fg;
  at_en.ae_u.gui.bg_color = bg;
  return get_attr_entry(&gui_attr_table, &at_en);
}
#endif

/*
 * Clear all highlight tables.
 */
void clear_hl_tables(void)
{
  int i;
  attrentry_T *taep;

  for (i = 0; i < term_attr_table.ga_len; ++i)
  {
    taep = &(((attrentry_T *)term_attr_table.ga_data)[i]);
    vim_free(taep->ae_u.term.start);
    vim_free(taep->ae_u.term.stop);
  }
  ga_clear(&term_attr_table);
  ga_clear(&cterm_attr_table);
}

/*
 * Combine special attributes (e.g., for spelling) with other attributes
 * (e.g., for syntax highlighting).
 * "prim_attr" overrules "char_attr".
 * This creates a new group when required.
 * Since we expect there to be few spelling mistakes we don't cache the
 * result.
 * Return the resulting attributes.
 */
int hl_combine_attr(int char_attr, int prim_attr)
{
  attrentry_T *char_aep = NULL;
  attrentry_T *spell_aep;
  attrentry_T new_en;

  if (char_attr == 0)
    return prim_attr;
  if (char_attr <= HL_ALL && prim_attr <= HL_ALL)
    return ATTR_COMBINE(char_attr, prim_attr);

  if (IS_CTERM)
  {
    if (char_attr > HL_ALL)
      char_aep = syn_cterm_attr2entry(char_attr);
    if (char_aep != NULL)
      new_en = *char_aep;
    else
    {
      vim_memset(&new_en, 0, sizeof(new_en));
      if (char_attr <= HL_ALL)
        new_en.ae_attr = char_attr;
    }

    if (prim_attr <= HL_ALL)
      new_en.ae_attr = ATTR_COMBINE(new_en.ae_attr, prim_attr);
    else
    {
      spell_aep = syn_cterm_attr2entry(prim_attr);
      if (spell_aep != NULL)
      {
        new_en.ae_attr = ATTR_COMBINE(new_en.ae_attr,
                                      spell_aep->ae_attr);
        if (spell_aep->ae_u.cterm.fg_color > 0)
          new_en.ae_u.cterm.fg_color = spell_aep->ae_u.cterm.fg_color;
        if (spell_aep->ae_u.cterm.bg_color > 0)
          new_en.ae_u.cterm.bg_color = spell_aep->ae_u.cterm.bg_color;
      }
    }
    return get_attr_entry(&cterm_attr_table, &new_en);
  }

  if (char_attr > HL_ALL)
    char_aep = syn_term_attr2entry(char_attr);
  if (char_aep != NULL)
    new_en = *char_aep;
  else
  {
    vim_memset(&new_en, 0, sizeof(new_en));
    if (char_attr <= HL_ALL)
      new_en.ae_attr = char_attr;
  }

  if (prim_attr <= HL_ALL)
    new_en.ae_attr = ATTR_COMBINE(new_en.ae_attr, prim_attr);
  else
  {
    spell_aep = syn_term_attr2entry(prim_attr);
    if (spell_aep != NULL)
    {
      new_en.ae_attr = ATTR_COMBINE(new_en.ae_attr, spell_aep->ae_attr);
      if (spell_aep->ae_u.term.start != NULL)
      {
        new_en.ae_u.term.start = spell_aep->ae_u.term.start;
        new_en.ae_u.term.stop = spell_aep->ae_u.term.stop;
      }
    }
  }
  return get_attr_entry(&term_attr_table, &new_en);
}

/*
 * Get the highlight attributes (HL_BOLD etc.) from an attribute nr.
 * Only to be used when "attr" > HL_ALL.
 */
int syn_attr2attr(int attr)
{
  attrentry_T *aep;
  if (IS_CTERM)
    aep = syn_cterm_attr2entry(attr);
  else
    aep = syn_term_attr2entry(attr);

  if (aep == NULL) /* highlighting not set */
    return 0;
  return aep->ae_attr;
}

attrentry_T *
syn_term_attr2entry(int attr)
{
  attr -= ATTR_OFF;
  if (attr >= term_attr_table.ga_len) /* did ":syntax clear" */
    return NULL;
  return &(TERM_ATTR_ENTRY(attr));
}

attrentry_T *
syn_cterm_attr2entry(int attr)
{
  attr -= ATTR_OFF;
  if (attr >= cterm_attr_table.ga_len) /* did ":syntax clear" */
    return NULL;
  return &(CTERM_ATTR_ENTRY(attr));
}

#define LIST_ATTR 1
#define LIST_STRING 2
#define LIST_INT 3

static void
highlight_list_one(int id)
{
  struct hl_group *sgp;
  int didh = FALSE;

  sgp = &HL_TABLE()[id - 1]; // index is ID minus one

  if (message_filtered(sgp->sg_name))
    return;

  didh = highlight_list_arg(id, didh, LIST_ATTR,
                            sgp->sg_term, NULL, "term");
  didh = highlight_list_arg(id, didh, LIST_STRING,
                            0, sgp->sg_start, "start");
  didh = highlight_list_arg(id, didh, LIST_STRING,
                            0, sgp->sg_stop, "stop");

  didh = highlight_list_arg(id, didh, LIST_ATTR,
                            sgp->sg_cterm, NULL, "cterm");
  didh = highlight_list_arg(id, didh, LIST_INT,
                            sgp->sg_cterm_fg, NULL, "ctermfg");
  didh = highlight_list_arg(id, didh, LIST_INT,
                            sgp->sg_cterm_bg, NULL, "ctermbg");

#if defined(FEAT_EVAL)
  didh = highlight_list_arg(id, didh, LIST_ATTR,
                            sgp->sg_gui, NULL, "gui");
  didh = highlight_list_arg(id, didh, LIST_STRING,
                            0, sgp->sg_gui_fg_name, "guifg");
  didh = highlight_list_arg(id, didh, LIST_STRING,
                            0, sgp->sg_gui_bg_name, "guibg");
  didh = highlight_list_arg(id, didh, LIST_STRING,
                            0, sgp->sg_gui_sp_name, "guisp");
#endif

  if (sgp->sg_link && !got_int)
  {
    (void)syn_list_header(didh, 9999, id);
    didh = TRUE;
    msg_puts_attr("links to", HL_ATTR(HLF_D));
    msg_putchar(' ');
    msg_outtrans(HL_TABLE()[HL_TABLE()[id - 1].sg_link - 1].sg_name);
  }

  if (!didh)
    highlight_list_arg(id, didh, LIST_STRING, 0, (char_u *)"cleared", "");
#ifdef FEAT_EVAL
  if (p_verbose > 0)
    last_set_msg(sgp->sg_script_ctx);
#endif
}

static int
highlight_list_arg(
    int id,
    int didh,
    int type,
    int iarg,
    char_u *sarg,
    char *name)
{
  char_u buf[100];
  char_u *ts;
  int i;

  if (got_int)
    return FALSE;
  if (type == LIST_STRING ? (sarg != NULL) : (iarg != 0))
  {
    ts = buf;
    if (type == LIST_INT)
      sprintf((char *)buf, "%d", iarg - 1);
    else if (type == LIST_STRING)
      ts = sarg;
    else /* type == LIST_ATTR */
    {
      buf[0] = NUL;
      for (i = 0; hl_attr_table[i] != 0; ++i)
      {
        if (iarg & hl_attr_table[i])
        {
          if (buf[0] != NUL)
            vim_strcat(buf, (char_u *)",", 100);
          vim_strcat(buf, (char_u *)hl_name_table[i], 100);
          iarg &= ~hl_attr_table[i]; /* don't want "inverse" */
        }
      }
    }

    (void)syn_list_header(didh,
                          (int)(vim_strsize(ts) + STRLEN(name) + 1), id);
    didh = TRUE;
    if (!got_int)
    {
      if (*name != NUL)
      {
        msg_puts_attr(name, HL_ATTR(HLF_D));
        msg_puts_attr("=", HL_ATTR(HLF_D));
      }
      msg_outtrans(ts);
    }
  }
  return didh;
}

/*
 * Output the syntax list header.
 * Return TRUE when started a new line.
 */
static int
syn_list_header(
    int did_header, /* did header already */
    int outlen,     /* length of string that comes */
    int id)         /* highlight group id */
{
  int endcol = 19;
  int newline = TRUE;

  if (!did_header)
  {
    msg_putchar('\n');
    if (got_int)
      return TRUE;
    msg_outtrans(HL_TABLE()[id - 1].sg_name);
    endcol = 15;
  }
  else if (msg_col + outlen + 1 >= Columns)
  {
    msg_putchar('\n');
    if (got_int)
      return TRUE;
  }
  else
  {
    if (msg_col >= endcol) /* wrap around is like starting a new line */
      newline = FALSE;
  }

  if (msg_col >= endcol) /* output at least one space */
    endcol = msg_col + 1;
  if (Columns <= endcol) /* avoid hang for tiny window */
    endcol = Columns - 1;

  msg_advance(endcol);

  /* Show "xxx" with the attributes. */
  if (!did_header)
  {
    msg_puts_attr("xxx", syn_id2attr(id));
    msg_putchar(' ');
  }

  return newline;
}

/*
 * Set the attribute numbers for a highlight group.
 * Called after one of the attributes has changed.
 */
static void
set_hl_attr(
    int idx) /* index in array */
{
  attrentry_T at_en;
  struct hl_group *sgp = HL_TABLE() + idx;

  /* The "Normal" group doesn't need an attribute number */
  if (sgp->sg_name_u != NULL && STRCMP(sgp->sg_name_u, "NORMAL") == 0)
    return;

  /*
     * For the term mode: If there are other than "normal" highlighting
     * attributes, need to allocate an attr number.
     */
  if (sgp->sg_start == NULL && sgp->sg_stop == NULL)
    sgp->sg_term_attr = sgp->sg_term;
  else
  {
    at_en.ae_attr = sgp->sg_term;
    at_en.ae_u.term.start = sgp->sg_start;
    at_en.ae_u.term.stop = sgp->sg_stop;
    sgp->sg_term_attr = get_attr_entry(&term_attr_table, &at_en);
  }

  /*
     * For the color term mode: If there are other than "normal"
     * highlighting attributes, need to allocate an attr number.
     */
  if (sgp->sg_cterm_fg == 0 && sgp->sg_cterm_bg == 0)
    sgp->sg_cterm_attr = sgp->sg_cterm;
  else
  {
    at_en.ae_attr = sgp->sg_cterm;
    at_en.ae_u.cterm.fg_color = sgp->sg_cterm_fg;
    at_en.ae_u.cterm.bg_color = sgp->sg_cterm_bg;
    sgp->sg_cterm_attr = get_attr_entry(&cterm_attr_table, &at_en);
  }
}

/*
 * Lookup a highlight group name and return its ID.
 * If it is not found, 0 is returned.
 */
int syn_name2id(char_u *name)
{
  int i;
  char_u name_u[200];

  /* Avoid using stricmp() too much, it's slow on some systems */
  /* Avoid alloc()/free(), these are slow too.  ID names over 200 chars
     * don't deserve to be found! */
  vim_strncpy(name_u, name, 199);
  vim_strup(name_u);
  for (i = highlight_ga.ga_len; --i >= 0;)
    if (HL_TABLE()[i].sg_name_u != NULL && STRCMP(name_u, HL_TABLE()[i].sg_name_u) == 0)
      break;
  return i + 1;
}

/*
 * Lookup a highlight group name and return its attributes.
 * Return zero if not found.
 */
int syn_name2attr(char_u *name)
{
  int id = syn_name2id(name);

  if (id != 0)
    return syn_id2attr(id);
  return 0;
}

#if defined(FEAT_EVAL) || defined(PROTO)
/*
 * Return TRUE if highlight group "name" exists.
 */
int highlight_exists(char_u *name)
{
  return (syn_name2id(name) > 0);
}

#if defined(FEAT_SEARCH_EXTRA) || defined(PROTO)
/*
 * Return the name of highlight group "id".
 * When not a valid ID return an empty string.
 */
char_u *
syn_id2name(int id)
{
  if (id <= 0 || id > highlight_ga.ga_len)
    return (char_u *)"";
  return HL_TABLE()[id - 1].sg_name;
}
#endif
#endif

/*
 * Like syn_name2id(), but take a pointer + length argument.
 */
int syn_namen2id(char_u *linep, int len)
{
  char_u *name;
  int id = 0;

  name = vim_strnsave(linep, len);
  if (name != NULL)
  {
    id = syn_name2id(name);
    vim_free(name);
  }
  return id;
}

/*
 * Find highlight group name in the table and return its ID.
 * The argument is a pointer to the name and the length of the name.
 * If it doesn't exist yet, a new entry is created.
 * Return 0 for failure.
 */
int syn_check_group(char_u *pp, int len)
{
  int id;
  char_u *name;

  name = vim_strnsave(pp, len);
  if (name == NULL)
    return 0;

  id = syn_name2id(name);
  if (id == 0) /* doesn't exist yet */
    id = syn_add_group(name);
  else
    vim_free(name);
  return id;
}

/*
 * Add new highlight group and return its ID.
 * "name" must be an allocated string, it will be consumed.
 * Return 0 for failure.
 */
static int
syn_add_group(char_u *name)
{
  char_u *p;

  /* Check that the name is ASCII letters, digits and underscore. */
  for (p = name; *p != NUL; ++p)
  {
    if (!vim_isprintc(*p))
    {
      emsg(_("E669: Unprintable character in group name"));
      vim_free(name);
      return 0;
    }
    else if (!ASCII_ISALNUM(*p) && *p != '_')
    {
      /* This is an error, but since there previously was no check only
	     * give a warning. */
      msg_source(HL_ATTR(HLF_W));
      msg(_("W18: Invalid character in group name"));
      break;
    }
  }

  /*
     * First call for this growarray: init growing array.
     */
  if (highlight_ga.ga_data == NULL)
  {
    highlight_ga.ga_itemsize = sizeof(struct hl_group);
    highlight_ga.ga_growsize = 10;
  }

  if (highlight_ga.ga_len >= MAX_HL_ID)
  {
    emsg(_("E849: Too many highlight and syntax groups"));
    vim_free(name);
    return 0;
  }

  /*
     * Make room for at least one other syntax_highlight entry.
     */
  if (ga_grow(&highlight_ga, 1) == FAIL)
  {
    vim_free(name);
    return 0;
  }

  vim_memset(&(HL_TABLE()[highlight_ga.ga_len]), 0, sizeof(struct hl_group));
  HL_TABLE()
  [highlight_ga.ga_len].sg_name = name;
  HL_TABLE()
  [highlight_ga.ga_len].sg_name_u = vim_strsave_up(name);
  ++highlight_ga.ga_len;

  return highlight_ga.ga_len; /* ID is index plus one */
}

/*
 * When, just after calling syn_add_group(), an error is discovered, this
 * function deletes the new name.
 */
static void
syn_unadd_group(void)
{
  --highlight_ga.ga_len;
  vim_free(HL_TABLE()[highlight_ga.ga_len].sg_name);
  vim_free(HL_TABLE()[highlight_ga.ga_len].sg_name_u);
}

/*
 * Translate a group ID to highlight attributes.
 */
int syn_id2attr(int hl_id)
{
  int attr;
  struct hl_group *sgp;

  hl_id = syn_get_final_id(hl_id);
  sgp = &HL_TABLE()[hl_id - 1]; /* index is ID minus one */

  if (IS_CTERM)
    attr = sgp->sg_cterm_attr;
  else
    attr = sgp->sg_term_attr;

  return attr;
}

#if defined(PROTO)
/*
 * Get the GUI colors and attributes for a group ID.
 * NOTE: the colors will be INVALCOLOR when not set, the color otherwise.
 */
int syn_id2colors(int hl_id, guicolor_T *fgp, guicolor_T *bgp)
{
  struct hl_group *sgp;

  hl_id = syn_get_final_id(hl_id);
  sgp = &HL_TABLE()[hl_id - 1]; /* index is ID minus one */

  *fgp = sgp->sg_gui_fg;
  *bgp = sgp->sg_gui_bg;
  return sgp->sg_gui;
}
#endif

/*
 * Translate a group ID to the final group ID (following links).
 */
int syn_get_final_id(int hl_id)
{
  int count;
  struct hl_group *sgp;

  if (hl_id > highlight_ga.ga_len || hl_id < 1)
    return 0; /* Can be called from eval!! */

  /*
     * Follow links until there is no more.
     * Look out for loops!  Break after 100 links.
     */
  for (count = 100; --count >= 0;)
  {
    sgp = &HL_TABLE()[hl_id - 1]; /* index is ID minus one */
    if (sgp->sg_link == 0 || sgp->sg_link > highlight_ga.ga_len)
      break;
    hl_id = sgp->sg_link;
  }

  return hl_id;
}

#if defined(PROTO)
/*
 * Call this function just after the GUI has started.
 * Also called when 'termguicolors' was set, gui.in_use will be FALSE then.
 * It finds the font and color handles for the highlighting groups.
 */
void highlight_gui_started(void)
{
  int idx;

  /* First get the colors from the "Normal" and "Menu" group, if set */
  if (USE_24BIT)
    set_normal_colors();

  for (idx = 0; idx < highlight_ga.ga_len; ++idx)
    gui_do_one_color(idx, FALSE, FALSE);

  highlight_changed();
}

static void
gui_do_one_color(
    int idx,
    int do_menu UNUSED,    /* TRUE: might set the menu font */
    int do_tooltip UNUSED) /* TRUE: might set the tooltip font */
{
  int didit = FALSE;

  if (HL_TABLE()[idx].sg_gui_fg_name != NULL)
  {
    HL_TABLE()
    [idx].sg_gui_fg =
        color_name2handle(HL_TABLE()[idx].sg_gui_fg_name);
    didit = TRUE;
  }
  if (HL_TABLE()[idx].sg_gui_bg_name != NULL)
  {
    HL_TABLE()
    [idx].sg_gui_bg =
        color_name2handle(HL_TABLE()[idx].sg_gui_bg_name);
    didit = TRUE;
  }
  if (didit) /* need to get a new attr number */
    set_hl_attr(idx);
}
#endif

/*
 * Translate the 'highlight' option into attributes in highlight_attr[] and
 * set up the user highlights User1..9.  If FEAT_STL_OPT is in use, a set of
 * corresponding highlights to use on top of HLF_SNC is computed.
 * Called only when the 'highlight' option has been changed and upon first
 * screen redraw after any :highlight command.
 * Return FAIL when an invalid flag is found in 'highlight'.  OK otherwise.
 */
int highlight_changed(void)
{
  int hlf;
  int i;
  char_u *p;
  int attr;
  char_u *end;
  int id;
#ifdef USER_HIGHLIGHT
  char_u userhl[30]; // use 30 to avoid compiler warning
#endif
  static int hl_flags[HLF_COUNT] = HL_FLAGS;

  need_highlight_changed = FALSE;

  /*
     * Clear all attributes.
     */
  for (hlf = 0; hlf < (int)HLF_COUNT; ++hlf)
    highlight_attr[hlf] = 0;

  /*
     * First set all attributes to their default value.
     * Then use the attributes from the 'highlight' option.
     */
  for (i = 0; i < 2; ++i)
  {
    if (i)
      p = p_hl;
    else
      p = get_highlight_default();
    if (p == NULL) /* just in case */
      continue;

    while (*p)
    {
      for (hlf = 0; hlf < (int)HLF_COUNT; ++hlf)
        if (hl_flags[hlf] == *p)
          break;
      ++p;
      if (hlf == (int)HLF_COUNT || *p == NUL)
        return FAIL;

      /*
	     * Allow several hl_flags to be combined, like "bu" for
	     * bold-underlined.
	     */
      attr = 0;
      for (; *p && *p != ','; ++p) /* parse upto comma */
      {
        if (VIM_ISWHITE(*p)) /* ignore white space */
          continue;

        if (attr > HL_ALL) /* Combination with ':' is not allowed. */
          return FAIL;

        switch (*p)
        {
        case 'b':
          attr |= HL_BOLD;
          break;
        case 'i':
          attr |= HL_ITALIC;
          break;
        case '-':
        case 'n': /* no highlighting */
          break;
        case 'r':
          attr |= HL_INVERSE;
          break;
        case 's':
          attr |= HL_STANDOUT;
          break;
        case 'u':
          attr |= HL_UNDERLINE;
          break;
        case 'c':
          attr |= HL_UNDERCURL;
          break;
        case 't':
          attr |= HL_STRIKETHROUGH;
          break;
        case ':':
          ++p;                   /* highlight group name */
          if (attr || *p == NUL) /* no combinations */
            return FAIL;
          end = vim_strchr(p, ',');
          if (end == NULL)
            end = p + STRLEN(p);
          id = syn_check_group(p, (int)(end - p));
          if (id == 0)
            return FAIL;
          attr = syn_id2attr(id);
          p = end - 1;
          break;
        default:
          return FAIL;
        }
      }
      highlight_attr[hlf] = attr;

      p = skip_to_option_part(p); /* skip comma and spaces */
    }
  }

#ifdef USER_HIGHLIGHT
  /* Setup the user highlights
     *
     * Temporarily utilize 28 more hl entries:
     * 9 for User1-User9 combined with StatusLineNC
     * 9 for User1-User9 combined with StatusLineTerm
     * 9 for User1-User9 combined with StatusLineTermNC
     * 1 for StatusLine default
     * Have to be in there simultaneously in case of table overflows in
     * get_attr_entry()
     */
  for (i = 0; i < 9; i++)
  {
    sprintf((char *)userhl, "User%d", i + 1);
    id = syn_name2id(userhl);
    if (id == 0)
    {
      highlight_user[i] = 0;
    }
    else
    {
      highlight_user[i] = syn_id2attr(id);
    }
  }

#endif /* USER_HIGHLIGHT */

  return OK;
}

#if defined(FEAT_CMDL_COMPL) || defined(PROTO)

static void highlight_list(void);
static void highlight_list_two(int cnt, int attr);

/*
 * Handle command line completion for :highlight command.
 */
void set_context_in_highlight_cmd(expand_T *xp, char_u *arg)
{
  char_u *p;

  /* Default: expand group names */
  xp->xp_context = EXPAND_HIGHLIGHT;
  xp->xp_pattern = arg;
  include_link = 2;
  include_default = 1;

  /* (part of) subcommand already typed */
  if (*arg != NUL)
  {
    p = skiptowhite(arg);
    if (*p != NUL) /* past "default" or group name */
    {
      include_default = 0;
      if (STRNCMP("default", arg, p - arg) == 0)
      {
        arg = skipwhite(p);
        xp->xp_pattern = arg;
        p = skiptowhite(arg);
      }
      if (*p != NUL) /* past group name */
      {
        include_link = 0;
        if (arg[1] == 'i' && arg[0] == 'N')
          highlight_list();
        if (STRNCMP("link", arg, p - arg) == 0 || STRNCMP("clear", arg, p - arg) == 0)
        {
          xp->xp_pattern = skipwhite(p);
          p = skiptowhite(xp->xp_pattern);
          if (*p != NUL) /* past first group name */
          {
            xp->xp_pattern = skipwhite(p);
            p = skiptowhite(xp->xp_pattern);
          }
        }
        if (*p != NUL) /* past group name(s) */
          xp->xp_context = EXPAND_NOTHING;
      }
    }
  }
}

/*
 * List highlighting matches in a nice way.
 */
static void
highlight_list(void)
{
  int i;

  for (i = 10; --i >= 0;)
    highlight_list_two(i, HL_ATTR(HLF_D));
  for (i = 40; --i >= 0;)
    highlight_list_two(99, 0);
}

static void
highlight_list_two(int cnt, int attr)
{
  msg_puts_attr(&("N \bI \b!  \b"[cnt / 11]), attr);
  msg_clr_eos();
  ui_delay(cnt == 99 ? 40L : (long)cnt * 50L, FALSE);
}

#endif /* FEAT_CMDL_COMPL */

#if defined(FEAT_CMDL_COMPL) || defined(FEAT_EVAL) || defined(FEAT_SIGNS) || defined(PROTO)
/*
 * Function given to ExpandGeneric() to obtain the list of group names.
 */
char_u *
get_highlight_name(expand_T *xp UNUSED, int idx)
{
  return get_highlight_name_ext(xp, idx, TRUE);
}

/*
 * Obtain a highlight group name.
 * When "skip_cleared" is TRUE don't return a cleared entry.
 */
char_u *
get_highlight_name_ext(expand_T *xp UNUSED, int idx, int skip_cleared)
{
  if (idx < 0)
    return NULL;

  /* Items are never removed from the table, skip the ones that were
     * cleared. */
  if (skip_cleared && idx < highlight_ga.ga_len && HL_TABLE()[idx].sg_cleared)
    return (char_u *)"";

#ifdef FEAT_CMDL_COMPL
  if (idx == highlight_ga.ga_len && include_none != 0)
    return (char_u *)"none";
  if (idx == highlight_ga.ga_len + include_none && include_default != 0)
    return (char_u *)"default";
  if (idx == highlight_ga.ga_len + include_none + include_default && include_link != 0)
    return (char_u *)"link";
  if (idx == highlight_ga.ga_len + include_none + include_default + 1 && include_link != 0)
    return (char_u *)"clear";
#endif
  if (idx >= highlight_ga.ga_len)
    return NULL;
  return HL_TABLE()[idx].sg_name;
}
#endif

#if defined(PROTO)
/*
 * Free all the highlight group fonts.
 * Used when quitting for systems which need it.
 */
void free_highlight_fonts(void)
{
  int idx;

  for (idx = 0; idx < highlight_ga.ga_len; ++idx)
  {
    gui_mch_free_font(HL_TABLE()[idx].sg_font);
    HL_TABLE()
    [idx].sg_font = NOFONT;
#ifdef FEAT_XFONTSET
    gui_mch_free_fontset(HL_TABLE()[idx].sg_fontset);
    HL_TABLE()
    [idx].sg_fontset = NOFONTSET;
#endif
  }

  gui_mch_free_font(gui.norm_font);
#ifdef FEAT_XFONTSET
  gui_mch_free_fontset(gui.fontset);
#endif
  gui_mch_free_font(gui.bold_font);
  gui_mch_free_font(gui.ital_font);
  gui_mch_free_font(gui.boldital_font);
}
#endif

/**************************************
 *  End of Highlighting stuff	      *
 **************************************/
