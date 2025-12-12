#include <stdio.h>

/* Test runners */
extern int run_bcache_tests(void);
extern int run_flexcache_core_tests(void);
extern int run_flexcache_ttl_tests(void);
/* extern int run_flexcache_eviction_tests(void); */

int
main(void)
{
    int failed = 0;

    printf("========================================\n");
    printf("       FlexCache Unit Tests\n");
    printf("========================================\n");

    failed += run_bcache_tests();
    failed += run_flexcache_core_tests();
    failed += run_flexcache_ttl_tests();
    /* failed += run_flexcache_eviction_tests(); */

    printf("\n========================================\n");
    if (failed == 0) {
        printf("  All tests PASSED\n");
    } else {
        printf("  %d test(s) FAILED\n", failed);
    }
    printf("========================================\n");

    return failed ? 1 : 0;
}