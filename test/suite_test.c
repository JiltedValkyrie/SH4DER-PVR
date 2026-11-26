/* SH4DER-PVR: the suite disc's host test

   demo/psm/suite/test/suite_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   The part of ../../psm_suite.h that is not the console's: finding the one mail in a binary's image. The rest (the
   load, arch_exec, the page timer) runs on a Dreamcast only and is not tested here.

   Build (from the repository's root):
     gcc -std=c99 -O2 -Wall -Wextra -Werror demo/psm/suite/test/suite_test.c -o build/psm_suite/suite_test
*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../psm_suite.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void) {
    static uint32_t words[4096];
    uint8_t *img = (uint8_t *)words;
    const char mark[16] = SUITE_MARK;
    suite_mail_t *m;

    CHECK(sizeof(suite_mail_t) == 32u);
    CHECK(suite_mail.index == SUITE_NO_INDEX && suite_mail.chain == 0u);
    CHECK(suite_mail_find(img, sizeof words) == NULL);                 /* none */
    memcpy(img + 1000u, mark, 16u);
    m = suite_mail_find(img, sizeof words);
    CHECK((uint8_t *)m == img + 1000u);                                /* one */
    if (m) { m->chain = 1u; m->index = 5u; m->status = SUITE_KEYS; CHECK(words[254] == 1u && words[255] == 5u && words[256] == SUITE_KEYS); }
    memcpy(img + 8000u, mark, 16u);
    CHECK(suite_mail_find(img, sizeof words) == NULL);                 /* two: refused */
    memset(img + 8000u, 0, 16u);
    memcpy(img + 8000u, mark, 15u);
    CHECK((uint8_t *)suite_mail_find(img, sizeof words) == img + 1000u);   /* a part of a mark is not one */
    memset(img, 0, sizeof words);
    memcpy(img + sizeof words - 16u, mark, 16u);
    CHECK(suite_mail_find(img, sizeof words) == NULL);                 /* a mark with no room for its words */
    memcpy(img + sizeof words - 32u, mark, 16u);
    memset(img + sizeof words - 16u, 0, 16u);
    CHECK((uint8_t *)suite_mail_find(img, sizeof words) == img + sizeof words - 32u);
    CHECK(suite_mail_find(img, 16u) == NULL);
    printf("suite_test: %s\n", fails ? "FAILED" : "ok");
    return fails ? 1 : 0;
}
