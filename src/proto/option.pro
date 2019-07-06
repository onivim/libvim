/* option.c */
void set_init_1(int clean_arg);
void set_string_default(char *name, char_u *val);
void set_number_default(char *name, long val);
void set_local_options_default(win_T *wp);
void free_all_options(void);
void set_init_2(void);
void set_init_3(void);
void set_helplang_default(char_u *lang);
void init_gui_options(void);
void set_title_defaults(void);
int do_set(char_u *arg, int opt_flags);
int string_to_key(char_u *arg, int multi_byte);
void set_options_bin(int oldval, int newval, int opt_flags);
int get_viminfo_parameter(int type);
char_u *find_viminfo_parameter(int type);
void check_options(void);
void check_buf_options(buf_T *buf);
void free_string_option(char_u *p);
void clear_string_option(char_u **pp);
int get_term_opt_idx(char_u **p);
int set_term_option_alloced(char_u **p);
int was_set_insecurely(char_u *opt, int opt_flags);
void set_string_option_direct(char_u *name, int opt_idx, char_u *val,
                              int opt_flags, int set_sid);
void set_string_option_direct_in_win(win_T *wp, char_u *name, int opt_idx,
                                     char_u *val, int opt_flags, int set_sid);
void set_string_option_direct_in_buf(buf_T *buf, char_u *name, int opt_idx,
                                     char_u *val, int opt_flags, int set_sid);
int valid_spellang(char_u *val);
char *check_colorcolumn(win_T *wp);
char *check_stl_option(char_u *s);
void set_term_option_sctx_idx(char *name, int opt_idx);
int get_option_value(char_u *name, long *numval, char_u **stringval,
                     int opt_flags);
int get_option_value_strict(char_u *name, long *numval, char_u **stringval,
                            int opt_type, void *from);
char_u *option_iter_next(void **option, int opt_type);
char *set_option_value(char_u *name, long number, char_u *string,
                       int opt_flags);
char_u *get_term_code(char_u *tname);
char_u *get_highlight_default(void);
char_u *get_encoding_default(void);
int makeset(FILE *fd, int opt_flags, int local_only);
int makefoldset(FILE *fd);
void clear_termoptions(void);
void free_termoptions(void);
void free_one_termoption(char_u *var);
void set_term_defaults(void);
void comp_col(void);
void unset_global_local_option(char_u *name, void *from);
char_u *get_equalprg(void);
void win_copy_options(win_T *wp_from, win_T *wp_to);
void copy_winopt(winopt_T *from, winopt_T *to);
void check_win_options(win_T *win);
void clear_winopt(winopt_T *wop);
void buf_copy_options(buf_T *buf, int flags);
void reset_modifiable(void);
void set_iminsert_global(void);
void set_imsearch_global(void);
void set_context_in_set_cmd(expand_T *xp, char_u *arg, int opt_flags);
int ExpandSettings(expand_T *xp, regmatch_T *regmatch, int *num_file,
                   char_u ***file);
int ExpandOldSetting(int *num_file, char_u ***file);
int langmap_adjust_mb(int c);
int has_format_option(int x);
int shortmess(int x);
void vimrc_found(char_u *fname, char_u *envname);
void change_compatible(int on);
int option_was_set(char_u *name);
int reset_option_was_set(char_u *name);
int can_bs(int what);
void save_file_ff(buf_T *buf);
int file_ff_differs(buf_T *buf, int ignore_empty);
int check_ff_value(char_u *p);
int tabstop_set(char_u *var, int **array);
int tabstop_padding(colnr_T col, int ts_arg, int *vts);
int tabstop_at(colnr_T col, int ts, int *vts);
colnr_T tabstop_start(colnr_T col, int ts, int *vts);
void tabstop_fromto(colnr_T start_col, colnr_T end_col, int ts_arg, int *vts,
                    int *ntabs, int *nspcs);
int tabstop_eq(int *ts1, int *ts2);
int *tabstop_copy(int *oldts);
int tabstop_count(int *ts);
int tabstop_first(int *ts);
long get_sw_value(buf_T *buf);
long get_sw_value_indent(buf_T *buf);
long get_sw_value_pos(buf_T *buf, pos_T *pos);
long get_sw_value_col(buf_T *buf, colnr_T col);
long get_sts_value(void);
long get_scrolloff_value(void);
long get_sidescrolloff_value(void);
void find_mps_values(int *initc, int *findc, int *backwards, int switchit);
unsigned int get_bkc_value(buf_T *buf);
int signcolumn_on(win_T *wp);
dict_T *get_winbuf_options(int bufopt);
/* vim: set ft=c : */
