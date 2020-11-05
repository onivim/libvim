#include "libvim.h"
#include "minunit.h"

void test_setup(void)
{
  vimKey("<esc>");
  vimKey("<esc>");

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

MU_TEST(test_pending_operator_insert)
{
  vimInput("i");

  mu_check((vimGetMode() & INSERT) == INSERT);

  pendingOp_T pendingOp;
  mu_check(vimGetPendingOperator(&pendingOp) == FALSE);
}

MU_TEST(test_pending_operator_cmdline)
{
  vimInput(":");

  mu_check((vimGetMode() & CMDLINE) == CMDLINE);

  pendingOp_T pendingOp;
  mu_check(vimGetPendingOperator(&pendingOp) == FALSE);
}

MU_TEST(test_pending_operator_visual)
{
  vimInput("v");

  mu_check((vimGetMode() & VISUAL) == VISUAL);

  pendingOp_T pendingOp;
  mu_check(vimGetPendingOperator(&pendingOp) == FALSE);
}

MU_TEST(test_pending_operator_delete)
{
  vimInput("d");

  pendingOp_T pendingOp;
  mu_check(vimGetPendingOperator(&pendingOp) == TRUE);
  mu_check(pendingOp.op_type == OP_DELETE);
  mu_check(pendingOp.count == 0);
}

MU_TEST(test_pending_operator_delete_count)
{
  vimInput("5");
  vimInput("d");

  //mu_check((vimGetMode() & VISUAL) == VISUAL);

  pendingOp_T pendingOp;
  mu_check(vimGetPendingOperator(&pendingOp) == TRUE);
  mu_check(pendingOp.op_type == OP_DELETE);
  mu_check(pendingOp.count == 5);
}

MU_TEST(test_pending_operator_change)
{
  vimInput("2");
  vimInput("c");

  pendingOp_T pendingOp;
  mu_check(vimGetPendingOperator(&pendingOp) == TRUE);
  mu_check(pendingOp.op_type == OP_CHANGE);
  mu_check(pendingOp.count == 2);
}

MU_TEST(test_pending_operator_comment)
{
  vimInput("g");
  vimInput("c");

  pendingOp_T pendingOp;
  mu_check(vimGetPendingOperator(&pendingOp) == TRUE);
  mu_check(pendingOp.op_type == OP_COMMENT);
}

MU_TEST(test_pending_operator_register)
{
  vimInput("\"");
  vimInput("a");
  vimInput("y");

  pendingOp_T pendingOp;
  mu_check(vimGetPendingOperator(&pendingOp) == TRUE);
  mu_check(pendingOp.op_type == OP_YANK);
  mu_check(pendingOp.regname == 'a');
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_delete_operator_pending);
  MU_RUN_TEST(test_pending_operator_insert);
  MU_RUN_TEST(test_pending_operator_cmdline);
  MU_RUN_TEST(test_pending_operator_visual);
  MU_RUN_TEST(test_pending_operator_delete);
  MU_RUN_TEST(test_pending_operator_delete_count);
  MU_RUN_TEST(test_pending_operator_change);
  MU_RUN_TEST(test_pending_operator_comment);
  MU_RUN_TEST(test_pending_operator_register);
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
