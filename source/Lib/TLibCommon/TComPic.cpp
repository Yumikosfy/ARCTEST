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

/** \file     TComPic.cpp
    \brief    picture class
*/

#include "TComPic.h"
#include "SEI.h"
#include "TComScale.h"

// ====================================================================================================================
// Constructor / destructor / create / destroy
// ====================================================================================================================

TComPic::TComPic() :
m_uiMaxWidth(0),
m_uiMaxHeight(0),
m_uiMaxDepth(0)
{
  m_uiTLayer          = 0;
  for (int i=0; i<NUM_PIC_RESOLUTIONS; ++i){
    m_apcPicSym[i]      = NULL;
    m_apcPicYuv[i][0]   = NULL;
    m_apcPicYuv[i][1]   = NULL;
    m_pcPicYuvPred[i]   = NULL;
    m_pcPicYuvResi[i]   = NULL;

#if PARALLEL_MERGED_DEBLK
    m_pcPicYuvDeblkBuf[i]     = NULL;
#endif
  }

  m_bReconstructed    = false;
  m_iPicSizeIndex = 0;
}

TComPic::~TComPic()
{
  destroy();
}

Void TComPic::create( Int iWidth, Int iHeight, UInt uiMaxWidth, UInt uiMaxHeight, UInt uiMaxDepth, Int iPicSizeIndex, Bool bIsVirtual )
{
#if JCT_ARC
  m_iPicSizeIndex = iPicSizeIndex;

  for (int i=0; i<NUM_PIC_RESOLUTIONS; ++i){
    m_iWidth[i] = iWidth >> i;
    m_iHeight[i] = iHeight >> i;
  }
  m_uiMaxWidth = uiMaxWidth;
  m_uiMaxHeight = uiMaxHeight;
  m_uiMaxDepth = uiMaxDepth;

  for (int i=0; i<NUM_PIC_RESOLUTIONS; ++i){
    m_apcPicSym[i]     = new TComPicSym;  m_apcPicSym[i]   ->create( m_iWidth[i], m_iHeight[i], uiMaxWidth, uiMaxHeight, uiMaxDepth );
  }
#endif
  // Only create the original picture data at level 0 initially : other levels created as needed on the fly
  if (!bIsVirtual)
  {
    m_apcPicYuv[0][0]  = new TComPicYuv;  m_apcPicYuv[0][0]->create( iWidth, iHeight, uiMaxWidth, uiMaxHeight, uiMaxDepth );
  }

  // Only create the reconstructed picture data at level iPicSizeIndex initially : other  levels created as needed on the fly
  m_apcPicYuv[iPicSizeIndex][1]  = new TComPicYuv;  m_apcPicYuv[iPicSizeIndex][1]->create( m_iWidth[iPicSizeIndex], m_iHeight[iPicSizeIndex], uiMaxWidth, uiMaxHeight, uiMaxDepth );
  
#if PARALLEL_MERGED_DEBLK
  m_pcPicYuvDeblkBuf[iPicSizeIndex]  = new TComPicYuv;  m_pcPicYuvDeblkBuf[iPicSizeIndex]->create( m_iWidth[iPicSizeIndex], m_iHeight[iPicSizeIndex], uiMaxWidth, uiMaxHeight, uiMaxDepth );
#endif

  /* there are no SEI messages associated with this picture initially */
  m_SEIs = NULL;

  return;
}
#if JCT_ARC
Void TComPic::setPictureSizeIdx( Int iPicSizeIndex ){

  if ( iPicSizeIndex!=m_iPicSizeIndex ){
    m_apcPicYuv[iPicSizeIndex][1]  = new TComPicYuv;  m_apcPicYuv[iPicSizeIndex][1]->create( m_iWidth[iPicSizeIndex], m_iHeight[iPicSizeIndex], m_uiMaxWidth, m_uiMaxHeight, m_uiMaxDepth );
    if (m_apcPicYuv[m_iPicSizeIndex][1]!=NULL){
      m_apcPicYuv[m_iPicSizeIndex][1]->destroy();
      delete m_apcPicYuv[m_iPicSizeIndex][1];
      m_apcPicYuv[m_iPicSizeIndex][1]  = NULL;
    }

#if PARALLEL_MERGED_DEBLK
    m_pcPicYuvDeblkBuf[iPicSizeIndex]  = new TComPicYuv;  m_pcPicYuvDeblkBuf[iPicSizeIndex]->create( m_iWidth[iPicSizeIndex], m_iHeight[iPicSizeIndex], m_uiMaxWidth, m_uiMaxHeight, m_uiMaxDepth );
    if (m_pcPicYuvDeblkBuf[m_iPicSizeIndex]!=NULL){
      m_pcPicYuvDeblkBuf[m_iPicSizeIndex]->destroy();
      delete m_pcPicYuvDeblkBuf[m_iPicSizeIndex];
      m_pcPicYuvDeblkBuf[m_iPicSizeIndex] = NULL;
    }
#endif
  }

  // Delete all originals except the top-level, which will be over-written anyway.
  for (int i=0; i<NUM_PIC_RESOLUTIONS; ++i){
    if (i>0 && m_apcPicYuv[i][0] != NULL){
      m_apcPicYuv[i][0]->destroy();
      delete m_apcPicYuv[i][0];
      m_apcPicYuv[i][0]  = NULL;
    }

  }

  m_iPicSizeIndex = iPicSizeIndex;

}

TComPicYuv*   TComPic::getPicYuvOrg(Int i){

  if (i==0){
    return m_apcPicYuv[0][0];
  } else {
    if ( m_apcPicYuv[i][0]!=NULL ){
      return m_apcPicYuv[i][0];
    } else {
      m_apcPicYuv[i][0]  = new TComPicYuv;  m_apcPicYuv[i][0]->create( m_iWidth[i], m_iHeight[i], m_uiMaxWidth, m_uiMaxHeight, m_uiMaxDepth );
      downScale( 0, i, m_apcPicYuv[0][0], m_apcPicYuv[i][0] );
      return m_apcPicYuv[i][0];
    }
  }

}

TComPicYuv*   TComPic::getPicYuvRec(Int i){

  Bool bScale = !m_bReconstructed;
  if (m_apcPicYuv[i][1]==NULL){
    m_apcPicYuv[i][1]  = new TComPicYuv;  m_apcPicYuv[i][1]->create( m_iWidth[i], m_iHeight[i], m_uiMaxWidth, m_uiMaxHeight, m_uiMaxDepth );
    bScale = true;
  }

  // Scale will only be enforced if requested resolution is not current resolution
  if (i!=m_iPicSizeIndex){
    if (bScale==true){
      if (i==0) { //scale up
        assert(m_iPicSizeIndex == 1);
        upScale( m_iPicSizeIndex, 0, m_apcPicYuv[m_iPicSizeIndex][1], m_apcPicYuv[0][1] );
      } else {// FIXME assume we're scaling down from 0. Need to support other cases
        assert(m_iPicSizeIndex==0);
        downScale( 0, i, m_apcPicYuv[m_iPicSizeIndex][1], m_apcPicYuv[i][1] );
      }
    }
  }
  return m_apcPicYuv[i][1];

}

#if PARALLEL_MERGED_DEBLK
TComPicYuv*   TComPic::getPicYuvDeblkBuf(){
  return  getPicYuvDeblkBuf(m_iPicSizeIndex);
}

TComPicYuv*  TComPic::getPicYuvDeblkBuf(Int i) {
  if (m_pcPicYuvDeblkBuf[i]==NULL){
    m_pcPicYuvDeblkBuf[i]  = new TComPicYuv;
    m_pcPicYuvDeblkBuf[i]->create( m_iWidth[i], m_iHeight[i], m_uiMaxWidth, m_uiMaxHeight, m_uiMaxDepth );
  }
  return  m_pcPicYuvDeblkBuf[i];
}
#endif

void TComPic::resetRecData(){

  for (int i=0; i<NUM_PIC_RESOLUTIONS; ++i){
    if (m_apcPicYuv[i][1])
    {
      m_apcPicYuv[i][1]->destroy();
      delete m_apcPicYuv[i][1];
      m_apcPicYuv[i][1]  = NULL;
    }
  
#if PARALLEL_MERGED_DEBLK
    if (m_pcPicYuvDeblkBuf[i])
    {
      m_pcPicYuvDeblkBuf[i]->destroy();
      delete m_pcPicYuvDeblkBuf[i];
      m_pcPicYuvDeblkBuf[i]  = NULL;
    }
#endif
  }
}

Void TComPic::destroy()
{
  for (int i=0; i<NUM_PIC_RESOLUTIONS; ++i){
    if (m_apcPicSym[i])
    {
      m_apcPicSym[i]->destroy();
      delete m_apcPicSym[i];
      m_apcPicSym[i] = NULL;
    }
  
    if (m_apcPicYuv[i][0])
    {
      m_apcPicYuv[i][0]->destroy();
      delete m_apcPicYuv[i][0];
      m_apcPicYuv[i][0]  = NULL;
    }
  }

  resetRecData();

  if (m_SEIs!=NULL){
    delete m_SEIs;
    m_SEIs = NULL;
  }

}
#endif
#if AMVP_BUFFERCOMPRESS
Void TComPic::compressMotion()
{
  TComPicSym* pPicSym = getPicSym();
  for ( UInt uiCUAddr = 0; uiCUAddr < pPicSym->getFrameHeightInCU()*pPicSym->getFrameWidthInCU(); uiCUAddr++ )
  {
    TComDataCU* pcCU = pPicSym->getCU(uiCUAddr);
    pcCU->compressMV();
  }
}
#endif

