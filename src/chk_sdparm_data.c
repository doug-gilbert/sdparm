/*
 * Copyright (c) 2005 Douglas Gilbert.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Version 1.0 20050906
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sdparm.h"

/*
 * This is a maintenance program for checking the integrity of
 * data in the sdparm_data.c tables.
 *
 * Build with a line like:
 *      gcc -Wall -o chk_sdparm_data sdparm_data.o chk_sdparm_data.c
 *
 */

void check(const struct sdparm_mode_page_item * mpi)
{
    unsigned char u[4096];
    unsigned char * up;
    unsigned char mask;
    const struct sdparm_mode_page_item * kp = mpi;
    const struct sdparm_mode_page_item * jp = mpi;
    const char * cp;
    int prev_mp, prev_msp, prev_pdt, sbyte, sbit, nbits;

    memset(u, 0, sizeof(u));
    for (prev_mp = 0, prev_msp = 0, prev_pdt = -1; kp->acron; ++kp) {
        cp = kp->acron;
        if ((prev_mp != kp->page_num) || (prev_msp != kp->subpage_num) ||
            ((prev_pdt >= 0) && (prev_pdt != kp->pdt))) {
            if (prev_mp > kp->page_num)
                printf("  mode page 0x%x,0x%x out of order\n", kp->page_num,
                        kp->subpage_num);
            if ((prev_mp == kp->page_num) && (prev_msp > kp->subpage_num))
                printf("  mode subpage 0x%x,0x%x out of order, smp was "
                       "0x%x\n", kp->page_num, kp->subpage_num, prev_msp);
            if ((prev_mp == kp->page_num) && (prev_msp == kp->subpage_num) &&
                (prev_pdt > kp->pdt))
                printf("  mode page 0x%x,0x%x pdt out of order, pdt was "
                       "%d, now %d\n", kp->page_num, kp->subpage_num,
                       prev_pdt, kp->pdt);
            prev_mp = kp->page_num;
            prev_msp = kp->subpage_num;
            prev_pdt = kp->pdt;
            memset(u, 0, sizeof(u));
        }
        for (jp = kp + 1; jp->acron; ++jp) {
            if (0 == strcmp(cp, jp->acron))
                printf("  acronym with this description: %s clashes with "
                       "%s\n", kp->description, jp->description);
        }
        sbyte = kp->start_byte;
        if ((unsigned)sbyte + 8 > sizeof(u)) {
            printf("  acronym: %s  start byte too large: %d\n", kp->acron,
                   sbyte);
            continue;
        }
        sbit = kp->start_bit;
        if ((unsigned)sbit > 7) {
            printf("  acronym: %s  start bit too large: %d\n", kp->acron,
                   sbit);
            continue;
        }
        nbits = kp->num_bits;
        if (nbits > 64) {
            printf("  acronym: %s  number of bits too large: %d\n",
                   kp->acron, nbits);
            continue;
        }
        if (nbits < 1) {
            printf("  acronym: %s  number of bits too small: %d\n",
                   kp->acron, nbits);
            continue;
        }
        up = u + sbyte;
        mask = (1 << (sbit + 1)) - 1;
        if ((nbits - 1) < sbit)
            mask &= ~((1 << (sbit + 1 - nbits)) - 1);
        if (*up & mask)
            printf("  0x%x,0x%x: clash as start_byte: %d, bit: %d "
                   "[acron: %s]\n", prev_mp, prev_msp, sbyte, sbit,
                   kp->acron ? kp->acron : "?");
        *up |= mask;
        if ((nbits - 1) > sbit) {
            nbits -= (sbit + 1);
            if ((nbits > 7) && (0 != (nbits % 8)))
                printf("  0x%x,0x%x: check nbits: %d, start_byte: %d, bit: "
                       "%d [acron: %s]\n", prev_mp, prev_msp, kp->num_bits,
                       sbyte, sbit, kp->acron ? kp->acron : "?");
            do {
                ++up;
                ++sbyte;
                mask = 0xff;
                if (nbits > 7)
                    nbits -= 8;
                else {
                    mask &= ~((1 << (8 - nbits)) - 1);
                    nbits = 0;
                }
                if (*up & mask)
                    printf("  0x%x,0x%x: clash as start_byte: %d, bit: %d "
                           "[acron: %s]\n", prev_mp, prev_msp, sbyte, sbit,
                           kp->acron ? kp->acron : "?");
                *up |= mask;
            } while (nbits > 0);
        }
    }

}

int main(int argc, char ** argv)
{
    int k;
    const struct sdparm_transport_pair * tp;

    printf("Check integrity of mode page item tables in sdparm\n");
    printf("Generic (i.e. non-transport specific) mode page items:\n");
    check(sdparm_mitem_arr);
    printf("\n");
    tp = sdparm_transport_mp;
    for (k = 0; k < 16; ++k, ++tp) {
        if (tp->mitem) {
            printf("%s mode page items:\n", sdparm_transport_id[k].name);
            check(tp->mitem);
            printf("\n");
        }
    }
    return 0;
}
