/*
 *
 *	ditpic -
 *		zoom filter and dither an image.
 *
 *			Paul Haeberli - 1997
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "izoom.h"
#include "hipass.h"
#include "propdit.h"

static void ctos(unsigned char *cptr, short *sptr, int n)
{
    while(n--) {
        if(n>=8) {
            sptr[0] = cptr[0];
            sptr[1] = cptr[1];
            sptr[2] = cptr[2];
            sptr[3] = cptr[3];
            sptr[4] = cptr[4];
            sptr[5] = cptr[5];
            sptr[6] = cptr[6];
            sptr[7] = cptr[7];
            sptr+=8;
            cptr+=8;
            n -= 7;
        } else {
            *sptr++ = *cptr++;
        }
    }
}

#if UNUSED
static void stoc(short *sptr, unsigned char *cptr, int n)
{
    while(n--) {
        if(n>=8) {
            cptr[0] = sptr[0];
            cptr[1] = sptr[1];
            cptr[2] = sptr[2];
            cptr[3] = sptr[3];
            cptr[4] = sptr[4];
            cptr[5] = sptr[5];
            cptr[6] = sptr[6];
            cptr[7] = sptr[7];
            sptr+=8;
            cptr+=8;
            n -= 7;
        } else {
            *cptr++ = *sptr++;
        }
    }
}
#endif

static void stob(short *sptr, unsigned char *cptr, int n)
{
    int needonemore = !((n+7) & 0x04);
    while(n--) {
        if(n>=4) {
            *cptr = ~((sptr[0] & 0xc0)
                      | ((sptr[1] & 0xc0) >> 2)
                      | ((sptr[2] & 0xc0) >> 4)
                      | ((sptr[3] & 0xc0) >> 6));
            sptr+=4;
            cptr++;
            n -= 3;
        } else {
            *cptr = (~(sptr[0]) & 0xc0)
                      | (((n > 1 ? ~(sptr[1]) : 0) & 0xc0) >> 2)
                      | (((n > 2 ? ~(sptr[2]) : 0) & 0xc0) >> 4);
            cptr++;
            n = 0;
        }
    }
    if (needonemore) *cptr = 0x00;
}

static unsigned char *ginbuf;
static int gixsize, giysize;
static zoom *gz;

static void getzrow(short *buf, int y)
{
    ctos(ginbuf+y*gixsize,buf,gixsize);
}

static void gethprow(short *buf, int y)
{
    getzoomrow(gz,buf,y);
}

/*
 *	ditpic! this is the function that does all the work!
 *	
 *	inbuf 	- points to an array of pixels with 1 byte per pixels.
 *		  this is the input image.
 *	ixsize 	- the width of the inbuf pixel array
 *	iysize 	- the height of the inbuf pixel array
 *
 *	outbuf 	- points to an array of pixels with 1 byte per pixel.
 *		  the zoomed, filtered and dithered results will be
 *		  placed here.
 *	oxsize 	- the width of the desired output image.
 *	oysize 	- the hgiht of the desired output image.
 *
 *	sharpen - a value between 0.0 and 1.5 or so that specifies how
 *		  much to sharpen the zoomed input image before dithering.
 */
void ditpic(unsigned char *inbuf, int ixsize, int iysize,
       unsigned char *outbuf, int oxsize, int oysize, float sharpen)
{
    int y;
    propdit *pd;
    hipass *hp;
    short *ibuf, *obuf;
    int oxrsize = ((oxsize+7)/8)*2;

    ginbuf = inbuf;
    gixsize = ixsize;
    giysize = iysize;

    pd = newpropdit(oxsize);
    hp = newhipass(gethprow,oxsize,oysize,sharpen);
    gz = newzoom(getzrow,ixsize,iysize,oxsize,oysize,BOX,1.0);

    ibuf = (short *)malloc(oxsize*sizeof(short));
    obuf = (short *)malloc(oxsize*sizeof(short));
    for(y=0; y<oysize; y++) {
	gethipassrow(hp,ibuf,y);
	propditrow(pd,ibuf,obuf);
	stob(obuf,outbuf+y*oxrsize,oxsize);
    }
    free(ibuf);
    free(obuf);
    freezoom(gz);
    freehipass(hp);
    freepropdit(pd);
}


/* #define TESTIT */
#ifdef TESTIT

#include "gl/image.h"

static unsigned char *readimage(char *name, int *xsize, int *ysize)
{
    IMAGE *iimage;
    int y;
    short *sbuf;
    unsigned char *cbuf;

    iimage = iopen(name,"r");
    if(!iimage) {
	fprintf(stderr,"readimage: can't open input file\n");
	exit(1);
    }
    sbuf = (short *)malloc(iimage->xsize*sizeof(short));
    cbuf = (unsigned char *)malloc(iimage->xsize*iimage->ysize*sizeof(char));
    for(y=0; y<iimage->ysize; y++) {
	getrow(iimage,sbuf,y,0);
	stoc(sbuf,cbuf+y*iimage->xsize,iimage->xsize);
    }
    free(sbuf);
    iclose(iimage);
    *xsize = iimage->xsize;
    *ysize = iimage->ysize;
    return cbuf;
}

static void writeimage(char *name, unsigned char *buf, int xsize, int ysize)
{
    IMAGE *oimage;
    short *sbuf;
    int y;

    oimage = iopen(name,"w",RLE(1),2,xsize,ysize,1);
    if(!oimage) {
	fprintf(stderr,"writeiamge: can't open output file\n");
	exit(1);
    }
    sbuf = (short *)malloc(oimage->xsize*sizeof(short));
    for(y=0; y<ysize; y++) {
	ctos(buf+y*xsize,sbuf,xsize);
	putrow(oimage,sbuf,y,0);
    }
    free(sbuf);
    iclose(oimage);
}

main(argc,argv)
int argc;
char **argv;
{
    unsigned char *inbuf, *outbuf;
    int xsize, ysize;
    int oxsize, oysize, i;
    float sharpen;

    if(argc<6) {
	fprintf(stderr,"ditpic: usage: propdit in.rgb out.bw oxsize oysize sharpen\n");
	exit(1);
    }
    oxsize = atoi(argv[3]);
    oysize = atoi(argv[4]);
    sharpen = atof(argv[5]);

    inbuf = readimage(argv[1],&xsize,&ysize);

    outbuf = (unsigned char *)malloc(oxsize*oysize*sizeof(char));

    ditpic(inbuf,xsize,ysize,outbuf,oxsize,oysize,sharpen);

    writeimage(argv[2],outbuf,oxsize,oysize);

    free(inbuf);
    free(outbuf);
    exit(0);
}

#endif
