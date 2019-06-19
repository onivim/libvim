#include "libvim.h"
#include "minunit.h"


MU_TEST(validate_memory_test_leak) {
    vim_mem_profile_reset();
    char_u* test = (char_u*)alloc(100 * sizeof(char_u));
    mu_check(vim_mem_profile_dump() > 0);
}

MU_TEST(validate_memory_test_noleak) {
    vim_mem_profile_reset();
    char_u* test = (char_u*)alloc(100 * sizeof(char_u));
    vim_free(test);
    mu_check(vim_mem_profile_dump() == 0);
}

MU_TEST_SUITE(test_suite) {
  MU_RUN_TEST(validate_memory_test_leak);
  MU_RUN_TEST(validate_memory_test_noleak);
}

int main(int argc, char **argv) {
  MU_RUN_SUITE(test_suite);
  MU_REPORT();
  MU_RETURN();
}
