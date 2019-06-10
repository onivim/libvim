#include "libvim.h"
#include "minunit.h"

void test_setup(void) {
  vimInput("<esc>");
  vimInput("<esc>");

  vimExecute("e!");
}

void test_teardown(void) {}

/* MU_TEST(test_search_forward_esc) { */
/*     printf("SERACHING!\n"); */
/*   vimInput("/"); */
/*   vimInput("h"); */
/*   vimInput("<esc>"); */
/*   printf("CURSOR COL: %d\n", vimCursorGetColumn()); */
/*   vimInput("n"); */
/*   printf("CURSOR COL: %d\n", vimCursorGetColumn()); */
/*   vimInput("n"); */
/*   printf("CURSOR COL: %d\n", vimCursorGetColumn()); */
/*   vimInput("n"); */
/*   printf("CURSOR COL: %d\n", vimCursorGetColumn()); */
/*   vimInput("n"); */
/*   printf("CURSOR COL: %d\n", vimCursorGetColumn()); */
/*   vimInput("n"); */
/*   printf("CURSOR COL: %d\n", vimCursorGetColumn()); */
/*   /1* mu_check((vimGetMode() & CMDLINE) == CMDLINE); *1/ */
/*   vimInput("<esc>"); */
/*   printf("DONE"); */
/*   /1* mu_check((vimGetMode() & NORMAL) == NORMAL); *1/ */
/* } */

MU_TEST(test_cmdline_esc) {
  vimInput(":");
  mu_check((vimGetMode() & CMDLINE) == CMDLINE);
  vimInput("<esc>");
  mu_check((vimGetMode() & NORMAL) == NORMAL);
}

MU_TEST(test_cmdline_enter) {
  vimInput(":");
  mu_check((vimGetMode() & CMDLINE) == CMDLINE);
  vimInput("<cr>");
  mu_check((vimGetMode() & NORMAL) == NORMAL);
}

MU_TEST(test_cmdline_execute) {
  buf_T *buffer = vimBufferGetCurrent();
  int lc = vimBufferGetLineCount(buffer);
  mu_check(lc == 3);

  vimInput(":");
  vimInput("1");
  vimInput(",");
  vimInput("2");
  vimInput("d");
  vimInput("<cr>");
  mu_check((vimGetMode() & NORMAL) == NORMAL);

  lc = vimBufferGetLineCount(buffer);
  mu_check(lc == 1);
}

MU_TEST(test_cmdline_substitution) {
  buf_T *buffer = vimBufferGetCurrent();
  int lc = vimBufferGetLineCount(buffer);
  mu_check(lc == 3);

  vimInput(":");
  vimInput("s");
  vimInput("!");
  vimInput("T");
  vimInput("!");
  vimInput("A");
  vimInput("!");
  vimInput("g");
  vimInput("<cr>");

  mu_check(strcmp(vimBufferGetLine(buffer, 1),
                  "Ahis is the first line of a test file") == 0);
}

MU_TEST_SUITE(test_suite) {
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  /* MU_RUN_TEST(test_search_forward_esc); */
  /* TODO: Fix */
  MU_RUN_TEST(test_cmdline_esc);
  MU_RUN_TEST(test_cmdline_enter);
  MU_RUN_TEST(test_cmdline_execute);
  MU_RUN_TEST(test_cmdline_substitution);
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
