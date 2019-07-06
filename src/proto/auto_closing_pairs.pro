/* state_machine.c */

void acp_set_pairs(autoClosingPair_T *pairs, int count);

int acp_should_pass_through(char_u c);
char_u acp_get_closing_character(char_u c);
int acp_is_opening_pair(char_u c);
int acp_is_closing_pair(char_u c);
int acp_is_cursor_between_pair(void);

/* vim: set ft=c : */
