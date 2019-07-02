#include "libvim.h"
#include "minunit.h"

static int cmdLineEnterCount = 0;
static int cmdLineLeaveCount = 0;
static int cmdLineChangedCount = 0;

void onAutoCommand(event_T command, buf_T *buf)
{
  switch (command)
  {
  case EVENT_CMDLINECHANGED:
    cmdLineChangedCount++;
    return;
  case EVENT_CMDLINEENTER:
    cmdLineEnterCount++;
    return;
  case EVENT_CMDLINELEAVE:
    cmdLineLeaveCount++;
    return;
  default:
    return;
  }
}

void test_setup(void)
{
  vimInput("<esc>");
  vimInput("<esc>");

  vimExecute("e!");
}

void test_teardown(void)
{
  cmdLineEnterCount = 0;
  cmdLineLeaveCount = 0;
  cmdLineChangedCount = 0;
}

MU_TEST(test_cmdline_esc)
{
  vimInput(":");
  mu_check((vimGetMode() & CMDLINE) == CMDLINE);
  vimInput("<esc>");
  mu_check((vimGetMode() & NORMAL) == NORMAL);
}

MU_TEST(test_cmdline_enter)
{
  vimInput(":");
  mu_check((vimGetMode() & CMDLINE) == CMDLINE);
  vimInput("<cr>");
  mu_check((vimGetMode() & NORMAL) == NORMAL);
}

MU_TEST(test_cmdline_autocmds)
{
  buf_T *buffer = vimBufferGetCurrent();
  int lc = vimBufferGetLineCount(buffer);
  mu_check(lc == 3);

  mu_check(cmdLineEnterCount == 0);
  vimInput(":");
  mu_check(cmdLineEnterCount == 1);
  mu_check(cmdLineChangedCount == 0);

  vimInput("a");
  mu_check(cmdLineChangedCount == 1);

  vimInput("b");
  mu_check(cmdLineChangedCount == 2);

  vimInput("c");
  mu_check(cmdLineChangedCount == 3);
  mu_check(cmdLineLeaveCount == 0);
  vimInput("<esc>");
  mu_check(cmdLineLeaveCount == 1);

  mu_check((vimGetMode() & NORMAL) == NORMAL);
}

MU_TEST(test_cmdline_no_execute_with_esc)
{
  buf_T *buffer = vimBufferGetCurrent();
  int lc = vimBufferGetLineCount(buffer);
  mu_check(lc == 3);

  vimInput(":");
  vimInput("1");
  vimInput(",");
  vimInput("2");
  vimInput("d");
  vimInput("<c-c>");
  mu_check((vimGetMode() & NORMAL) == NORMAL);

  lc = vimBufferGetLineCount(buffer);
  mu_check(lc == 3);
}

MU_TEST(test_cmdline_execute)
{
  buf_T *buffer = vimBufferGetCurrent();
  int lc = vimBufferGetLineCount(buffer);
  mu_check(lc == 3);

  vimInput(":");
  vimInput("1");
  vimInput(",");
  vimInput("2");
  vimInput("d");
  vimInput("<cr>");
  mu_check((vimGetMode() & NORMAL) == NORMAL);

  lc = vimBufferGetLineCount(buffer);
  mu_check(lc == 1);
}

MU_TEST(test_cmdline_substitution)
{
  buf_T *buffer = vimBufferGetCurrent();
  int lc = vimBufferGetLineCount(buffer);
  mu_check(lc == 3);

  vimInput(":");
  vimInput("s");
  vimInput("!");
  vimInput("T");
  vimInput("!");
  vimInput("A");
  vimInput("!");
  vimInput("g");
  vimInput("<cr>");

  mu_check(strcmp(vimBufferGetLine(buffer, 1),
                  "Ahis is the first line of a test file") == 0);
}

MU_TEST(test_cmdline_get_type)
{
  vimInput(":");
  mu_check(vimCommandLineGetType() == ':');
  vimInput("<esc>");

  vimInput("/");
  mu_check(vimCommandLineGetType() == '/');
  vimInput("<esc>");

  vimInput("?");
  mu_check(vimCommandLineGetType() == '?');
  vimInput("<esc>");
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  /* MU_RUN_TEST(test_search_forward_esc); */
  MU_RUN_TEST(test_cmdline_autocmds);
  MU_RUN_TEST(test_cmdline_no_execute_with_esc);
  MU_RUN_TEST(test_cmdline_esc);
  MU_RUN_TEST(test_cmdline_enter);
  MU_RUN_TEST(test_cmdline_execute);
  MU_RUN_TEST(test_cmdline_substitution);
  MU_RUN_TEST(test_cmdline_get_type);
}

int main(int argc, char **argv)
{
  vimSetAutoCommandCallback(&onAutoCommand);
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
