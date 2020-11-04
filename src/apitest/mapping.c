#include "libvim.h"
#include "minunit.h"
#include "vim.h"

static int mappingCallbackCount = 0;
static const mapblock_T *lastMapping = NULL;

static int unmappingCallbackCount = 0;
static char_u *lastUnmapKeys = NULL;
static int lastUnmapMode = -1;

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

void onUnmap(int mode, const char_u *keys)
{
  //  printf("onUnmapping - mode: %d keys: %s\n",
  //         mode,
  //         keys);

  lastUnmapMode = mode;
  if (keys != NULL)
  {
    lastUnmapKeys = vim_strsave((char_u *)keys);
  }
  unmappingCallbackCount++;
};

void test_setup(void)
{
  vimKey("<esc>");
  vimKey("<esc>");
  vimExecute("e!");

  vimInput("g");
  vimInput("g");

  vimExecute("mapclear");
  mappingCallbackCount = 0;
  unmappingCallbackCount = 0;
  lastMapping = NULL;
  if (lastUnmapKeys != NULL)
  {
    vim_free(lastUnmapKeys);
    lastUnmapKeys = NULL;
  }
  lastUnmapMode = -1;
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

MU_TEST(test_map_same_keys_multiple_modes)
{
  vimExecute("inoremap jj <Esc>");

  mu_check(mappingCallbackCount == 1);

  vimExecute("nnoremap jj <F1>");

  mu_check(mappingCallbackCount == 2);
  mu_check(lastMapping->m_mode == NORMAL);
  mu_check(strcmp("jj", lastMapping->m_orig_keys) == 0);
  mu_check(strcmp("<F1>", lastMapping->m_orig_str) == 0);
};

MU_TEST(test_sid_resolution)
{
  vimExecute("source collateral/map_plug_sid.vim");
  mu_check(mappingCallbackCount == 1);

  vimExecute("call <SNR>1_sayhello()");
};

MU_TEST(test_simple_unmap)
{
  vimExecute("imap jj <Esc>");

  mu_check(mappingCallbackCount == 1);

  vimExecute("iunmap jj");

  mu_check(unmappingCallbackCount == 1);
  mu_check(strcmp("jj", lastUnmapKeys) == 0);
};

MU_TEST(test_map_clear)
{
  //  vimExecute("inoremap jj <Esc>");
  //
  //  mu_check(mappingCallbackCount == 1);

  vimExecute("mapclear");

  mu_check(lastUnmapKeys == NULL);
  mu_check(unmappingCallbackCount == 1);
};

MU_TEST_SUITE(test_suite)
{
  MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

  MU_RUN_TEST(test_simple_mapping);
  MU_RUN_TEST(test_map_same_keys_multiple_modes);
  MU_RUN_TEST(test_lhs_termcode);
  MU_RUN_TEST(test_map_same_keys);
  MU_RUN_TEST(test_sid_resolution);
  MU_RUN_TEST(test_simple_unmap);
  MU_RUN_TEST(test_map_clear);
}

int main(int argc, char **argv)
{
  vimInit(argc, argv);

  vimSetInputMapCallback(&onMap);
  vimSetInputUnmapCallback(&onUnmap);
  vimSetMessageCallback(&onMessage);

  win_setwidth(5);
  win_setheight(100);

  vimBufferOpen("collateral/testfile.txt", 1, 0);

  MU_RUN_SUITE(test_suite);
  MU_REPORT();

  return minunit_status;
}
