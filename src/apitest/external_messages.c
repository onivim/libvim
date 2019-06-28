#include "vim.h"
#include "libvim.h"
#include "minunit.h"

void onMessage(char_u* msg) {
  // printf("on message?\n");
  // printf("Got message: %s!\n", msg);
};

void test_setup(void) {
  printf("a\n");
  vimInput("<esc>");
  vimInput("<esc>");
  printf("b\n");
  vimExecute("e!");
  printf("e\n");

  vimInput("g");
  vimInput("g");
}

void test_teardown(void) {}

MU_TEST(test_msg2_put) {
  msg_T* msg = msg2_create(MSG_INFO);  
  msg2_put("a", msg);

  mu_check(strcmp(msg2_get_contents(msg), "a") == 0);

  msg2_free(msg);
};

MU_TEST(test_msg2_put_multiple) {
  msg_T* msg = msg2_create(MSG_INFO);  
  msg2_put("ab", msg);
  msg2_put("\n", msg);
  msg2_put("c", msg);

  mu_check(strcmp(msg2_get_contents(msg), "ab\nc") == 0);

  msg2_free(msg);
};

MU_TEST(test_echo) {
  vimExecute("echomsg 'hi'");

  mu_check(1 == 0);
}

MU_TEST(test_error) {
  vimExecute("buf 999");

  mu_check(1 == 0);
}

MU_TEST(test_autocmd) {
  vimExecute("jumps");

  mu_check(1 == 0);
}

MU_TEST(test_changes) {
  vimExecute("changes");

  mu_check(1 == 0);
}

MU_TEST_SUITE(test_suite) {
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_msg2_put);
  MU_RUN_TEST(test_msg2_put_multiple);
  MU_RUN_TEST(test_echo);
  MU_RUN_TEST(test_error);
  MU_RUN_TEST(test_autocmd);
  MU_RUN_TEST(test_changes);
}

int main(int argc, char **argv) {
  vimInit(argc, argv);

  // vimSetMessageCallback(&onMessage);

  win_setwidth(5);
  win_setheight(100);

  buf_T *buf = vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();

  return minunit_status;
}
