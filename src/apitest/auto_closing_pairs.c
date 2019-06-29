#include "vim.h"
#include "libvim.h"
#include "minunit.h"

void test_setup(void) {
  vimInput("<esc>");
  vimInput("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
  vimInput("0");

  vimExecute("set acp");

  autoClosingPair_T *pairs = (autoClosingPair_T*)(alloc(sizeof(autoClosingPair_T) * 3));
  pairs[0].open = '{';
  pairs[0].close = '}';
  pairs[1].open = '[';
  pairs[1].close =']';
  pairs[2].open ='"';
  pairs[2].close ='"';

  acp_set_pairs(pairs, 3);
  vim_free(pairs);
}

void test_teardown(void) {}

MU_TEST(test_backspace_matching_pair) {
    vimInput("i");
    vimInput("{");
    vimInput("[");

    mu_check(strcmp(vimBufferGetLine(curbuf, 1), "{[]}This is the first line of a test file") == 0);
    
    vimInput("<bs>");
    mu_check(strcmp(vimBufferGetLine(curbuf, 1), "{}This is the first line of a test file") == 0);
    
    vimInput("<bs>");
    mu_check(strcmp(vimBufferGetLine(curbuf, 1), "This is the first line of a test file") == 0);
}

MU_TEST(test_matching_pair_undo_redo) {
    vimInput("i");
    vimInput("{");
    vimInput("[");
    vimInput("<esc>");

    mu_check(strcmp(vimBufferGetLine(curbuf, 1), "{[]}This is the first line of a test file") == 0);

    vimInput("u");
    mu_check(strcmp(vimBufferGetLine(curbuf, 1), "This is the first line of a test file") == 0);

    vimInput("<c-r>");
    mu_check(strcmp(vimBufferGetLine(curbuf, 1), "{[]}This is the first line of a test file") == 0);
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

MU_TEST(test_enter_between_pairs) {
    /* vimInput("q"); */
    /* vimInput("a"); */
    vimInput("I");
    vimInput("{");
    vimInput("<cr>");
    vimInput("a");
    /* vimInput("<bs>"); */
    /* vimInput("<bs>"); */
    /* vimInput("<bs>"); */
    vimInput("b");
    vimInput("<esc>");

    printf("REDO BUFFER: |%s|\n", get_inserted());

    /* printf("dot!\n"); */
    /* vimInput("j"); */
    /* vimInput("."); */
    /* /1* vimInput("a"); *1/ */
    /* vimInput("j"); */
    /* vimInput("."); */

    printf("First line: |%s\n", vimBufferGetLine(curbuf, 1));
    printf("Second line: |%s\n", vimBufferGetLine(curbuf, 2));
    printf("Third line: |%s\n", vimBufferGetLine(curbuf, 3));

    vimInput("j");
    vimInput("j");
    vimInput(".");

    printf("First line: |%s\n", vimBufferGetLine(curbuf, 1));
    printf("Second line: |%s\n", vimBufferGetLine(curbuf, 2));
    printf("Third line: |%s\n", vimBufferGetLine(curbuf, 3));
    printf("Fourth line: |%s\n", vimBufferGetLine(curbuf, 4));
    printf("Fifth line: |%s\n", vimBufferGetLine(curbuf, 5));
    printf("Sixth line: |%s\n", vimBufferGetLine(curbuf, 6));
}

MU_TEST(test_enter_between_pairs_undo) {
    /* vimInput("q"); */
    /* vimInput("a"); */
    vimInput("I");
    vimInput("{");
    vimInput("<cr>");
    vimInput("a");
    /* vimInput("<bs>"); */
    /* vimInput("<bs>"); */
    /* vimInput("<bs>"); */
    vimInput("b");
    vimInput("<esc>");

    /* printf("dot!\n"); */
    /* vimInput("j"); */
    /* vimInput("."); */
    /* /1* vimInput("a"); *1/ */
    /* vimInput("j"); */
    /* vimInput("."); */

    printf("First line: |%s\n", vimBufferGetLine(curbuf, 1));
    printf("Second line: |%s\n", vimBufferGetLine(curbuf, 2));
    printf("Third line: |%s\n", vimBufferGetLine(curbuf, 3));

    vimInput("u");

    printf("First line: |%s\n", vimBufferGetLine(curbuf, 1));
    printf("Second line: |%s\n", vimBufferGetLine(curbuf, 2));
    printf("Third line: |%s\n", vimBufferGetLine(curbuf, 3));
    
    vimInput("<c-r>");

    printf("First line: |%s\n", vimBufferGetLine(curbuf, 1));
    printf("Second line: |%s\n", vimBufferGetLine(curbuf, 2));
    printf("Third line: |%s\n", vimBufferGetLine(curbuf, 3));
}

MU_TEST(test_pass_through_in_pairs) {
    vimInput("I");
    vimInput("{");
    vimInput("}");
    vimInput("a");
    vimInput("<esc>");

    printf("REDO BUFFER: |%s|\n", get_inserted());

    vimInput("j");
    vimInput(".");

    printf("First line: |%s\n", vimBufferGetLine(curbuf, 1));
    printf("Second line: |%s\n", vimBufferGetLine(curbuf, 2));
}

MU_TEST(test_pass_through_in_pairs_undo_redo) {
    vimInput("I");
    vimInput("{");
    vimInput("}");
    vimInput("a");
    vimInput("<esc>");

    printf("REDO BUFFER: |%s|\n", get_inserted());

    printf("First line: |%s\n", vimBufferGetLine(curbuf, 1));
    printf("Second line: |%s\n", vimBufferGetLine(curbuf, 2));
	
	vimInput("u");
    
    printf("First line: |%s\n", vimBufferGetLine(curbuf, 1));
    printf("Second line: |%s\n", vimBufferGetLine(curbuf, 2));
	
	vimInput("<c-r>");
    
    printf("First line: |%s\n", vimBufferGetLine(curbuf, 1));
    printf("Second line: |%s\n", vimBufferGetLine(curbuf, 2));
}

MU_TEST(test_setting_acp_option) {
    vimExecute("set autoclosingpairs");
    mu_check(p_acp == TRUE);

    vimExecute("set noautoclosingpairs");
    mu_check(p_acp == FALSE);

    vimExecute("set acp");
    mu_check(p_acp == TRUE);

    vimExecute("set noacp");
    mu_check(p_acp == FALSE);
}

MU_TEST(test_acp_should_pass_through) {
    mu_check(acp_should_pass_through('a') == FALSE);
}

MU_TEST_SUITE(test_suite) {
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_matching_pair_undo_redo);
  MU_RUN_TEST(test_backspace_matching_pair);
/*  MU_RUN_TEST(test_backspace_matching_macro_insert);
  MU_RUN_TEST(test_enter_between_pairs);
  MU_RUN_TEST(test_enter_between_pairs_undo);
  MU_RUN_TEST(test_pass_through_in_pairs);
  MU_RUN_TEST(test_pass_through_in_pairs_undo_redo);
  MU_RUN_TEST(test_setting_acp_option);
  MU_RUN_TEST(test_acp_should_pass_through);*/
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
