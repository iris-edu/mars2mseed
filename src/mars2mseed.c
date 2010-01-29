/***************************************************************************
 * mars2mseed.c
 *
 * Simple waveform data conversion from MARS 88/lite data files to Mini-SEED
 *
 * Written by Chad Trabant, IRIS Data Management Center
 *
 * modified 2007.284
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <libmseed.h>

#include "marsio.h"

#define VERSION "1.3"
#define PACKAGE "mars2mseed"

/* Pre-defined channel transmogrifications */
static char *transmatrix[5][3] = {
  {"Z", "N", "E"},
  {"SHZ", "SHN", "SHE"},
  {"BHZ", "BHN", "BHE"},
  {"HHZ", "HHN", "HHE"},
  {"EHZ", "EHN", "EHE"}
};

struct listnode {
  char *key;
  char *data;
  struct listnode *next;
};

static void packtraces (flag flush);
static int mars2group (char *mfile, MSTraceGroup *mstg);
static int parameter_proc (int argcount, char **argvec);
static char *getoptval (int argcount, char **argvec, int argopt);
static int readlistfile (char *listfile);
static void addnode (struct listnode **listroot, char *key, char *data);
static void addmapnode (struct listnode **listroot, char *mapping);
static void record_handler (char *record, int reclen, void *handlerdata);
static void usage (void);

static int   verbose     = 0;
static int   parseonly   = 0;
static int   packreclen  = -1;
static int   encoding    = -1;
static int   byteorder   = -1;
static int   scaling     = 8;
static char  bufferall   = 0;
static char *forcesta    = 0;
static char *forcenet    = 0;
static char *forceloc    = 0;
static int   transchan   = -1;
static char *outputfile  = 0;
static FILE *ofp         = 0;

/* A list of input files */
struct listnode *filelist = 0;

/* A list of component to channel translations */
struct listnode *chanlist = 0;

/* Internal data buffers */
static MSTraceGroup *mstg = 0;

static int packedtraces  = 0;
static int packedsamples = 0;
static int packedrecords = 0;

int
main (int argc, char **argv)
{
  struct listnode *flp;
  
  /* Process given parameters (command line and parameter file) */
  if (parameter_proc (argc, argv) < 0)
    return -1;
  
  /* Init MSTraceGroup */
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
          ms_log (2, "Cannot open output file: %s (%s)\n",
		  outputfile, strerror(errno));
          return -1;
        }
    }
  
  /* Read input MARS files into MSTraceGroup */
  flp = filelist;
  while ( flp != 0 )
    {
      if ( verbose )
	ms_log (1, "Reading %s\n", flp->data);

      mars2group (flp->data, mstg);
      
      flp = flp->next;
    }
  
  /* Pack any remaining, possibly all data */
  if ( ! parseonly )
    {
      if ( bufferall )
	{
	  packtraces (1);
	  packedtraces += mstg->numtraces;
	}

      ms_log (1, "Packed %d trace(s) of %d samples into %d records\n",
	      packedtraces, packedsamples, packedrecords);
      
      ms_log (1, "All data samples have been scaled by %d and are now %d nanovolts!\n",
	      scaling, (scaling)?(1000/scaling):0);
    }
  
  /* Make sure everything is cleaned up */
  mst_freegroup (&mstg);
  
  if ( ofp )
    fclose (ofp);
  
  return 0;
}  /* End of main() */


/***************************************************************************
 * packtraces:
 *
 * Pack all traces in a group using per-MSTrace templates.
 *
 * Returns 0 on success, and -1 on failure
 ***************************************************************************/
static void
packtraces (flag flush)
{
  MSTrace *mst;
  int trpackedsamples = 0;
  int trpackedrecords = 0;
  
  mst = mstg->traces;
  while ( mst )
    {
      if ( mst->numsamples <= 0 )
        {
          mst = mst->next;
          continue;
        }

      trpackedrecords = mst_pack (mst, &record_handler, 0, packreclen, encoding, byteorder,
                                  &trpackedsamples, flush, verbose-2, NULL);
      if ( trpackedrecords < 0 )
        {
          ms_log (2, "Cannot pack data\n");
        }
      else
        {
          packedrecords += trpackedrecords;
          packedsamples += trpackedsamples;
        }
      
      mst = mst->next;
    }
}  /* End of packtraces() */


/***************************************************************************
 * mars2group:
 *
 * Read a MARS data file and add data samples to a MSTraceGroup.
 * As the data is read in a MSRecord struct is used as a holder for
 * the input information.
 *
 * Returns 0 on success, and -1 on failure
 ***************************************************************************/
static int
mars2group (char *mfile, MSTraceGroup *mstg)
{
  MSRecord *msr = 0;
  struct listnode *clp;
  int retval = 0;
  int truncwarn = 0;
  char mapped;
  
  marsStream *hMS;
  int        *hData, *hD, scale;
  double      gain, totalgain, sample;
  
  /* Open MARS data file */
  if ( ! (hMS = marsStreamOpen(mfile) ) )
    {
      ms_log (2, "Cannot open input file: %s (%s)\n", mfile, strerror(errno));
      return -1;
    }

  /* Open .mseed output file if needed */
  if ( ! ofp && ! parseonly )
    {
      char mseedoutputfile[1024];
      snprintf (mseedoutputfile, sizeof(mseedoutputfile), "%s.mseed", mfile);
      
      if ( (ofp = fopen (mseedoutputfile, "wb")) == NULL )
        {
          ms_log (2, "Cannot open output file: %s (%s)\n",
		  mseedoutputfile, strerror(errno));
          return -1;
        }
    }
  
  if ( ! (msr = msr_init(msr)) )
    {
      ms_log (2, "Cannot initialize MSRecord strcture\n");
      return -1;
    }
  
  /* Loop over MARS blocks */
  while ( (hMS = marsStreamGetNextBlock(verbose)) != NULL )
    {
      if ( verbose >= 4 )
	marsStreamDumpBlock (hMS);
      
      if ( verbose >= 2 )
	ms_log (1, "MB sta='%s' chan=%d samprate=%g scale=%d time=%d c2uV=%d maxamp=%d\n",
		mbGetStationCode(hMS->block), mbGetChan(hMS->block),
		mbGetSampRate(hMS->block), mbGetScale(hMS->block),
		mbGetTime(hMS->block),
		marsBlockGetScaleFactor(hMS->block), mbGetMaxamp(hMS->block));
      
      hData = marsBlockDecodeData (hMS->block, &scale);
      
      if ( hData && ! parseonly )
	{
	  /* Scale data samples, some potential gain values can result in non-integer samples */
	  gain = marsBlockGetGain(hMS->block);
	  
	  totalgain = gain * scaling;
	  
	  if ( verbose >= 2 )
	    ms_log (1, "Applying gain: %f c/uV and scaling: %d for total: %f\n",
		    gain, scaling, totalgain);
	  
	  /* Apply gain & scaling to decoded data samples */
	  truncwarn = 0;
	  for ( hD=hData; hD < (hData+marsBlockSamples); hD++)
	    {
	      sample = (*hD) * totalgain;
	      
	      if ( ! truncwarn && ( sample - (int)sample ))
		{
		  ms_log (1, "WARNING: sample value truncation occurring, change scaling\n");
		  ms_log (1, "  Sample: %f, scaling: %d, gain: %g, total gain: %f\n",
			  sample, scaling, gain, totalgain);
		  truncwarn = 1;
		}
	      
	      *hD = (int)sample;
	    }
	  
	  /* Populate a MSRecord and add data to MSTraceGroup */
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
	  
	  /* Transmogrify the channel numbers to channel codes first
	     using any custom mappings, then pre-defined mappings and
	     finally just copy. */
	  mapped = 0;
	  if ( chanlist )
	    {
	      clp = chanlist;
	      while ( clp != 0 )
		{
		  if ( *(clp->key) == ('0' + (int)mbGetChan(hMS->block)) )
		    {
		      strncpy (msr->channel, clp->data, 10);
		      mapped = 1;
		      break;
		    }
		  
		  clp = clp->next;
		}
	    }
	  if ( ! mapped && transchan >= 0 && transchan <= 4 )
	    {
	      snprintf (msr->channel, 10, "%s",
			transmatrix[transchan][(int)mbGetChan(hMS->block)]);	      
	      mapped = 1;
	    }
	  if ( ! mapped )
	    {
	      snprintf (msr->channel, 10, "%d", mbGetChan(hMS->block));
	    }
	  
	  /* If MARS88, check for a valid time lag and warn that it's not applied */
	  if ( mbGetBlockFormat(hMS->block) == DATABLK_FORMAT )
	    if ( ((m88Head *)(hMS->block))->time.delta != NO_WORD )
	      ms_log (1, "Warning: Time lag of %d ms NOT applied to N: '%s', S: '%s', L: '%s', C: '%s'\n",
		      ((m88Head *)(hMS->block))->time.delta,
		      msr->network, msr->station,  msr->location, msr->channel);
	  
	  if ( verbose >= 1 )
	    {
	      ms_log (1, "[%s] %d samps @ %.4f Hz for N: '%s', S: '%s', L: '%s', C: '%s'\n",
		      mfile, msr->numsamples, msr->samprate,
		      msr->network, msr->station,  msr->location, msr->channel);
	    }
	  
	  /* Add data to MSTraceGroup data buffer */
	  if ( ! mst_addmsrtogroup (mstg, msr, 0, -1.0, -1.0) )
	    {
	      ms_log (2, "[%s] Cannot add samples to MSTraceGroup\n", mfile);
	    }
	  
	  /* Pack whatever can be packed if not buffering all data */
	  if ( ! bufferall )
	    {
	      packtraces (0);
	    }
	  
	  /* Cleanup and reset MSRecord state */
	  msr->datasamples = 0;
	  msr = msr_init (msr);
	}
    }
  
  /* Flush data buffers after each file */
  if ( ! bufferall && ! parseonly )
    {
      packtraces (1);
      packedtraces += mstg->numtraces;
      mst_initgroup (mstg);
    }
  
  if ( ofp  && ! outputfile )
    {
      fclose (ofp);
      ofp = 0;
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
	  ms_log (1, "%s version: %s\n", PACKAGE, VERSION);
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
      else if (strcmp (argvec[optind], "-B") == 0)
	{
	  bufferall = 1;
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
	  packreclen = strtol (getoptval(argcount, argvec, optind++), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-e") == 0)
	{
	  encoding = strtol (getoptval(argcount, argvec, optind++), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-b") == 0)
	{
	  byteorder = strtol (getoptval(argcount, argvec, optind++), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-o") == 0)
	{
	  outputfile = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-g") == 0)
	{
	  scaling = strtol (getoptval(argcount, argvec, optind++), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-t") == 0)
	{
	  transchan = strtol (getoptval(argcount, argvec, optind++), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-T") == 0)
	{
	  addmapnode (&chanlist, getoptval(argcount, argvec, optind++));
	}
      else if (strncmp (argvec[optind], "-", 1) == 0 &&
	       strlen (argvec[optind]) > 1 )
	{
	  ms_log (2, "Unknown option: %s\n", argvec[optind]);
	  exit (1);
	}
      else
	{
	  addnode (&filelist, NULL, argvec[optind]);
	}
    }

  /* Make sure an output file was specified if buffering all input */
  if ( bufferall && ! outputfile )
    {
      ms_log (2, "Need to specify output file with -o if using -B\n");
      exit (1);
    }
  
  /* Make sure an input files were specified */
  if ( filelist == 0 )
    {
      ms_log (2, "No input files were specified\n\n");
      ms_log (1, "%s version %s\n\n", PACKAGE, VERSION);
      ms_log (1, "Try %s -h for usage\n", PACKAGE);
      exit (1);
    }
  
  /* Report the program version */
  if ( verbose )
    ms_log (1, "%s version: %s\n", PACKAGE, VERSION);
  
  /* Check the input files for any list files, if any are found
   * remove them from the list and add the contained list */
  if ( filelist )
    {
      struct listnode *prevln, *ln;
      
      prevln = ln = filelist;
      while ( ln != 0 )
        {
          if ( *(ln->data) == '@' )
            {
              /* Remove this node from the list */
              if ( ln == filelist )
                filelist = ln->next;
              else
                prevln->next = ln->next;
              
              /* Read list file, skip the '@' first character */
              readlistfile (ln->data + 1);
              
              /* Free memory for this node */
              if ( ln->key )
                free (ln->key);
              free (ln->data);
              free (ln);
            }
          else
            {
              prevln = ln;
            }
          
          ln = ln->next;
        }
    }
  
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
    ms_log (2, "getoptval(): NULL option requested\n");
    exit (1);
    return 0;
  }
  
  /* Special case of '-o -' usage */
  if ( (argopt+1) < argcount && strcmp (argvec[argopt], "-o") == 0 )
    if ( strcmp (argvec[argopt+1], "-") == 0 )
      return argvec[argopt+1];
  
  if ( (argopt+1) < argcount && *argvec[argopt+1] != '-' )
    return argvec[argopt+1];
  
  ms_log (2, "Option %s requires a value, try -h for usage\n", argvec[argopt]);
  exit (1);
  return 0;
}  /* End of getoptval() */


/***************************************************************************
 * readlistfile:
 * Read a list of files from a file and add them to the filelist for
 * input data.
 *
 * Returns the number of file names parsed from the list or -1 on error.
 ***************************************************************************/
static int
readlistfile (char *listfile)
{
  FILE *fp;
  char line[1024];
  char *ptr;
  int  filecnt = 0;
  int  nonspace;

  char filename[1024];
  int  fields;
  
  /* Open the list file */
  if ( (fp = fopen (listfile, "rb")) == NULL )
    {
      if (errno == ENOENT)
        {
          ms_log (2, "Could not find list file %s\n", listfile);
          return -1;
        }
      else
        {
          ms_log (2, "Cannot open list file %s: %s\n",
		  listfile, strerror (errno));
          return -1;
        }
    }
  
  if ( verbose )
    ms_log (1, "Reading list of input files from %s\n", listfile);
  
  while ( (fgets (line, sizeof(line), fp)) !=  NULL)
    {
      /* Truncate line at first \r or \n and count non-space characters */
      nonspace = 0;
      ptr = line;
      while ( *ptr )
        {
          if ( *ptr == '\r' || *ptr == '\n' || *ptr == '\0' )
            {
              *ptr = '\0';
              break;
            }
          else if ( *ptr != ' ' )
            {
              nonspace++;
            }
          
          ptr++;
        }
      
      /* Skip empty lines */
      if ( nonspace == 0 )
        continue;
      
      fields = sscanf (line, "%s", filename);
      
      if ( fields != 1 )
        {
          ms_log (2, "Cannot parse filename from: %s\n", line);
          continue;
        }
      
      if ( verbose > 1 )
        ms_log (1, "Adding '%s' to input file list\n", filename);
      
      addnode (&filelist, NULL, filename);
      filecnt++;
      
      continue;
    }
  
  fclose (fp);
  
  return filecnt;
}  /* End readlistfile() */


/***************************************************************************
 * addnode:
 *
 * Add node to the specified list.
 ***************************************************************************/
static void
addnode (struct listnode **listroot, char *key, char *data)
{
  struct listnode *lastlp, *newlp;
  
  if ( data == NULL )
    {
      ms_log (2, "addnode(): No file name specified\n");
      return;
    }
  
  lastlp = *listroot;
  while ( lastlp != 0 )
    {
      if ( lastlp->next == 0 )
        break;
      
      lastlp = lastlp->next;
    }
  
  newlp = (struct listnode *) malloc (sizeof (struct listnode));
  if ( key ) newlp->key = strdup(key);
  else newlp->key = key;
  if ( data) newlp->data = strdup(data);
  else newlp->data = data;
  newlp->next = 0;
  
  if ( lastlp == 0 )
    *listroot = newlp;
  else
    lastlp->next = newlp;
  
}  /* End of addnode() */


/***************************************************************************
 * addmapnode:
 *
 * Add a node to a list deriving the key and data from the supplied
 * mapping string: 'key=data'.
 ***************************************************************************/
static void
addmapnode (struct listnode **listroot, char *mapping)
{
  char *key;
  char *data;

  key = mapping;
  data = strchr (mapping, '=');
  
  if ( ! data )
    {
      ms_log (2, "addmapmnode(): Cannot find '=' in mapping '%s'\n", mapping);
      return;
    }

  *data++ = '\0';
  
  /* Add to specified list */
  addnode (listroot, key, data);
  
}  /* End of addmapnode() */


/***************************************************************************
 * record_handler:
 * Saves passed records to the output file.
 ***************************************************************************/
static void
record_handler (char *record, int reclen, void *handlerdata)
{
  if ( fwrite(record, reclen, 1, ofp) != 1 )
    {
      ms_log (2, "Cannot write to output file\n");
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
	   " -B             Buffer data in memory before packing\n"
	   " -s stacode     Force the SEED station code, default is from input data\n"
	   " -n netcode     Force the SEED network code, default is blank\n"
	   " -l loccode     Force the SEED location code, default is blank\n"
	   " -r bytes       Specify record length in bytes for packing, default: 4096\n"
	   " -e encoding    Specify SEED encoding format for packing, default: 11 (Steim2)\n"
	   " -b byteorder   Specify byte order for packing, MSBF: 1 (default), LSBF: 0\n"
	   " -o outfile     Specify the output file, default is <inputfile>.mseed\n"
	   " -g scaling     Specify scaling for output data samples:\n"
	   "                   1->1000nV, 2->500nV, 4->250nV, 8->125nV (default), 10->100nV\n" 
	   " -t chanset     Transmogrify channel numbers to common channel codes:\n"
	   "                   0: 0->Z, 1->N, 2->E\n"
	   "                   1: 0->SHZ, 1->SHN, 2->SHE\n"
	   "                   2: 0->BHZ, 1->BHN, 2->BHE\n"
	   "                   3: 0->HHZ, 1->HHN, 2->HHE\n"
	   "                   4: 0->EHZ, 1->EHN, 2->EHE\n"
	   "\n"
	   " -T #=chan      Specify custom channel number to codes mapping\n"
	   "                  e.g.: '-T 0=LLZ -T 1=LLN -T 2=LLZ'\n"
	   "\n"
	   " file(s)        File(s) of MARS input data\n"
	   "                  If a file is prefixed with an '@' it is assumed to contain\n"
           "                  a list of data files to be read, one file  per line.\n"
	   "\n"
	   "Supported Mini-SEED encoding formats:\n"
	   " 1  : 16-bit integers (only works if samples can be represented in 16-bits)\n"
	   " 3  : 32-bit integers\n"
	   " 10 : Steim 1 compression\n"
	   " 11 : Steim 2 compression\n"
	   "\n"
	   "NOTE:\n"
	   "Sample values will be scaled by default and may not necessarily be microvolts\n"
	   "\n");
}  /* End of usage() */
