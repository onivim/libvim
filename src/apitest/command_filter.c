#include "libvim.h"
#include "minunit.h"

int debugCount = 0;
int hitCount = 0;

void test_setup(void)
{
  vimKey("<esc>");
  vimKey("<esc>");

  vimExecute("e!");
  vimInput("g");
  vimInput("g");
  vimInput("0");
  debugCount = 0;
  hitCount = 0;
}

int onCommand(exCommand_T *command)
{
  hitCount++;
  if (strncmp(command->cmd, "debug", strlen("debug")) == 0)
  {
    // Handled, do nothing..
    debugCount++;
    return 1;
  }
  else
  {
    return 0;
  }
}

void test_teardown(void) {}

MU_TEST(test_handle_command_via_command_line)
{
  vimSetCustomCommandHandler(&onCommand);
  vimInput(":");
  vimInput("debug");
  vimKey("<cr>");
  mu_check(debugCount == 1);
  mu_check(hitCount == 1);
}

MU_TEST(test_handle_command_via_execute)
{
  vimSetCustomCommandHandler(&onCommand);
  vimExecute("debug .");
  mu_check(debugCount == 1);
  mu_check(hitCount == 1);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_handle_command_via_command_line);
  MU_RUN_TEST(test_handle_command_via_execute);
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
