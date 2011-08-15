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

typedef struct {
    float i;
    float q;
} cmplx_float;

struct float_vec {
    cmplx_float *smpls;
    int len;
    int real;
};

enum dsp_err {
    DSP_ERR_NONE,
    DSP_ERR_UNSUPPORTED,
};

void mac_real(cmplx_float *a, cmplx_float *b, cmplx_float *c);
void mac_cmplx(cmplx_float *a, cmplx_float *b, cmplx_float *c);

void mac_real_vec_3(cmplx_float *a, cmplx_float *b, cmplx_float *c);
void mac_real_vec_4(cmplx_float *a, cmplx_float *b, cmplx_float *c);

/* All sample vectors (including real) stored in complex form */ 
int convlv_nosym(struct float_vec *a_vec, struct float_vec *b_vec,
                 struct float_vec *c_vec, int start_idx);
