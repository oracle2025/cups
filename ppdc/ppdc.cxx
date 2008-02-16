//
// "$Id$"
//
//   PPD file compiler main entry for the CUPS PPD Compiler.
//
//   Copyright 2007 by Apple Inc.
//   Copyright 2002-2007 by Easy Software Products.
//
//   These coded instructions, statements, and computer programs are the
//   property of Apple Inc. and are protected by Federal copyright
//   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
//   which should have been included with this file.  If this file is
//   file is missing or damaged, see the license at "http://www.cups.org/".
//
// Contents:
//
//   main()  - Main entry for the PPD compiler.
//   usage() - Show usage and exit.
//

//
// Include necessary headers...
//

#include "ppdc.h"
#include <sys/stat.h>
#include <sys/types.h>


//
// Local functions...
//

static void	usage(void);


//
// 'main()' - Main entry for the PPD compiler.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int			i, j;		// Looping vars
  ppdcCatalog		*catalog;	// Message catalog
  const char		*outdir;	// Output directory
  ppdcSource		*src;		// PPD source file data
  ppdcDriver		*d;		// Current driver
  cups_file_t		*fp;		// PPD file
  char			*opt,		// Current option
			*value,		// Value in option
			pcfilename[1024],
					// Lowercase pcfilename
			filename[1024];	// PPD filename
  int			verbose;	// Verbosity
  ppdcArray		*locales;	// List of locales
  int			comp;		// Compress
  ppdcLineEnding	le;		// Line ending to use


  // Scan the command-line...
  catalog = NULL;
  outdir  = "ppd";
  src     = new ppdcSource();
  verbose = 0;
  locales = NULL;
  comp    = 0;
  le      = PPDC_LFONLY;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
        switch (*opt)
	{
	  case 'c' :			// Message catalog...
	      i ++;
              if (i >= argc)
                usage();

              if (verbose > 1)
	        printf("ppdc: Loading messages from \"%s\"...\n", argv[i]);

              if (!catalog)
	        catalog = new ppdcCatalog("en");

              if (catalog->load_messages(argv[i]))
	      {
        	fprintf(stderr,
		        "ppdc: Unable to load localization file \"%s\" - %s\n",
	        	argv[i], strerror(errno));
                return (1);
	      }
	      break;

          case 'd' :			// Output directory...
	      i ++;
	      if (i >= argc)
        	usage();

              if (verbose > 1)
	        printf("ppdc: Writing PPD files to directory \"%s\"...\n",
		       argv[i]);

	      outdir = argv[i];
	      break;

          case 'l' :			// Language(s)...
	      i ++;
	      if (i >= argc)
        	usage();

              if (strchr(argv[i], ','))
	      {
	        // Comma-delimited list of languages...
		char	temp[1024],	// Copy of language list
			*start,		// Start of current locale name
			*end;		// End of current locale name


		locales = new ppdcArray();

		strlcpy(temp, argv[i], sizeof(temp));
		for (start = temp; *start; start = end)
		{
		  if ((end = strchr(start, ',')) != NULL)
		    *end++ = '\0';
		  else
		    end = start + strlen(start);

                  if (end > start)
		    locales->add(new ppdcString(start));
		}
	      }
	      else
	      {
        	if (verbose > 1)
	          printf("ppdc: Loading messages for locale \"%s\"...\n",
			 argv[i]);

        	if (catalog)
	          delete catalog;

        	catalog = new ppdcCatalog(argv[i]);

		if (catalog->messages->count == 0)
		{
        	  fprintf(stderr,
		          "ppdc: Unable to find localization for \"%s\" - %s\n",
	        	  argv[i], strerror(errno));
                  return (1);
		}
	      }
	      break;

          case 'D' :			// Define variable
	      i ++;
	      if (i >= argc)
	        usage();

              if ((value = strchr(argv[i], '=')) != NULL)
	      {
	        *value++ = '\0';

	        src->set_variable(argv[i], value);
	      }
	      else
	        src->set_variable(argv[i], "1");
              break;

          case 'I' :			// Include directory...
	      i ++;
	      if (i >= argc)
        	usage();

              if (verbose > 1)
	        printf("ppdc: Adding include directory \"%s\"...\n", argv[i]);

	      ppdcSource::add_include(argv[i]);
	      break;

          case 'v' :			// Be verbose...
	      verbose ++;
	      break;
	    
          case 'z' :			// Compress files...
	      comp = 1;
	      break;

	  case '-' :			// --option
	      if (!strcmp(opt, "-lf"))
	      {
		le  = PPDC_LFONLY;
		opt += strlen(opt) - 1;
		break;
	      }
	      else if (!strcmp(opt, "-cr"))
	      {
		le  = PPDC_CRONLY;
		opt += strlen(opt) - 1;
		break;
	      }
	      else if (!strcmp(opt, "-crlf"))
	      {
		le  = PPDC_CRLF;
		opt += strlen(opt) - 1;
		break;
	      }
	    
	  default :			// Unknown
	      usage();
	      break;
	}
    }
    else
    {
      // Open and load the driver info file...
      if (verbose > 1)
        printf("ppdc: Loading driver information file \"%s\"...\n", argv[i]);

      src->read_file(argv[i]);
    }


  if (src->drivers->count > 0)
  {
    // Create the output directory...
    if (mkdir(outdir, 0777))
    {
      if (errno != EEXIST)
      {
	fprintf(stderr, "ppdc: Unable to create output directory %s: %s\n",
	        outdir, strerror(errno));
        return (1);
      }
    }

    // Write PPD files...
    for (d = (ppdcDriver *)src->drivers->first();
         d;
	 d = (ppdcDriver *)src->drivers->next())
    {
      // Write the PPD file for this driver...
      if (strstr(d->pc_file_name->value, ".PPD"))
      {
	// Convert PCFileName to lowercase...
	for (j = 0;
	     d->pc_file_name->value[j] && j < (int)(sizeof(pcfilename) - 1);
	     j ++)
	  pcfilename[j] = tolower(d->pc_file_name->value[j]);

	pcfilename[j] = '\0';
      }
      else
      {
	// Leave PCFileName as-is...
	strlcpy(pcfilename, d->pc_file_name->value, sizeof(pcfilename));
      }

      // Open the PPD file for writing...
      if (comp)
	snprintf(filename, sizeof(filename), "%s/%s.gz", outdir, pcfilename);
      else
	snprintf(filename, sizeof(filename), "%s/%s", outdir, pcfilename);

      fp = cupsFileOpen(filename, comp ? "w9" : "w");
      if (!fp)
      {
	fprintf(stderr, "ppdc: Unable to create PPD file \"%s\" - %s.\n",
		filename, strerror(errno));
	return (1);
      }

      if (verbose)
	printf("ppdc: Writing %s...\n", filename);

      if (d->write_ppd_file(fp, catalog, locales, src, le))
      {
	cupsFileClose(fp);
	return (1);
      }

      cupsFileClose(fp);
    }
  }
  else
    usage();

  // Delete the printer driver information...
  delete src;

  // Message catalog...
  if (catalog)
    delete catalog;

  // Return with no errors.
  return (0);
}


//
// 'usage()' - Show usage and exit.
//

static void
usage(void)
{
  puts("Usage: ppdc [options] filename.drv [ ... filenameN.drv ]");
  puts("Options:");
  puts("  -D name=value        Set named variable to value.");
  puts("  -I include-dir       Add include directory to search path.");
  puts("  -c catalog.po        Load the specified message catalog.");
  puts("  -d output-dir        Specify the output directory.");
  puts("  -l lang[,lang,...]   Specify the output language(s) (locale).");
  puts("  -v                   Be verbose (more v's for more verbosity).");
  puts("  -z                   Compress PPD files using GNU zip.");
  puts("  --cr                 End lines with CR (Mac OS 9).");
  puts("  --crlf               End lines with CR + LF (Windows).");
  puts("  --lf                 End lines with LF (UNIX/Linux/Mac OS X).");

  exit(1);
}


//
// End of "$Id$".
//