/***************************************************************************
 * mars2mseed.c
 *
 * Simple waveform data conversion from MARS 88/lite data files to Mini-SEED
 *
 * Written by Chad Trabant, IRIS Data Management Center
 *
 * modified 2005.123
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

#include <libmseed.h>

#include "marsio.h"

#define VERSION "0.2"
#define PACKAGE "mars2mseed"

/* Pre-defined channel transmogrifications */
static char *transmatrix[4][3] = {
  {"Z", "N", "E"},
  {"SHZ", "SHN", "SHE"},
  {"BHZ", "BHN", "BHE"},
  {"HHZ", "HHN", "HHE"}
};

static int mars2group (char *mfile, TraceGroup *mstg);
static int parameter_proc (int argcount, char **argvec);
static char *getoptval (int argcount, char **argvec, int argopt);
static void addfile (char *filename);
static void record_handler (char *record, int reclen);
static void usage (void);
static void term_handler (int sig);

static int   verbose     = 0;
static int   parseonly   = 0;
static int   packreclen  = -1;
static int   encoding    = -1;
static int   byteorder   = -1;
static char *forcesta    = 0;
static char *forcenet    = 0;
static char *forceloc    = 0;
static int   transchan   = -1;
static char *outputfile  = 0;
static FILE *ofp         = 0;

struct filelink {
  char *filename;
  struct filelink *next;
};

struct filelink *filelist = 0;

int
main (int argc, char **argv)
{
  struct filelink *flp;
  TraceGroup *mstg = 0;

  int packedsamples = 0;
  int packedrecords = 0;
  
  /* Signal handling, use POSIX calls with standardized semantics */
  struct sigaction sa;
  
  sa.sa_flags = SA_RESTART;
  sigemptyset (&sa.sa_mask);
  
  sa.sa_handler = term_handler;
  sigaction (SIGINT, &sa, NULL);
  sigaction (SIGQUIT, &sa, NULL);
  sigaction (SIGTERM, &sa, NULL);
  
  sa.sa_handler = SIG_IGN;
  sigaction (SIGHUP, &sa, NULL);
  sigaction (SIGPIPE, &sa, NULL);

  /* Process given parameters (command line and parameter file) */
  if (parameter_proc (argc, argv) < 0)
    return -1;
  
  /* Init TraceGroup */
  mstg = mst_initgroup (mstg);
  
  /* Open the output file if specified otherwise stdout */
  if ( outputfile )
    {
      if ( strcmp (outputfile, "-") == 0 )
        {
          ofp = stdout;
        }
      else if ( (ofp = fopen (outputfile, "w")) == NULL )
        {
          fprintf (stderr, "Cannot open output file: %s (%s)\n",
                   outputfile, strerror(errno));
          return -1;
        }
    }
  else
    {
      ofp = stdout;
    }
  
  /* Read input MARS files into TraceGroup */
  flp = filelist;
  
  while ( flp != 0 )
    {
      if ( verbose )
	fprintf (stderr, "Reading %s\n", flp->filename);

      mars2group (flp->filename, mstg);
      
      flp = flp->next;
    }
  
  /* Pack data into Mini-SEED records */
  packedrecords = mst_packgroup (mstg, &record_handler, packreclen, encoding, byteorder,
				 &packedsamples, 1, verbose-2, NULL);
  
  if ( packedrecords < 0 )
    {
      fprintf (stderr, "Error packing data\n");
    }
  else
    {
      fprintf (stderr, "Packed %d trace(s) of %d samples into %d records\n",
	       mstg->numtraces, packedsamples, packedrecords);
    }
  
  /* Make sure everything is cleaned up */
  mst_freegroup (&mstg);
  
  if ( ofp )
    fclose (ofp);
  
  return 0;
}  /* End of main() */


/***************************************************************************
 * mars2group:
 *
 * Read a MARS data file and add data samples to a TraceGroup.
 * As the data is read in a MSrecord struct is used as a holder for
 * the input information.
 *
 * Returns 0 on success, and -1 on failure
 ***************************************************************************/
static int
mars2group (char *mfile, TraceGroup *mstg)
{
  MSrecord *msr = 0;
  int retval = 0;
  
  marsStream *hMS;
  long	     *hData, *hD, scale;
  double      gain;
  
  /* Open MARS data file */
  if ( ! (hMS = marsStreamOpen(mfile) ) )
    {
      fprintf (stderr, "Cannot open input file: %s (%s)\n", mfile, strerror(errno));
      return -1;
    }
  
  if ( ! (msr = msr_init(msr)) )
    {
      fprintf (stderr, "Cannot initialize MSrecord strcture\n");
      return -1;
    }
  
  /* Loop over MARS blocks */
  while ( (hMS = marsStreamGetNextBlock(verbose)) != NULL )
    {
      if ( verbose >= 4 ) marsStreamDumpBlock (hMS);
      
      if ( verbose >= 2 )
	fprintf (stderr, "MB sta='%s' chan=%d samprate=%g scale=%d time=%ld c2uV=%ld maxamp=%d\n",
		 mbGetStationCode(hMS->block), mbGetChan(hMS->block),
		 mbGetSampRate(hMS->block), mbGetScale(hMS->block),
		 mbGetTime(hMS->block),
		 marsBlockGetScaleFactor(hMS->block), mbGetMaxamp(hMS->block));
      
      hData = marsBlockDecodeData (hMS->block, &scale);
      
      if ( hData && ! parseonly )
	{ /* Convert to 10s of microvolts since for at least one gain we can have 0.5 uV */
	  for( hD=hData, gain=marsBlockGetGain(hMS->block); hD < (hData+marsBlockSamples); hD++)
	    *hD *= (gain*10);
	  
	  /* Populate a MSrecord and add data to TraceGroup */
	  msr->datasamples = hData;
	  msr->numsamples = marsBlockSamples;
	  msr->samplecnt = marsBlockSamples;
	  msr->sampletype = 'i';
	  msr->samprate = mbGetSampRate(hMS->block);
	  msr->starttime = MS_EPOCH2HPTIME (mbGetTime(hMS->block));
	  
          ms_strncpclean (msr->network, forcenet, 2);
	  if ( forcesta ) ms_strncpclean (msr->station, forcesta, 5);
	  else ms_strncpclean (msr->station, mbGetStationCode(hMS->block), 5);
          ms_strncpclean (msr->location, forceloc, 2);
	  
	  /* Transmogrify the channel numbers to channel codes or just copy */
	  if ( transchan >= 0 && transchan <= 3 )
	    {
	      snprintf (msr->channel, 10, "%s",
			transmatrix[transchan][(int)mbGetChan(hMS->block)]);
	    }
	  else
	    {
	      snprintf (msr->channel, 10, "%d", mbGetChan(hMS->block));
	    }
	  
	  /* If MARS88, check for a valid time lag and warn that it's not applied */
	  if ( mbGetBlockFormat(hMS->block) == DATABLK_FORMAT )
	    if ( ((m88Head *)(hMS->block))->time.delta != NO_WORD )
	      fprintf (stderr, "Warning: Time lag of %d ms NOT applied to N: '%s', S: '%s', L: '%s', C: '%s'\n",
		       ((m88Head *)(hMS->block))->time.delta,
		       msr->network, msr->station,  msr->location, msr->channel);
	  
	  if ( verbose >= 1 )
	    {
	      fprintf (stderr, "[%s] %d samps @ %.4f Hz for N: '%s', S: '%s', L: '%s', C: '%s'\n",
		       mfile, msr->numsamples, msr->samprate,
		       msr->network, msr->station,  msr->location, msr->channel);
	    }
	  
	  if ( ! mst_addmsrtogroup (mstg, msr, -1.0, -1.0) )
	    {
	      fprintf (stderr, "[%s] Error adding samples to TraceGroup\n", mfile);
	    }
	  
	  /* Cleanup and reset MSrecord state */
	  msr->datasamples = 0;
	  msr = msr_init (msr);
	}
    }
  
  if ( hMS )
    marsStreamClose();
  
  if ( msr )
    msr_free (&msr);
  
  return retval;
}  /* End of mars2group() */


/***************************************************************************
 * parameter_proc:
 * Process the command line parameters.
 *
 * Returns 0 on success, and -1 on failure
 ***************************************************************************/
static int
parameter_proc (int argcount, char **argvec)
{
  int optind;

  /* Process all command line arguments */
  for (optind = 1; optind < argcount; optind++)
    {
      if (strcmp (argvec[optind], "-V") == 0)
	{
	  fprintf (stderr, "%s version: %s\n", PACKAGE, VERSION);
	  exit (0);
	}
      else if (strcmp (argvec[optind], "-h") == 0)
	{
	  usage();
	  exit (0);
	}
      else if (strncmp (argvec[optind], "-v", 2) == 0)
	{
	  verbose += strspn (&argvec[optind][1], "v");
	}
      else if (strcmp (argvec[optind], "-p") == 0)
	{
	  parseonly = 1;
	}
      else if (strcmp (argvec[optind], "-s") == 0)
	{
	  forcesta = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-n") == 0)
	{
	  forcenet = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-l") == 0)
	{
	  forceloc = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-r") == 0)
	{
	  packreclen = atoi (getoptval(argcount, argvec, optind++));
	}
      else if (strcmp (argvec[optind], "-e") == 0)
	{
	  encoding = atoi (getoptval(argcount, argvec, optind++));
	}
      else if (strcmp (argvec[optind], "-b") == 0)
	{
	  byteorder = atoi (getoptval(argcount, argvec, optind++));
	}
      else if (strcmp (argvec[optind], "-o") == 0)
	{
	  outputfile = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-t") == 0)
	{
	  transchan = atoi (getoptval(argcount, argvec, optind++));
	}
      else if (strncmp (argvec[optind], "-", 1) == 0 &&
	       strlen (argvec[optind]) > 1 )
	{
	  fprintf(stderr, "Unknown option: %s\n", argvec[optind]);
	  exit (1);
	}
      else
	{
	  addfile (argvec[optind]);
	}
    }

  /* Make sure an input files were specified */
  if ( filelist == 0 )
    {
      fprintf (stderr, "No input files were specified\n\n");
      fprintf (stderr, "%s version %s\n\n", PACKAGE, VERSION);
      fprintf (stderr, "Try %s -h for usage\n", PACKAGE);
      exit (1);
    }

  /* Report the program version */
  if ( verbose )
    fprintf (stderr, "%s version: %s\n", PACKAGE, VERSION);

  return 0;
}  /* End of parameter_proc() */


/***************************************************************************
 * getoptval:
 * Return the value to a command line option; checking that the value is 
 * itself not an option (starting with '-') and is not past the end of
 * the argument list.
 *
 * argcount: total arguments in argvec
 * argvec: argument list
 * argopt: index of option to process, value is expected to be at argopt+1
 *
 * Returns value on success and exits with error message on failure
 ***************************************************************************/
static char *
getoptval (int argcount, char **argvec, int argopt)
{
  if ( argvec == NULL || argvec[argopt] == NULL ) {
    fprintf (stderr, "getoptval(): NULL option requested\n");
    exit (1);
  }
  
  /* Special case of '-o -' usage */
  if ( (argopt+1) < argcount && strcmp (argvec[argopt], "-o") == 0 )
    if ( strcmp (argvec[argopt+1], "-") == 0 )
      return argvec[argopt+1];
  
  if ( (argopt+1) < argcount && *argvec[argopt+1] != '-' )
    return argvec[argopt+1];
  
  fprintf (stderr, "Option %s requires a value\n", argvec[argopt]);
  exit (1);
}  /* End of getoptval() */


/***************************************************************************
 * addfile:
 *
 * Add file to end of the global file list (filelist).
 ***************************************************************************/
static void
addfile (char *filename)
{
  struct filelink *lastlp, *newlp;
  
  if ( filename == NULL )
    {
      fprintf (stderr, "addfile(): No file name specified\n");
      return;
    }
  
  lastlp = filelist;
  while ( lastlp != 0 )
    {
      if ( lastlp->next == 0 )
        break;
      
      lastlp = lastlp->next;
    }
  
  newlp = (struct filelink *) malloc (sizeof (struct filelink));
  newlp->filename = strdup(filename);
  newlp->next = 0;
  
  if ( lastlp == 0 )
    filelist = newlp;
  else
    lastlp->next = newlp;
  
}  /* End of addfile() */


/***************************************************************************
 * record_handler:
 * Saves passed records to the output file.
 ***************************************************************************/
static void
record_handler (char *record, int reclen)
{
  if ( fwrite(record, reclen, 1, ofp) != 1 )
    {
      fprintf (stderr, "Error writing to output file\n");
    }
}  /* End of record_handler() */


/***************************************************************************
 * usage:
 * Print the usage message and exit.
 ***************************************************************************/
static void
usage (void)
{
  fprintf (stderr, "%s version: %s\n\n", PACKAGE, VERSION);
  fprintf (stderr, "Convert MARS 88/lite data files to Mini-SEED\n\n");
  fprintf (stderr, "Usage: %s [options] file1 file2 file3...\n\n", PACKAGE);
  fprintf (stderr,
	   " ## Options ##\n"
	   " -V             Report program version\n"
	   " -h             Show this usage message\n"
	   " -v             Be more verbose, multiple flags can be used\n"
	   " -p             Parse MARS data only, do not write Mini-SEED\n"
	   " -s stacode     Force the SEED station code\n"
	   " -n netcode     Force the SEED network code\n"
	   " -l loccode     Force the SEED location code\n"
	   " -r bytes       Specify record length in bytes for packing, default: 4096\n"
	   " -e encoding    Specify SEED encoding format for packing, default: 11 (Steim2)\n"
	   " -b byteorder   Specify byte order for packing, MSBF: 1 (default), LSBF: 0\n"
	   " -o outfile     Specify the output file, default is stdout.\n"
	   " -t chanset     Transmogrify channel numbers to common channel codes:\n"
	   "                   0: 0->Z, 1->N, 2->E\n"
	   "                   1: 0->SHZ, 1->SHN, 2->SHE\n"
	   "                   2: 0->BHZ, 1->BHN, 2->BHE\n"
	   "                   3: 0->HHZ, 1->HHN, 2->HHE\n"
	   "\n"
	   " file(s)        File(s) of MARS input data\n"
	   "\n"
	   "Supported Mini-SEED encoding formats:\n"
	   " 1  : 16-bit integers (only works if samples can be represented in 16-bits)\n"
	   " 3  : 32-bit integers\n"
	   " 10 : Steim 1 compression\n"
	   " 11 : Steim 2 compression\n"
	   "\n"
	   "Note: resulting sample values are 10s of microvolts\n"
	   "\n");
}  /* End of usage() */


/***************************************************************************
 * term_handler:
 * Signal handler routine.
 ***************************************************************************/
static void
term_handler (int sig)
{
  exit (0);
}
