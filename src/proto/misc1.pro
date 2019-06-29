/* misc1.c */
int get_indent(void);
int get_indent_lnum(linenr_T lnum);
int get_indent_buf(buf_T *buf, linenr_T lnum);
int get_indent_str(char_u *ptr, int ts, int list);
int get_indent_str_vtab(char_u *ptr, int ts, int *vts, int list);
int set_indent(int size, int flags);
int get_number_indent(linenr_T lnum);
int get_breakindent_win(win_T *wp, char_u *line);
int get_leader_len(char_u *line, char_u **flags, int backward,
                   int include_space);
int get_last_leader_offset(char_u *line, char_u **flags);
int plines(linenr_T lnum);
int plines_win(win_T *wp, linenr_T lnum, int winheight);
int plines_nofill(linenr_T lnum);
int plines_win_nofill(win_T *wp, linenr_T lnum, int winheight);
int plines_win_nofold(win_T *wp, linenr_T lnum);
int plines_win_col(win_T *wp, linenr_T lnum, long column);
int plines_m_win(win_T *wp, linenr_T first, linenr_T last);
int gchar_pos(pos_T *pos);
int gchar_cursor(void);
void pchar_cursor(int c);
int inindent(int extra);
char_u *skip_to_option_part(char_u *p);
void check_status(buf_T *buf);
int ask_yesno(char_u *str, int direct);
int is_mouse_key(int c);
int get_keystroke(void);
int get_number(int colon, int *mouse_used);
int prompt_for_number(int *mouse_used);
void msgmore(long n);
void beep_flush(void);
void vim_beep(unsigned val);
void init_homedir(void);
void free_homedir(void);
void free_users(void);
char_u *expand_env_save(char_u *src);
char_u *expand_env_save_opt(char_u *src, int one);
void expand_env(char_u *src, char_u *dst, int dstlen);
void expand_env_esc(char_u *srcp, char_u *dst, int dstlen, int esc, int one,
                    char_u *startstr);
char_u *vim_getenv(char_u *name, int *mustfree);
void vim_unsetenv(char_u *var);
void vim_setenv(char_u *name, char_u *val);
char_u *get_env_name(expand_T *xp, int idx);
char_u *get_users(expand_T *xp, int idx);
int match_user(char_u *name);
void home_replace(buf_T *buf, char_u *src, char_u *dst, int dstlen, int one);
char_u *home_replace_save(buf_T *buf, char_u *src);
int fullpathcmp(char_u *s1, char_u *s2, int checkname, int expandenv);
char_u *gettail(char_u *fname);
char_u *gettail_sep(char_u *fname);
char_u *getnextcomp(char_u *fname);
char_u *get_past_head(char_u *path);
int vim_ispathsep(int c);
int vim_ispathsep_nocolon(int c);
void shorten_dir(char_u *str);
int dir_of_file_exists(char_u *fname);
int vim_fnamecmp(char_u *x, char_u *y);
int vim_fnamencmp(char_u *x, char_u *y, size_t len);
char_u *concat_fnames(char_u *fname1, char_u *fname2, int sep);
char_u *concat_str(char_u *str1, char_u *str2);
void add_pathsep(char_u *p);
char_u *FullName_save(char_u *fname, int force);
void prepare_to_exit(void);
void preserve_exit(void);
int vim_fexists(char_u *fname);
void line_breakcheck(void);
void fast_breakcheck(void);
int expand_wildcards_eval(char_u **pat, int *num_file, char_u ***file,
                          int flags);
int expand_wildcards(int num_pat, char_u **pat, int *num_files, char_u ***files,
                     int flags);
int match_suffix(char_u *fname);
int unix_expandpath(garray_T *gap, char_u *path, int wildoff, int flags,
                    int didstar);
void remove_duplicates(garray_T *gap);
int gen_expand_wildcards(int num_pat, char_u **pat, int *num_file,
                         char_u ***file, int flags);
void addfile(garray_T *gap, char_u *f, int flags);
char_u *get_cmd_output(char_u *cmd, char_u *infile, int flags, int *ret_len);
void FreeWild(int count, char_u **files);
int goto_im(void);
char_u *get_isolated_shell_name(void);
int path_is_url(char_u *p);
int path_with_url(char_u *fname);
int vim_isAbsName(char_u *name);
int vim_FullName(char_u *fname, char_u *buf, int len, int force);
/* vim: set ft=c : */
