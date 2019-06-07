#include "libvim.h"
#include "minunit.h"

void test_setup(void) {
  vimInput("<esc>");
  vimInput("<esc>");

  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

MU_TEST(test_visual_is_active) {
  mu_check(vimVisualIsActive() == 0);

  vimInput("v");
  mu_check(vimVisualGetType() == 'v');
  mu_check(vimVisualIsActive() == 1);

  vimInput("<esc>");
  mu_check(vimVisualIsActive() == 0);

  vimInput("<c-v>");
  mu_check(vimVisualGetType() == Ctrl_V);
  mu_check(vimVisualIsActive() == 1);

  vimInput("<esc>");
  mu_check(vimVisualIsActive() == 0);

  vimInput("V");
  mu_check(vimVisualGetType() == 'V');
  mu_check(vimVisualIsActive() == 1);
}

MU_TEST_SUITE(test_suite) {
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_visual_is_active);
}

int main(int argc, char **argv) {
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  buf_T *buf = vimBufferOpen("testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
