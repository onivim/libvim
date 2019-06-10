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
  vimInput("1,2d");
  vimInput("<cr>");
  mu_check((vimGetMode() & NORMAL) == NORMAL);

  lc = vimBufferGetLineCount(buffer);
  mu_check(lc == 1);
}

/* MU_TEST(test_cmdline_substitution) { */
/*     printf("here"); */
/*   vimInput(":"); */
/*     printf("after"); */
/*   vimInput("e"); */
/*   vimInput(" "); */
/*   vimInput("b"); */
/*   vimInput("s"); */

/*   char *line = vimBufferGetLine(curbuf, 1); */
/*   printf("LINE: %s\n", line); */
/*   int comp = strcmp(line, "Thit it the firtt line of a tett file"); */
/*   mu_check(comp == 0); */
/* } */

MU_TEST_SUITE(test_suite) {
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  /* MU_RUN_TEST(test_cmdline_substitution); */
  /* MU_RUN_TEST(test_search_forward_esc); */
  MU_RUN_TEST(test_cmdline_esc);
  MU_RUN_TEST(test_cmdline_enter);
  MU_RUN_TEST(test_cmdline_execute);
}

int main(int argc, char **argv) {
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  buf_T *buf = vimBufferOpen("testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();

  return minunit_status;
}
