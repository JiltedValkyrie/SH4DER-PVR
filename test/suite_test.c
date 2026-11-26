/* SH4DER-PVR: the suite disc's host test

   test/suite_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   The part of ../include/sh4der_suite.h that is not the console's: finding the one mail in a binary's image, the mark's
   text, and the two text helpers with no menu present. The load, arch_exec and the page timer run on a Dreamcast only
   and are not tested here.

   Build (from the repository's root):
     gcc -std=c99 -O2 -Wall -Wextra -Werror test/suite_test.c -o build/suite_test
*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../include/sh4der_suite.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void) {
    static uint32_t words[4096];
    uint8_t *img = (uint8_t *)words;
    const char mark[16] = SUITE_MARK;
    const char text[] = "PSM-SUITE-MAIL-1";                 /* 16 characters and the NUL: the first 16 are the mark */
    suite_mail_t *m;
    char rev[64];

    /* The mark: its bytes are the text the menu and every test match on, byte for byte. */
    CHECK(memcmp(mark, text, 16u) == 0);
    CHECK(memcmp((const void *)suite_mail.mark, text, 16u) == 0);
    CHECK(sizeof(suite_mail_t) == 32u);
    CHECK(suite_mail.index == SUITE_NO_INDEX && suite_mail.chain == 0u);

    /* none */
    CHECK(suite_mail_find(img, sizeof words) == NULL);

    /* one, at a multiple of 4: found, and its words are the image's own */
    memcpy(img + 1000u, mark, 16u);
    m = suite_mail_find(img, sizeof words);
    CHECK((uint8_t *)m == img + 1000u);
    if (m) { m->chain = 1u; m->index = 5u; m->status = SUITE_KEYS; CHECK(words[254] == 1u && words[255] == 5u && words[256] == SUITE_KEYS); }

    /* two: refused */
    memcpy(img + 8000u, mark, 16u);
    CHECK(suite_mail_find(img, sizeof words) == NULL);
    memset(img + 8000u, 0, 16u);

    /* a part of a mark is not one */
    memcpy(img + 8000u, mark, 15u);
    CHECK((uint8_t *)suite_mail_find(img, sizeof words) == img + 1000u);
    memset(img + 8000u, 0, 16u);

    /* a mark at an offset that is not a multiple of 4 is not found: the search steps by 4 */
    memset(img, 0, sizeof words);
    memcpy(img + 1002u, mark, 16u);
    CHECK(suite_mail_find(img, sizeof words) == NULL);
    memset(img, 0, sizeof words);
    memcpy(img + 1004u, mark, 16u);
    CHECK((uint8_t *)suite_mail_find(img, sizeof words) == img + 1004u);

    /* a mark with no room for its words at the end: not one */
    memset(img, 0, sizeof words);
    memcpy(img + sizeof words - 16u, mark, 16u);
    CHECK(suite_mail_find(img, sizeof words) == NULL);

    /* a mark at the very end of the image, with its words: found */
    memset(img, 0, sizeof words);
    memcpy(img + sizeof words - 32u, mark, 16u);
    CHECK((uint8_t *)suite_mail_find(img, sizeof words) == img + sizeof words - 32u);

    /* an image smaller than a mail: never found, and the size 32 is exactly one */
    memset(img, 0, sizeof words);
    memcpy(img, mark, 16u);
    CHECK(suite_mail_find(img, 16u) == NULL);
    CHECK(suite_mail_find(img, 31u) == NULL);
    CHECK(suite_mail_find(img, 0u) == NULL);
    CHECK((uint8_t *)suite_mail_find(img, 32u) == img);

    /* no menu on a host build: the helpers say so */
    CHECK(suite_present() == 0);
    CHECK(suite_chained() == 0);
    CHECK(strcmp(suite_keys_text("ABXY"), "ABXY") == 0);
    suite_rev_text(rev, sizeof rev);
    CHECK(strcmp(rev, "HOLLY REV ---- SB --") == 0);

    printf("suite_test: %s\n", fails ? "FAILED" : "ok");
    return fails ? 1 : 0;
}
