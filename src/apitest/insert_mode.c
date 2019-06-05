#include "libvim.h"
#include "minunit.h"

void test_setup(void) {
  vimInput("\033");
  vimInput("\033");
  vimExecute("e!");
}

void test_teardown(void) {

}

MU_TEST(insert_beginning) {
  vimInput("I");
  vimInput("a");
  vimInput("b");
  vimInput("c");

  char_u *line = vimBufferGetLine(curbuf, vimWindowGetCursorLine());
  mu_check(strcmp(line, "abcThis is the first line of a test file") == 0);
}

MU_TEST(insert_end) {
  vimInput("A");
  vimInput("a");
  vimInput("b");
  vimInput("c");
  char_u *line = vimBufferGetLine(curbuf, vimWindowGetCursorLine());

  printf("LINE: %s\n", line);

  mu_check(strcmp(line, "This is the first line of a test fileabc") == 0);
}

MU_TEST_SUITE(test_suite) {
    MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

    MU_RUN_TEST(insert_beginning);
    MU_RUN_TEST(insert_end);
}

int main(int argc, char **argv) {
  vimInit(argc, argv);

  win_setwidth(5);
  win_setheight(100);

  buf_T *buf = vimBufferOpen("testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();

  return minunit_status;
}
