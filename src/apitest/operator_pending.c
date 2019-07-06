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

MU_TEST(test_delete_operator_pending)
{
  vimInput("d");

  // Pressing 'd' should bring us to operator-pending state
  mu_check((vimGetMode() & OP_PENDING) == OP_PENDING);

  vimInput("2");

  // Should still be in op_pending since this didn't finish the motion...
  mu_check((vimGetMode() & OP_PENDING) == OP_PENDING);

  // Should now be back to normal
  vimInput("j");

  mu_check((vimGetMode() & OP_PENDING) != OP_PENDING);
  mu_check((vimGetMode() & NORMAL) == NORMAL);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_delete_operator_pending);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/curswant.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
