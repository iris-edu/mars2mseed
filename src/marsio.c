/*
Revision 1.7  2006-07-27 16:55:00-08  ctrabant
Change long's to int's to make 64-bit possible

Revision 1.6  2005-05-03 14:00:00-08  ctrabant

Revision 1.5  2005-02-17 14:00:00-08  ctrabant
Modifications for mars2mseed.

Revision 1.4  2005-02-04 14:31:32+00  aladino
Backup revision

Revision 1.3  2005-02-04 13:11:49+00  aladino
Backup revision

Revision 1.2  2005-02-03 16:24:04+00  aladino
Backup revision

Revision 1.1  2005-02-03 14:27:13+00  aladino
Backup revision
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h> 
#include <sys/stat.h> 
#include <unistd.h> 

#include <string.h>
#include <math.h>

#include <libmseed.h>

#include "marsio.h"

static marsStream MS;
static int m88BlockDecodedData[marsBlockSamples];
static char mbNameBuf[64];


int isMarsDataBlock (char *blk)
{
  m88Head *head = (m88Head *)blk;
  
  return ( (((head->format_id).magic == LEMAGICle) || ((head->format_id).magic == LEMAGICbe)) &&
	   (((head->format_id).block_format == DATABLK_FORMAT) ||
	    ((head->format_id).block_format == LITE_BLOCK_FORMAT)) );
}


char *mbGetStationCode (char *blk)
{
  switch( mbGetBlockFormat(blk) )
    {
    case DATABLK_FORMAT:
      sprintf (mbNameBuf, "%04X", (unsigned int)(((m88Head *)blk)->dev_id & 0xFFFF));
      break;
    case LITE_BLOCK_FORMAT:
      strcpy (mbNameBuf, ((mlHead*)blk)->station_name);
      break;
    default:
      sprintf (mbNameBuf, "????");
      break;
    }
  
  return mbNameBuf;
}


int mbGetStationSerial (char *blk)
{
  switch( mbGetBlockFormat(blk) )
    {
    case DATABLK_FORMAT:
      return (int)(((m88Head *)blk)->dev_id & 0xFFFF);
      break;
    case LITE_BLOCK_FORMAT:
      return 0;
      break;
    default:
      return -1;
      break;
    }
  
  return -1;
}


int convert_word (short input, short format)
{
  int exponent;
  int mantissa;
  int shift;
  
  switch (format)
    {
    case 0:
      return (int) input;
      break;
    case 1:
      mantissa = input & (~0x03);
      exponent = input & 0x03;
      return ((int) mantissa << (16 - exponent));
      break;
    case 2:
      mantissa = input & (~0x07);
      exponent = input & 0x07;
      return ((int) mantissa << (16 - exponent));
      break;
    case 3:
      mantissa = input & (~0x0f);
      exponent = input & 0x0f;
      return ((int) mantissa << (16 - exponent));
      break;
    /***** MARSlite data format here! ************************/
    case 4:    /* MARSlite format; non-differential and     */
    case 5:    /* differential use the same word format     */
      mantissa = input & (~0x07);
      exponent = input & 0x07;
      shift = 2 * exponent;
      return ((int) mantissa << (16 -shift));
      /* your application program will want to convert this to a      */
      /* floating point number, taking the "scale" value into account */
      break;
    default:
      fprintf (stderr, "convert_word(): illegal data format %d\n", format);
      return 0L;
    }
}


int marsBlockGetScaleFactor (char *blk)
{
/*	Negative values means multiply
	Positive means divide
	0 invalid
*/
  m88Head   *head=(m88Head *)blk;
  int 	     scale;
  
  switch ( mbGetDataFormat(head) )
    {
    case 0:
      scale = -(1 << head->scale);
      break;
    case 1:
    case 2:
    case 3:
      scale = (1<<(LEm88Base - head->scale));
      break;
    case 4:
    case 5:
      scale = (1<<(LEliteBase - head->scale));
      break;
    default: /* invalid data format */
      scale = 0;
      fprintf (stderr, "marsBlockGetScaleFactor() : invalid data format 0x%1X\n",
	       mbGetDataFormat(head));
    }
  
  return scale;
}


double marsBlockGetGain (char *blk)
{
  m88Head   *head=(m88Head *)blk;
  double     gain=0;
  
  switch ( mbGetDataFormat(head) )
    {
    case 0:
      gain = (double)( 1 << head->scale );
      break;
    case 1:
    case 2:
    case 3:
      gain = 1.0 / (double)(1 << (LEm88Base - head->scale));
      break;
    case 4:
    case 5:
      gain = 1.0 / (double)(1 << (LEliteBase - head->scale));
      break;
    default:
      gain = 0;
      fprintf (stderr, "marsBlockGetGain() : invalid data format 0x%1X\n",
	       (head->format_id).data_format);
    }
  
  return gain;
}


int *marsBlockDecodeData (char *block, int *scale)
{
  int *data=m88BlockDecodedData;
  m88Block *buf=(m88Block *)block;
  
  int    i;
  int    data_format;
  int   mantissa;
  short  temp;
  int    exponent;
  int	 block_duration;
  int	 samp_interval;
  int    shift;
  int	 sum = 0;
  short  codedsum;

  /*
   *	correct for timing error in MARS-88 data for sampling intervals >= 32 msec.
   *	In these cases, blocks are delayed by one block length. The block
   *	length can be calculated: 1 << (samp -1 ) 
   *	where samp is the 2's log of the sampling interval.
   */
  samp_interval = buf->head.samp_rate;
  if (samp_interval >= 5)
    {
      block_duration =  1 << (samp_interval-1);
      buf->head.time.time -= block_duration;
    }
  
  if (buf->head.format_id.block_format == MONBLK_FORMAT)
    {
      data_format = 0;
    }
  else
    {
      data_format = buf->head.format_id.data_format;
    }
  if (data_format == 5)
    {
      codedsum=((mlHead *)block)->dstart;
      sum = convert_word (codedsum, data_format);
      /* P.L.Bragato:	QUESTO per il vecchio formato di uscita di "lt2m88"
	 Chad's translation: THIS for the old format of escape of "lt2m88"
	 codedsum = ((buf->head.dummy[0]) << 8) | ( (buf->head.dummy[1]) & 0x00ff);
	 sum = convert_word (codedsum, data_format);
      */
#ifdef DEBUG
      fprintf(stderr, "START at %d (0x%04x\n",sum,(buf->head.dstart)&0xffff);
#endif
    }	
  
  switch (data_format) {
  case 0:
    for (i = 0; i < 500; i++)
      {
	data[i] = (int) buf->data[i];
      }
    *scale=(1<<(buf->head).scale);
    break;
  case 1:
    for (i = 0; i < 500; i++)
      {
	temp = buf->data[i];
	mantissa = temp & (~0x03);
	exponent = temp & 0x03;
	data[i] = (int) mantissa << (16 - exponent);
      }
    *scale=(1<<(LEm88Base -(buf->head).scale));
    break;
  case 2:
    for (i = 0; i < 500; i++)
      {
	temp = buf->data[i];
	mantissa = temp & (~0x07);
	exponent = temp & 0x07;
	data[i] = (int) mantissa << (16 - exponent);
      }
    *scale=(1<<(LEm88Base -(buf->head).scale));
    break;
  case 3:
    for (i = 0; i < 500; i++)
      {
	temp = buf->data[i];
	mantissa = temp & (~0x0f);
	exponent = temp & 0x0f;
	data[i] = (int) mantissa << (16 - exponent);
      }
    *scale=(1<<(LEm88Base -(buf->head).scale));
    break;
  /***** MARSlite data format here! ************************/
  case 4:    /* MARSlite format; non-differential   */
    for (i=0; i<500; i++)
      {
	temp = buf->data[i];
	mantissa = temp & (~0x07);
	exponent = temp & 0x07;
	shift = 2 * exponent;
	data[i] = (int) mantissa << (16 - shift);
      } 
    *scale=(1<<(LEliteBase -(buf->head).scale));
    break;
  case 5:    /* differential */
    for(i=0; i<500; i++)
      {
	temp = buf->data[i];
	exponent = temp & 0x07;
	mantissa = temp & (~0x07);
	shift = 2 * exponent;
	data[i] = (int) mantissa << (16 - shift);
	sum += data[i];
#ifdef DEBUG
	fprintf(stderr, "%3d: delta %12ld (0x%04x mant 0x%4x exp 0x%1x) sum %12ld\n",
		i,data[i],(buf->data[i])&0xffff,mantissa&0xffff,exponent&0xf,sum);
#endif
	data[i] = sum;
      }
    *scale=(1<<(LEliteBase -(buf->head).scale));
    break;
    
  default:
    fprintf (stderr, "marsBlockDecodeData(): illegal data format %d\n", data_format);
    return NULL;
  }

  return data;
}


void m88SwapBlock (m88Block *block)
{
  m88Head  *head = &block->head;
  short    *data;
  
  /* Swap header */
  gswap4 (&head->dev_id);
  gswap4 (&(head->time).time);
  gswap2 (&(head->time).delta);
  gswap2 (&(head->time).mode);
  gswap2 (&head->maxamp);
  
  /* Swap data */
  for ( data = block->data; data < block->data+marsBlockSamples; data++)
    gswap2 (data);
}


void mlSwapBlock (mlBlock *block)
{
  mlHead  *head = &block->head;
  short   *data;
  
  /* Swap header */
  gswap4 (&head->time);
  gswap2 (&head->maxamp);
  gswap2 (&head->dstart);
  
  /* Swap data */
  for ( data = block->data; data < block->data+marsBlockSamples; data++)
    gswap2 (data);
}


marsStream *marsStreamOpen (char *name)
{
  struct stat	fs;
  
  memset (&MS, 0, sizeof(marsStream));
  
  if ( stat(name,&fs) )
    {
      fprintf (stderr, "Cannot stat file \'%s\' - %s\n", name, strerror(errno));
      return NULL;
    }
  
  if ( (MS.hf = fopen(name,"rb")) == NULL )
    {
      fprintf (stderr, "Cannot open file \'%s\' - %s\n", name, strerror(errno));
      return NULL;
    }
  
  /* fill marsStream structure */
  MS.size=fs.st_size;
  MS.time=fs.st_mtime;
  strcpy (MS.name, name);
  
  MS.status|=msStreamActive;
  
  return &MS;
}


int marsStreamDumpBlock (marsStream *hMS)
{
  char 	*hB=hMS->block;
  char   timestr[50];
  hptime_t hptime;
  int	*hData, *hD, scale;
  double gain;
  
  if ( isMarsDataBlock(hB) )
    {
      hData = marsBlockDecodeData (hB,&scale);
      hptime = MS_EPOCH2HPTIME (mbGetTime(hB));
      ms_hptime2isotimestr (hptime, timestr);
      fprintf (stderr, "MB sta='%4s' chano=%d block=%d samp=%d scale=%d time=%s c2uV=%d maxamp=%d",
	       mbGetStationCode(hB), mbGetChan(hB), mbGetBlockFormat(hB),
	       mbGetSamp(hB), mbGetScale(hB),
	       timestr,
	       marsBlockGetScaleFactor(hB),
	       mbGetMaxamp(hB));
      
      if ( hData )
	{
	  fprintf (stderr, "\n");
	  for (hD=hData, gain=marsBlockGetGain(hB); hD<(hData+marsBlockSamples); hD++)
	    fprintf (stderr, "%d => %g\n",*hD, *hD*gain);
	  return 1;
	}
      else
	fprintf (stderr, ":ERROR: CANNOT DECODE\n");
    }
  
  return 0;
}


marsStream *marsStreamGetNextBlock (int verbose)
{
  
  while ( fread(MS.block, marsBlockSize, 1, MS.hf) == 1 )
    {
      /* Byte swap block if necessary (i.e. host is big-endian) */
      if ( mbGetMagic(MS.block) == LEMAGICbe )
	switch ( ((leFormat *) &(MS.block))->block_format )
	  {
	  case 1:  /* MARS-88 data block */
	  case 2:  /* MARS-88 monitor block */
	    m88SwapBlock ((m88Block *) &(MS.block));
	    break;
	  case 3:  /* MARSlite data block */
	  case 4:  /* MARSlite monitor block */
	    mlSwapBlock ((mlBlock *) &(MS.block));
	    break;
	  }
      
      if ( verbose >= 2 )
	fprintf (stderr, "MB 0x%016X : block %d : %s : 0x%04X : %d : %d : chan %d\n",
		 (unsigned int)MS.offset,(int)(MS.offset/marsBlockSize),
		 isMarsDataBlock(MS.block)?"DATA":"MON ",
		 mbGetMagic(MS.block),mbGetBlockFormat(MS.block),mbGetDataFormat(MS.block),
		 mbGetChan(MS.block));
      
      if ( isMarsDataBlock(MS.block) && mbGetChan(MS.block) < 3 )
	{ /* do checks */
	  MS.offset += marsBlockSize;
	  return &MS;
	}
      
      MS.offset += marsBlockSize;
    }
  
  return NULL;
}


marsStream *marsStreamGetCurrent (void)
{
  return &MS;
}


void marsStreamClose (void)
{
  if ( MS.hf != NULL )
    fclose (MS.hf);
  
  memset (&MS, 0, sizeof(marsStream));
}
