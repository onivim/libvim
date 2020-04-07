#include "libvim.h"
#include "minunit.h"

static int gotoCount = 0;
static int lastLnum = 0;
static int lastCol = 0;
static gotoTarget_T lastTarget = DEFINITION;

int onGoto(gotoRequest_T gotoRequest)
{
  lastLnum = gotoRequest.location.lnum;
  lastCol = gotoRequest.location.col;
  lastTarget = gotoRequest.target;
  gotoCount++;
  return 1;
}

void test_setup(void)
{
  vimSetGotoCallback(&onGoto);

  vimInput("<esc>");
  vimInput("<esc>");

  vimExecute("e!");

  vimInput("g");
  vimInput("g");

  gotoCount = 0;
  lastLnum = 0;
  lastCol = 0;
  lastTarget = DEFINITION;
}

void test_teardown(void) {}

MU_TEST(test_goto_no_callback)
{
  vimSetGotoCallback(NULL);
  vimInput("g");
  vimInput("d");

  mu_check(gotoCount == 0);
}

MU_TEST(test_goto_definition)
{
  vimInput("g");
  vimInput("d");

  mu_check(gotoCount == 1);
  mu_check(lastLnum == 1);
  mu_check(lastCol == 0);
  mu_check(lastTarget == DEFINITION);
}

MU_TEST(test_goto_declaration)
{
  vimInput("g");
  vimInput("D");

  mu_check(gotoCount == 1);
  mu_check(lastLnum == 1);
  mu_check(lastCol == 0);
  mu_check(lastTarget == DECLARATION);
}

// TODO: Implement goto-implementation
/*MU_TEST(test_goto_implementation)
{
  vimInput("<C-]>");

  mu_check(gotoCount == 1);
  mu_check(lastLnum == 1);
  mu_check(lastCol == 1);
  mu_check(lastTarget == IMPLEMENTATION);
}*/

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_goto_no_callback);
  MU_RUN_TEST(test_goto_definition);
  MU_RUN_TEST(test_goto_declaration);
  //MU_RUN_TEST(test_goto_implementation);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimSetGotoCallback(&onGoto);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
