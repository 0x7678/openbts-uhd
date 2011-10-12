/*
* Copyright 2008 Free Software Foundation, Inc.
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



#include "sigProcLib.h"  
#include "GSMCommon.h"
#include "LinkedLists.h"
#include "radioDevice.h"
#include "radioVector.h"
#include "radioClock.h"

/** samples per GSM symbol */
#define SAMPSPERSYM 1 
#define INCHUNK    625
#define OUTCHUNK   625

/** class to interface the transceiver with the USRP */
class RadioInterface {

private:

  Thread mAlignRadioServiceLoopThread;	      ///< thread that synchronizes transmit and receive sections

  VectorFIFO  mReceiveFIFO;		      ///< FIFO that holds receive  bursts

  RadioDevice *mRadio;			      ///< the USRP object
 
  float sendBuffer[2*2*INCHUNK];
  unsigned sendCursor;

  float rcvBuffer[2*2*OUTCHUNK];
  unsigned rcvCursor;
 
  bool underrun;			      ///< indicates writes to USRP are too slow
  bool overrun;				      ///< indicates reads from USRP are too slow
  TIMESTAMP writeTimestamp;		      ///< sample timestamp of next packet written to USRP
  TIMESTAMP readTimestamp;		      ///< sample timestamp of next packet read from USRP

  RadioClock mClock;                          ///< the basestation clock!

  int samplesPerSymbol;			      ///< samples per GSM symbol
  int receiveOffset;                          ///< offset b/w transmit and receive GSM timestamps, in timeslots
  int mRadioOversampling;
  int mTransceiverOversampling;

  bool mOn;				      ///< indicates radio is on

  double powerScaling;

  /** format samples to USRP */ 
  int radioifyVector(signalVector &wVector,
                     float *floatVector,
                     float scale,
                     bool zero);

  /** format samples from USRP */
  int unRadioifyVector(float *floatVector, signalVector &wVector);

  /** push GSM bursts into the transmit buffer */
  void pushBuffer(void);

  /** pull GSM bursts from the receive buffer */
  void pullBuffer(void);

public:

  /** start the interface */
  void start();

  /** constructor */
  RadioInterface(RadioDevice* wRadio = NULL,
		 int receiveOffset = 3,
		 int wRadioOversampling = SAMPSPERSYM,
		 int wTransceiverOversampling = SAMPSPERSYM,
		 GSM::Time wStartTime = GSM::Time(0));
    
  /** destructor */
  ~RadioInterface();

  /** check for underrun, resets underrun value */
  bool isUnderrun();
  
  /** attach an existing USRP to this interface */
  void attach(RadioDevice *wRadio, int wRadioOversampling);

  /** return the receive FIFO */
  VectorFIFO* receiveFIFO() { return &mReceiveFIFO;}

  /** return the basestation clock */
  RadioClock* getClock(void) { return &mClock;};

  /** set transmit frequency */
  bool tuneTx(double freq);

  /** set receive frequency */
  bool tuneRx(double freq);

  /** set receive gain */
  double setRxGain(double dB);

  /** get receive gain */
  double getRxGain(void);

  /** drive transmission of GSM bursts */
  void driveTransmitRadio(signalVector &radioBurst, bool zeroBurst);

  /** drive reception of GSM bursts */
  void driveReceiveRadio();

  void setPowerAttenuation(double atten); 

  /** returns the full-scale transmit amplitude **/
  double fullScaleInputValue();

  /** returns the full-scale receive amplitude **/
  double fullScaleOutputValue();

  /** set thread priority on current thread */
  void setPriority() { mRadio->setPriority(); }

protected:

  /** drive synchronization of Tx/Rx of USRP */
  void alignRadio();

  /** reset the interface */
  void reset();

  friend void *AlignRadioServiceLoopAdapter(RadioInterface*);

};

/** synchronization thread loop */
void *AlignRadioServiceLoopAdapter(RadioInterface*);
