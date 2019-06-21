#include "libvim.h"
#include "minunit.h"

static int updateCount = 0;
static int lastLnum = 0;
static int lastLnume = 0;
static long lastXtra = 0;

void test_setup(void) {
  vimInput("<esc>");
  vimInput("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

MU_TEST(test_inoremap_jk) {
  // Remap 'jk' to leave insert mode
  vimExecute("inoremap jk <esc>");

  vimInput("I");
  vimInput("a");
  vimInput("b");

  vimInput("j");
  vimInput("k");

  char_u *line = vimBufferGetLine(curbuf, 1);

  printf("LINE: %s\n", line);
  mu_check((vimGetMode() & NORMAL) == NORMAL);
  mu_check(strcmp(line, "abThis is the first line of a test file") == 0);
}

MU_TEST_SUITE(test_suite) {
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_inoremap_jk);
}

int main(int argc, char **argv) {
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  buf_T *buf = vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();

  return minunit_status;
}
