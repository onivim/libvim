#include "libvim.h"
#include "minunit.h"

static int quitCount = 0;
static int lastForce = 0;
static buf_T *lastQuitBuf = 0;

void onQuit(buf_T *buffer, int force)
{

  lastQuitBuf = buffer;
  lastForce = force;
  quitCount++;
}

void test_setup(void)
{
  vimExecute("e!");
  vimInput("g");
  vimInput("g");
  quitCount = 0;
}

void test_teardown(void) {}

MU_TEST(test_q)
{
  vimExecute("q");

  mu_check(quitCount == 1);
  mu_check(lastQuitBuf == curbuf);
  mu_check(lastForce == FALSE);
}

MU_TEST(test_q_force)
{
  vimExecute("q!");

  mu_check(quitCount == 1);
  mu_check(lastQuitBuf == curbuf);
  mu_check(lastForce == TRUE);
}

MU_TEST(test_xall)
{
  vimExecute("xall");

  mu_check(quitCount == 1);
  mu_check(lastQuitBuf == NULL);
  mu_check(lastForce == FALSE);
}

MU_TEST(test_xit)
{
  vimExecute("xit!");

  mu_check(quitCount == 1);
  mu_check(lastQuitBuf == curbuf);
  mu_check(lastForce == TRUE);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_q);
  MU_RUN_TEST(test_q_force);
  MU_RUN_TEST(test_xall);
  MU_RUN_TEST(test_xit);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimSetQuitCallback(&onQuit);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
