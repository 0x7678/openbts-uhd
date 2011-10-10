/*
 * Radio device I/O interface
 * Written by Thomas Tsou <ttsou@vt.edu>
 *
 * Copyright 2011 Free Software Foundation, Inc.
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * See the COPYING file in the main directory for details.
 */

#include <radioInterface.h>
#include <Logger.h>

/* Unset if the device cannot handle arbitrary transfer sizes */
#define TX_FULL_BUFFER

/* Device side buffers */
static short rx_buf[OUTCHUNK * 2 * 2];
static short tx_buf[INCHUNK * 2 * 2];

/* Maintain tx write index if we can't send full buffers */
#ifndef TX_FULL_BUFFER
static int tx_wr_indx = 0;
#endif

/* Complex float to short conversion */
static int float_to_short(short *shrt_out, float *flt_in, int num)
{
	int i;

	for (i = 0; i < num; i++) {
		shrt_out[2 * i + 0] = flt_in[2 * i + 0];
		shrt_out[2 * i + 1] = flt_in[2 * i + 1];
	}

	return i;
}

/* Comlpex short to float conversion */
static int short_to_float(float *flt_out, short *shrt_in, int num)
{
	int i;

	for (i = 0; i < num; i++) {
		flt_out[2 * i + 0] = shrt_in[2 * i + 0];
		flt_out[2 * i + 1] = shrt_in[2 * i + 1];
	}

	return i;
}

/* Receive a timestamped chunk from the device */ 
void RadioInterface::pullBuffer()
{
	bool local_underrun;

	/* Read samples. Fail if we don't get what we want. */
	int num_rd = mRadio->readSamples(rx_buf, OUTCHUNK, &overrun,
					    readTimestamp, &local_underrun);

	LOG(DEEPDEBUG) << "Rx read " << num_rd << " samples from device";
	assert(num_rd == OUTCHUNK);

	underrun |= local_underrun;
	readTimestamp += (TIMESTAMP) num_rd;

	short_to_float(rcvBuffer + 2 * rcvCursor, rx_buf, num_rd);
	rcvCursor += num_rd;
}

#ifdef TX_FULL_BUFFER
/* Send timestamped chunk to the device with arbitrary size */ 
void RadioInterface::pushBuffer()
{
	if (sendCursor < INCHUNK)
		return;

	float_to_short(tx_buf, sendBuffer, sendCursor);

	/* Write samples. Fail if we don't get what we want. */
	int num_smpls = mRadio->writeSamples(tx_buf,
					     sendCursor,
					     &underrun,
					     writeTimestamp);
	assert(num_smpls == sendCursor);

	writeTimestamp += (TIMESTAMP) num_smpls;
	sendCursor = 0;
}
#else
/* Send a timestamped chunk to the device with fixed size */ 
void RadioInterface::pushBuffer()
{
	if (sendCursor < INCHUNK)
		return;

	float_to_short(tx_buf + 2 * tx_wr_indx, sendBuffer, sendCursor);

	/* Write samples. Fail if we don't get what we want. */
	int num_smpls = mRadio->writeSamples(tx_buf,
					     INCHUNK,
					     &underrun,
					     writeTimestamp);

	assert(num_smpls <= tx_wr_indx);
	writeTimestamp += (TIMESTAMP) num_smpls;

	/* Update buffer indices */
	if (num_smpls == tx_wr_indx) {
		tx_wr_indx = 0;
	} else {
		sz = (tx_wr_indx - num_smpls) * 2 * sizeof(short);
		memmove(tx_buf, tx_buf + 2 * num_smpls, sz);
		tx_wr_indx = tx_wr_indx - num_smpls;
	}

	sendCursor = 0;
}
#endif
