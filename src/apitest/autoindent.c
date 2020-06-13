#include "libvim.h"
#include "minunit.h"

int alwaysIndent(buf_T *buf, char_u *prevLine, char_u *line)
{
  return 1;
}

int alwaysUnindent(buf_T *buf, char_u *prevLine, char_u *line)
{
  return -1;
}

int neverIndent(buf_T *buf, char_u *prevLine, char_u *line)
{
  return 0;
}

void test_setup(void)
{
  vimInput("<Esc>");
  vimInput("<Esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

/* TODO: Get this test green */
/* MU_TEST(insert_count) { */
/*   vimInput("5"); */
/*   vimInput("i"); */
/*   vimInput("a"); */
/*   vimInput("\033"); */

/*   char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine()); */
/*   printf("LINE: %s\n", line); */
/*   mu_check(strcmp(line, "aaaaaThis is the first line of a test file") == 0);
 */
/* } */

MU_TEST(test_autoindent_tab_normal_o)
{
  vimOptionSetInsertSpaces(false);
  vimSetAutoIndentCallback(&alwaysIndent);
  vimInput("o");
  vimInput("a");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  char_u *line2 = "\ta";
  mu_check(strcmp(line, line2) == 0);
}

MU_TEST(test_autoindent_spaces_normal_o)
{
  vimOptionSetInsertSpaces(true);
  vimOptionSetTabSize(7);
  vimSetAutoIndentCallback(&alwaysIndent);
  vimInput("o");
  vimInput("a");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  char_u *line2 = "       a";
  mu_check(strcmp(line, line2) == 0);
}

MU_TEST(test_autounindent_spaces_normal_o)
{
  vimOptionSetInsertSpaces(true);
  vimOptionSetTabSize(2);
  vimSetAutoIndentCallback(&alwaysUnindent);
  vimInput("o");
  vimInput("  a");
  vimInput("<cr>");
  vimInput("b");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  char_u *line2 = "b";
  mu_check(strcmp(line, line2) == 0);
}

MU_TEST(test_autounindent_spaces_no_indent)
{
  vimOptionSetInsertSpaces(true);
  vimOptionSetTabSize(2);
  vimSetAutoIndentCallback(&alwaysUnindent);
  vimInput("A");
  vimInput("<cr>");
  vimInput("b");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  char_u *line2 = "b";
  mu_check(strcmp(line, line2) == 0);
}

MU_TEST(test_autoindent_tab_insert_cr)
{
  vimOptionSetInsertSpaces(false);
  vimSetAutoIndentCallback(&alwaysIndent);
  vimInput("A");
  vimInput("<cr>");
  vimInput("a");
  vimInput("<cr>");
  vimInput("a");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: |%s|\n", line);
  char_u *line3 = "\t\ta";
  mu_check(strcmp(line, line3) == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_autoindent_tab_normal_o);
  MU_RUN_TEST(test_autoindent_spaces_normal_o);
  MU_RUN_TEST(test_autoindent_tab_insert_cr);
  MU_RUN_TEST(test_autounindent_spaces_normal_o);
  MU_RUN_TEST(test_autounindent_spaces_no_indent);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
