#include "libvim.h"
#include "minunit.h"

void test_setup(void) {
  vimInput("<esc>");
  vimInput("<esc>");

  vimExecute("e!");
}

void test_teardown(void) {}

MU_TEST(test_cmdline_null) {
  // Verify values are expected when we're not in command line mode

  mu_check(vimCommandLineGetText() == NULL);
  mu_check(vimCommandLineGetType() == NULL);
  mu_check(vimCommandLineGetPosition() == 0);

  char **completions;
  int count = 1;
  vimCommandLineGetCompletions(&completions, &count);
  mu_check(count == 0);

  FreeWild(&count, &completions);
}

MU_TEST(test_cmdline_get_type) {
  vimInput(":");
  mu_check(vimCommandLineGetType() == ':');
}

MU_TEST(test_cmdline_get_text) {
  vimInput(":");
  mu_check(strcmp(vimCommandLineGetText(), "") == 0);
  mu_check(vimCommandLineGetPosition() == 0);

  vimInput("a");
  mu_check(strcmp(vimCommandLineGetText(), "a") == 0);
  mu_check(vimCommandLineGetPosition() == 1);

  vimInput("b");
  mu_check(strcmp(vimCommandLineGetText(), "ab") == 0);
  mu_check(vimCommandLineGetPosition() == 2);

  vimInput("c");
  mu_check(strcmp(vimCommandLineGetText(), "abc") == 0);
  mu_check(vimCommandLineGetPosition() == 3);

  vimInput("<c-h>");
  mu_check(strcmp(vimCommandLineGetText(), "ab") == 0);
  mu_check(vimCommandLineGetPosition() == 2);

  vimInput("<cr>");
}

MU_TEST(test_cmdline_completions) {
  char **completions;
  int count = 1;

  vimInput(":");

  vimInput("e");
  vimCommandLineGetCompletions(&completions, &count);
  mu_check(count == 20);
  FreeWild(&count, &completions);

  vimInput("d");
  vimCommandLineGetCompletions(&completions, &count);
  mu_check(count == 1);
  FreeWild(&count, &completions);

  vimInput(" ");
  vimInput(".");
  vimInput("/");
  vimInput("c");
  vimInput("o");
  vimCommandLineGetCompletions(&completions, &count);
  mu_check(count == 1);
  FreeWild(&count, &completions);
}

MU_TEST_SUITE(test_suite) {
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_cmdline_null);
  MU_RUN_TEST(test_cmdline_get_text);
  MU_RUN_TEST(test_cmdline_get_type);
  MU_RUN_TEST(test_cmdline_completions);
}

int main(int argc, char **argv) {
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  buf_T *buf = vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
