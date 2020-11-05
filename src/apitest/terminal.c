#include "libvim.h"
#include "minunit.h"

static int terminalCallCount = 0;
static terminalRequest_t lastTerminalRequest;

void onTerminal(terminalRequest_t *termRequest)
{
  if (termRequest->cmd != NULL)
  {
    lastTerminalRequest.cmd = strdup(termRequest->cmd);
  }
  else
  {
    lastTerminalRequest.cmd = NULL;
  }
  lastTerminalRequest.curwin = termRequest->curwin;
  lastTerminalRequest.finish = termRequest->finish;
  printf("onTerminal called! %s\n", lastTerminalRequest.cmd);
  terminalCallCount++;
}

void test_setup(void)
{
  vimKey("<esc>");
  vimKey("<esc>");

  vimExecute("e!");
}

void test_teardown(void)
{
  free(lastTerminalRequest.cmd);
  lastTerminalRequest.cmd = 0;

  terminalCallCount = 0;
}

MU_TEST(test_term_noargs)
{
  vimInput(":term");
  vimKey("<cr>");

  mu_check(terminalCallCount == 1);
  mu_check(lastTerminalRequest.curwin == 0);
  mu_check(lastTerminalRequest.cmd == NULL);
  mu_check(lastTerminalRequest.finish == 'c');
}

MU_TEST(test_term_noclose)
{
  vimInput(":term ++noclose");
  vimKey("<cr>");

  mu_check(terminalCallCount == 1);
  mu_check(lastTerminalRequest.curwin == 0);
  mu_check(lastTerminalRequest.cmd == NULL);
  mu_check(lastTerminalRequest.finish == 'n');
}

MU_TEST(test_term_bash)
{
  vimInput(":term bash");
  vimKey("<cr>");

  mu_check(terminalCallCount == 1);
  mu_check(lastTerminalRequest.curwin == 0);
  mu_check(strcmp(lastTerminalRequest.cmd, "bash") == 0);
  printf("Finish: %c\n", lastTerminalRequest.finish);
  mu_check(lastTerminalRequest.finish == 'c');
}

MU_TEST(test_term_curwin)
{
  vimInput(":term ++curwin");
  vimKey("<cr>");

  mu_check(terminalCallCount == 1);
  mu_check(lastTerminalRequest.curwin == 1);
  mu_check(lastTerminalRequest.cmd == NULL);
  mu_check(lastTerminalRequest.finish == 'c');
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_term_noargs);
  MU_RUN_TEST(test_term_noclose);
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
