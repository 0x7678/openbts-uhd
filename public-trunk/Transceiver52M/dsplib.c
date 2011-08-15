/*
 * Copyright (C) 2011 Thomas Tsou <ttsou@vt.edu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 * See the COPYING file in the main directory for details.
 */

#include <dsplib.h>
#include <string.h>
#include <stdio.h>

#define ASM

/* Convolve vector lengths
 *      26 / 16 / C-C
 *      36 / 16 / C-C
 *      36 / 21 / C-R
 *      41 / 41 / C-C
 *     156 /  3 / C-R
 *     156 /  7 / C-C
 *     156 / 21 / C-R
 *     157 /  3 / C-R
 *     157 / 41 / C-C
 */

#ifndef ASM
void mac_real(cmplx_float *a, cmplx_float *b, cmplx_float *c)
{
    c->i += a->i * b->i;
    c->q += a->q * b->i;
}

void mac_cmplx(cmplx_float *a, cmplx_float *b, cmplx_float *c)
{
    c->i += a->i * b->i - a->q * b->q;
    c->q += a->i * b->q + a->q * b->i;
}

void mac_real_vec_3(cmplx_float *a, cmplx_float *b, cmplx_float *c)
{
    mac_real(&a[0], &b[2], c);
    mac_real(&a[1], &b[1], c);
    mac_real(&a[2], &b[0], c);
}

void mac_real_vec_4(cmplx_float *a, cmplx_float *b, cmplx_float *c)
{
    mac_real(&a[0], &b[3], c);
    mac_real(&a[1], &b[2], c);
    mac_real(&a[2], &b[1], c);
    mac_real(&a[3], &b[0], c);
}
#endif

void mac_cmplx_vec_1(cmplx_float *a, cmplx_float *b, cmplx_float *c)
{
    mac_cmplx(a, b, c);
}

void mac_real_vec_1(cmplx_float *a, cmplx_float *b, cmplx_float *c)
{
    mac_real(a, b, c);
}

void mac_cmplx_vec_4(cmplx_float *a, cmplx_float *b, cmplx_float *c)
{
    mac_cmplx(&a[0], &b[3], c);
    mac_cmplx(&a[1], &b[2], c);
    mac_cmplx(&a[2], &b[1], c);
    mac_cmplx(&a[3], &b[0], c);
}

void mac_cmplx_vec_7(cmplx_float *a, cmplx_float *b, cmplx_float *c)
{
    mac_cmplx_vec_4(&a[0], &b[3], c);
    mac_cmplx_vec_1(&a[4], &b[2], c);
    mac_cmplx_vec_1(&a[5], &b[1], c);
    mac_cmplx_vec_1(&a[6], &b[0], c);
}

void mac_cmplx_vec_16(cmplx_float *a, cmplx_float *b, cmplx_float *c)
{
    mac_cmplx_vec_4(&a[0],  &b[12], c);
    mac_cmplx_vec_4(&a[4],  &b[8],  c);
    mac_cmplx_vec_4(&a[8],  &b[4],  c);
    mac_cmplx_vec_4(&a[12], &b[0],  c);
}

void mac_real_vec_16(cmplx_float *a, cmplx_float *b, cmplx_float *c)
{
    mac_real_vec_4(&a[0],  &b[12], c);
    mac_real_vec_4(&a[4],  &b[8],  c);
    mac_real_vec_4(&a[8],  &b[4],  c);
    mac_real_vec_4(&a[12], &b[0],  c);
}

void mac_real_vec_21(cmplx_float *a, cmplx_float *b, cmplx_float *c)
{
    mac_real_vec_16(&a[0],  &b[5], c);
    mac_real_vec_4 (&a[16], &b[1], c);
    mac_real_vec_1 (&a[20], &b[0], c);
}

void mac_cmplx_vec_41(cmplx_float *a, cmplx_float *b, cmplx_float *c)
{
    mac_cmplx_vec_16(&a[0],  &b[25], c);
    mac_cmplx_vec_16(&a[16], &b[9],  c);
    mac_cmplx_vec_4 (&a[32], &b[5],  c);
    mac_cmplx_vec_4 (&a[36], &b[1],  c);
    mac_cmplx_vec_1 (&a[40], &b[0],  c);
}
          
int convlv_nosym(struct float_vec *a_vec, struct float_vec *b_vec,
                 struct float_vec *c_vec, int start_idx)
{
    int i, n, mid_idx, end_idx;
    cmplx_float *a, *b, *c;
    void (*mac_func)(cmplx_float *, cmplx_float *, cmplx_float *);
    void (*mac_vec_func)(cmplx_float *, cmplx_float *, cmplx_float *);

    /* Vector 'a' must equal or longer than 'b' */
    if (a_vec->len < b_vec->len)
        return -DSP_ERR_UNSUPPORTED;

    /* Don't handle this case */
    if (a_vec->real)
        return -DSP_ERR_UNSUPPORTED;

    /* Set function pointers */
    if (b_vec->real)
        mac_func = mac_real;
    else
        mac_func = mac_cmplx;

    switch (b_vec->len) {
    case 1:
        if (b_vec->real)
            return -DSP_ERR_UNSUPPORTED;
        mac_vec_func = mac_cmplx_vec_1;
        break;
    case 3:
        if (!b_vec->real)
            return -DSP_ERR_UNSUPPORTED;
        mac_vec_func = mac_real_vec_3;
        break;
    case 4:
        if (!b_vec->real)
            return -DSP_ERR_UNSUPPORTED;
        mac_vec_func = mac_real_vec_4;
        break;
    case 7:
        if (b_vec->real)
            return -DSP_ERR_UNSUPPORTED;
        mac_vec_func = mac_cmplx_vec_7;
        break;
    case 16:
        if (b_vec->real)
            mac_vec_func = mac_real_vec_16;
        else
            mac_vec_func = mac_cmplx_vec_16;
        break;
    case 21:
        if (!b_vec->real)
            return -DSP_ERR_UNSUPPORTED;
        mac_vec_func = mac_real_vec_21;
        break;
    case 41:
        if (b_vec->real)
            return -DSP_ERR_UNSUPPORTED;
        mac_vec_func = mac_cmplx_vec_41;
        break;
    default:
        return -DSP_ERR_UNSUPPORTED;
    }

    memset((void *) c, 0, c_vec->len * sizeof(cmplx_float));

    end_idx = c_vec->len + start_idx;
    a = a_vec->smpls;
    b = b_vec->smpls;
    c = c_vec->smpls;

    /* Convolve head */
    for (i = start_idx; i < b_vec->len; i++) {
        n = 0;
        while (n <= i) {
            (*mac_func)(&a[i - n], &b[n], &c[i - start_idx]);
            n++;
        }
    }

    /* Set bounds for start or end in main section */
    if ((start_idx > b_vec->len) && (end_idx > a_vec->len)) {
        i = start_idx;
        mid_idx = a_vec->len;
    } else if ((start_idx > b_vec->len) && (end_idx < a_vec->len)) { 
        i = start_idx;
        mid_idx = end_idx;
    } else if ((start_idx < b_vec->len) && (end_idx < a_vec->len)) { 
        i = b_vec->len;
        mid_idx = end_idx;
    } else {
        i = b_vec->len;
        mid_idx = a_vec->len;
    }

    /* Convolve main */
    for (i; i < mid_idx; i++) {
        (*mac_vec_func)(&a[i - b_vec->len + 1], b, &c[i - start_idx]);
    }

    /* Convolve tail */
    for (i = a_vec->len; i < end_idx; i++) {
        n = i - a_vec->len + 1;
        while (n < b_vec->len) {
            (*mac_func)(&a[i - n], &b[n], &c[i - start_idx]);
            n++;
        }
    }

    return 0;
}
