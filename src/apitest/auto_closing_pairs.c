#include "libvim.h"
#include "minunit.h"

void test_setup(void) {
  vimInput("<esc>");
  vimInput("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
  vimInput("0");
}

void test_teardown(void) {}

MU_TEST(test_backspace_matching_pair) {
    vimInput("i");
    vimInput("{");
    vimInput("[");
    printf("LINE before backspace: %s\n", vimBufferGetLine(curbuf, 1));
    vimInput("<bs>");
    printf("LINE after backspace: %s\n", vimBufferGetLine(curbuf, 1));
    vimInput("<bs>");
    printf("LINE after another backspace: %s\n", vimBufferGetLine(curbuf, 1));

    vimInput("<c-c>");
    vimInput("u");
    printf("LINE after undo: %s\n", vimBufferGetLine(curbuf, 1));
    vimInput("<c-r>");
    printf("LINE after redo: %s\n", vimBufferGetLine(curbuf, 1));
}

MU_TEST(test_backspace_matching_macro_insert) {
    /* vimInput("q"); */
    /* vimInput("a"); */
    vimInput("A");
    vimInput("a");
    vimInput("b");
    vimInput("c");
    vimInput("{");
    vimInput("[");
    vimInput("{");
    vimInput("d");
    /* vimInput("<bs>"); */
    /* vimInput("<bs>"); */
    /* vimInput("<bs>"); */
    vimInput("<esc>");
    /* vimInput("q"); */

    printf("REDO BUFFER: |%s|\n", get_inserted());

    printf("dot!\n");
    vimInput("j");
    vimInput(".");
    /* vimInput("a"); */
    vimInput("j");
    vimInput(".");

    printf("First line: %s\n", vimBufferGetLine(curbuf, 1));
    printf("Second line: %s\n", vimBufferGetLine(curbuf, 2));
    printf("Third line: %s\n", vimBufferGetLine(curbuf, 3));
}

MU_TEST_SUITE(test_suite) {
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_backspace_matching_pair);
  MU_RUN_TEST(test_backspace_matching_macro_insert);
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
