#include "libvim.h"
#include "minunit.h"

void test_setup(void) {
  vimInput("<esc>");
  vimInput("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

MU_TEST(test_arrow_keys_normal) {
  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);

  char leftArrow[50];
  char rightArrow[50];
  char upArrow[50];
  char downArrow[50];
  sprintf(leftArrow, "%c", K_LEFT);
  sprintf(rightArrow, "%c", K_RIGHT);
  sprintf(upArrow, "%c", K_UP);
  sprintf(downArrow, "%c", K_DOWN);

  vimInput("<left>");

  /* printf("K_LEFT: %c", K_LEFT); */
  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 1);

  vimInput(downArrow);
  mu_check(vimCursorGetLine() == 2);
  mu_check(vimCursorGetColumn() == 1);

  vimInput(leftArrow);
  mu_check(vimCursorGetLine() == 2);
  mu_check(vimCursorGetColumn() == 0);

  vimInput(upArrow);
  mu_check(vimCursorGetLine() == 1);
  mu_check(vimCursorGetColumn() == 0);
}

MU_TEST_SUITE(test_suite) {
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_arrow_keys_normal);
}

int main(int argc, char **argv) {
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  buf_T *buf = vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
