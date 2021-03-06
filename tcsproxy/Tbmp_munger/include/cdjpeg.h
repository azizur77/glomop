/*
 * cdjpeg.h
 *
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains common declarations for the sample applications
 * cjpeg and djpeg.  It is NOT used by the core JPEG library.
 *
 * This file slightly butchered by Steven Gribble, Feb 1st/1997.
 */

#ifndef CDJPEG_INCL_H
#define CDJPEG_INCL_H

#define JPEG_CJPEG_DJPEG	/* define proper options in jconfig.h */
#define JPEG_INTERNAL_OPTIONS	/* cjpeg.c,djpeg.c need to see xxx_SUPPORTED */
#include "jinclude.h"
#include "jpeglib.h"
#include "jerror.h"		/* get library error codes too */
#include "cderror.h"		/* get application-specific error codes */


/*
 * Object interface for cjpeg's source file decoding modules
 */

typedef struct cjpeg_source_struct * cjpeg_source_ptr;

struct cjpeg_source_struct {
  JMETHOD(void, start_input, (j_compress_ptr cinfo,
			      cjpeg_source_ptr sinfo));
  JMETHOD(JDIMENSION, get_pixel_rows, (j_compress_ptr cinfo,
				       cjpeg_source_ptr sinfo));
  JMETHOD(void, finish_input, (j_compress_ptr cinfo,
			       cjpeg_source_ptr sinfo));

  JOCTET *srcbuf;   /* The source data buffer */
  INT32   size;     /* How big the buffer is */
  INT32   offset;   /* Current offset into the buffer */

  JSAMPARRAY buffer;
  JDIMENSION buffer_height;
};

cjpeg_source_ptr jinit_read_gif(j_compress_ptr cinfo, JOCTET *srcbuf,
				INT32 size);

#ifndef EXIT_FAILURE		/* define exit() codes if not provided */
#define EXIT_FAILURE  1
#endif
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS  0
#endif
#ifndef EXIT_WARNING
#define EXIT_WARNING  2
#endif

#endif
