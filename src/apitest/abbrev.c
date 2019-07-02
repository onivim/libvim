#include "libvim.h"
#include "minunit.h"

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

MU_TEST(insert_abbrev_multiple)
{

  vimExecute("iabbrev waht what");

  vimInput("I");
  vimInput("w");
  vimInput("a");
  vimInput("h");
  vimInput("t");
  vimInput(" ");
  vimInput("w");
  vimInput("a");
  vimInput("h");
  vimInput("t");
  vimInput(" ");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: %s\n", line);
  mu_check(strcmp(line, "what what This is the first line of a test file") ==
           0);
}

MU_TEST(insert_abbrev_no_recursive)
{
  vimExecute("iabbrev waht what");
  vimExecute("iabbrev what what2");

  vimInput("I");
  vimInput("w");
  vimInput("a");
  vimInput("h");
  vimInput("t");
  vimInput(" ");
  vimInput("w");
  vimInput("h");
  vimInput("a");
  vimInput("t");
  vimInput(" ");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: %s\n", line);
  mu_check(strcmp(line, "what what2 This is the first line of a test file") ==
           0);
}

MU_TEST(insert_abbrev_expr)
{
  vimExecute("iabbrev <expr> waht col('.')");

  vimInput("I");
  vimInput("w");
  vimInput("a");
  vimInput("h");
  vimInput("t");
  vimInput(" ");
  vimInput("w");
  vimInput("a");
  vimInput("h");
  vimInput("t");
  vimInput(" ");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: %s\n", line);
  mu_check(strcmp(line, "5 7 This is the first line of a test file") == 0);
}

MU_TEST(command_abbrev)
{
  vimExecute("cabbrev abc def");

  vimInput(":");
  vimInput("a");
  vimInput("b");
  vimInput("c");
  vimInput(" ");
  vimInput("a");
  vimInput("b");
  vimInput("c");
  vimInput(" ");

  char_u *line = vimCommandLineGetText();
  printf("LINE: %s\n", line);
  mu_check(strcmp(line, "def def ") == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(insert_abbrev_multiple);
  MU_RUN_TEST(insert_abbrev_no_recursive);
  MU_RUN_TEST(insert_abbrev_expr);
  MU_RUN_TEST(command_abbrev);
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
