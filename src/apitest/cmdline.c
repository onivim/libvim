#include "libvim.h"
#include "minunit.h"

int messageCount = 0;
void onMessage(char_u *title, char_u *msg, msgPriority_T priority)
{
  printf("onMessage - title: |%s| contents: |%s|", title, msg);
  messageCount++;
};

void test_setup(void)
{
  vimKey("<esc>");
  vimKey("<esc>");

  vimExecute("e!");
  vimInput("g");
  vimInput("g");
  vimInput("0");

  messageCount = 0;
}

void test_teardown(void) {}

MU_TEST(test_insert_literal_ctrl_v)
{
  vimInput(":");
  vimInput("a");
  vimKey("<c-v>");
  vimInput("1");
  vimInput("2");
  vimInput("6");
  vimInput("b");
  mu_check(strcmp(vimCommandLineGetText(), "a~b") == 0);
}

MU_TEST(test_insert_literal_ctrl_q)
{
  vimInput(":");
  vimInput("a");
  vimKey("<c-q>");
  vimInput("1");
  vimInput("2");
  vimInput("6");
  vimInput("b");
  mu_check(strcmp(vimCommandLineGetText(), "a~b") == 0);
}

MU_TEST(test_typing_function_command)
{
  vimInput(":");
  vimInput("function! Test()");
  vimKey("<CR>");
  //Should get an error message for multiline construct
  mu_check(messageCount == 1);
}

MU_TEST(test_multiline_command_sends_message)
{
  mu_check(messageCount == 0);
  vimExecute("function! Test()");
  // Should get an error message for multiline construct
  mu_check(messageCount == 1);
}

MU_TEST(test_valid_multiline_command)
{
  mu_check(messageCount == 0);

  char_u *lines[] = {
      "function! SomeCommandTest()",
      "return 42",
      "endfunction"};

  vimExecuteLines(lines, 3);
  mu_check(messageCount == 0);

  char_u *result = vimEval("SomeCommandTest()");
  printf("Got result: %s\n", result);
  mu_check(strcmp(result, "42") == 0);
  vim_free(result);
}

MU_TEST(test_multiline_multiple_functions)
{
  mu_check(messageCount == 0);

  char_u *lines[] = {
      "function! SomeCommandTest()",
      "return 42",
      "endfunction",
      "function! AnotherFunction()",
      "return 99",
      "endfunction"};

  vimExecuteLines(lines, 6);
  mu_check(messageCount == 0);

  char_u *result = vimEval("AnotherFunction()");
  printf("Got result: %s\n", result);
  mu_check(strcmp(result, "99") == 0);
  vim_free(result);
}

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_insert_literal_ctrl_v);
  MU_RUN_TEST(test_insert_literal_ctrl_q);
  MU_RUN_TEST(test_typing_function_command);
  MU_RUN_TEST(test_multiline_command_sends_message);
  MU_RUN_TEST(test_valid_multiline_command);
  MU_RUN_TEST(test_multiline_multiple_functions);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimSetMessageCallback(&onMessage);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
