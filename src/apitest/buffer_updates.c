#include "libvim.h"
#include "minunit.h"

static int updateCount = 0;
static int lastLnum = 0;
static int lastLnume = 0;
static long lastXtra = 0;

void onBufferUpdate(bufferUpdate_T update) {
  lastLnum = update.lnum;
  lastLnume = update.lnume;
  lastXtra = update.xtra;
  updateCount++;
}

void test_setup(void) {
  vimExecute("e!");

  vimInput("g");
  vimInput("g");

  updateCount = 0;
  lastLnum = 0;
  lastLnume = 0;
  lastXtra = 0;
}

void test_teardown(void) {}

MU_TEST(test_single_line_update) {
  vimInput("x");

  mu_check(updateCount == 1);
  mu_check(lastLnum == 1);
  mu_check(lastLnume == 2);
  mu_check(lastXtra == 0);
}

MU_TEST(test_add_line) {
  vimInput("y");
  vimInput("y");
  vimInput("p");

  mu_check(updateCount == 1);
  mu_check(lastLnum == 2);
  mu_check(lastLnume == 2);
  mu_check(lastXtra == 1);
}

MU_TEST(test_add_multiple_lines) {
  vimInput("y");
  vimInput("y");
  vimInput("2");
  vimInput("p");

  mu_check(updateCount == 1);
  mu_check(lastLnum == 2);
  mu_check(lastLnume == 2);
  mu_check(lastXtra == 2);
}

MU_TEST(test_delete_line) {
  vimInput("d");
  vimInput("d");

  mu_check(updateCount == 1);
  mu_check(lastLnum == 1);
  mu_check(lastLnume == 2);
  mu_check(lastXtra == -1);
}

MU_TEST(test_delete_multiple_lines) {
  vimInput("d");
  vimInput("2");
  vimInput("j");

  mu_check(updateCount == 1);
  mu_check(lastLnum == 1);
  mu_check(lastLnume == 4);
  mu_check(lastXtra == -3);
}

MU_TEST(test_insert) {
  vimInput("i");
  vimInput("a");
  vimInput("b");

  mu_check(updateCount == 2);
  mu_check(lastLnum == 1);
  mu_check(lastLnume == 2);
  mu_check(lastXtra == 0);
}

MU_TEST_SUITE(test_suite) {
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_single_line_update);
  MU_RUN_TEST(test_add_line);
  MU_RUN_TEST(test_add_multiple_lines);
  MU_RUN_TEST(test_delete_line);
  MU_RUN_TEST(test_delete_multiple_lines);
  MU_RUN_TEST(test_insert);
}

int main(int argc, char **argv) {
  vimInit(argc, argv);

  vimSetBufferUpdateCallback(&onBufferUpdate);

  win_setwidth(5);
  win_setheight(100);

  buf_T *buf = vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
