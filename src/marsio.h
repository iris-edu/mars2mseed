/*
$Header: /usr/local/cvs/repository/mars2mseed/src/marsio.h,v 1.4 2006-07-27 17:28:00 chad Exp $
*/
#ifndef MARSIO_H_
 #define MARSIO_H_

 #include <stdio.h>
 #include <time.h>
 
 #include <libmseed.h>

 #include "mars.h"
 
 #define msStreamActive       0x00000001 
 #define msLongHeaders        0x00000100
 
 #define msCheckStatus(a,b)   ( (a)&(b) )
 
 #define mbHeaderMacros
 #define mbGetMagic(a)	      ((a)->m88Head.format_id.magic)
 #define mbGetBlockFormat(a)  ((a)->m88Head.format_id.block_format)
 #define mbGetDataFormat(a)   ((a)->m88Head.format_id.data_format)
 
 #define mbGetChan(a)	      ((a)->m88Head.chno)
 #define mbGetSamp(a)	      (1<<((a)->m88Head.samp_rate))
 #define mbGetSampRate(a)     (1000.0/mbGetSamp(a))
 #define mbGetMaxamp(a)	      ((a)->m88Head.maxamp)
 #define mbGetScale(a)	      (1<<(a)->m88Head.scale)
 #define mbGetTime(a)	      ( ((a)->m88Head.format_id.data_format < LITE_BLOCK_FORMAT) ? (a)->m88Head.time.time : (a)->mlHead.time  )

 typedef union
 {
  char		data[marsBlockSize];
  leFormat	leFormat;
  leTime	leTime;
  m88Head	m88Head;
  mlHead	mlHead;
  m88Block	m88Block;
  mlBlock	mlBlock;
 } block_t;

 typedef struct
 {
  FILE	*hf;
  off_t	offset;
  block_t	block;
  
  size_t  status;
  
  /*	file info	*/
  off_t		size;
  time_t	time;
  char 		name[4096];
 
 } marsStream;
 
/*********************************************************
***   Function Prototypes
**********************************************************/
 #ifdef __cplusplus
  extern "C" {
 #endif
 
 marsStream *marsStreamOpen(char *name);
 marsStream *marsStreamGetNextBlock(int verbose);
 marsStream *marsStreamGetCurrent(void);
 
 void marsStreamClose(void);
 void m88SwapBlock(m88Block *blk);
 
 int *marsBlockDecodeData(block_t *block,int *scale);
 int marsStreamDumpBlock(marsStream *hMS);

 double marsBlockGetGain(block_t *blk);
 int marsBlockGetScaleFactor(block_t *blk);
 char *mbGetStationCode(block_t *blk);
 int mbGetStationSerial(block_t *blk);
 
 #ifdef __cplusplus
  }
 #endif
#endif
