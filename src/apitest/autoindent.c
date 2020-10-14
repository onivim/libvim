#include "libvim.h"
#include "minunit.h"

static int lastLnum = -1;

int alwaysIndent(int lnum, buf_T *buf, char_u *prevLine, char_u *line)
{
  printf("alwaysIndent - lnum: %d\n", lnum);
  lastLnum = lnum;
  return 1;
}

int alwaysIndentDouble(int lnum, buf_T *buf, char_u *prevLine, char_u *line)
{
  printf("alwaysIndentDouble - lnum: %d\n", lnum);
  lastLnum = lnum;
  return 2;
}

int alwaysUnindent(int lnum, buf_T *buf, char_u *prevLine, char_u *line)
{
  printf("alwaysUnindent - lnum: %d\n", lnum);
  lastLnum = lnum;
  return -1;
}

int alwaysUnindentDouble(int lnum, buf_T *buf, char_u *prevLine, char_u *line)
{
  printf("alwaysUnindentDouble - lnum: %d\n", lnum);
  lastLnum = lnum;
  return -2;
}

int neverIndent(int lnum, buf_T *buf, char_u *prevLine, char_u *line)
{
  printf("neverIndent - lnum: %d\n", lnum);
  lastLnum = lnum;
  return 0;
}

void test_setup(void)
{
  vimKey("<Esc>");
  vimKey("<Esc>");
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
  vimOptionSetInsertSpaces(FALSE);
  vimSetAutoIndentCallback(&alwaysIndent);
  vimInput("o");
  vimInput("a");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  char_u *line2 = "\ta";
  mu_check(strcmp(line, line2) == 0);
  mu_check(lastLnum == 2);
}

MU_TEST(test_autoindent_spaces_normal_o)
{
  vimOptionSetInsertSpaces(TRUE);
  vimOptionSetTabSize(7);
  vimSetAutoIndentCallback(&alwaysIndent);
  vimInput("o");
  vimInput("a");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  char_u *line2 = "       a";
  mu_check(strcmp(line, line2) == 0);
  mu_check(lastLnum == 2);
}

MU_TEST(test_autounindent_spaces_normal_o)
{
  vimOptionSetInsertSpaces(TRUE);
  vimOptionSetTabSize(2);
  vimSetAutoIndentCallback(&alwaysUnindent);
  vimInput("o");
  vimInput("  a");
  vimKey("<cr>");
  vimInput("b");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  char_u *line2 = "b";
  mu_check(strcmp(line, line2) == 0);
  mu_check(lastLnum == 3);
}

MU_TEST(test_autounindent_double_spaces_overflow_normal_o)
{
  vimOptionSetInsertSpaces(TRUE);
  vimOptionSetTabSize(2);
  vimSetAutoIndentCallback(&alwaysUnindentDouble);
  vimInput("o");
  vimInput("  a");
  vimKey("<cr>");
  vimInput("b");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  char_u *line2 = "b";
  mu_check(strcmp(line, line2) == 0);
  mu_check(lastLnum == 3);
}

MU_TEST(test_autounindent_double_spaces_normal_o)
{
  vimOptionSetInsertSpaces(TRUE);
  vimOptionSetTabSize(2);
  vimSetAutoIndentCallback(&alwaysUnindentDouble);
  vimInput("o");
  vimInput("    a");
  vimKey("<cr>");
  vimInput("b");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  char_u *line2 = "b";
  mu_check(strcmp(line, line2) == 0);
  mu_check(lastLnum == 3);
}

MU_TEST(test_autounindent_spaces_no_indent)
{
  vimOptionSetInsertSpaces(TRUE);
  vimOptionSetTabSize(2);
  vimSetAutoIndentCallback(&alwaysUnindent);
  vimInput("A");
  vimKey("<cr>");
  vimInput("b");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  char_u *line2 = "b";
  mu_check(strcmp(line, line2) == 0);
  mu_check(lastLnum == 2);
}

MU_TEST(test_autoindent_double_tab)
{
  vimOptionSetInsertSpaces(FALSE);
  vimSetAutoIndentCallback(&alwaysIndentDouble);
  vimInput("A");
  vimKey("<cr>");
  vimInput("a");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: |%s|\n", line);
  char_u *line2 = "\t\ta";
  mu_check(strcmp(line, line2) == 0);
  mu_check(lastLnum == 2);
}

MU_TEST(test_autoindent_tab_insert_cr)
{
  vimOptionSetInsertSpaces(FALSE);
  vimSetAutoIndentCallback(&alwaysIndent);
  vimInput("A");
  vimKey("<cr>");
  vimInput("a");
  vimKey("<cr>");
  vimInput("a");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: |%s|\n", line);
  char_u *line3 = "\t\ta";
  mu_check(strcmp(line, line3) == 0);
  mu_check(lastLnum == 3);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_autoindent_tab_normal_o);
  MU_RUN_TEST(test_autoindent_spaces_normal_o);
  MU_RUN_TEST(test_autoindent_tab_insert_cr);
  MU_RUN_TEST(test_autounindent_spaces_normal_o);
  MU_RUN_TEST(test_autounindent_spaces_no_indent);
  MU_RUN_TEST(test_autoindent_double_tab);

  MU_RUN_TEST(test_autounindent_double_spaces_overflow_normal_o);
  MU_RUN_TEST(test_autounindent_double_spaces_normal_o);
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
