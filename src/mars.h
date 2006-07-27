/*
$Header: /usr/local/cvs/repository/mars2mseed/src/mars.h,v 1.3 2006-07-27 17:28:00 chad Exp $
*/
#ifndef MARS_H_
 #define MARS_H_

 /* typedef for TIMES structure */
 /* low nibble: Sync Mode. These are the same values used inside MARS-88! */
 #define SM_NOSYNC 0x01   /* Sync_mode: none             */
 #define SM_TSIG   0x02   /* Sync_mode: time signal      */
 #define SM_DCF77  0x04   /* Sync_mode: DCF              */

 /* hi  nibble: clock state. */
 #define DCF_OK    0x10   /* Time checked by DCF         */
 #define SYNC_OK   0x20   /* Clock synchronized          */

 #define TM_VALID  0x80

 #define NO_WORD   0x7fff /* leTime.delta=NO_WORD: no valid delta */
 
 #define LEm88Base	16
 #define LEliteBase	14

 #define M88_MINSAMPLING	0x07
 #define M88_MAXSAMPLING	0x01
 #define M88_CHANMASK		0x03

 /* m88 block status */
 #define m88HeadError		0x0FFF0000
 #define m88NoValidFormat	0x00020000
 #define m88NoValidDevID	0x00040000
 #define m88NoValidTime		0x00080000
 #define m88NoValidChan		0x00100000
 #define m88NoValidRate		0x00200000
 #define m88NoValidAmp		0x00400000
 #define m88NoValidScale	0x00800000

 #define m88DataError		0xF0000000
 #define m88MisplacedBlock	0x10000000
 #define m88TimeZappedBlock	0x20000000

 #define m88ResetHeadError(a)	( (a) = ( (a)&(~m88HeadError) ) )

 #define mlDiskBlockSize        512
 #define mlDiskPartInfo         (2*mlDiskBlockSize)
 #define mlPartInfoEntries      4

 #define marsBlockSamples       500
 #define marsBlockDataSize      1000
 #define marsBlockSize          1024

 typedef struct mlPartitionInfo
 {
  char	  label[8];
  int 	  offset,uoffset;
  int 	  length,ulength;
  int	  dummy,udummy;
 } mlPartInfo;
 
 #define mlDATA	      0
 #define mlMONITOR    1
 #define mlLOGGING    2
 #define mlSETUP      3
 
 #define LE_MAGIC	('l' + ('e'<<8))
 #define LEMAGICle	0x656C  /* 'l'+'e' value on little-endian */
 #define LEMAGICbe	0x6C65  /* 'l'+'e' value on big-endian    */
 
 #define DATABLK_FORMAT     1   /* MARS-88 data block 	  */
 #define MONBLK_FORMAT      2   /* MARS-88 monitor block  */
 #define LITE_BLOCK_FORMAT  3   /* MARSlite data block    */
 #define LITE_MONBLK_FORMAT 4   /* MARSlite monitor block */

 typedef struct
 {
  short	magic;		    /* 'l' 'e'         */
  char 	block_format;       /* block format ID */
  char 	data_format;        /* data format ID, same values as DSP   */
                            /* 0: straight 16-bit word              */
                            /* 1: 14 bits mantissa, 2 bits exponent, MARS-88 */
                            /* 2: 13 bits mantissa, 3 bits exponent, MARS-88 */
                            /* 3: 12 bits mantissa, 4 bits exponent, MARS-88 */
                            /* 4: 3 bits exponent/2, 13 bits mantissa, non-differential, lite */
                            /* 5: 3 bits exponent/2, 13 bits mantissa, differential, lite */
 } leFormat;

 typedef struct
 {
    int    time;	    /* time/date in ctime(3C) format */
    short  delta;	    /* time lag in ms                */
    short  mode;	    /* low nibble: sync mode; hi nibble: clock state */
 } leTime;

 typedef struct
 { 			    /*                         length  total   */
    leFormat format_id;	    /* header-data format ID      4      4     */
    int    dev_id;	    /* hardware device ID         4      8     */
    leTime time;	    /* time, date, time delta,..  8     16     */
    char   chno;	    /* channel number             1     17     */
    char   samp_rate;	    /* sampling rate (2^N ms)     1     18     */
    short  maxamp;	    /* max.amplitude              2     20     */
    char   scale;	    /* scale (2^N uV)             1     21     */
    char   dummy[3];	    /* ... padding                3     24     */
 } m88Head;

 typedef struct 
 {                          /*                          length  total   */
  leFormat format_id;       /* header-data format ID       4      4     */
  char     station_name[4]; /* device name                 4      8     */
  char     _rsrvd[4];       /* reserved                    4     12     */
  int      time;            /* time, date                  4     16     */
  char     chno;            /* channel number              1     17     */
  char     samp_interval;   /* sampling interval (2^N ms)  1     18     */
  short    maxamp;          /* max.amplitude               2     20     */
  char     scale;           /* scale (2^N uV)              1     21     */
  unsigned char triggidx;   /* trigger index / 2           1     22     */
  short    dstart;          /* start value f. diff format  2     24     */
 } mlHead;

 typedef struct
 {
    m88Head	head;
    short	data[marsBlockSamples];
 } m88Block;

 typedef struct
 {
    mlHead	head;
    short	data[marsBlockSamples];
 } mlBlock;

#endif
