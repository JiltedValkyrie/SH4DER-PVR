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

/* The page loop's decisions (suite_pager_step): a clock of the test's own, 20 ms a pass as the tests' loops. */
static void pager(void) {
    suite_pager_t p;
    unsigned page, pm = 0, t, fired, holds, was;
    int ev;

    /* a button down on the first pass does nothing until it is let go */
    memset(&p, 0, sizeof p); page = 0;
    CHECK(suite_pager_step(&p, CONT_A, 0u, &page, 4u, 1, 0, &pm) == SUITE_PG_NONE && page == 0u);
    CHECK(suite_pager_step(&p, CONT_A, 20u, &page, 4u, 1, 0, &pm) == SUITE_PG_NONE && page == 0u);
    CHECK(suite_pager_step(&p, 0u, 40u, &page, 4u, 1, 0, &pm) == SUITE_PG_NONE);

    /* A: the next page at the press, up to the last; B: the page before, round from the first */
    CHECK(suite_pager_step(&p, CONT_A, 60u, &page, 4u, 1, 0, &pm) == SUITE_PG_TURN && page == 1u);
    CHECK(suite_pager_step(&p, CONT_A, 80u, &page, 4u, 1, 0, &pm) == SUITE_PG_NONE && page == 1u);     /* held: one turn a press */
    CHECK(suite_pager_step(&p, 0u, 100u, &page, 4u, 1, 0, &pm) == SUITE_PG_NONE);
    CHECK(suite_pager_step(&p, CONT_B, 120u, &page, 4u, 1, 0, &pm) == SUITE_PG_TURN && page == 0u);
    CHECK(suite_pager_step(&p, 0u, 140u, &page, 4u, 1, 0, &pm) == SUITE_PG_NONE);
    CHECK(suite_pager_step(&p, CONT_B, 160u, &page, 4u, 1, 0, &pm) == SUITE_PG_TURN && page == 3u);     /* the last page */
    CHECK(suite_pager_step(&p, 0u, 180u, &page, 4u, 1, 0, &pm) == SUITE_PG_NONE);

    /* the last page, a tap of A: HOLD while it is down, the first page at its release */
    CHECK(suite_pager_step(&p, CONT_A, 200u, &page, 4u, 1, 0, &pm) == SUITE_PG_HOLD && page == 3u && pm == 0u);
    CHECK(suite_pager_step(&p, CONT_A, 300u, &page, 4u, 1, 0, &pm) == SUITE_PG_HOLD && pm == 50u);
    CHECK(suite_pager_step(&p, 0u, 320u, &page, 4u, 1, 0, &pm) == SUITE_PG_TURN && page == 0u);
    CHECK(suite_pager_step(&p, 0u, 340u, &page, 4u, 1, 0, &pm) == SUITE_PG_NONE);

    /* A held from the page before: on the last page it is not a hold until it is let go and pressed again */
    page = 2u;
    CHECK(suite_pager_step(&p, 0u, 400u, &page, 4u, 1, 0, &pm) == SUITE_PG_NONE);
    CHECK(suite_pager_step(&p, CONT_A, 420u, &page, 4u, 1, 0, &pm) == SUITE_PG_TURN && page == 3u);
    for (t = 440u; t < 3000u; t += 20u) CHECK(suite_pager_step(&p, CONT_A, t, &page, 4u, 1, 0, &pm) == SUITE_PG_NONE && page == 3u);
    CHECK(suite_pager_step(&p, 0u, 3000u, &page, 4u, 1, 0, &pm) == SUITE_PG_NONE && page == 3u);       /* its release is not a tap */

    /* the last page, A held: HOLD with the progress rising, LEAVE_HOLD once at 2 s, the page not turned */
    fired = 0; holds = 0; was = 0;
    for (t = 0; t <= SH4DER_MENU_HOLD_MS; t += 20u) {
        ev = suite_pager_step(&p, CONT_A, 4000u + t, &page, 4u, 1, 0, &pm);
        if (ev == SUITE_PG_HOLD) { holds++; CHECK(pm >= was && pm < 1000u); was = pm; }
        if (ev == SUITE_PG_LEAVE_HOLD) { fired++; CHECK(t == SH4DER_MENU_HOLD_MS); }
    }
    CHECK(fired == 1u && holds == SH4DER_MENU_HOLD_MS / 20u && page == 3u);

    /* let go at 1.9 s: stays (a tap: the first page), nothing leaves */
    memset(&p, 0, sizeof p); page = 3u;
    suite_pager_step(&p, 0u, 0u, &page, 4u, 1, 0, &pm);
    for (t = 20u; t <= 1900u; t += 20u) CHECK(suite_pager_step(&p, CONT_A, t, &page, 4u, 1, 0, &pm) == SUITE_PG_HOLD);
    CHECK(suite_pager_step(&p, 0u, 1920u, &page, 4u, 1, 0, &pm) == SUITE_PG_TURN && page == 0u);

    /* one page: it is the last; a tap draws it again, a hold leaves */
    memset(&p, 0, sizeof p); page = 0u;
    suite_pager_step(&p, 0u, 0u, &page, 1u, 1, 0, &pm);
    CHECK(suite_pager_step(&p, CONT_A, 20u, &page, 1u, 1, 0, &pm) == SUITE_PG_HOLD);
    CHECK(suite_pager_step(&p, 0u, 40u, &page, 1u, 1, 0, &pm) == SUITE_PG_TURN && page == 0u);
    CHECK(suite_pager_step(&p, CONT_A, 60u, &page, 1u, 1, 0, &pm) == SUITE_PG_HOLD);
    CHECK(suite_pager_step(&p, CONT_A, 60u + SH4DER_MENU_HOLD_MS, &page, 1u, 1, 0, &pm) == SUITE_PG_LEAVE_HOLD);
    CHECK(suite_pager_step(&p, CONT_B, 5000u, &page, 0u, 1, 0, &pm) == SUITE_PG_TURN && page == 0u);    /* 0 pages is read as 1 */

    /* no menu (a disc of the test's own): A and B turn the pages, the last page turns at the press, nothing leaves */
    memset(&p, 0, sizeof p); page = 3u;
    suite_pager_step(&p, 0u, 0u, &page, 4u, 0, 0, &pm);
    CHECK(suite_pager_step(&p, CONT_A, 20u, &page, 4u, 0, 0, &pm) == SUITE_PG_TURN && page == 0u);
    CHECK(suite_pager_step(&p, SUITE_KEYS_ALL, 40u, &page, 4u, 0, 0, &pm) != SUITE_PG_LEAVE_KEYS);
    page = 3u;
    for (t = 60u; t < 6000u; t += 20u) { ev = suite_pager_step(&p, CONT_A, t, &page, 4u, 0, 0, &pm); CHECK(ev != SUITE_PG_LEAVE_HOLD && ev != SUITE_PG_HOLD); }

    /* the four buttons leave from any page, before anything else */
    memset(&p, 0, sizeof p); page = 1u;
    suite_pager_step(&p, 0u, 0u, &page, 4u, 1, 0, &pm);
    CHECK(suite_pager_step(&p, SUITE_KEYS_ALL, 20u, &page, 4u, 1, 0, &pm) == SUITE_PG_LEAVE_KEYS && page == 1u);

    /* a chained run: a page a SUITE_PAGE_MS, then the menu after the last; A on the last page is a plain next page */
    memset(&p, 0, sizeof p); page = 0u;
    suite_pager_step(&p, 0u, 0u, &page, 2u, 1, 1, &pm);
    CHECK(suite_pager_step(&p, 0u, SUITE_PAGE_MS - 1u, &page, 2u, 1, 1, &pm) == SUITE_PG_NONE && page == 0u);
    CHECK(suite_pager_step(&p, 0u, SUITE_PAGE_MS, &page, 2u, 1, 1, &pm) == SUITE_PG_TURN && page == 1u);
    CHECK(suite_pager_step(&p, 0u, 2u * SUITE_PAGE_MS - 1u, &page, 2u, 1, 1, &pm) == SUITE_PG_NONE);
    CHECK(suite_pager_step(&p, CONT_A, 2u * SUITE_PAGE_MS - 1u, &page, 2u, 1, 1, &pm) == SUITE_PG_TURN && page == 0u);
    page = 1u;
    CHECK(suite_pager_step(&p, 0u, 2u * SUITE_PAGE_MS, &page, 2u, 1, 1, &pm) == SUITE_PG_NONE);          /* the page's time starts again */
    CHECK(suite_pager_step(&p, 0u, 3u * SUITE_PAGE_MS, &page, 2u, 1, 1, &pm) == SUITE_PG_LEAVE_CHAIN && page == 1u);

    /* the test turned the page itself: the page's time starts again, a hold in progress is dropped */
    memset(&p, 0, sizeof p); page = 3u;
    suite_pager_step(&p, 0u, 0u, &page, 4u, 1, 0, &pm);
    CHECK(suite_pager_step(&p, CONT_A, 20u, &page, 4u, 1, 0, &pm) == SUITE_PG_HOLD);
    page = 1u;
    CHECK(suite_pager_step(&p, CONT_A, 40u, &page, 4u, 1, 0, &pm) == SUITE_PG_NONE && page == 1u);
    page = 3u;
    for (t = 60u; t < 4000u; t += 20u) CHECK(suite_pager_step(&p, CONT_A, t, &page, 4u, 1, 0, &pm) == SUITE_PG_NONE);

    /* the mail's spare word is the place's size */
    CHECK(sizeof suite_mail.spare == sizeof(uint32_t) && sizeof(sh4der_menu_pack(&(sh4der_menu_t){ 0 })) == sizeof(uint32_t));
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
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

    pager();
    printf("suite_test: %s\n", fails ? "FAILED" : "ok");
    return fails ? 1 : 0;
}
