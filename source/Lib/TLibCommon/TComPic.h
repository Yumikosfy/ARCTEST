/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.  
 *
 * Copyright (c) 2010-2011, ITU/ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file     TComPic.h
    \brief    picture class (header)
*/

#ifndef __TCOMPIC__
#define __TCOMPIC__

// Include files
#include "CommonDef.h"
#include "TComPicSym.h"
#include "TComPicYuv.h"
#include "TComBitStream.h"

class SEImessages;

// ====================================================================================================================
// Class definition
// ====================================================================================================================

/// picture class (symbol + YUV buffers)
class TComPic
{
private:
  Int                   m_iWidth[NUM_PIC_RESOLUTIONS];
  Int                   m_iHeight[NUM_PIC_RESOLUTIONS];
  UInt                  m_uiMaxWidth;
  UInt                  m_uiMaxHeight;
  UInt                  m_uiMaxDepth;
  UInt                  m_uiTLayer;               //  Temporal layer

  TComPicSym*           m_apcPicSym[NUM_PIC_RESOLUTIONS];              //  Symbol
  TComPicYuv*           m_apcPicYuv[NUM_PIC_RESOLUTIONS][2];           //  Texture,  0:org / 1:rec
  TComPicYuv*           m_pcPicYuvPred[NUM_PIC_RESOLUTIONS];           //  Prediction
  TComPicYuv*           m_pcPicYuvResi[NUM_PIC_RESOLUTIONS];           //  Residual
#if PARALLEL_MERGED_DEBLK
  TComPicYuv*           m_pcPicYuvDeblkBuf[NUM_PIC_RESOLUTIONS];
#endif
  Bool                  m_bReconstructed;
  UInt                  m_uiCurrSliceIdx;         // Index of current slice
  Int                   m_iPicSizeIndex;
  
  SEImessages* m_SEIs; ///< Any SEI messages that have been received.  If !NULL we own the object.

public:
  TComPic();
  virtual ~TComPic();
  
  Void          create( Int iWidth, Int iHeight, UInt uiMaxWidth, UInt uiMaxHeight, UInt uiMaxDepth, Int iPicSizeIndex = 0, Bool bIsVirtual = false );
  Void          destroy();
  
  UInt          getTLayer()                { return m_uiTLayer;   }
  Void          setTLayer( UInt uiTLayer ) { m_uiTLayer = uiTLayer; }
#if JCT_ARC
  Int           getPictureSizeIdx()     { return m_iPicSizeIndex; }
  Void          setPictureSizeIdx(Int i);
#endif

  Int           getWidth()            { return m_iWidth[m_iPicSizeIndex]; }
  Int           getWidth(Int i)       { return m_iWidth[i]; }
  Int           getHeight()           { return m_iHeight[m_iPicSizeIndex]; }
  Int           getHeight(Int i)      { return m_iHeight[i]; }

  TComPicSym*   getPicSym()           { return  m_apcPicSym[m_iPicSizeIndex];    }
  TComPicSym*   getPicSym(Int i)      { return  m_apcPicSym[i];    }
  TComSlice*    getSlice(Int s)       { return  m_apcPicSym[m_iPicSizeIndex]->getSlice(s);  }
  TComSlice*    getSlice(Int s, Int i){ return  m_apcPicSym[i]->getSlice(s);  }
  Int           getPOC()              { return  m_apcPicSym[m_iPicSizeIndex]->getSlice(m_uiCurrSliceIdx)->getPOC();  }
  Bool          getDRBFlag()          { return  m_apcPicSym[m_iPicSizeIndex]->getSlice(m_uiCurrSliceIdx)->getDRBFlag();  }
  Int           getERBIndex()         { return  m_apcPicSym[m_iPicSizeIndex]->getSlice(m_uiCurrSliceIdx)->getERBIndex();  }
  TComDataCU*&  getCU( UInt uiCUAddr )  { return  m_apcPicSym[m_iPicSizeIndex]->getCU( uiCUAddr ); }
  TComDataCU*&  getCU( UInt uiCUAddr, Int i )  { return  m_apcPicSym[i]->getCU( uiCUAddr ); }
  
  TComPicYuv*   getPicYuvOrg(Int i);
  TComPicYuv*   getPicYuvOrg()        { return  this->getPicYuvOrg(m_iPicSizeIndex); }
  TComPicYuv*   getPicYuvRec(Int i);
  TComPicYuv*   getPicYuvRec()        { return  this->getPicYuvRec(m_iPicSizeIndex); }
  
  TComPicYuv*   getPicYuvPred(Int i)  { return  m_pcPicYuvPred[i]; }
  TComPicYuv*   getPicYuvPred()       { return  m_pcPicYuvPred[m_iPicSizeIndex]; }
  TComPicYuv*   getPicYuvResi(Int i)  { return  m_pcPicYuvResi[i]; }
  TComPicYuv*   getPicYuvResi()       { return  m_pcPicYuvResi[m_iPicSizeIndex]; }
  Void          setPicYuvPred( TComPicYuv* pcPicYuv )       { m_pcPicYuvPred[m_iPicSizeIndex] = pcPicYuv; }
  Void          setPicYuvPred( TComPicYuv* pcPicYuv, Int i ){ m_pcPicYuvPred[i] = pcPicYuv; }
  Void          setPicYuvResi( TComPicYuv* pcPicYuv )       { m_pcPicYuvResi[m_iPicSizeIndex] = pcPicYuv; }
  Void          setPicYuvResi( TComPicYuv* pcPicYuv, Int i ){ m_pcPicYuvResi[i] = pcPicYuv; }
  
  UInt          getNumCUsInFrame()       { return m_apcPicSym[m_iPicSizeIndex]->getNumberOfCUsInFrame(); }
  UInt          getNumCUsInFrame(Int i)  { return m_apcPicSym[i]->getNumberOfCUsInFrame(); }
  UInt          getNumPartInWidth()      { return m_apcPicSym[m_iPicSizeIndex]->getNumPartInWidth();     }
  UInt          getNumPartInWidth(Int i) { return m_apcPicSym[i]->getNumPartInWidth();     }
  UInt          getNumPartInHeight()     { return m_apcPicSym[m_iPicSizeIndex]->getNumPartInHeight();    }
  UInt          getNumPartInHeight(Int i){ return m_apcPicSym[i]->getNumPartInHeight();    }
  UInt          getNumPartInCU()         { return m_apcPicSym[m_iPicSizeIndex]->getNumPartition();       }
  UInt          getNumPartInCU(Int i)    { return m_apcPicSym[i]->getNumPartition();       }
  UInt          getFrameWidthInCU()      { return m_apcPicSym[m_iPicSizeIndex]->getFrameWidthInCU();     }
  UInt          getFrameWidthInCU(Int i) { return m_apcPicSym[i]->getFrameWidthInCU();     }
  UInt          getFrameHeightInCU()     { return m_apcPicSym[m_iPicSizeIndex]->getFrameHeightInCU();    }
  UInt          getFrameHeightInCU(Int i){ return m_apcPicSym[i]->getFrameHeightInCU();    }
  UInt          getMinCUWidth()          { return m_apcPicSym[m_iPicSizeIndex]->getMinCUWidth();         }
  UInt          getMinCUHeight()         { return m_apcPicSym[m_iPicSizeIndex]->getMinCUHeight();        }
  
  UInt          getParPelX(UChar uhPartIdx) { return getParPelX(uhPartIdx); }
  UInt          getParPelY(UChar uhPartIdx) { return getParPelX(uhPartIdx); }
  
  Int           getStride()           { return m_apcPicYuv[m_iPicSizeIndex][1]->getStride(); }
  Int           getStride(Int i)      { return m_apcPicYuv[i][1]->getStride(); }
  Int           getCStride()          { return m_apcPicYuv[m_iPicSizeIndex][1]->getCStride(); }
  Int           getCStride(Int i)     { return m_apcPicYuv[i][1]->getCStride(); }
  
  Void          setReconMark (Bool b) { m_bReconstructed = b;     }
  Bool          getReconMark ()       { return m_bReconstructed;  }

  Void          resetRecData();

  Void          setPOC(Int p)         { for (int i=0; i<NUM_PIC_RESOLUTIONS; ++i) { m_apcPicSym[i]->getSlice(0)->setPOC(p); } }

#if AMVP_BUFFERCOMPRESS
  Void          compressMotion(); 
#endif 
  UInt          getCurrSliceIdx()            { return m_uiCurrSliceIdx;                }
  Void          setCurrSliceIdx(UInt i)      { m_uiCurrSliceIdx = i;                   }
  UInt          getNumAllocatedSlice()       {return m_apcPicSym[m_iPicSizeIndex]->getNumAllocatedSlice();}
  UInt          getNumAllocatedSlice(Int i)  {return m_apcPicSym[i]->getNumAllocatedSlice();}
  Void          allocateNewSlice()           {m_apcPicSym[m_iPicSizeIndex]->allocateNewSlice();         }
  Void          allocateNewSlice(Int i)      {m_apcPicSym[i]->allocateNewSlice();         }
  Void          clearSliceBuffer()           {m_apcPicSym[m_iPicSizeIndex]->clearSliceBuffer();         }
  Void          clearSliceBuffer(Int i)      {m_apcPicSym[i]->clearSliceBuffer();         }
  
#if PARALLEL_MERGED_DEBLK
  TComPicYuv*   getPicYuvDeblkBuf();
  TComPicYuv*   getPicYuvDeblkBuf(Int i);
#endif

  /** transfer ownership of @seis to @this picture */
  void setSEIs(SEImessages* seis) { m_SEIs = seis; }

  /**
   * return the current list of SEI messages associated with this picture.
   * Pointer is valid until @this->destroy() is called */
  SEImessages* getSEIs() { return m_SEIs; }

  /**
   * return the current list of SEI messages associated with this picture.
   * Pointer is valid until @this->destroy() is called */
  const SEImessages* getSEIs() const { return m_SEIs; }

};// END CLASS DEFINITION TComPic


#endif // __TCOMPIC__

