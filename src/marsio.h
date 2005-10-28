/*
$Header: /usr/local/cvs/repository/mars2mseed/src/marsio.h,v 1.2 2005-10-28 04:14:52 chad Exp $
*/
#ifndef MARSIO_H_
 #define MARSIO_H_

 #include <stddef.h>
 #include <unistd.h>
 #include <stdio.h>
 #include <time.h>
 
 #include "mars.h"
 
 #define msStreamActive       0x00000001 
 #define msLongHeaders        0x00000100
 
 #define msCheckStatus(a,b)   ( (a)&(b) )
 
 #define mbHeaderMacros
 #define mbGetMagic(a)	      ((((m88Head *)(a))->format_id).magic)
 #define mbGetBlockFormat(a)  ((((m88Head *)(a))->format_id).block_format)
 #define mbGetDataFormat(a)   ((((m88Head *)(a))->format_id).data_format)
 
 #define mbGetChan(a)	      (((m88Head *)(a))->chno)
 #define mbGetSamp(a)	      (1<<(((m88Head *)(a))->samp_rate))
 #define mbGetSampRate(a)     (1000.0/mbGetSamp(a))
 #define mbGetMaxamp(a)	      (((m88Head *)(a))->maxamp)
 #define mbGetScale(a)	      (1<<((m88Head *)(a))->scale)
 #define mbGetTime(a)	      ( ((((m88Head *)(a))->format_id).data_format < LITE_BLOCK_FORMAT) ? (((m88Head *)(a))->time).time : ((mlHead *)(a))->time  )

 typedef struct
 {
  FILE	*hf;
  off_t	offset;
  char	block[marsBlockSize];
  
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
 
 long *marsBlockDecodeData(char *block,long *scale);
 int marsStreamDumpBlock(marsStream *hMS);

 double marsBlockGetGain(char *blk);
 long marsBlockGetScaleFactor(char *blk);
 char *mbGetStationCode(char *blk);
 long mbGetStationSerial(char *blk);
 
 #ifdef __cplusplus
  }
 #endif
#endif
