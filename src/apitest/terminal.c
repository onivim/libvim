#include "libvim.h"
#include "minunit.h"

static int terminalCallCount = 0;
static terminalRequest_t lastTerminalRequest;

void onTerminal(terminalRequest_t *termRequest)
{
  lastTerminalRequest.cmd = strdup(termRequest->cmd);
  lastTerminalRequest.curwin = termRequest->curwin;
  printf("onTerminal called! %s\n", lastTerminalRequest.cmd);
  terminalCallCount++;
}

void test_setup(void)
{
  vimInput("<esc>");
  vimInput("<esc>");

  vimExecute("e!");
}

void test_teardown(void)
{
  free(lastTerminalRequest.cmd);
  lastTerminalRequest.cmd = 0;

  terminalCallCount = 0;
}

MU_TEST(test_term_bash)
{
  vimInput(":");
  vimInput("t");
  vimInput("e");
  vimInput("r");
  vimInput("m");
  vimInput(" ");
  vimInput("b");
  vimInput("a");
  vimInput("s");
  vimInput("h");
  vimInput("<cr>");

  mu_check(terminalCallCount == 1);
  mu_check(lastTerminalRequest.curwin == 0);
  mu_check(strcmp(lastTerminalRequest.cmd, "bash") == 0);
}

MU_TEST(test_term_curwin)
{
  vimInput(":");
  vimInput("t");
  vimInput("e");
  vimInput("r");
  vimInput("m");
  vimInput(" ");
  vimInput("+");
  vimInput("+");
  vimInput("c");
  vimInput("u");
  vimInput("r");
  vimInput("w");
  vimInput("i");
  vimInput("n");
  vimInput("<cr>");

  mu_check(terminalCallCount == 1);
  mu_check(lastTerminalRequest.curwin == 1);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_term_bash);
  MU_RUN_TEST(test_term_curwin);
}

int main(int argc, char **argv)
{
  vimSetTerminalCallback(&onTerminal);
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
