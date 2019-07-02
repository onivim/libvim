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

MU_TEST(test_visual_linewise_delete)
{
  vimInput("V");

  vimInput("d");

  mu_check(vimBufferGetLineCount(curbuf) == 2);
  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: %s\n", line);
  mu_check(strcmp(line, "This is the second line of a test file") == 0);
}

MU_TEST(test_visual_linewise_motion_delete)
{
  vimInput("V");

  vimInput("2");
  vimInput("j");

  vimInput("d");

  mu_check(vimBufferGetLineCount(curbuf) == 1);
  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: %s\n", line);
  mu_check(strcmp(line, "") == 0);
}

MU_TEST(test_visual_character_delete)
{
  vimInput("v");
  vimInput("l");
  vimInput("d");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: %s\n", line);
  mu_check(strcmp(line, "is is the first line of a test file") == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_visual_linewise_delete);
  MU_RUN_TEST(test_visual_linewise_motion_delete);
  MU_RUN_TEST(test_visual_character_delete);
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
