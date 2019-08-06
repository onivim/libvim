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

/* When clipboard is not enabled, the '*' register
 * should just behave like a normal register
 */
MU_TEST(test_clipboard_not_enabled_star)
{
  printf("\n: INPUT: slashn");
  vimInput("\"");
  printf("\n: INPUT: *\n");
  vimInput("*");

  printf("\n: INPUT: y1\n");
  vimInput("y");
  printf("\n: INPUT: y2\n");
  vimInput("y");

  int numLines;
  char_u **lines;
  vimRegisterGet(0, &numLines, &lines);

  mu_check(numLines == 1);
  printf("LINE: %s\n", lines[0]);
  mu_check(strcmp(lines[0], "This is the first line of a test file") == 0);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_clipboard_not_enabled_star);
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
