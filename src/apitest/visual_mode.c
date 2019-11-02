#include "libvim.h"
#include "minunit.h"

void test_setup(void)
{
  vimInput("<esc>");
  vimInput("<esc>");

  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

MU_TEST(test_visual_is_active)
{
  mu_check(vimVisualIsActive() == 0);

  vimInput("v");
  mu_check(vimVisualGetType() == 'v');
  mu_check(vimVisualIsActive() == 1);
  mu_check((vimGetMode() & VISUAL) == VISUAL);

  vimInput("<esc>");
  mu_check((vimGetMode() & NORMAL) == NORMAL);
  mu_check(vimVisualIsActive() == 0);

  vimInput("<c-v>");
  mu_check(vimVisualGetType() == Ctrl_V);
  mu_check(vimVisualIsActive() == 1);
  mu_check((vimGetMode() & VISUAL) == VISUAL);

  vimInput("<esc>");
  mu_check((vimGetMode() & NORMAL) == NORMAL);
  mu_check(vimVisualIsActive() == 0);

  vimInput("V");
  mu_check(vimVisualGetType() == 'V');
  mu_check(vimVisualIsActive() == 1);
  mu_check((vimGetMode() & VISUAL) == VISUAL);
}

MU_TEST(test_characterwise_range)
{
  vimInput("v");

  vimInput("l");
  vimInput("l");

  pos_T start;
  pos_T end;

  // Get current range
  vimVisualGetRange(&start, &end);
  mu_check(start.lnum == 1);
  mu_check(start.col == 0);
  mu_check(end.lnum == 1);
  mu_check(end.col == 2);

  vimInput("<esc>");
  vimInput("j");

  // Validate we still get previous range
  vimVisualGetRange(&start, &end);
  mu_check(start.lnum == 1);
  mu_check(start.col == 0);
  mu_check(end.lnum == 1);
  mu_check(end.col == 2);
}

MU_TEST(test_ctrl_q)
{
  vimInput("<c-q>");

  mu_check((vimGetMode() & VISUAL) == VISUAL);
  mu_check(vimVisualGetType() == Ctrl_V);
  mu_check(vimVisualIsActive() == 1);
}

MU_TEST(test_ctrl_Q)
{
  vimInput("<c-Q>");

  mu_check((vimGetMode() & VISUAL) == VISUAL);
  mu_check(vimVisualGetType() == Ctrl_V);
  mu_check(vimVisualIsActive() == 1);
}

MU_TEST(test_insert_block_mode)
{
  vimInput("<c-v>");
  vimInput("j");
  vimInput("j");
  vimInput("j");

  vimInput("I");

  mu_check((vimGetMode() & INSERT) == INSERT);

  vimInput("a");
  vimInput("b");
  vimInput("c");
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_visual_is_active);
  MU_RUN_TEST(test_characterwise_range);
  MU_RUN_TEST(test_ctrl_q);
  MU_RUN_TEST(test_ctrl_Q);
  MU_RUN_TEST(test_insert_block_mode);
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
