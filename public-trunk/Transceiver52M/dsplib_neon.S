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

	.arch armv7-a
	.fpu neon
	.text
	.align 2
	.p2align 4,,15
	.global mac_real
	.type mac_real, %function
mac_real:
	vld1.32          d0, [r0]
	flds             s2, [r1]
	vmov             s3, s2
	vld1.32          d2, [r2]
	vmla.f32         d2, d0, d1
	vst1.64          d2, [r2] 
	bx               lr
	.size mac_real, .-mac_real
	.align 2
	.p2align 4,,15
	.global mac_cmplx
	.type mac_cmplx, %function
mac_cmplx:
	vld1.32          d0, [r0]
	vld1.32          d1, [r1]
	vld1.32          d5, [r2]
	vmul.f32         d3, d0, d1
	vrev64.32        d1, d1
	vmul.f32         d4, d0, d1
	vsub.f32         s4, s6, s7
	vadd.f32         s5, s8, s9
	vadd.f32         d2, d5
	vst1.64          d2, [r2] 
	bx               lr
	.size mac_cmplx, .-mac_cmplx
	.align 2
	.p2align 4,,15
	.global mac_real_vec_4
	.type mac_real_vec_4, %function
mac_real_vec_4:
	vld2.32     {q0,q1}, [r0]
	vld2.32     {q2,q3}, [r1]
	vld2.32     {q8,q9}, [r2]
	vswp             d4, d5
	vrev64.32        q2, q2
	vmla.f32         q8, q0, q2 
	vmla.f32         q9, q1, q2
	vst2.32     {q8,q9}, [r2] 
	bx               lr
	.size mac_real_vec_4, .-mac_real_vec_4
	.align 2
	.p2align 4,,15
	.global mac_sum_real_vec_3
	.type mac_sum_real_vec_3, %function
mac_sum_real_vec_3:
	vld2.32     {q0-q1}, [r0]
	vld2.32     {q2-q3}, [r1]
	vmov             s4, s7
	vmov             s4, s6
	vmov             s7, s6
	vmul.f32         q3, q0, q2 
	vadd.f32         s0, s12, s13
	vadd.f32         s0, s14
	vmul.f32         q3, q1, q2
	vadd.f32         s1, s12, s13
	vadd.f32         s1, s14
	vld1.32          d8, [r2]
	vadd.f32         d0, d8
	vst1.64          d0, [r2] 
	bx               lr
	.size mac_sum_real_vec_3, .-mac_sum_real_vec_3
	.align 2
	.p2align 4,,15
	.global mac_sum_real_vec_4
	.type mac_sum_real_vec_4, %function
mac_sum_real_vec_4:
	vld2.32     {q0-q1}, [r0]
	vld2.32     {q2-q3}, [r1]
	vswp             d4, d5
	vrev64.32        q2, q2
	vmul.f32         q3, q0, q2 
	vpadd.f32        d0, d6, d7
	vadd.f32         s0, s1
	vld1.32          d8, [r2]
	vmul.f32         q3, q1, q2
	vpadd.f32        d1, d6, d7
	vadd.f32         s1, s2, s3
	vadd.f32         d0, d8
	vst1.64          d0, [r2] 
	bx               lr
	.size mac_sum_real_vec_4, .-mac_sum_real_vec_4
	.section .note.GNU-stack,"",%progbits
