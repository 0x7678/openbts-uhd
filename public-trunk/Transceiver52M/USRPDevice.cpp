/*
* Copyright 2008, 2009 Free Software Foundation, Inc.
*
* This software is distributed under the terms of the GNU Affero Public License.
* See the COPYING file in the main directory for details.
*
* This use of this software may be subject to additional restrictions.
* See the LEGAL file in the main directory for details.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/


/*
	Compilation Flags

	SWLOOPBACK	compile for software loopback testing
*/ 


#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "Threads.h"
#include "USRPDevice.h"

#include <Logger.h>


using namespace std;

string write_it(unsigned v) {
  string s = "   ";
  s[0] = (v>>16) & 0x0ff;
  s[1] = (v>>8) & 0x0ff;
  s[2] = (v) & 0x0ff;
  return s;
}


const float USRPDevice::LO_OFFSET = 4.0e6;

bool USRPDevice::compute_regs(double freq,
			      unsigned *R,
			      unsigned *control,
			      unsigned *N,
			      double *actual_freq) 
{
  if (freq < 1.2e9) {
	DIV2 = 1;
	freq_mult = 2;
  }
  else {
	DIV2 = 0;
	freq_mult = 1;
  }

  float phdet_freq = masterClockRate/R_DIV;
  int desired_n = (int) round(freq*freq_mult/phdet_freq);
  *actual_freq = desired_n * phdet_freq/freq_mult;
  float B = floor(desired_n/16);
  float A = desired_n - 16*B;
  unsigned B_DIV = int(B);
  unsigned A_DIV = int(A);
  if (B < A) return false;
  *R = (R_RSV<<22) | 
    (BSC << 20) | 
    (TEST << 19) | 
    (LDP << 18) | 
    (ABP << 16) | 
    (R_DIV << 2);
  *control = (P<<22) | 
    (PD<<20) | 
    (CP2 << 17) | 
    (CP1 << 14) | 
    (PL << 12) | 
    (MTLD << 11) | 
    (CPG << 10) | 
    (CP3S << 9) | 
    (PDP << 8) | 
    (MUXOUT << 5) | 
    (CR << 4) | 
    (PC << 2);
  *N = (DIVSEL<<23) | 
    (DIV2<<22) | 
    (CPGAIN<<21) | 
    (B_DIV<<8) | 
    (N_RSV<<7) | 
    (A_DIV<<2);
  return true;
}

bool USRPDevice::rx_setFreq(double freq, double *actual_freq) 
{
  unsigned R, control, N;
  if (!compute_regs(freq, &R, &control, &N, actual_freq)) return false;
  if (R==0) return false;
 
  writeLock.lock(); 
  m_uRx->_write_spi(0,SPI_ENABLE_RX_B,SPI_FMT_MSB | SPI_FMT_HDR_0,
		    write_it((R & ~0x3) | 1));
  m_uRx->_write_spi(0,SPI_ENABLE_RX_B,SPI_FMT_MSB | SPI_FMT_HDR_0,
		    write_it((control & ~0x3) | 0));
  usleep(10000);
  m_uRx->_write_spi(0,SPI_ENABLE_RX_B,SPI_FMT_MSB | SPI_FMT_HDR_0,
		    write_it((N & ~0x3) | 2));
  writeLock.unlock();
  
  if (m_uRx->read_io(1) & PLL_LOCK_DETECT)  return true;
  if (m_uRx->read_io(1) & PLL_LOCK_DETECT)  return true;
  return false;
}

enum dboardConfigType {
  TXA_RXB,
  TXB_RXA,
  TXA_RXA,
  TXB_RXB
};

const dboardConfigType dboardConfig = TXA_RXB;
const double USRPDevice::masterClockRate = 52.0e6;

USRPDevice::USRPDevice (double _desiredSampleRate) 
{
  LOG(INFO) << "creating USRP device...";
  decimRate = (unsigned int) round(masterClockRate/_desiredSampleRate);
  actualSampleRate = masterClockRate/decimRate;
  rxGain = 0;

#ifdef SWLOOPBACK 
  samplePeriod = 1.0e6/actualSampleRate;
  loopbackBufferSize = 0;
  gettimeofday(&lastReadTime,NULL);
  firstRead = false;
#endif
}

bool USRPDevice::make(bool wSkipRx) 
{
  skipRx = wSkipRx;

  writeLock.unlock();

  LOG(INFO) << "making USRP device..";
#ifndef SWLOOPBACK 
  string rbf = "std_inband.rbf";
  //string rbf = "inband_1rxhb_1tx.rbf"; 
  m_uRx.reset();
  if (!skipRx) {
  try {
    m_uRx = usrp_standard_rx_sptr(usrp_standard_rx::make(0,decimRate,1,-1,
                                                         usrp_standard_rx::FPGA_MODE_NORMAL,
                                                         1024,16*8,rbf));
#ifdef HAVE_LIBUSRP_3_2
    m_uRx->set_fpga_master_clock_freq(masterClockRate);
#endif
  }
  
  catch(...) {
    LOG(ALARM) << "make failed on Rx";
    m_uRx.reset();
    return false;
  }

  if (m_uRx->fpga_master_clock_freq() != masterClockRate)
  {
    LOG(ALARM) << "WRONG FPGA clock freq = " << m_uRx->fpga_master_clock_freq()
               << ", desired clock freq = " << masterClockRate;
    m_uRx.reset();
    return false;
  }
  }

  try {
    m_uTx = usrp_standard_tx_sptr(usrp_standard_tx::make(0,decimRate*2,1,-1,
                                                         1024,16*8,rbf));
#ifdef HAVE_LIBUSRP_3_2
    m_uTx->set_fpga_master_clock_freq(masterClockRate);
#endif
  }
  
  catch(...) {
    LOG(ALARM) << "make failed on Tx";
    m_uTx.reset();
    return false;
  }

  if (m_uTx->fpga_master_clock_freq() != masterClockRate)
  {
    LOG(ALARM) << "WRONG FPGA clock freq = " << m_uTx->fpga_master_clock_freq()
               << ", desired clock freq = " << masterClockRate;
    m_uTx.reset();
    return false;
  }

  if (!skipRx) m_uRx->stop();
  m_uTx->stop();
  
#endif

  switch (dboardConfig) {
  case TXA_RXB:
    txSubdevSpec = usrp_subdev_spec(0,0);
    rxSubdevSpec = usrp_subdev_spec(1,0);
    break;
  case TXB_RXA:
    txSubdevSpec = usrp_subdev_spec(1,0);
    rxSubdevSpec = usrp_subdev_spec(0,0);
    break;
  case TXA_RXA:
    txSubdevSpec = usrp_subdev_spec(0,0);
    rxSubdevSpec = usrp_subdev_spec(0,0);
    break;
  case TXB_RXB:
    txSubdevSpec = usrp_subdev_spec(1,0);
    rxSubdevSpec = usrp_subdev_spec(1,0);
    break;
  default:
    txSubdevSpec = usrp_subdev_spec(0,0);
    rxSubdevSpec = usrp_subdev_spec(1,0);
  }

  m_dbTx = m_uTx->selected_subdev(txSubdevSpec);
  m_dbRx = m_uRx->selected_subdev(rxSubdevSpec);

  samplesRead = 0;
  samplesWritten = 0;
  started = false;
  
  return true;
}



bool USRPDevice::start() 
{
  LOG(INFO) << "starting USRP...";
#ifndef SWLOOPBACK 
  if (!m_uRx && !skipRx) return false;
  if (!m_uTx) return false;
  
  if (!skipRx) m_uRx->stop();
  m_uTx->stop();

  writeLock.lock();
  // power up and configure daughterboards
  m_dbTx->set_enable(true);
  m_uTx->set_mux(m_uTx->determine_tx_mux_value(txSubdevSpec));
  m_uRx->set_mux(0x00000032);

  if (!m_dbRx->select_rx_antenna(1))
    m_dbRx->select_rx_antenna(0);

  writeLock.unlock();

  // Set all PGA gains to max
  m_uTx->set_pga(0,m_uTx->pga_max());
  m_uTx->set_pga(1,m_uTx->pga_max());
  m_uRx->set_pga(2,m_uRx->pga_max());
  m_uRx->set_pga(3,m_uRx->pga_max());

  setRxGain(47);

  data = new short[currDataSize];
  dataStart = 0;
  dataEnd = 0;
  timeStart = 0;
  timeEnd = 0;
  timestampOffset = 0;
  latestWriteTimestamp = 0;
  lastPktTimestamp = 0;
  hi32Timestamp = 0;
  isAligned = false;

 
  if (!skipRx) 
  started = (m_uRx->start() && m_uTx->start());
  else
  started = m_uTx->start();
  return started;
#else
  gettimeofday(&lastReadTime,NULL);
  return true;
#endif
}

bool USRPDevice::stop() 
{
#ifndef SWLOOPBACK 
  if (!m_uRx) return false;
  if (!m_uTx) return false;
  
  // power down
  m_uTx->write_io(0,(POWER_UP|RX_TXN),(POWER_UP|RX_TXN|ENABLE));
  m_uRx->write_io(1,POWER_UP,(POWER_UP|ENABLE));
  
  delete[] currData;
  
  started = !(m_uRx->stop() && m_uTx->stop());
  return !started;
#else
  return true;
#endif
}

double USRPDevice::setTxGain(double dB) {
 
   writeLock.lock();
   if (dB > maxTxGain()) dB = maxTxGain();
   if (dB < minTxGain()) dB = minTxGain();

   m_uTx->set_pga(0,dB);
   m_uTx->set_pga(1,dB);

   LOG(NOTICE) << "Setting TX PGA to " << dB << " dB.";

   writeLock.unlock();
  
   return dB;
}


double USRPDevice::setRxGain(double dB) {

   writeLock.lock();
   if (dB > maxRxGain()) dB = maxRxGain();
   if (dB < minRxGain()) dB = minRxGain();
   
   double dBret = dB;

   dB = dB - minRxGain();

   double rfMax = 70.0;
   if (dB > rfMax) {
        m_uRx->set_pga(2,dB-rfMax);
        m_uRx->set_pga(3,dB-rfMax);
        dB = rfMax;
   }
   else {
        m_uRx->set_pga(2,0);
        m_uRx->set_pga(3,0);
   }
   m_uRx->write_aux_dac(1,0,
        (int) ceil((1.2 + 0.02 - (dB/rfMax))*4096.0/3.3));

   LOG(DEBUG) << "Setting DAC voltage to " << (1.2+0.02 - (dB/rfMax)) << " " << (int) ceil((1.2 + 0.02 - (dB/rfMax))*4096.0/3.3);

   rxGain = dBret;
   
   writeLock.unlock();
   
   return dBret;
}


// NOTE: Assumes sequential reads
int USRPDevice::readSamples(short *buf, int len, bool *overrun, 
			    TIMESTAMP timestamp,
			    bool *underrun,
			    unsigned *RSSI) 
{
#ifndef SWLOOPBACK 
  if (!m_uRx) return 0;
  
  timestamp += timestampOffset;
  
  if (timestamp + len < timeStart) {
    memset(buf,0,len*2*sizeof(short));
    return len;
  }

  if (underrun) *underrun = false;
 
  uint32_t readBuf[2000];
 
  while (1) {
    //guestimate USB read size
    int readLen=0;
    {
      int numSamplesNeeded = timestamp + len - timeEnd;
      if (numSamplesNeeded <=0) break;
      readLen = 512 * ((int) ceil((float) numSamplesNeeded/126.0));
      if (readLen > 8000) readLen= (8000/512)*512;
    }
    
    // read USRP packets, parse and save A/D data as needed
    readLen = m_uRx->read((void *)readBuf,readLen,overrun);
    for(int pktNum = 0; pktNum < (readLen/512); pktNum++) {
      // tmpBuf points to start of a USB packet
      uint32_t* tmpBuf = (uint32_t *) (readBuf+pktNum*512/4);
      TIMESTAMP pktTimestamp = usrp_to_host_u32(tmpBuf[1]);
      uint32_t word0 = usrp_to_host_u32(tmpBuf[0]);
      uint32_t chan = (word0 >> 16) & 0x1f;
      unsigned payloadSz = word0 & 0x1ff;
      LOG(DEEPDEBUG) << "first two bytes: " << hex << word0 << " " << dec << pktTimestamp;

      bool incrementHi32 = ((lastPktTimestamp & 0x0ffffffffll) > pktTimestamp);
      if (incrementHi32 && (timeStart!=0)) {
           LOG(DEBUG) << "high 32 increment!!!";
           hi32Timestamp++;
      }
      pktTimestamp = (((TIMESTAMP) hi32Timestamp) << 32) | pktTimestamp;
      lastPktTimestamp = pktTimestamp;

      if (chan == 0x01f) {
	// control reply, check to see if its ping reply
        uint32_t word2 = usrp_to_host_u32(tmpBuf[2]);
	if ((word2 >> 16) == ((0x01 << 8) | 0x02)) {
          timestamp -= timestampOffset;
	  timestampOffset = pktTimestamp - pingTimestamp + PINGOFFSET;
	  LOG(DEBUG) << "updating timestamp offset to: " << timestampOffset;
          timestamp += timestampOffset;
	  isAligned = true;
	}
	continue;
      }
      if (chan != 0) {
	LOG(DEBUG) << "chan: " << chan << ", timestamp: " << pktTimestamp << ", sz:" << payloadSz;
	continue;
      }
      if ((word0 >> 28) & 0x04) {
	if (underrun) *underrun = true; 
	LOG(DEBUG) << "UNDERRUN in TRX->USRP interface";
      }
      if (RSSI) *RSSI = (word0 >> 21) & 0x3f;
      
      if (!isAligned) continue;
      
      unsigned cursorStart = pktTimestamp - timeStart + dataStart;
      while (cursorStart*2 > currDataSize) {
	cursorStart -= currDataSize/2;
      }
      if (cursorStart*2 + payloadSz/2 > currDataSize) {
	// need to circle around buffer
	memcpy(data+cursorStart*2,tmpBuf+2,(currDataSize-cursorStart*2)*sizeof(short));
	memcpy(data,tmpBuf+2+(currDataSize/2-cursorStart),payloadSz-(currDataSize-cursorStart*2)*sizeof(short));
      }
      else {
	memcpy(data+cursorStart*2,tmpBuf+2,payloadSz);
      }
      if (pktTimestamp + payloadSz/2/sizeof(short) > timeEnd) 
	timeEnd = pktTimestamp+payloadSz/2/sizeof(short);

      LOG(DEEPDEBUG) << "timeStart: " << timeStart << ", timeEnd: " << timeEnd << ", pktTimestamp: " << pktTimestamp;

    }	
  }     
 
  // copy desired data to buf
  unsigned bufStart = dataStart+(timestamp-timeStart);
  if (bufStart + len < currDataSize/2) { 
    LOG(DEEPDEBUG) << "bufStart: " << bufStart;
    memcpy(buf,data+bufStart*2,len*2*sizeof(short));
    memset(data+bufStart*2,0,len*2*sizeof(short));
  }
  else {
    LOG(DEEPDEBUG) << "len: " << len << ", currDataSize/2: " << currDataSize/2 << ", bufStart: " << bufStart;
    unsigned firstLength = (currDataSize/2-bufStart);
    LOG(DEEPDEBUG) << "firstLength: " << firstLength;
    memcpy(buf,data+bufStart*2,firstLength*2*sizeof(short));
    memset(data+bufStart*2,0,firstLength*2*sizeof(short));
    memcpy(buf+firstLength*2,data,(len-firstLength)*2*sizeof(short));
    memset(data,0,(len-firstLength)*2*sizeof(short));
  }
  dataStart = (bufStart + len) % (currDataSize/2);
  timeStart = timestamp + len;

  // do IQ swap here
  for (int i = 0; i < len; i++) {
    short tmp = usrp_to_host_short(buf[2*i]);
    buf[2*i] = usrp_to_host_short(buf[2*i+1]);
    buf[2*i+1] = tmp;
  } 
 
  return len;
  
#else
  if (loopbackBufferSize < 2) return 0;
  int numSamples = 0;
  struct timeval currTime;
  gettimeofday(&currTime,NULL);
  double timeElapsed = (currTime.tv_sec - lastReadTime.tv_sec)*1.0e6 + 
    (currTime.tv_usec - lastReadTime.tv_usec);
  if (timeElapsed < samplePeriod) {return 0;}
  int numSamplesToRead = (int) floor(timeElapsed/samplePeriod);
  if (numSamplesToRead < len) return 0;
  
  if (numSamplesToRead > len) numSamplesToRead = len;
  if (numSamplesToRead > loopbackBufferSize/2) {
    firstRead =false; 
    numSamplesToRead = loopbackBufferSize/2;
  }
  memcpy(buf,loopbackBuffer,sizeof(short)*2*numSamplesToRead);
  loopbackBufferSize -= 2*numSamplesToRead;
  memcpy(loopbackBuffer,loopbackBuffer+2*numSamplesToRead,
	 sizeof(short)*loopbackBufferSize);
  numSamples = numSamplesToRead;
  if (firstRead) {
    int new_usec = lastReadTime.tv_usec + (int) round((double) numSamplesToRead * samplePeriod);
    lastReadTime.tv_sec = lastReadTime.tv_sec + new_usec/1000000;
    lastReadTime.tv_usec = new_usec % 1000000;
  }
  else {
    gettimeofday(&lastReadTime,NULL);
    firstRead = true;
  }
  samplesRead += numSamples;
  
  return numSamples;
#endif
}

int USRPDevice::writeSamples(short *buf, int len, bool *underrun, 
			     unsigned long long timestamp,
			     bool isControl) 
{
  writeLock.lock();

#ifndef SWLOOPBACK 
  if (!m_uTx) return 0;
 
  static uint32_t outData[128*20];
 
  for (int i = 0; i < len*2; i++) {
	buf[i] = host_to_usrp_short(buf[i]);
  }

  int numWritten = 0;
  unsigned isStart = 1;
  unsigned RSSI = 0;
  unsigned CHAN = (isControl) ? 0x01f : 0x00;
  len = len*2*sizeof(short);
  int numPkts = (int) ceil((float)len/(float)504);
  unsigned isEnd = (numPkts < 2);
  uint32_t *outPkt = outData;
  int pktNum = 0;
  while (numWritten < len) {
    // pkt is pointer to start of a USB packet
    uint32_t *pkt = outPkt + pktNum*128;
    isEnd = (len - numWritten <= 504);
    unsigned payloadLen = ((len - numWritten) < 504) ? (len-numWritten) : 504;
    pkt[0] = (isStart << 12 | isEnd << 11 | (RSSI & 0x3f) << 5 | CHAN) << 16 | payloadLen;
    pkt[1] = timestamp & 0x0ffffffffll;
    memcpy(pkt+2,buf+(numWritten/sizeof(short)),payloadLen);
    numWritten += payloadLen;
    timestamp += payloadLen/2/sizeof(short);
    isStart = 0;
    pkt[0] = host_to_usrp_u32(pkt[0]);
    pkt[1] = host_to_usrp_u32(pkt[1]);
    pktNum++;
  }
  m_uTx->write((const void*) outPkt,sizeof(uint32_t)*128*numPkts,NULL);

  samplesWritten += len/2/sizeof(short);
  writeLock.unlock();

  return len/2/sizeof(short);
#else
  int retVal = len;
  memcpy(loopbackBuffer+loopbackBufferSize,buf,sizeof(short)*2*len);
  samplesWritten += retVal;
  loopbackBufferSize += retVal*2;
   
  return retVal;
#endif
}

bool USRPDevice::updateAlignment(TIMESTAMP timestamp) 
{
#ifndef SWLOOPBACK 
  short data[] = {0x00,0x02,0x00,0x00};
  uint32_t *wordPtr = (uint32_t *) data;
  *wordPtr = host_to_usrp_u32(*wordPtr);
  bool tmpUnderrun;
  if (writeSamples((short *) data,1,&tmpUnderrun,timestamp & 0x0ffffffffll,true)) {
    pingTimestamp = timestamp;
    return true;
  }
  return false;
#else
  return true;
#endif
}

#ifndef SWLOOPBACK 
bool USRPDevice::setTxFreq(double wFreq)
{
  usrp_tune_result result;

  if (m_uTx->tune(txSubdevSpec.side, m_dbTx, wFreq, &result)) {
    LOG(INFO) << "set TX: " << wFreq << std::endl
              << "    baseband freq: " << result.baseband_freq << std::endl
              << "    DDC freq:      " << result.dxc_freq << std::endl
              << "    residual freq: " << result.residual_freq;
    return true;
  }
  else {
    LOG(ERROR) << "set TX: " << wFreq << "failed" << std::endl
               << "    baseband freq: " << result.baseband_freq << std::endl
               << "    DDC freq:      " << result.dxc_freq << std::endl
               << "    residual freq: " << result.residual_freq;
    return false;
  }
}

bool USRPDevice::setRxFreq(double wFreq) {
  // Tune to wFreq-2*LO_OFFSET, to
  //   1) prevent LO bleedthrough (as with the setTxFreq method above)
  //   2) The extra LO_OFFSET pushes potential transmitter energy (GSM BS->MS transmissions 
  //        are 45Mhz above MS->BS transmissions) into a notch of the baseband lowpass filter 
  //        in front of the ADC.  This possibly gives us an extra 10-20dB Tx/Rx isolation.
  double actFreq;
  // FIXME -- This should bo configurable.
  if (!rx_setFreq(wFreq-2*LO_OFFSET,&actFreq)) return false;
  bool retVal = m_uRx->set_rx_freq(0,(wFreq-actFreq));
  LOG(DEBUG) << "set RX: " << wFreq-actFreq << " actual RX: " << m_uRx->rx_freq(0);
  return retVal;
};

#else
bool USRPDevice::setTxFreq(double wFreq) { return true;};
bool USRPDevice::setRxFreq(double wFreq) { return true;};
#endif
