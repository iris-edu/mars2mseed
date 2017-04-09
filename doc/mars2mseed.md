# <p >MARS 88/lite data to Mini-SEED converter</p>

1. [Name](#)
1. [Synopsis](#synopsis)
1. [Description](#description)
1. [Options](#options)
1. [List Files](#list-files)
1. [About Mars](#about-mars)
1. [Author](#author)

## <a id='synopsis'>Synopsis</a>

<pre >
mars2mseed [options] file1 [file2 file3 ...]
</pre>

## <a id='description'>Description</a>

<p ><b>mars2mseed</b> converts MARS-88 and MARSlite waveform data files to Mini-SEED.  One or more input files may be specified on the command line.  If an input file name is prefixed with an '@' character the file is assumed to contain a list of input data files, see <i>LIST FILES</i> below.</p>

<p >By default the MARS channel numbers are translated directly into a SEED channel codes.  This is not the expected SEED convention.  Some preset channel mappings can be specified with the -t option. Futhermore custom mappings may be explicitly specified using the -T option.</p>

<p >By default all data from a given input file is written to a file of the same name with a ".mseed" suffix.  The output data may be re-directed to a single file or stdout using the -o option.</p>

## <a id='options'>Options</a>

<b>-V</b>

<p style="padding-left: 30px;">Print program version and exit.</p>

<b>-h</b>

<p style="padding-left: 30px;">Print program usage and exit.</p>

<b>-v</b>

<p style="padding-left: 30px;">Be more verbose.  This flag can be used multiple times ("-v -v" or "-vv") for more verbosity.</p>

<b>-p</b>

<p style="padding-left: 30px;">Parse the input MARS data only, do not buffer data or write Mini-SEED. This is useful to inspect the input data without any conversion.</p>

<b>-B</b>

<p style="padding-left: 30px;">Buffer all input data into memory before packing it into Mini-SEED records.  The host computer must have enough memory to store all of the data.  By default the program will pack data as it's read in and flush it's data buffers after each input file is read.  An output file must be specified with the -o option when using this option.</p>

<b>-s </b><i>stacode</i>

<p style="padding-left: 30px;">Specify the SEED station code to use.  If not specified the station information from the input data is used.  In the case of MARS-88 data this is usually a 4 digit number.  In the case of MARSlite data this is usually a 1-4 character station code.</p>

<b>-n </b><i>netcode</i>

<p style="padding-left: 30px;">Specify the SEED network code to use, if not specified the network code will be blank.  It is highly recommended to specify a network code.</p>

<b>-l </b><i>loccode</i>

<p style="padding-left: 30px;">Specify the SEED location code to use, if not specified the location code will be blank.</p>

<b>-r </b><i>bytes</i>

<p style="padding-left: 30px;">Specify the Mini-SEED record length in <i>bytes</i>, default is 4096.</p>

<b>-e </b><i>encoding</i>

<p style="padding-left: 30px;">Specify the Mini-SEED data encoding format, default is 11 (Steim-2 compression).  Other supported encoding formats include 10 (Steim-1 compression), 1 (16-bit integers) and 3 (32-bit integers).  The 16-bit integers encoding should only be used if all data samples can be represented in 16 bits.</p>

<b>-b </b><i>byteorder</i>

<p style="padding-left: 30px;">Specify the Mini-SEED byte order, default is 1 (big-endian or most significant byte first).  The other option is 0 (little-endian or least significant byte first).  It is highly recommended to always create big-endian SEED.</p>

<b>-o </b><i>outfile</i>

<p style="padding-left: 30px;">Write all Mini-SEED records to <i>outfile</i>, if <i>outfile</i> is a single dash (-) then all Mini-SEED output will go to stdout.  All diagnostic output from the program is written to stderr and should never get mixed with data going to stdout.</p>

<b>-g </b><i>scaling</i>

<p style="padding-left: 30px;">Specify a scaling to apply to the sample values.  The default units for MARS data is microvolts with some potential gains that will result in non-integer values; scaling is required to store the values as integer data in Mini-SEED without truncation.  By default data are scaled by 8 resulting in amplitude units of 125 nanovolts.  Other recommended possibilities include 1=microvolts (no scaling), 2=500 nV, 4=250 nV, 10=100nV.  It is important to chose a scaling that will not trucate any sample values.  It is also important to make certain any metadata for the converted data includes the scaling used.</p>

<b>-t </b><i>chanset</i>

<p style="padding-left: 30px;">Use the specified preset channel number to code mapping.  The <i>chanset</i> value dictates the following mappings:</p>
<pre style="padding-left: 30px;">
  0: 0->Z, 1->N, 2->E
  1: 0->SHZ, 1->SHN, 2->SHE
  2: 0->BHZ, 1->BHN, 2->BHE
  3: 0->HHZ, 1->HHN, 2->HHE
  4: 0->EHZ, 1->EHN, 2->EHE
</pre>

<b>-T </b><i>#=chan</i>

<p style="padding-left: 30px;">Specify an explicit channel number to channel code mapping, this option may be used several times (e.g. '-T 0=SHZ -T 1=SHN -T 2=SHE').</p>

## <a id='list-files'>List Files</a>

<p >If an input file is prefixed with an '@' character the file is assumed to contain a list of file for input, one file per line.</p>

<p >Multiple list files can be combined with multiple input files on the command line.</p>

<p >An example of a simple list file:</p>

<pre >
mars88.data
marslite.data
</pre>

## <a id='about-mars'>About Mars</a>

<p >The MARS line of dataloggers is designed and produced by Lennartz Electronic GmbH: http://www.lennartz-electronic.de/</p>

## <a id='author'>Author</a>

<pre >
Chad Trabant
IRIS Data Management Center

Thanks to Aladino Govoni from OGS for providing most of the base code
for reading and parsing MARS data blocks.
</pre>


(man page 2006/07/26)
