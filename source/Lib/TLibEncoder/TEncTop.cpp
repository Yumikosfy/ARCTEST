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

/** \file     TEncTop.cpp
    \brief    encoder class
*/

#include "../TLibCommon/CommonDef.h"
#include "TEncTop.h"

// ====================================================================================================================
// Constructor / destructor / create / destroy
// ====================================================================================================================

TEncTop::TEncTop()
{
  m_iPOCLast          = -1;
  m_iNumPicRcvd       =  0;
  m_uiNumAllPicCoded  =  0;
  m_pppcRDSbacCoder   =  NULL;
  m_pppcBinCoderCABAC =  NULL;
  m_cRDGoOnSbacCoder.init( &m_cRDGoOnBinCoderCABAC );
#if ENC_DEC_TRACE
  g_hTrace = fopen( "TraceEnc.txt", "wb" );
  g_bJustDoIt = g_bEncDecTraceDisable;
  g_nSymbolCounter = 0;
#endif

  m_iMaxRefPicNum     = 0;

  m_pcSPS[0] = new TComSPS;
  for (int j=0; j<NUM_PIC_RESOLUTIONS; ++j ){
    m_pcPPS[j] = new TComPPS;
  }
}

TEncTop::~TEncTop()
{
#if ENC_DEC_TRACE
  fclose( g_hTrace );
#endif
  delete m_pcSPS[0];
  for (int j=0; j<NUM_PIC_RESOLUTIONS; ++j ){
    delete m_pcPPS[j];
  }

}

Void TEncTop::create ()
{
  // initialize global variables
  initROM();
 
  // create processing unit classes
  m_cGOPEncoder.        create( getSourceWidth(), getSourceHeight(), g_uiMaxCUWidth, g_uiMaxCUHeight );
  m_cSliceEncoder.      create( getSourceWidth(), getSourceHeight(), g_uiMaxCUWidth, g_uiMaxCUHeight, g_uiMaxCUDepth );
  m_cCuEncoder.         create( g_uiMaxCUDepth, g_uiMaxCUWidth, g_uiMaxCUHeight );
#if MTK_SAO
  if (m_bUseSAO)
  {
    for (int j=0; j<NUM_PIC_RESOLUTIONS; ++j){
      m_cEncSAO[j].create( getSourceWidth()>>j, getSourceHeight()>>j, g_uiMaxCUWidth, g_uiMaxCUHeight, g_uiMaxCUDepth );
      m_cEncSAO[j].createEncBuffer();
    }
  }
#endif
  for (int j=0; j<NUM_PIC_RESOLUTIONS; ++j){
    m_cAdaptiveLoopFilter[j].create( getSourceWidth()>>j, getSourceHeight()>>j, g_uiMaxCUWidth, g_uiMaxCUHeight, g_uiMaxCUDepth );
  }
  m_cLoopFilter.create( g_uiMaxCUDepth );

#if MQT_BA_RA && MQT_ALF_NPASS
  if(m_bUseALF)
  {
    for (int j=0; j<NUM_PIC_RESOLUTIONS; ++j){
      m_cAdaptiveLoopFilter[j].setGOPSize( getGOPSize() );
      m_cAdaptiveLoopFilter[j].createAlfGlobalBuffers(m_iALFEncodePassReduction);
    }
  }
#endif

  // if SBAC-based RD optimization is used
  if( m_bUseSBACRD )
  {
    m_pppcRDSbacCoder = new TEncSbac** [g_uiMaxCUDepth+1];
    m_pppcBinCoderCABAC = new TEncBinCABAC** [g_uiMaxCUDepth+1];
    
    for ( Int iDepth = 0; iDepth < g_uiMaxCUDepth+1; iDepth++ )
    {
      m_pppcRDSbacCoder[iDepth] = new TEncSbac* [CI_NUM];
      m_pppcBinCoderCABAC[iDepth] = new TEncBinCABAC* [CI_NUM];
      
      for (Int iCIIdx = 0; iCIIdx < CI_NUM; iCIIdx ++ )
      {
        m_pppcRDSbacCoder[iDepth][iCIIdx] = new TEncSbac;
        m_pppcBinCoderCABAC [iDepth][iCIIdx] = new TEncBinCABAC;
        m_pppcRDSbacCoder   [iDepth][iCIIdx]->init( m_pppcBinCoderCABAC [iDepth][iCIIdx] );
      }
    }
  }
}

Void TEncTop::destroy ()
{
#if MQT_BA_RA && MQT_ALF_NPASS
  if(m_bUseALF)
  {
    for (int j=0; j<NUM_PIC_RESOLUTIONS; ++j){
      m_cAdaptiveLoopFilter[j].destroyAlfGlobalBuffers();
    }
  }
#endif

  // destroy processing unit classes
  m_cGOPEncoder.        destroy();
  m_cSliceEncoder.      destroy();
  m_cCuEncoder.         destroy();
#if MTK_SAO
  if (m_pcSPS[0]->getUseSAO())
  {
    for (int j=0; j<NUM_PIC_RESOLUTIONS; ++j){
      m_cEncSAO[j].destroy();
      m_cEncSAO[j].destroyEncBuffer();
    }
  }
#endif
  for (int j=0; j<NUM_PIC_RESOLUTIONS; ++j){
    m_cAdaptiveLoopFilter[j].destroy();
  }
  m_cLoopFilter.        destroy();
  
  // SBAC RD
  if( m_bUseSBACRD )
  {
    Int iDepth;
    for ( iDepth = 0; iDepth < g_uiMaxCUDepth+1; iDepth++ )
    {
      for (Int iCIIdx = 0; iCIIdx < CI_NUM; iCIIdx ++ )
      {
        delete m_pppcRDSbacCoder[iDepth][iCIIdx];
        delete m_pppcBinCoderCABAC[iDepth][iCIIdx];
      }
    }
    
    for ( iDepth = 0; iDepth < g_uiMaxCUDepth+1; iDepth++ )
    {
      delete [] m_pppcRDSbacCoder[iDepth];
      delete [] m_pppcBinCoderCABAC[iDepth];
    }
    
    delete [] m_pppcRDSbacCoder;
    delete [] m_pppcBinCoderCABAC;
  }
  
  // destroy ROM
  destroyROM();
  
  return;
}

Void TEncTop::init()
{
  UInt *aTable4=NULL, *aTable8=NULL;
#if QC_MOD_LCEC
  UInt* aTableLastPosVlcIndex=NULL; 
#endif
  // initialize SPS and PPS
  xInitSPS();
  for (int j=0; j<NUM_PIC_RESOLUTIONS; ++j){
    xInitPPS(j);
  }
  

  // initialize processing unit classes
  m_cGOPEncoder.  init( this );
  m_cSliceEncoder.init( this );
  m_cCuEncoder.   init( this );
  
  // initialize transform & quantization class
  m_pcCavlcCoder = getCavlcCoder();
#if !CAVLC_COEF_LRG_BLK
  aTable8 = m_pcCavlcCoder->GetLP8Table();
#endif
  aTable4 = m_pcCavlcCoder->GetLP4Table();
#if QC_MOD_LCEC
  aTableLastPosVlcIndex=m_pcCavlcCoder->GetLastPosVlcIndexTable();
  
  m_cTrQuant.init( g_uiMaxCUWidth, g_uiMaxCUHeight, 1 << m_uiQuadtreeTULog2MaxSize, m_iSymbolMode, aTable4, aTable8, 
    aTableLastPosVlcIndex, m_bUseRDOQ, true );
#else
  m_cTrQuant.init( g_uiMaxCUWidth, g_uiMaxCUHeight, 1 << m_uiQuadtreeTULog2MaxSize, m_iSymbolMode, aTable4, aTable8, m_bUseRDOQ, true );
#endif
  
  // initialize encoder search class
  m_cSearch.init( this, &m_cTrQuant, m_iSearchRange, m_bipredSearchRange, m_iFastSearch, 0, &m_cEntropyCoder, &m_cRdCost, getRDSbacCoder(), getRDGoOnSbacCoder() );

#if MQT_ALF_NPASS
  if(m_bUseALF)
  {
    for (int j=0; j<NUM_PIC_RESOLUTIONS; ++j){
      m_cAdaptiveLoopFilter[j].setALFEncodePassReduction( m_iALFEncodePassReduction );
    }
  }
#endif

  m_iMaxRefPicNum = 0;
}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

Void TEncTop::deletePicBuffer()
{
  TComList<TComPic*>::iterator iterPic = m_cListPic.begin();
  Int iSize = Int( m_cListPic.size() );
  
  for ( Int i = 0; i < iSize; i++ )
  {
    TComPic* pcPic = *(iterPic++);
    
    pcPic->destroy();
    delete pcPic;
    pcPic = NULL;
  }
}

/**
 - Application has picture buffer list with size of GOP + 1
 - Picture buffer list acts like as ring buffer
 - End of the list has the latest picture
 .
 \param   bEos                true if end-of-sequence is reached
 \param   pcPicYuvOrg         original YUV picture
 \retval  rcListPicYuvRecOut  list of reconstruction YUV pictures
 \retval  rcListBitstreamOut  list of output bitstreams
 \retval  iNumEncoded         number of encoded pictures
 */
Void TEncTop::encode( bool bEos, TComPicYuv* pcPicYuvOrg, TComList<TComPicYuv*>& rcListPicYuvRecOut, list<AccessUnit>& accessUnitsOut, Int& iNumEncoded )
{
  TComPic* pcPicCurr = NULL;

  // get original YUV
  xGetNewPicBuffer( pcPicCurr );

  // Copy in to highest resolution buffer
  pcPicYuvOrg->copyToPic( pcPicCurr->getPicYuvOrg( 0 ) );

  if ( m_iPOCLast != 0 && ( m_iNumPicRcvd != m_iGOPSize && m_iGOPSize ) && !bEos )
  {
    iNumEncoded = 0;
    return;
  }

  m_cGOPEncoder.compressGOP(m_iPOCLast, m_iNumPicRcvd, m_cListPic, rcListPicYuvRecOut, accessUnitsOut);

  iNumEncoded         = m_iNumPicRcvd;
  m_iNumPicRcvd       = 0;
  m_uiNumAllPicCoded += iNumEncoded;

  if (bEos)
  {
    m_cGOPEncoder.printOutSummary (m_uiNumAllPicCoded);
  }
}

// ====================================================================================================================
// Protected member functions
// ====================================================================================================================

/**
 - Application has picture buffer list with size of GOP + 1
 - Picture buffer list acts like as ring buffer
 - End of the list has the latest picture
 .
 \retval rpcPic obtained picture buffer
 */
Void TEncTop::xGetNewPicBuffer ( TComPic*& rpcPic )
{
  TComSlice::sortPicList(m_cListPic);
  
  // bug-fix - erase frame memory (previous GOP) which is not used for reference any more
  if (m_cListPic.size() >= (UInt)(m_iGOPSize + 2 * getNumOfReference() + 1) )  // 2)   //  K. Lee bug fix - for multiple reference > 2
  {
    rpcPic = m_cListPic.popFront();
    
    // is it necessary without long-term reference?
    if ( rpcPic->getERBIndex() > 0 && abs(rpcPic->getPOC() - m_iPOCLast) <= 0 )
    {
      m_cListPic.pushFront(rpcPic);
      
      TComList<TComPic*>::iterator iterPic  = m_cListPic.begin();
      rpcPic = *(++iterPic);
      if ( abs(rpcPic->getPOC() - m_iPOCLast) <= m_iGOPSize )
      {
        rpcPic = new TComPic;
        rpcPic->create( m_iSourceWidth, m_iSourceHeight, g_uiMaxCUWidth, g_uiMaxCUHeight, g_uiMaxCUDepth, 0 , false );
      }
      else
      {
        m_cListPic.erase( iterPic );
        TComSlice::sortPicList( m_cListPic );
      }
    }
  }
  else
  {
    rpcPic = new TComPic;
    rpcPic->create( m_iSourceWidth, m_iSourceHeight, g_uiMaxCUWidth, g_uiMaxCUHeight, g_uiMaxCUDepth, 0, false );
  }
  
  m_cListPic.pushBack( rpcPic );
  rpcPic->setReconMark (false);
  
  m_iPOCLast++;
  m_iNumPicRcvd++;

  rpcPic->setPOC( m_iPOCLast );
  // mark it should be extended
  rpcPic->getPicYuvRec()->setBorderExtension(false);
}

Void TEncTop::xInitSPS()
{
  m_pcSPS[0]->setNominalWidth  ( m_iSourceWidth      );
  m_pcSPS[0]->setNominalHeight ( m_iSourceHeight     );
  m_pcSPS[0]->setSPSId( 0 );
  m_pcSPS[0]->setPad           ( m_aiPad             );
  m_pcSPS[0]->setMaxCUWidth    ( g_uiMaxCUWidth      );
  m_pcSPS[0]->setMaxCUHeight   ( g_uiMaxCUHeight     );
  m_pcSPS[0]->setMaxCUDepth    ( g_uiMaxCUDepth      );
  m_pcSPS[0]->setMinTrDepth    ( 0                   );
  m_pcSPS[0]->setMaxTrDepth    ( 1                   );
  
#if E057_INTRA_PCM
  m_pcSPS[0]->setPCMLog2MinSize (m_uiPCMLog2MinSize);
#endif

  m_pcSPS[0]->setUseALF        ( m_bUseALF           );
  
  m_pcSPS[0]->setQuadtreeTULog2MaxSize( m_uiQuadtreeTULog2MaxSize );
  m_pcSPS[0]->setQuadtreeTULog2MinSize( m_uiQuadtreeTULog2MinSize );
  m_pcSPS[0]->setQuadtreeTUMaxDepthInter( m_uiQuadtreeTUMaxDepthInter    );
  m_pcSPS[0]->setQuadtreeTUMaxDepthIntra( m_uiQuadtreeTUMaxDepthIntra    );
  
  m_pcSPS[0]->setUseDQP        ( m_iMaxDeltaQP != 0  );
  m_pcSPS[0]->setUseLDC        ( m_bUseLDC           );
  m_pcSPS[0]->setUsePAD        ( m_bUsePAD           );
  
  m_pcSPS[0]->setUseMRG        ( m_bUseMRG           ); // SOPH:

#if LM_CHROMA 
  m_pcSPS[0]->setUseLMChroma   ( m_bUseLMChroma           );  
#endif
  
  m_pcSPS[0]->setMaxTrSize   ( 1 << m_uiQuadtreeTULog2MaxSize );
  
#if DCM_COMB_LIST
  m_pcSPS[0]->setUseLComb    ( m_bUseLComb           );
  m_pcSPS[0]->setLCMod       ( m_bLCMod   );
#endif

  Int i;
#if HHI_AMVP_OFF
  for ( i = 0; i < g_uiMaxCUDepth; i++ )
  {
    m_pcSPS[0]->setAMVPMode( i, AM_NONE );
  }
#else
  for ( i = 0; i < g_uiMaxCUDepth; i++ )
  {
    m_pcSPS[0]->setAMVPMode( i, AM_EXPL );
  }
#endif
  
  
#if HHI_RMP_SWITCH
  m_pcSPS[0]->setUseRMP( m_bUseRMP );
#endif
  
  m_pcSPS[0]->setBitDepth    ( g_uiBitDepth        );
  m_pcSPS[0]->setBitIncrement( g_uiBitIncrement    );

#if MTK_NONCROSS_INLOOP_FILTER
  m_pcSPS[0]->setLFCrossSliceBoundaryFlag( m_bLFCrossSliceBoundaryFlag );
#endif
#if MTK_SAO
  m_pcSPS[0]->setUseSAO             ( m_bUseSAO         );
#endif

  if ( m_bTLayering )
  {
    Int iMaxTLayers = 1;
    for ( i = 1; ; i++)
    {
      iMaxTLayers = i;
      if ( (m_iRateGOPSize >> i) == 0 ) 
      {
        break;
      }
    }
  
    m_pcSPS[0]->setMaxTLayers( (UInt)iMaxTLayers );

    Bool bTemporalIdNestingFlag = true;
    for ( i = 0; i < m_pcSPS[0]->getMaxTLayers()-1; i++ )
    {
      if ( !m_abTLayerSwitchingFlag[i] )
      {
        bTemporalIdNestingFlag = false;
        break;
      }
    }

    m_pcSPS[0]->setTemporalIdNestingFlag( bTemporalIdNestingFlag );
  }
  else
  {
    m_pcSPS[0]->setMaxTLayers( 1 );
    m_pcSPS[0]->setTemporalIdNestingFlag( false );
  }
#if E057_INTRA_PCM && E192_SPS_PCM_BIT_DEPTH_SYNTAX
  m_pcSPS[0]->setPCMBitDepthLuma (g_uiPCMBitDepthLuma);
  m_pcSPS[0]->setPCMBitDepthChroma (g_uiPCMBitDepthChroma);
#endif
#if E057_INTRA_PCM && E192_SPS_PCM_FILTER_DISABLE_SYNTAX
  m_pcSPS[0]->setPCMFilterDisableFlag  ( m_bPCMFilterDisableFlag );
#endif
}

Void TEncTop::xInitPPS(Int j)
{
  m_pcPPS[j]->setPPSId( j );
  m_pcPPS[j]->setSPS(m_pcSPS[0]);
  m_pcPPS[j]->setPictureSizeIdx(j);
#if CONSTRAINED_INTRA_PRED
  m_pcPPS[j]->setConstrainedIntraPred( m_bUseConstrainedIntraPred );
#endif

  if ( m_pcSPS[0]->getTemporalIdNestingFlag() ) 
  {
    m_pcPPS[j]->setNumTLayerSwitchingFlags( 0 );
    for ( UInt i = 0; i < m_pcSPS[0]->getMaxTLayers() - 1; i++ )
    {
      m_pcPPS[j]->setTLayerSwitchingFlag( i, true );
    }
  }
  else
  {
    m_pcPPS[j]->setNumTLayerSwitchingFlags( m_pcSPS[0]->getMaxTLayers() - 1 );
    for ( UInt i = 0; i < m_pcPPS[j]->getNumTLayerSwitchingFlags(); i++ )
    {
      m_pcPPS[j]->setTLayerSwitchingFlag( i, m_abTLayerSwitchingFlag[i] );
    }
  }   

#if SUB_LCU_DQP
  if( m_pcPPS[j]->getSPS()->getUseDQP() )
  {
    m_pcPPS[j]->setMaxCuDQPDepth( m_iMaxCuDQPDepth );
    m_pcPPS[j]->setMinCuDQPSize( m_pcPPS[j]->getSPS()->getMaxCUWidth() >> ( m_pcPPS[j]->getMaxCuDQPDepth()) );
  }
  else
  {
    m_pcPPS[j]->setMaxCuDQPDepth( 0 );
    m_pcPPS[j]->setMinCuDQPSize( m_pcPPS[j]->getSPS()->getMaxCUWidth() >> ( m_pcPPS[j]->getMaxCuDQPDepth()) );
  }
#endif
}
