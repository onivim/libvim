#include "libvim.h"
#include "minunit.h"

static int indentSize = 0;

int onIndent(int tabSize, int prevIndent, char_u *prevLine, char_u *line)
{
  printf("onIndent: ts: %d prevIndent: %d prev |%s| line: |%s|", tabSize, prevIndent, prevLine, line);

  return indentSize;
};

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

MU_TEST(test_autoindent_normal_o)
{
  indentSize = 3;
  vimInput("o");
  vimInput("a");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: |%s|\n", line);
  char_u *line2 = "\t  a";
  printf("c0: %d", line[0]);
  printf("c1: %d", line[1]);
  printf("c2: %d", line[2]);
  printf("c3: %d", line[3]);
  mu_check(strcmp(line, line2) == 0);
}

MU_TEST(test_autoindent_insert_cr)
{
  indentSize = 3;
  vimInput("A");
  vimInput("<cr>");
  vimInput("a");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: |%s|\n", line);
  char_u *line2 = "\t  a";
  mu_check(strcmp(line, line2) == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_autoindent_normal_o);
  MU_RUN_TEST(test_autoindent_insert_cr);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);
  //vimSetIndentationCallback(&onIndent);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
