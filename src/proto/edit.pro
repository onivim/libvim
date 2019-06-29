/* edit.c */
void *state_edit_initialize(int cmdchar, int startln, long count);
void state_edit_cleanup(void *ctx);
executionStatus_T state_edit_execute(void *ctx, int c);

int edit(int cmdchar, int startln, long count);
int ins_need_undo_get(void);
void ins_redraw(int ready);
void edit_putchar(int c, int highlight);
char_u *prompt_text(void);
int prompt_curpos_editable(void);
void edit_unputchar(void);
void display_dollar(colnr_T col);
void change_indent(int type, int amount, int round, int replaced,
                   int call_changed_bytes);
void truncate_spaces(char_u *line);
void backspace_until_column(int col);
int get_literal(void);
void insertchar(int c, int flags, int second_indent);
void auto_format(int trailblank, int prev_line);
int comp_textwidth(int ff);
void start_arrow(pos_T *end_insert_pos);
int stop_arrow(void);
void set_last_insert(int c);
void free_last_insert(void);
char_u *add_char2buf(int c, char_u *s);
void beginline(int flags);
int oneright(void);
int oneleft(void);
int cursor_up(long n, int upd_topline);
int cursor_down(long n, int upd_topline);
int stuff_inserted(int c, long count, int no_esc);
char_u *get_last_insert(void);
char_u *get_last_insert_save(void);
void replace_push(int c);
int replace_push_mb(char_u *p);
int hkmap(int c);
int bracketed_paste(paste_mode_T mode, int drop, garray_T *gap);
void ins_scroll(void);
void ins_horscroll(void);
int ins_eol(int c);
int ins_copychar(linenr_T lnum);
colnr_T get_nolist_virtcol(void);
int can_cindent_get(void);
int ins_apply_autocmds(event_T event);
/* vim: set ft=c : */
