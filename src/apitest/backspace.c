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

MU_TEST(backspace_beyond_insert)
{
  // Go to end of 'This'
  vimInput("e");

  // Enter insert after 'This'
  vimInput("a");

  // Backspace a couple of times...
  // This verifies we have the correct backspace settings
  // (default doesn't backspace past insert region)
  vimInput("<c-h>");
  vimInput("<c-h>");

  char_u *line = vimBufferGetLine(curbuf, vimCursorGetLine());
  printf("LINE: %s\n", line);
  mu_check(strcmp(line, "Th is the first line of a test file") == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(backspace_beyond_insert);
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
