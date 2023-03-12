/*
 * Copyright (c) 2005-2023, Douglas Gilbert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sdparm.h"
#include "sg_lib.h"
#include "sg_unaligned.h"
#include "sg_pr2serr.h"

/* sdparm_access.c : helpers for sdparm to access tables in
 * sdparm_data.c
 */


/* Returns 1 if strings equal (same length, characters same or only differ
 * by case), else returns 0. Assumes 7 bit ASCII (English alphabet). */
int
sdp_strcase_eq(const char * s1p, const char * s2p)
{
    int c1, c2;

    do {
        c1 = *s1p++;
        c2 = *s2p++;
        if (c1 != c2) {
            if (c2 >= 'a')
                c2 = toupper(c2);
            else if (c1 >= 'a')
                c1 = toupper(c1);
            else
                return 0;
            if (c1 != c2)
                return 0;
        }
    } while (c1);
    return 1;
}

/* Returns 1 if strings equal up to the nth character (characters same or only
 * differ by case), else returns 0. Assumes 7 bit ASCII (English alphabet). */
int
sdp_strcase_eq_upto(const char * s1p, const char * s2p, int n)
{
    int k, c1, c2;

    for (k = 0; k < n; ++k) {
        c1 = *s1p++;
        c2 = *s2p++;
        if (c1 != c2) {
            if (c2 >= 'a')
                c2 = toupper(c2);
            else if (c1 >= 'a')
                c1 = toupper(c1);
            else
                return 0;
            if (c1 != c2)
                return 0;
        }
        if (0 == c1)
            break;
    }
    return 1;
}

/* Returns length of mode page. Assumes mp pointing at start of a mode
 * page (not the start of a MODE SENSE response). */
int
sdp_mpage_len(const uint8_t * mp)
{
    /* if SPF (byte 0, bit 6) is set then 4 byte header, else 2 byte header */
    return (mp[0] & 0x40) ? (sg_get_unaligned_be16(mp + 2) + 4) : (mp[1] + 2);
}

enum mode_page_class {MPC_VENDOR, MPC_TRANSPORT, MPC_GENERAL};

const struct sdparm_mp_name_t *
sdp_get_mp_nm(int page_num, int subpage_num, int pdt, int transp_proto,
              int vendor_id)
{
    enum mode_page_class mpc;
    int decayed_pdt;
    const struct sdparm_mp_name_t * mnp;

    if (vendor_id >= 0) {
        const struct sdparm_vendor_pair * vpp;

        vpp = sdp_get_vendor_pair(vendor_id);
        mnp = (vpp ? vpp->mpage : NULL);
        mpc = MPC_VENDOR;
    } else if ((transp_proto >= 0) && (transp_proto < 16)) {
        mnp = sdparm_transport_mp[transp_proto].mpage;
        mpc = MPC_TRANSPORT;
    } else {
        mnp = sdparm_gen_mode_pg;
        mpc = MPC_GENERAL;
    }
    if (NULL == mnp)
        return NULL;

try_again:
    for ( ; mnp->mp_acron; ++mnp) {
        if ((page_num == mnp->page) && (subpage_num == mnp->subpage)) {
            if ((pdt < 0) || (mnp->com_pdt < 0) ||
                sg_pdt_s_eq(mnp->com_pdt, pdt))
                return mnp;
        }
    }
    if (MPC_GENERAL == mpc) {
        decayed_pdt = sg_lib_pdt_decay(pdt);
        if (decayed_pdt != pdt) {
            pdt = decayed_pdt;
            goto try_again;
        }
    }
    return NULL;
}

/* Returns pointer to a sdparm_mp_name_t object matching the first 5
 * arguments. If no match returns NULL. If match and bp is non-NULL
 * then outputs a the name, acronym (if plus_acron) and hex values
 * (if 'hex' == true) to bp . */
const struct sdparm_mp_name_t *
sdp_get_mp_nm_with_str(int page_num, int subpage_num, int pdt, int transp_proto,
                       int vendor_id, bool plus_acron, bool hex, int b_len,
                       char * bp)
{
    int len = b_len - 1;
    const struct sdparm_mp_name_t * mnp = NULL;
    const char * cp;

    /* first try to match given pdt */
    mnp = sdp_get_mp_nm(page_num, subpage_num, pdt, transp_proto, vendor_id);
    if (NULL == mnp) /* didn't match specific pdt so try -1 (ie. SPC) */
        mnp = sdp_get_mp_nm(page_num, subpage_num, -1, transp_proto,
                            vendor_id);
    if ((len < 0) || (NULL == bp))
        return mnp;
    bp[len] = '\0';
    if (mnp && mnp->mp_name) {
        cp = mnp->mp_acron;
        if (NULL == cp)
            cp = "";
        if (hex) {
            if (0 == subpage_num) {
                if (plus_acron)
                    snprintf(bp, len, "%s [%s: 0x%x]", mnp->mp_name, cp,
                             page_num);
                else
                    snprintf(bp, len, "%s [0x%x]", mnp->mp_name, page_num);
            } else {
                if (plus_acron)
                    snprintf(bp, len, "%s [%s: 0x%x,0x%x]", mnp->mp_name, cp,
                             page_num, subpage_num);
                else
                    snprintf(bp, len, "%s [0x%x,0x%x]", mnp->mp_name,
                             page_num, subpage_num);
            }
        } else {
            if (plus_acron)
                snprintf(bp, len, "%s [%s]", mnp->mp_name, cp);
            else
                snprintf(bp, len, "%s", mnp->mp_name);
        }
    } else {
        if (0 == subpage_num)
            snprintf(bp, len, "[0x%x]", page_num);
        else
            snprintf(bp, len, "[0x%x,0x%x]", page_num, subpage_num);
    }
    return mnp;
}

const struct sdparm_mp_name_t *
sdp_find_mp_nm_by_acron(const char * ap, int transp_proto, int vendor_id)
{
    const struct sdparm_mp_name_t * mnp;

    if (vendor_id >= 0) {
        const struct sdparm_vendor_pair * vpp;

        vpp = sdp_get_vendor_pair(vendor_id);
        mnp = (vpp ? vpp->mpage : NULL);
    } else if ((transp_proto >= 0) && (transp_proto < 16))
        mnp = sdparm_transport_mp[transp_proto].mpage;
    else
        mnp = sdparm_gen_mode_pg;
    if (NULL == mnp)
        return NULL;

    for ( ; mnp->mp_acron; ++mnp) {
        if (sdp_strcase_eq(mnp->mp_acron, ap))
            return mnp;
    }
    return NULL;
}

const struct sdparm_vpd_page_t *
sdp_get_vpd_detail(int page_num, int subvalue, int pdt)
{
    const struct sdparm_vpd_page_t * vpp;
    int sv, ty;

    sv = (subvalue < 0) ? 1 : 0;
    ty = (pdt < 0) ? 1 : 0;
    for (vpp = sdparm_vpd_pg; vpp->vpd_acron; ++vpp) {
        if ((page_num == vpp->vpd_num) &&
            (sv || (subvalue == vpp->subvalue)) &&
            (ty || (pdt == vpp->com_pdt)))
            return vpp;
    }
    if (! ty)
        return sdp_get_vpd_detail(page_num, subvalue, -1);
    if (! sv)
        return sdp_get_vpd_detail(page_num, -1, -1);
    return NULL;
}

const struct sdparm_vpd_page_t *
sdp_find_vpd_by_acron(const char * ap)
{
    const struct sdparm_vpd_page_t * vpp;

    for (vpp = sdparm_vpd_pg; vpp->vpd_acron; ++vpp) {
        if (sdp_strcase_eq(vpp->vpd_acron, ap))
            return vpp;
    }
    return NULL;
}

char *
sdp_get_transport_name(int proto_num, int b_len, char * b)
{
    char d[128];

    if (NULL == b)
        ;
    else if (b_len < 2) {
        if (1 == b_len)
            b[0] = '\0';
    } else
        snprintf(b, b_len, "%s", sg_get_trans_proto_str(proto_num, sizeof(d),
                 d));
    return b;
}

int
sdp_find_transport_id_by_acron(const char * ap)
{
    const struct sdparm_val_desc_t * t_vdp;
    const struct sdparm_val_desc_t * t_addp;

    for (t_vdp = sdparm_transport_id; t_vdp->desc; ++t_vdp) {
        if (sdp_strcase_eq(t_vdp->desc, ap))
            return t_vdp->val;
    }
    /* No match ... try additional transport acronyms */
    for (t_addp = sdparm_add_transport_acron; t_addp->desc; ++t_addp) {
        if (sdp_strcase_eq(t_addp->desc, ap))
            return t_addp->val;
    }
    return -1;
}

const char *
sdp_get_vendor_name(int vendor_id)
{
    const struct sdparm_vendor_name_t * vnp;

    for (vnp = sdparm_vendor_id; vnp->acron; ++vnp) {
        if (vendor_id == vnp->vendor_id)
            return vnp->name;
    }
    return NULL;
}

const struct sdparm_vendor_name_t *
sdp_find_vendor_by_acron(const char * ap)
{
    const struct sdparm_vendor_name_t * vnp;

    for (vnp = sdparm_vendor_id; vnp->acron; ++vnp) {
        if (sdp_strcase_eq_upto(vnp->acron, ap, strlen(vnp->acron)))
            return vnp;
    }
    return NULL;
}

const struct sdparm_vendor_pair *
sdp_get_vendor_pair(int vendor_id)
{
     return ((vendor_id >= 0) && (vendor_id < sdparm_vendor_mp_len))
            ? (sdparm_vendor_mp + vendor_id) : NULL;
}

/* Searches mpage items table from (and including) the current position
 * looking for the first match on 'ap' (pointer to acromym). Checks
 * against the inbuilt table (in sdparm_data.c) of generic (when both
 * transp_proto and vendor_id are -1), transport (when transp_proto is
 * >= 0) or vendor (when vendor_id is >= 0) mode page items (fields).
 * If found a pointer to that mitem is returned and *from_p is set to
 * the offset after the match. If not found then NULL is returned and
 * *from_p is set to the offset of the sentinel at the end of the
 * selected mitem array. Start iteration by setting from_p to NULL or
 * point it at -1. */
const struct sdparm_mp_item_t *
sdp_find_mitem_by_acron(const char * ap, int * from_p, int transp_proto,
                        int vendor_id)
{
    int k = 0;
    const struct sdparm_mp_item_t * mpi;

    if (from_p) {
        k = *from_p;
        if (k < 0)
            k = 0;
    }
    if (vendor_id >= 0) {
        const struct sdparm_vendor_pair * vpp;

        vpp = sdp_get_vendor_pair(vendor_id);
        mpi = (vpp ? vpp->mitem : NULL);
    } else if ((transp_proto >= 0) && (transp_proto < 16))
        mpi = sdparm_transport_mp[transp_proto].mitem;
    else
        mpi = sdparm_mitem_arr;
    if (NULL == mpi)
        return NULL;

    for (mpi += k; mpi->acron; ++k, ++mpi) {
        if (sdp_strcase_eq(mpi->acron, ap))
            break;
    }
    if (NULL == mpi->acron)
        mpi = NULL;
    if (from_p)
        *from_p = (mpi ? (k + 1) : k);
    return mpi;
}

uint64_t
sdp_mitem_get_value(const struct sdparm_mp_item_t *mpi,
                    const uint8_t * mp)
{
    return sg_get_big_endian(mp + mpi->start_byte, mpi->start_bit,
                              mpi->num_bits);
}

/* Gets a mode page item's value given a pointer to the mode page response
 * (mp). If all_setp is non-NULL then checks 8, 16, 24, 32, 48 and 64 bit
 * quantities for all bits set (e.g. for 8 bits that would be 0xff) and if
 * so sets the bool addressed by all_setp to true. Otherwise if all_setp
 * is non-NULL then it sets the bool addressed by all_setp to false.
 * Returns the value in an unsigned 64 bit integer. To print a value as a
 * signed quantity use sdp_print_signed_decimal(). */
uint64_t
sdp_mitem_get_value_check(const struct sdparm_mp_item_t *mpi,
                          const uint8_t * mp, bool * all_setp)
{
    uint64_t res;

    res = sg_get_big_endian(mp + mpi->start_byte, mpi->start_bit,
                             mpi->num_bits);
    if (all_setp) {
        switch (mpi->num_bits) {
        case 8:
            if (0xff == res)
                *all_setp = true;
            break;
        case 16:
            if (0xffff == res)
                *all_setp = true;
            break;
        case 24:
            if (0xffffff == res)
                *all_setp = true;
            break;
        case 32:
            if (0xffffffff == res)
                *all_setp = true;
            break;
        case 48:
            if (0xffffffffffffULL == res)
                *all_setp = true;
            break;
        case 64:
            if (0xffffffffffffffffULL == res)
                *all_setp = true;
            break;
        default:
            *all_setp = false;
            break;
        }
    }
    return res;
}

char *
sdp_signed_decimal_str(uint64_t u, int num_bits, bool leading_zeros,
                       char * b, int blen)
{
    unsigned int ui;
    uint8_t uc;

    switch (num_bits) {
    /* could support other num_bits, as required */
    case 4:     /* -8 to 7 */
        uc = u & 0xf;
        if (0x8 & uc)
            uc |= 0xf0;         /* sign extend */
        if (leading_zeros)
            sg_scnpr(b, blen, "%02hhd", (signed char)uc);
        else
            sg_scnpr(b, blen, "%hhd", (signed char)uc);
        break;
    case 8:     /* -128 to 127 */
        if (leading_zeros)
            sg_scnpr(b, blen, "%02hhd", (signed char)u);
        else
            sg_scnpr(b, blen, "%hhd", (signed char)u);
        break;
    case 16:    /* -32768 to 32767 */
        if (leading_zeros)
            sg_scnpr(b, blen, "%02hd", (short int)u);
        else
            sg_scnpr(b, blen, "%hd", (short int)u);
        break;
    case 24:
        ui = 0xffffff & u;
        if (0x800000 & ui)
            ui |= 0xff000000;
        if (leading_zeros)
            sg_scnpr(b, blen, "%02d", (int)ui);
        else
            sg_scnpr(b, blen, "%d", (int)ui);
        break;
    case 32:
        if (leading_zeros)
            sg_scnpr(b, blen, "%02d", (int)u);
        else
            sg_scnpr(b, blen, "%d", (int)u);
        break;
    case 64:
    default:
        if (leading_zeros)
            sg_scnpr(b, blen, "%02" PRId64 , (int64_t)u);
        else
            sg_scnpr(b, blen, "%" PRId64 , (int64_t)u);
        break;
    }
    return b;
}

/* Place 'val' at an offset to 'mp' as indicated by mpi. */
void
sdp_mitem_set_value(uint64_t val, const struct sdparm_mp_item_t * mpi,
                    uint8_t * mp)
{
    sg_set_big_endian(val, mp + mpi->start_byte, mpi->start_bit,
                      mpi->num_bits);
}

int
sdp_get_desc_id(int flags)
{
    return (MF_DESC_ID_MASK & flags) >> MF_DESC_ID_SHIFT;
}

char *
sdp_mp_convert2snake(const char * in_name, char * sn_name,
                     int max_sn_name_len)
{
    int inlen, k;
    const char * lpar;
    const char * rpar;
    const char * lbra;
    const char * rbra;
    char b[168];
    static const int blen = sizeof(b);
    static const char * s_mp_s = " mode page";
    static const char * dummy_in_s = "null mode page";

    inlen = strlen(in_name ? in_name : dummy_in_s);
    if (inlen >= (blen - 1)) {
        pr2serr("%s: buffer too short\n", __func__);
        return sn_name;
    }
    memcpy(b, in_name ? in_name : dummy_in_s,
           inlen + 1);  /* want trailing null */
    lpar = strchr(b, '(');
    rpar = strchr(b, ')');
    if (lpar && rpar) {         /* remove parentheses */
        memmove((char *)lpar, rpar + 1, rpar + 1 - lpar);
        inlen = strlen(b);
    }
    lbra = strchr(b, '[');
    rbra = strchr(b, ']');
    if (lbra && rbra) {         /* remove parentheses */
        memmove((char *)lbra, rbra + 1, rbra + 1 - lbra);
        inlen = strlen(b);
    }
    for (k = inlen; k < blen; ++k) {
        b[k] = s_mp_s[k - inlen];
        if ('\0' == b[k])
            break;
    }
    if (k == blen)
        b[k - 1] = '\0';
    return sgj_convert2snake(b, sn_name, max_sn_name_len);
}
