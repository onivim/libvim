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

MU_TEST(test_toggle_uncommented)
{
  vimInput("g");
  vimInput("c");
  vimInput("c");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: %s\n", line);

  mu_check(strcmp(line, "//This is the first line of a test file") == 0);
}

MU_TEST(test_toggle_there_and_back_again)
{
  vimInput("g");
  vimInput("c");
  vimInput("c");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: %s\n", line);

  mu_check(strcmp(line, "//This is the first line of a test file") == 0);
  
  vimInput("g");
  vimInput("c");
  vimInput("c");

  line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: %s\n", line);

  mu_check(strcmp(line, "This is the first line of a test file") == 0);
}

MU_TEST(test_toggle_uncommented_visual)
{
  vimInput("V");
  vimInput("g");
  vimInput("c");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: %s\n", line);
  
  mu_check(strcmp(line, "//This is the first line of a test file") == 0);
}


MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_toggle_uncommented);
  MU_RUN_TEST(test_toggle_there_and_back_again);
  MU_RUN_TEST(test_toggle_uncommented_visual);
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
