#include "libvim.h"
#include "minunit.h"
#include "vim.h"

static int mappingCallbackCount = 0;
static const mapblock_T *lastMapping = NULL;

void onMessage(char_u *title, char_u *msg, msgPriority_T priority)
{
  printf("onMessage - title: |%s| contents: |%s|", title, msg);
};

void onMap(const mapblock_T *mapping)
{
  printf("onMapping - orig_keys: |%s| keys: |%s| orig_str: |%s| script id: |%d|\n",
         mapping->m_orig_keys,
         mapping->m_keys,
         mapping->m_orig_str,
         mapping->m_script_ctx.sc_sid);

  lastMapping = mapping;
  mappingCallbackCount++;
};

void test_setup(void)
{
  mappingCallbackCount = 0;
  vimKey("<esc>");
  vimKey("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");
}

void test_teardown(void) {}

MU_TEST(test_simple_mapping)
{
  vimExecute("inoremap jk <Esc>");

  mu_check(strcmp("jk", lastMapping->m_orig_keys) == 0);
  mu_check(strcmp("<Esc>", lastMapping->m_orig_str) == 0);
  mu_check(mappingCallbackCount == 1);
};

MU_TEST(test_lhs_termcode)
{
  vimExecute("inoremap <Esc> jk");

  mu_check(strcmp("<Esc>", lastMapping->m_orig_keys) == 0);
  mu_check(strcmp("jk", lastMapping->m_orig_str) == 0);
  mu_check(mappingCallbackCount == 1);
};

MU_TEST(test_map_same_keys)
{
  vimExecute("inoremap jj <Esc>");

  mu_check(mappingCallbackCount == 1);

  vimExecute("inoremap jj <F1>");

  mu_check(mappingCallbackCount == 2);
  mu_check(strcmp("jj", lastMapping->m_orig_keys) == 0);
  mu_check(strcmp("<F1>", lastMapping->m_orig_str) == 0);
};

MU_TEST(test_sid_resolution)
{
  vimExecute("source collateral/map_plug_sid.vim");
  mu_check(mappingCallbackCount == 1);

  vimExecute("call <SNR>1_sayhello()");
};

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_simple_mapping);
  MU_RUN_TEST(test_lhs_termcode);
  MU_RUN_TEST(test_map_same_keys);
  MU_RUN_TEST(test_sid_resolution);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimSetInputMapCallback(&onMap);
  vimSetMessageCallback(&onMessage);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();

  return minunit_status;
}
