#include "libvim.h"
#include "minunit.h"

void test_setup(void)
{
  vimInput("<esc>");
  vimInput("<esc>");

  vimExecute("e!");
  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

MU_TEST(test_yank_to_register)
{
  vimInput("\"");
  vimInput("a");

  vimInput("y");
  vimInput("y");

  int numLines;
  char_u **lines;
  vimRegisterGet('a', &numLines, &lines);

  mu_check(numLines == 1);
  printf("LINE: %s\n", lines[0]);
  mu_check(strcmp(lines[0], "This is the first line of a test file") == 0);
}

MU_TEST(test_delete_to_register)
{
  vimInput("\"");
  vimInput("b");

  vimInput("d");
  vimInput("j");

  int numLines;
  char_u **lines;
  vimRegisterGet('b', &numLines, &lines);

  mu_check(numLines == 2);
  printf("LINE: %s\n", lines[1]);
  mu_check(strcmp(lines[1], "This is the second line of a test file") == 0);
}

MU_TEST(test_extra_yank_doesnt_reset)
{
  vimInput("\"");
  vimInput("a");

  vimInput("y");
  vimInput("y");

  vimInput("j");
  vimInput("y");
  vimInput("y");

  int numLines;
  char_u **lines;
  vimRegisterGet('a', &numLines, &lines);

  mu_check(numLines == 1);
  printf("LINE: %s\n", lines[0]);
  mu_check(strcmp(lines[0], "This is the first line of a test file") == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_yank_to_register);
  MU_RUN_TEST(test_delete_to_register);
  MU_RUN_TEST(test_extra_yank_doesnt_reset);
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
