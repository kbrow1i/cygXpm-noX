/* sxpm-win: derived from rpng-win in the libpng distribution.
   Original license text below. These modifications

   Copyright (c) 2009  Charles Wilson

   and released under the same license terms as the original (that is,
   DUAL-LICENSED under "BSD-like with advertising clause" and GNU
   GPL v2 or later. (The portions adapted from XEmacs, which are clearly
   marked, are GPL v2 or later, only).

  ---------------------------------------------------------------------------

      Copyright (c) 1998-2008 Greg Roelofs.  All rights reserved.

      This software is provided "as is," without warranty of any kind,
      express or implied.  In no event shall the author or contributors
      be held liable for any damages arising in any way from the use of
      this software.

      The contents of this file are DUAL-LICENSED.  You may modify and/or
      redistribute this software according to the terms of one of the
      following two licenses (at your option):


      LICENSE 1 ("BSD-like with advertising clause"):

      Permission is granted to anyone to use this software for any purpose,
      including commercial applications, and to alter it and redistribute
      it freely, subject to the following restrictions:

      1. Redistributions of source code must retain the above copyright
         notice, disclaimer, and this list of conditions.
      2. Redistributions in binary form must reproduce the above copyright
         notice, disclaimer, and this list of conditions in the documenta-
         tion and/or other materials provided with the distribution.
      3. All advertising materials mentioning features or use of this
         software must display the following acknowledgment:

            This product includes software developed by Greg Roelofs
            and contributors for the book, "PNG: The Definitive Guide,"
            published by O'Reilly and Associates.


      LICENSE 2 (GNU GPL v2 or later):

      This program is free software; you can redistribute it and/or modify
      it under the terms of the GNU General Public License as published by
      the Free Software Foundation; either version 2 of the License, or
      (at your option) any later version.

      This program is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty of
      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
      GNU General Public License for more details.

      You should have received a copy of the GNU General Public License
      along with this program; if not, write to the Free Software Foundation,
      Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  ---------------------------------------------------------------------------*/

#define PROGNAME  "sxpm-win"
#define LONGNAME  "Simple XPM Viewer for Windows"
#define VERSION   "1.0 of 13 March 2009"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <ctype.h>
#define _WIN32_WINNT 0x0501
#include <windows.h>
#include <X11/xpm.h>
#include <errno.h>

/* this enables the Trace() macros */
/* #define DEBUG  : */

#define alpha_composite(composite, fg, alpha, bg) {               \
    ush temp = ((ush)(fg)*(ush)(alpha) +                          \
                (ush)(bg)*(ush)(255 - (ush)(alpha)) + (ush)128);  \
    (composite) = (uch)((temp + (temp >> 8)) >> 8);               \
}

typedef unsigned char   uch;
typedef unsigned short  ush;
typedef unsigned long   ulg;
#ifndef TRUE
#  define TRUE 1
#  define FALSE 0
#endif
#ifdef DEBUG
#  define Trace(x)  {fprintf x ; fflush(stderr); fflush(stdout);}
#else
#  define Trace(x)  ;
#endif


/* local prototypes */
static void       usage(FILE* f, const char* prog);
static int        string_to_ulong(const char *string, unsigned long *value);
static int        sxpm_win_create_window(HINSTANCE hInst, int showmode);
static int        sxpm_win_display_image(void);
static void       sxpm_win_cleanup(void);
LRESULT CALLBACK  sxpm_win_wndproc(HWND, UINT, WPARAM, LPARAM);
static void       dump_color(int n);
static void       dump_xpm(void);
static COLORREF   mswindows_string_to_color (const char *name);


static char titlebar[1024];
static char *progname = PROGNAME;
static char *appname = LONGNAME;
static char *filename;
static FILE *infile;

static uch bg_red=0, bg_green=0, bg_blue=0;
static long zoom_value = 1;

static XpmImage xpmimage;
static XpmInfo xpminfo;
static ulg image_width, image_height;
static int verbose_flag = FALSE;
static int have_xpm = FALSE;

/* Windows-specific variables */
static ulg wimage_rowbytes;
static uch *dib;
static uch *wimage_data;
static BITMAPINFOHEADER *bmih;

static HWND global_hwnd;

static struct option long_options[] = {
  {"bgcolor", required_argument, NULL, 'b'},
  {"bg", required_argument, NULL, 'b'},
  {"zoom", required_argument, NULL, 'z'},
  {"help", no_argument, NULL, 'h'},
  {"verbose", no_argument, NULL, 'v'},
  {NULL, no_argument, NULL, 0}
};

int main(int argc, char **argv)
{
  int rc, c, alen, flen;
  int error = 0;
  int have_bg = FALSE;
  COLORREF bg_color;
  MSG msg;
  HWND hwndC = GetConsoleWindow();
  HINSTANCE hInstCons = (HINSTANCE)GetWindowLongPtr( hwndC, GWLP_HINSTANCE);

  /* Now parse the command line for options and the XPM filename. */

  while (1)
    {
      int option_index = 0;
      unsigned long ulong_value;

      c = getopt_long (argc, argv, "b:z:hv", long_options, &option_index);
      if (c == -1)
        break;

      switch (c)
        {
        case 'h':
          usage (stdout, PROGNAME);
          exit (0);
          break;
        case 'v':
          verbose_flag = TRUE;
          break;
        case 'b':
          bg_color = mswindows_string_to_color(optarg);
          if (bg_color == (COLORREF)-1)
            {
              fprintf (stderr, "Invalud argument for %s: %s\n",
                       long_options[option_index].name, optarg);
              usage (stderr, PROGNAME);
              exit (1);
            }
            have_bg = TRUE;
          break;
        case 'z':
          if (string_to_ulong (optarg, &ulong_value) != 0)
            {
              fprintf (stderr, "Invalid argument for %s: %s\n", 
                       long_options[option_index].name, optarg);
              usage (stderr, PROGNAME);
              exit (1);
            }
          if (ulong_value < 1 || ulong_value > 64)
            {
              fprintf (stderr, "Argument for %s out of range [1,64]: %lu\n",
                       long_options[option_index].name, ulong_value);
              usage (stderr, PROGNAME);
              exit (1);
            }
          zoom_value = ulong_value;
          
          break;
        case '?':
          break;
        default:
          usage (stderr, PROGNAME);
          exit (1);
          break;
        }
    }

  if (argc - optind < 1)
    {
      fprintf(stderr, "An xpm filename is required.\n");
      usage (stderr, PROGNAME);
      exit (1);
    }
  if (argc - optind > 1)
    {
      fprintf(stderr, "Too many arguments.\n");
      usage (stderr, PROGNAME);
      exit (1);
    }
  filename = argv[optind];


    /* set the title-bar string, but make sure buffer doesn't overflow */

    alen = strlen(appname);
    flen = strlen(filename);
    if (alen + flen + 3 > 1023)
        sprintf(titlebar, "%s:  ...%s", appname, filename+(alen+flen+6-1023));
    else
        sprintf(titlebar, "%s:  %s", appname, filename);


    /* if the user didn't specify a background color on the command line,
     * the initialized values of 0 (black) will be used */

    if (have_bg) {
        bg_red   = (uch) bg_color         & 0x00ff;
        bg_green = (uch) (bg_color >> 8 ) & 0x00ff;
        bg_blue  = (uch) (bg_color >> 16) & 0x00ff;
    }

    /* kludge to avoid if ... if ... if ... indenting  */

    do
    {

      if (!filename) {
        ++error;
      } else if (!(infile = fopen(filename, "rb"))) {
        fprintf(stderr, PROGNAME ":  can't open XPM file [%s]\n", filename);
        ++error;
      } else {
        fclose(infile);
      }
      if (error) break;

      xpminfo.valuemask = XpmReturnComments | XpmReturnExtensions;

      /* load the image */
      Trace((stderr, "calling XpmReadFileToXpmImage()\n"))
      rc = XpmReadFileToXpmImage(filename, &xpmimage, &xpminfo);
      Trace((stderr, "done with XpmReadFileToXpmImage()\n"))

      if (rc != XpmSuccess) {
        fprintf(stderr, PROGNAME ":  can't parse XPM data [%s]\n", filename);
        ++error;
      } else {
        image_width = xpmimage.width;
        image_height = xpmimage.height;
        have_xpm = TRUE; 

        if (verbose_flag)
          dump_xpm();
      }
    }
    while(0); 

    image_width *= zoom_value;
    image_height *= zoom_value;

    /* print usage screen if any errors up to this point */
    if (error) {
      usage(stderr, PROGNAME);
      if (have_xpm == TRUE) {
        XpmFreeXpmImage(&xpmimage);
        XpmFreeXpmInfo(&xpminfo);
      }
      exit(1);
    }


    /* do the basic Windows initialization stuff, make the window and fill it
     * with the background color */

    if (sxpm_win_create_window(hInstCons, SW_SHOWNORMAL))
    {
      if (have_xpm == TRUE) {
        XpmFreeXpmImage(&xpmimage);
        XpmFreeXpmInfo(&xpminfo);
      }
      exit(2);
    }

    /* display image (composite with background if requested) */

    Trace((stderr, "calling sxpm_win_display_image()\n"))
    if (sxpm_win_display_image()) {
      if (have_xpm == TRUE) {
        XpmFreeXpmImage(&xpmimage);
        XpmFreeXpmInfo(&xpminfo);
      }
      exit(4);
    }
    Trace((stderr, "done with sxpm_win_display_image()\n"))


    /* wait for the user to tell us when to quit */

    printf(
      "Done.  Press Q, Esc or mouse button 1 (within image window) to quit.\n"
    );
    fflush(stdout);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }


    /* OK, we're done:  clean up all image and Windows resources and go away */

    sxpm_win_cleanup();
    return msg.wParam;
}

static int
string_to_ulong (const char *string, unsigned long *value)
{
  unsigned long number = 0;
  char * endp;
  errno = 0;

  /* null or empty input */
  if (!string || !*string)
    return 1;

  number = strtoul (string, &endp, 0);

  /* out of range */
  if (ERANGE == errno)
    return 1;

  /* no valid numeric input */
  if (endp == string)
    return 1;

  /* non-numeric trailing characters */
  if (*endp != '\0')
    return 1;

  *value = number;
  return 0;
}




static int
sxpm_win_create_window(HINSTANCE hInst, int showmode)
{
    uch *dest;
    int extra_width, extra_height;
    ulg i, j;
    WNDCLASSEX wndclass;


/*---------------------------------------------------------------------------
    Allocate memory for the display-specific version of the image (round up
    to multiple of 4 for Windows DIB).
  ---------------------------------------------------------------------------*/

    wimage_rowbytes = ((3*image_width + 3L) >> 2) << 2;

    if (!(dib = (uch *)malloc(sizeof(BITMAPINFOHEADER) +
                              wimage_rowbytes*image_height)))
    {
        return 4;   /* fail */
    }

/*---------------------------------------------------------------------------
    Initialize the DIB.  Negative height means to use top-down BMP ordering
    (must be uncompressed, but that's what we want).  Bit count of 1, 4 or 8
    implies a colormap of RGBX quads, but 24-bit BMPs just use B,G,R values
    directly => wimage_data begins immediately after BMP header.
  ---------------------------------------------------------------------------*/

    memset(dib, 0, sizeof(BITMAPINFOHEADER));
    bmih = (BITMAPINFOHEADER *)dib;
    bmih->biSize = sizeof(BITMAPINFOHEADER);
    bmih->biWidth = image_width;
    bmih->biHeight = -((long)image_height);
    bmih->biPlanes = 1;
    bmih->biBitCount = 24;
    bmih->biCompression = 0;
    wimage_data = dib + sizeof(BITMAPINFOHEADER);

/*---------------------------------------------------------------------------
    Fill in background color (black by default); data are in BGR order.
  ---------------------------------------------------------------------------*/

    for (j = 0;  j < image_height;  ++j) {
        dest = wimage_data + j*wimage_rowbytes;
        for (i = image_width;  i > 0;  --i) {
            *dest++ = bg_blue;
            *dest++ = bg_green;
            *dest++ = bg_red;
        }
    }

/*---------------------------------------------------------------------------
    Set the window parameters.
  ---------------------------------------------------------------------------*/

    memset(&wndclass, 0, sizeof(wndclass));

    wndclass.cbSize = sizeof(wndclass);
    wndclass.style = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc = sxpm_win_wndproc;
    wndclass.hInstance = hInst;
    wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndclass.hbrBackground = (HBRUSH)GetStockObject(DKGRAY_BRUSH);
    wndclass.lpszMenuName = NULL;
    wndclass.lpszClassName = progname;
    wndclass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClassEx(&wndclass);

/*---------------------------------------------------------------------------
    Finally, create the window.
  ---------------------------------------------------------------------------*/

    extra_width  = 2*(GetSystemMetrics(SM_CXBORDER) +
                      GetSystemMetrics(SM_CXDLGFRAME));
    extra_height = 2*(GetSystemMetrics(SM_CYBORDER) +
                      GetSystemMetrics(SM_CYDLGFRAME)) +
                      GetSystemMetrics(SM_CYCAPTION);

    global_hwnd = CreateWindow(progname, titlebar, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, image_width+extra_width,
      image_height+extra_height, NULL, NULL, hInst, NULL);

    ShowWindow(global_hwnd, showmode);
    UpdateWindow(global_hwnd);

    return 0;

} /* end function sxpm_win_create_window() */




static int
sxpm_win_display_image()
{
    uch *src, *dest;
    uch r, g, b, a;
    ulg i, row, lastrow, mr, mc;
    RECT rect;
    COLORREF *colortbl = NULL;
    COLORREF color;
    int transp_idx, transp;
    unsigned char *dptr;
    unsigned int *sptr;
    unsigned char *data;
    int image_channels;

    transp = FALSE;
 
    Trace((stderr, "(width = %ld, height = %ld)\n",
          xpmimage.width, xpmimage.height))
    Trace((stderr, "scaled to (width = %ld, height = %ld)\n",
          image_width, image_height))

    /* build a color table */
    Trace((stderr, "Creating the colortable\n"))
    colortbl = (COLORREF*)calloc(xpmimage.ncolors, sizeof(COLORREF));
    if (!colortbl)
    {
      return 1;
    }

    for (i = 0; i < (ulg) xpmimage.ncolors; i++)
    {
      XpmColor c = xpmimage.colorTable[i];
      const char* colorname = 
        (c.c_color ? c.c_color :
         (c.g_color ? c.g_color :
          (c.g4_color ? c.g4_color :
           (c.m_color ? c.m_color : NULL))));
#ifdef DEBUG
      dump_color(i);
#endif
      if (c.symbolic)
      {
        if (  !strcasecmp (c.symbolic, "BgColor")
           || !strcasecmp (c.symbolic, "None"))
        {
          transp = TRUE;
          colortbl[i] = 0x00ffffff; /* don't care */
          transp_idx = i;
          continue;
        }
        else if (colorname == 0)
        {
          free (colortbl);
          return 1;
        }
      }
      if (!strcasecmp (colorname, "None"))
      {
        transp = TRUE;
        colortbl[i] = 0x00ffffff; /* don't care */
        transp_idx = i;
        continue;
      }
      if (colorname == 0)
      {
        free (colortbl);
        return 1;
      }
      colortbl[i] = mswindows_string_to_color(colorname);
      Trace((stderr, "%d: 0x%08x\n", i, colortbl[i]))
    }
    Trace((stderr, "Finished creating the colortable\n"))

    /* allocate intermediate storage. could do this all at once
       when blitting to screen buffer, but this is simpler. */
    image_channels = (transp ? 4 : 3);
    data = (unsigned char *)malloc(image_channels * xpmimage.width * xpmimage.height);
    if (!data)
    {
      free(colortbl);
      return 1;
    }

    /* convert the image */
    Trace((stderr, "Creating the raster image\n"))
    sptr = xpmimage.data;
    dptr = data;
    for (i = 0; i < xpmimage.width * xpmimage.height; i++)
    {
      color = colortbl[*sptr];
      if (transp)
      {
        if (*sptr == transp_idx)
        {
          r=0xff;
          g=0xff;
          b=0xff;
          a=0x00; /* fully transparent */
        }
        else
        {
          /* split out the 0x02bbggrr colorref into an rgb triple */
          r=color         & 0x00ff; /* red */
          g=(color >> 8 ) & 0x00ff; /* green */
          b=(color >> 16) & 0x00ff; /* blue */
          a=0xff; /* not transparent */
        }
#if 0
        Trace((stderr, "pixel %d (color %d): (r/g/b/a)=(%02x/%02x/%02x/%0x2)\n",
               i, *sptr, r, g, b, a));
#endif
      } else {
        /* split out the 0x02bbggrr colorref into an rgb triple */
        r=color         & 0x00ff; /* red */
        g=(color >> 8 ) & 0x00ff; /* green */
        b=(color >> 16) & 0x00ff; /* blue */
#if 0
        Trace((stderr, "pixel %d (color %d): (r/g/b)=(%02x/%02x/%02x)\n",
               i, *sptr, r, g, b));
#endif
      }

      *dptr++=r;
      *dptr++=g;
      *dptr++=b;
      if (transp)
        *dptr++=a;
      ++sptr;
    }
    free (colortbl);
    Trace((stderr, "Finished creating the raster image\n"))
 
/*---------------------------------------------------------------------------
    Blast image data to buffer.  This whole routine takes place before the
    message loop begins, so there's no real point in any pseudo-progressive
    display...
  ---------------------------------------------------------------------------*/

    Trace((stderr, "Compositing to the window\n"))
    for (lastrow = row = 0;  row < xpmimage.height;  ++row)
    {
        for (mr = 0; mr < zoom_value; mr++)
        {
            src = data + row*xpmimage.width*image_channels;
            dest = wimage_data + (mr+row*zoom_value)*wimage_rowbytes;
            if (image_channels == 3)
            {
                for (i = xpmimage.width;  i > 0;  --i)
                {
                    r = *src++;
                    g = *src++;
                    b = *src++;
                    for (mc = 0; mc < zoom_value; mc++)
                    {
                        *dest++ = b;
                        *dest++ = g;   /* note reverse order */
                        *dest++ = r;
                    }
                }
            } else /* if (image_channels == 4) */ {
                for (i = xpmimage.width;  i > 0;  --i)
                {
                    r = *src++;
                    g = *src++;
                    b = *src++;
                    a = *src++;
                    for (mc = 0; mc < zoom_value; mc++)
                    {
                        if (a == 255) {
                            *dest++ = b;
                            *dest++ = g;
                            *dest++ = r;
                        } else if (a == 0) {
                            *dest++ = bg_blue;
                            *dest++ = bg_green;
                            *dest++ = bg_red;
                        } else {
                            /* this macro (copied from png.h) composites the
                             * foreground and background values and puts the
                             * result into the first argument; there are no
                             * side effects with the first argument */
                            alpha_composite(*dest++, b, a, bg_blue);
                            alpha_composite(*dest++, g, a, bg_green);
                            alpha_composite(*dest++, r, a, bg_red);
                        }
                    }
                }
            }
        }
        /* display after every 16 lines */
        if (((row+1) & 0xf) == 0) {
            rect.left = 0L;
            rect.top = (LONG)lastrow * zoom_value;
            rect.right = (LONG)image_width;                  /* possibly off by one? */
            rect.bottom = (LONG)(lastrow + 16L)*zoom_value;  /* possibly off by one? */
            InvalidateRect(global_hwnd, &rect, FALSE);
            UpdateWindow(global_hwnd);     /* similar to XFlush() */
            lastrow = row + 1;
        }
    }
    free (data);
    Trace((stderr, "Finished compositing to the window\n"))

    Trace((stderr, "calling final image-flush routine\n"))
    if (lastrow < image_height) {
        rect.left = 0L;
        rect.top = (LONG)lastrow;
        rect.right = (LONG)image_width;      /* possibly off by one? */
        rect.bottom = (LONG)image_height;    /* possibly off by one? */
        InvalidateRect(global_hwnd, &rect, FALSE);
        UpdateWindow(global_hwnd);     /* similar to XFlush() */
    }

/*
    last param determines whether or not background is wiped before paint
    InvalidateRect(global_hwnd, NULL, TRUE);
    UpdateWindow(global_hwnd);
 */

    return 0;
}





static void
sxpm_win_cleanup()
{
    if (have_xpm == TRUE) {
      XpmFreeXpmImage(&xpmimage);
      XpmFreeXpmInfo(&xpminfo);
    }
    
    if (dib) {
        free(dib);
        dib = NULL;
    }
}





LRESULT CALLBACK sxpm_win_wndproc(HWND hwnd, UINT iMsg, WPARAM wP, LPARAM lP)
{
    HDC         hdc;
    PAINTSTRUCT ps;
    int rc;

    switch (iMsg) {
        case WM_CREATE:
            /* one-time processing here, if any */
            return 0;

        case WM_PAINT:
            hdc = BeginPaint(hwnd, &ps);
                    /*                    dest                          */
            rc = StretchDIBits(hdc, 0, 0, image_width, image_height,
                    /*                    source                        */
                                    0, 0, image_width, image_height,
                                    wimage_data, (BITMAPINFO *)bmih,
                    /*              iUsage: no clue                     */
                                    0, SRCCOPY);
            EndPaint(hwnd, &ps);
            return 0;

        /* wait for the user to tell us when to quit */
        case WM_CHAR:
            switch (wP) {      /* only need one, so ignore repeat count */
                case 'q':
                case 'Q':
                case 0x1B:     /* Esc key */
                    PostQuitMessage(0);
            }
            return 0;

        case WM_LBUTTONDOWN:   /* another way of quitting */
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, iMsg, wP, lP);
}

static void
usage(FILE* f, const char* prog)
{
  int xpmIncVer=XpmLibraryVersion();
  int xpmFmt=(int)( ((float)xpmIncVer) / 10000.0F);
  int xpmVer=xpmIncVer - xpmFmt*10000;
  xpmVer=(int)( ((float)xpmVer) / 100.0F);
  int xpmRev=xpmIncVer - xpmFmt*10000 - xpmVer*100;

  fprintf(f, "%s %s:  %s\n", prog, VERSION, LONGNAME);
  fprintf(f, "compiled against libXpm version:   %d.%d.%d (%d)\n",
    XpmFormat, XpmVersion, XpmRevision, XpmIncludeVersion);
  fprintf(f, "libXpm itself compiled as version: %d.%d.%d (%d)\n",
    xpmFmt, xpmVer, xpmRev, xpmIncVer);
  
  
  fprintf(f, "\n"
    "Usage:  %s [--bgcolor bg] [--zoom sc] [-hv] file.xpm\n\n"
    "   -b, --bgcolor=bg     desired background color in one of several\n"
    "                        formats, or wellknown color names. Formats\n"
    "                        include: #rrggbb, #rrrgggbbb, #rrrrggggbbbb,\n"
    "                        rgb:rrrr/gggg/bbbb, MediumPurple4, Gray85, ...\n"
    "                        This only matters with transparent images.\n"
    "   -z, --zoom=sc        desired scale (must be 1 or larger integer)\n"
    "   -h, --help           display this help\n"
    "   -v, --verbose        display XPM information\n"
    "\nPress Q, Esc or mouse button 1 (within image window, after image\n"
    "is displayed) to quit.\n"
    "\n", prog);
}


static void dump_color(int n)
{
   XpmColor c = xpmimage.colorTable[n];
   fprintf(stderr, "%d: '%s' '%s' m'%s' g4'%s' g'%s' c'%s'\n",
      n,
      (c.string   ? c.string   : "(null)"),
      (c.symbolic ? c.symbolic : "(null)"),
      (c.m_color  ? c.m_color  : "(null)"),
      (c.g4_color ? c.g4_color : "(null)"),
      (c.g_color  ? c.g_color  : "(null)"),
      (c.c_color  ? c.c_color  : "(null)"));
}

static void dump_xpm(void)
{
   int n;

   fprintf(stderr, "WxH = %dx%d\n", xpmimage.width, xpmimage.height);
   fprintf(stderr, "cpp = %d\n", xpmimage.cpp);
   fprintf(stderr, "ncol= %d\n", xpmimage.ncolors);
   for (n = 0; n < xpmimage.ncolors; n++) {
     dump_color(n);
   }
#if 0
   for (n = 0; n < xpmimage.width * xpmimage.height; n++) {
     fprintf(stderr, "%d\n", xpmimage.data[n]);
   }
#endif
}

/* =============================================================
 * The following helper functions were adapted from XEmacs,
 * objects-msw.c.  This isn't the "normal" Xpm way to do things;
 * libXpm-noX exports windows versions of XParseColor and
 * XAllocColor, which do much the same thing. But this seemed
 * less obtuse to me. It's just a demo/test program anyway...
 * =============================================================
 */

/* mswindows-specific Lisp objects.
   Copyright (C) 1993, 1994 Free Software Foundation, Inc.
   Copyright (C) 1995 Board of Trustees, University of Illinois.
   Copyright (C) 1995 Tinker Systems.
   Copyright (C) 1995, 1996, 2000, 2001, 2002, 2004 Ben Wing.
   Copyright (C) 1995 Sun Microsystems, Inc.
   Copyright (C) 1997 Jonathan Harris.

This file is part of XEmacs.

XEmacs is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

XEmacs is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with XEmacs; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

typedef struct colormap_t
{
  const char *name;
  COLORREF colorref;
} colormap_t;

/* Colors from X11R6 "XConsortium: rgb.txt,v 10.41 94/02/20 18:39:36 rws Exp" */
/* MSWindows tends to round up the numbers in its palette, ie where X uses
 * 127, MSWindows uses 128. Colors commented as "Adjusted" are tweaked to
 * match the Windows standard palette to increase the likelihood of
 * mswindows_color_to_string() finding a named match.
 */
static const colormap_t mswindows_X_color_map[] =
{
  {"white"		, PALETTERGB (255, 255, 255) },
  {"black"		, PALETTERGB (0, 0, 0) },
  {"snow"		, PALETTERGB (255, 250, 250) },
  {"GhostWhite"		, PALETTERGB (248, 248, 255) },
  {"WhiteSmoke"		, PALETTERGB (245, 245, 245) },
  {"gainsboro"		, PALETTERGB (220, 220, 220) },
  {"FloralWhite"	, PALETTERGB (255, 250, 240) },
  {"OldLace"		, PALETTERGB (253, 245, 230) },
  {"linen"		, PALETTERGB (250, 240, 230) },
  {"AntiqueWhite"	, PALETTERGB (250, 235, 215) },
  {"PapayaWhip"		, PALETTERGB (255, 239, 213) },
  {"BlanchedAlmond"	, PALETTERGB (255, 235, 205) },
  {"bisque"		, PALETTERGB (255, 228, 196) },
  {"PeachPuff"		, PALETTERGB (255, 218, 185) },
  {"NavajoWhite"	, PALETTERGB (255, 222, 173) },
  {"moccasin"		, PALETTERGB (255, 228, 181) },
  {"cornsilk"		, PALETTERGB (255, 248, 220) },
  {"ivory"		, PALETTERGB (255, 255, 240) },
  {"LemonChiffon"	, PALETTERGB (255, 250, 205) },
  {"seashell"		, PALETTERGB (255, 245, 238) },
  {"honeydew"		, PALETTERGB (240, 255, 240) },
  {"MintCream"		, PALETTERGB (245, 255, 250) },
  {"azure"		, PALETTERGB (240, 255, 255) },
  {"AliceBlue"		, PALETTERGB (240, 248, 255) },
  {"lavender"		, PALETTERGB (230, 230, 250) },
  {"LavenderBlush"	, PALETTERGB (255, 240, 245) },
  {"MistyRose"		, PALETTERGB (255, 228, 225) },
  {"DarkSlateGray"	, PALETTERGB (47, 79, 79) },
  {"DarkSlateGrey"	, PALETTERGB (47, 79, 79) },
  {"DimGray"		, PALETTERGB (105, 105, 105) },
  {"DimGrey"		, PALETTERGB (105, 105, 105) },
  {"SlateGray"		, PALETTERGB (112, 128, 144) },
  {"SlateGrey"		, PALETTERGB (112, 128, 144) },
  {"LightSlateGray"	, PALETTERGB (119, 136, 153) },
  {"LightSlateGrey"	, PALETTERGB (119, 136, 153) },
  {"gray"		, PALETTERGB (190, 190, 190) },
  {"grey"		, PALETTERGB (190, 190, 190) },
  {"LightGrey"		, PALETTERGB (211, 211, 211) },
  {"LightGray"		, PALETTERGB (211, 211, 211) },
  {"MidnightBlue"	, PALETTERGB (25, 25, 112) },
  {"navy"		, PALETTERGB (0, 0, 128) },
  {"NavyBlue"		, PALETTERGB (0, 0, 128) },
  {"CornflowerBlue"	, PALETTERGB (100, 149, 237) },
  {"DarkSlateBlue"	, PALETTERGB (72, 61, 139) },
  {"SlateBlue"		, PALETTERGB (106, 90, 205) },
  {"MediumSlateBlue"	, PALETTERGB (123, 104, 238) },
  {"LightSlateBlue"	, PALETTERGB (132, 112, 255) },
  {"MediumBlue"		, PALETTERGB (0, 0, 205) },
  {"RoyalBlue"		, PALETTERGB (65, 105, 225) },
  {"blue"		, PALETTERGB (0, 0, 255) },
  {"DodgerBlue"		, PALETTERGB (30, 144, 255) },
  {"DeepSkyBlue"	, PALETTERGB (0, 191, 255) },
  {"SkyBlue"		, PALETTERGB (135, 206, 235) },
  {"LightSkyBlue"	, PALETTERGB (135, 206, 250) },
  {"SteelBlue"		, PALETTERGB (70, 130, 180) },
  {"LightSteelBlue"	, PALETTERGB (176, 196, 222) },
  {"LightBlue"		, PALETTERGB (173, 216, 230) },
  {"PowderBlue"		, PALETTERGB (176, 224, 230) },
  {"PaleTurquoise"	, PALETTERGB (175, 238, 238) },
  {"DarkTurquoise"	, PALETTERGB (0, 206, 209) },
  {"MediumTurquoise"	, PALETTERGB (72, 209, 204) },
  {"turquoise"		, PALETTERGB (64, 224, 208) },
  {"cyan"		, PALETTERGB (0, 255, 255) },
  {"LightCyan"		, PALETTERGB (224, 255, 255) },
  {"CadetBlue"		, PALETTERGB (95, 158, 160) },
  {"MediumAquamarine"	, PALETTERGB (102, 205, 170) },
  {"aquamarine"		, PALETTERGB (127, 255, 212) },
  {"DarkGreen"		, PALETTERGB (0, 128, 0) },	/* Adjusted */
  {"DarkOliveGreen"	, PALETTERGB (85, 107, 47) },
  {"DarkSeaGreen"	, PALETTERGB (143, 188, 143) },
  {"SeaGreen"		, PALETTERGB (46, 139, 87) },
  {"MediumSeaGreen"	, PALETTERGB (60, 179, 113) },
  {"LightSeaGreen"	, PALETTERGB (32, 178, 170) },
  {"PaleGreen"		, PALETTERGB (152, 251, 152) },
  {"SpringGreen"	, PALETTERGB (0, 255, 127) },
  {"LawnGreen"		, PALETTERGB (124, 252, 0) },
  {"green"		, PALETTERGB (0, 255, 0) },
  {"chartreuse"		, PALETTERGB (127, 255, 0) },
  {"MediumSpringGreen"	, PALETTERGB (0, 250, 154) },
  {"GreenYellow"	, PALETTERGB (173, 255, 47) },
  {"LimeGreen"		, PALETTERGB (50, 205, 50) },
  {"YellowGreen"	, PALETTERGB (154, 205, 50) },
  {"ForestGreen"	, PALETTERGB (34, 139, 34) },
  {"OliveDrab"		, PALETTERGB (107, 142, 35) },
  {"DarkKhaki"		, PALETTERGB (189, 183, 107) },
  {"khaki"		, PALETTERGB (240, 230, 140) },
  {"PaleGoldenrod"	, PALETTERGB (238, 232, 170) },
  {"LightGoldenrodYellow", PALETTERGB (250, 250, 210) },
  {"LightYellow"	, PALETTERGB (255, 255, 224) },
  {"LightYellow"	, PALETTERGB (255, 255, 225) },	/* Adjusted */
  {"yellow"		, PALETTERGB (255, 255, 0) },
  {"gold"		, PALETTERGB (255, 215, 0) },
  {"LightGoldenrod"	, PALETTERGB (238, 221, 130) },
  {"goldenrod"		, PALETTERGB (218, 165, 32) },
  {"DarkGoldenrod"	, PALETTERGB (184, 134, 11) },
  {"RosyBrown"		, PALETTERGB (188, 143, 143) },
  {"IndianRed"		, PALETTERGB (205, 92, 92) },
  {"SaddleBrown"	, PALETTERGB (139, 69, 19) },
  {"sienna"		, PALETTERGB (160, 82, 45) },
  {"peru"		, PALETTERGB (205, 133, 63) },
  {"burlywood"		, PALETTERGB (222, 184, 135) },
  {"beige"		, PALETTERGB (245, 245, 220) },
  {"wheat"		, PALETTERGB (245, 222, 179) },
  {"SandyBrown"		, PALETTERGB (244, 164, 96) },
  {"tan"		, PALETTERGB (210, 180, 140) },
  {"chocolate"		, PALETTERGB (210, 105, 30) },
  {"firebrick"		, PALETTERGB (178, 34, 34) },
  {"brown"		, PALETTERGB (165, 42, 42) },
  {"DarkSalmon"		, PALETTERGB (233, 150, 122) },
  {"salmon"		, PALETTERGB (250, 128, 114) },
  {"LightSalmon"	, PALETTERGB (255, 160, 122) },
  {"orange"		, PALETTERGB (255, 165, 0) },
  {"DarkOrange"		, PALETTERGB (255, 140, 0) },
  {"coral"		, PALETTERGB (255, 127, 80) },
  {"LightCoral"		, PALETTERGB (240, 128, 128) },
  {"tomato"		, PALETTERGB (255, 99, 71) },
  {"OrangeRed"		, PALETTERGB (255, 69, 0) },
  {"red"		, PALETTERGB (255, 0, 0) },
  {"HotPink"		, PALETTERGB (255, 105, 180) },
  {"DeepPink"		, PALETTERGB (255, 20, 147) },
  {"pink"		, PALETTERGB (255, 192, 203) },
  {"LightPink"		, PALETTERGB (255, 182, 193) },
  {"PaleVioletRed"	, PALETTERGB (219, 112, 147) },
  {"maroon"		, PALETTERGB (176, 48, 96) },
  {"MediumVioletRed"	, PALETTERGB (199, 21, 133) },
  {"VioletRed"		, PALETTERGB (208, 32, 144) },
  {"magenta"		, PALETTERGB (255, 0, 255) },
  {"violet"		, PALETTERGB (238, 130, 238) },
  {"plum"		, PALETTERGB (221, 160, 221) },
  {"orchid"		, PALETTERGB (218, 112, 214) },
  {"MediumOrchid"	, PALETTERGB (186, 85, 211) },
  {"DarkOrchid"		, PALETTERGB (153, 50, 204) },
  {"DarkViolet"		, PALETTERGB (148, 0, 211) },
  {"BlueViolet"		, PALETTERGB (138, 43, 226) },
  {"purple"		, PALETTERGB (160, 32, 240) },
  {"MediumPurple"	, PALETTERGB (147, 112, 219) },
  {"thistle"		, PALETTERGB (216, 191, 216) },
  {"snow1"		, PALETTERGB (255, 250, 250) },
  {"snow2"		, PALETTERGB (238, 233, 233) },
  {"snow3"		, PALETTERGB (205, 201, 201) },
  {"snow4"		, PALETTERGB (139, 137, 137) },
  {"seashell1"		, PALETTERGB (255, 245, 238) },
  {"seashell2"		, PALETTERGB (238, 229, 222) },
  {"seashell3"		, PALETTERGB (205, 197, 191) },
  {"seashell4"		, PALETTERGB (139, 134, 130) },
  {"AntiqueWhite1"	, PALETTERGB (255, 239, 219) },
  {"AntiqueWhite2"	, PALETTERGB (238, 223, 204) },
  {"AntiqueWhite3"	, PALETTERGB (205, 192, 176) },
  {"AntiqueWhite4"	, PALETTERGB (139, 131, 120) },
  {"bisque1"		, PALETTERGB (255, 228, 196) },
  {"bisque2"		, PALETTERGB (238, 213, 183) },
  {"bisque3"		, PALETTERGB (205, 183, 158) },
  {"bisque4"		, PALETTERGB (139, 125, 107) },
  {"PeachPuff1"		, PALETTERGB (255, 218, 185) },
  {"PeachPuff2"		, PALETTERGB (238, 203, 173) },
  {"PeachPuff3"		, PALETTERGB (205, 175, 149) },
  {"PeachPuff4"		, PALETTERGB (139, 119, 101) },
  {"NavajoWhite1"	, PALETTERGB (255, 222, 173) },
  {"NavajoWhite2"	, PALETTERGB (238, 207, 161) },
  {"NavajoWhite3"	, PALETTERGB (205, 179, 139) },
  {"NavajoWhite4"	, PALETTERGB (139, 121, 94) },
  {"LemonChiffon1"	, PALETTERGB (255, 250, 205) },
  {"LemonChiffon2"	, PALETTERGB (238, 233, 191) },
  {"LemonChiffon3"	, PALETTERGB (205, 201, 165) },
  {"LemonChiffon4"	, PALETTERGB (139, 137, 112) },
  {"cornsilk1"		, PALETTERGB (255, 248, 220) },
  {"cornsilk2"		, PALETTERGB (238, 232, 205) },
  {"cornsilk3"		, PALETTERGB (205, 200, 177) },
  {"cornsilk4"		, PALETTERGB (139, 136, 120) },
  {"ivory1"		, PALETTERGB (255, 255, 240) },
  {"ivory2"		, PALETTERGB (240, 240, 208) },	/* Adjusted */
  {"ivory3"		, PALETTERGB (205, 205, 193) },
  {"ivory4"		, PALETTERGB (139, 139, 131) },
  {"honeydew1"		, PALETTERGB (240, 255, 240) },
  {"honeydew2"		, PALETTERGB (224, 238, 224) },
  {"honeydew3"		, PALETTERGB (193, 205, 193) },
  {"honeydew4"		, PALETTERGB (131, 139, 131) },
  {"LavenderBlush1"	, PALETTERGB (255, 240, 245) },
  {"LavenderBlush2"	, PALETTERGB (238, 224, 229) },
  {"LavenderBlush3"	, PALETTERGB (205, 193, 197) },
  {"LavenderBlush4"	, PALETTERGB (139, 131, 134) },
  {"MistyRose1"		, PALETTERGB (255, 228, 225) },
  {"MistyRose2"		, PALETTERGB (238, 213, 210) },
  {"MistyRose3"		, PALETTERGB (205, 183, 181) },
  {"MistyRose4"		, PALETTERGB (139, 125, 123) },
  {"azure1"		, PALETTERGB (240, 255, 255) },
  {"azure2"		, PALETTERGB (224, 238, 238) },
  {"azure3"		, PALETTERGB (193, 205, 205) },
  {"azure4"		, PALETTERGB (131, 139, 139) },
  {"SlateBlue1"		, PALETTERGB (131, 111, 255) },
  {"SlateBlue2"		, PALETTERGB (122, 103, 238) },
  {"SlateBlue3"		, PALETTERGB (105, 89, 205) },
  {"SlateBlue4"		, PALETTERGB (71, 60, 139) },
  {"RoyalBlue1"		, PALETTERGB (72, 118, 255) },
  {"RoyalBlue2"		, PALETTERGB (67, 110, 238) },
  {"RoyalBlue3"		, PALETTERGB (58, 95, 205) },
  {"RoyalBlue4"		, PALETTERGB (39, 64, 139) },
  {"blue1"		, PALETTERGB (0, 0, 255) },
  {"blue2"		, PALETTERGB (0, 0, 238) },
  {"blue3"		, PALETTERGB (0, 0, 205) },
  {"blue4"		, PALETTERGB (0, 0, 139) },
  {"DodgerBlue1"	, PALETTERGB (30, 144, 255) },
  {"DodgerBlue2"	, PALETTERGB (28, 134, 238) },
  {"DodgerBlue3"	, PALETTERGB (24, 116, 205) },
  {"DodgerBlue4"	, PALETTERGB (16, 78, 139) },
  {"SteelBlue1"		, PALETTERGB (99, 184, 255) },
  {"SteelBlue2"		, PALETTERGB (92, 172, 238) },
  {"SteelBlue3"		, PALETTERGB (79, 148, 205) },
  {"SteelBlue4"		, PALETTERGB (54, 100, 139) },
  {"DeepSkyBlue1"	, PALETTERGB (0, 191, 255) },
  {"DeepSkyBlue2"	, PALETTERGB (0, 178, 238) },
  {"DeepSkyBlue3"	, PALETTERGB (0, 154, 205) },
  {"DeepSkyBlue4"	, PALETTERGB (0, 104, 139) },
  {"SkyBlue1"		, PALETTERGB (135, 206, 255) },
  {"SkyBlue2"		, PALETTERGB (126, 192, 238) },
  {"SkyBlue3"		, PALETTERGB (108, 166, 205) },
  {"SkyBlue4"		, PALETTERGB (74, 112, 139) },
  {"LightSkyBlue1"	, PALETTERGB (176, 226, 255) },
  {"LightSkyBlue2"	, PALETTERGB (164, 211, 238) },
  {"LightSkyBlue3"	, PALETTERGB (141, 182, 205) },
  {"LightSkyBlue4"	, PALETTERGB (96, 123, 139) },
  {"SlateGray1"		, PALETTERGB (198, 226, 255) },
  {"SlateGray2"		, PALETTERGB (185, 211, 238) },
  {"SlateGray3"		, PALETTERGB (159, 182, 205) },
  {"SlateGray4"		, PALETTERGB (108, 123, 139) },
  {"LightSteelBlue1"	, PALETTERGB (202, 225, 255) },
  {"LightSteelBlue2"	, PALETTERGB (188, 210, 238) },
  {"LightSteelBlue3"	, PALETTERGB (162, 181, 205) },
  {"LightSteelBlue4"	, PALETTERGB (110, 123, 139) },
  {"LightBlue1"		, PALETTERGB (191, 239, 255) },
  {"LightBlue2"		, PALETTERGB (178, 223, 238) },
  {"LightBlue3"		, PALETTERGB (154, 192, 205) },
  {"LightBlue4"		, PALETTERGB (104, 131, 139) },
  {"LightCyan1"		, PALETTERGB (224, 255, 255) },
  {"LightCyan2"		, PALETTERGB (209, 238, 238) },
  {"LightCyan3"		, PALETTERGB (180, 205, 205) },
  {"LightCyan4"		, PALETTERGB (122, 139, 139) },
  {"PaleTurquoise1"	, PALETTERGB (187, 255, 255) },
  {"PaleTurquoise2"	, PALETTERGB (174, 238, 238) },
  {"PaleTurquoise3"	, PALETTERGB (150, 205, 205) },
  {"PaleTurquoise4"	, PALETTERGB (102, 139, 139) },
  {"CadetBlue1"		, PALETTERGB (152, 245, 255) },
  {"CadetBlue2"		, PALETTERGB (144, 220, 240) },	/* Adjusted */
  {"CadetBlue3"		, PALETTERGB (122, 197, 205) },
  {"CadetBlue4"		, PALETTERGB (83, 134, 139) },
  {"turquoise1"		, PALETTERGB (0, 245, 255) },
  {"turquoise2"		, PALETTERGB (0, 229, 238) },
  {"turquoise3"		, PALETTERGB (0, 197, 205) },
  {"turquoise4"		, PALETTERGB (0, 134, 139) },
  {"cyan1"		, PALETTERGB (0, 255, 255) },
  {"cyan2"		, PALETTERGB (0, 238, 238) },
  {"cyan3"		, PALETTERGB (0, 205, 205) },
  {"cyan4"		, PALETTERGB (0, 139, 139) },
  {"DarkSlateGray1"	, PALETTERGB (151, 255, 255) },
  {"DarkSlateGray2"	, PALETTERGB (141, 238, 238) },
  {"DarkSlateGray3"	, PALETTERGB (121, 205, 205) },
  {"DarkSlateGray4"	, PALETTERGB (82, 139, 139) },
  {"aquamarine1"	, PALETTERGB (127, 255, 212) },
  {"aquamarine2"	, PALETTERGB (118, 238, 198) },
  {"aquamarine3"	, PALETTERGB (102, 205, 170) },
  {"aquamarine4"	, PALETTERGB (69, 139, 116) },
  {"DarkSeaGreen1"	, PALETTERGB (193, 255, 193) },
  {"DarkSeaGreen2"	, PALETTERGB (180, 238, 180) },
  {"DarkSeaGreen3"	, PALETTERGB (155, 205, 155) },
  {"DarkSeaGreen4"	, PALETTERGB (105, 139, 105) },
  {"SeaGreen1"		, PALETTERGB (84, 255, 159) },
  {"SeaGreen2"		, PALETTERGB (78, 238, 148) },
  {"SeaGreen3"		, PALETTERGB (67, 205, 128) },
  {"SeaGreen4"		, PALETTERGB (46, 139, 87) },
  {"PaleGreen1"		, PALETTERGB (154, 255, 154) },
  {"PaleGreen2"		, PALETTERGB (144, 238, 144) },
  {"PaleGreen3"		, PALETTERGB (124, 205, 124) },
  {"PaleGreen4"		, PALETTERGB (84, 139, 84) },
  {"SpringGreen1"	, PALETTERGB (0, 255, 127) },
  {"SpringGreen2"	, PALETTERGB (0, 238, 118) },
  {"SpringGreen3"	, PALETTERGB (0, 205, 102) },
  {"SpringGreen4"	, PALETTERGB (0, 139, 69) },
  {"green1"		, PALETTERGB (0, 255, 0) },
  {"green2"		, PALETTERGB (0, 238, 0) },
  {"green3"		, PALETTERGB (0, 205, 0) },
  {"green4"		, PALETTERGB (0, 139, 0) },
  {"chartreuse1"	, PALETTERGB (127, 255, 0) },
  {"chartreuse2"	, PALETTERGB (118, 238, 0) },
  {"chartreuse3"	, PALETTERGB (102, 205, 0) },
  {"chartreuse4"	, PALETTERGB (69, 139, 0) },
  {"OliveDrab1"		, PALETTERGB (192, 255, 62) },
  {"OliveDrab2"		, PALETTERGB (179, 238, 58) },
  {"OliveDrab3"		, PALETTERGB (154, 205, 50) },
  {"OliveDrab4"		, PALETTERGB (105, 139, 34) },
  {"DarkOliveGreen1"	, PALETTERGB (202, 255, 112) },
  {"DarkOliveGreen2"	, PALETTERGB (188, 238, 104) },
  {"DarkOliveGreen3"	, PALETTERGB (162, 205, 90) },
  {"DarkOliveGreen4"	, PALETTERGB (110, 139, 61) },
  {"khaki1"		, PALETTERGB (255, 246, 143) },
  {"khaki2"		, PALETTERGB (238, 230, 133) },
  {"khaki3"		, PALETTERGB (205, 198, 115) },
  {"khaki4"		, PALETTERGB (139, 134, 78) },
  {"LightGoldenrod1"	, PALETTERGB (255, 236, 139) },
  {"LightGoldenrod2"	, PALETTERGB (238, 220, 130) },
  {"LightGoldenrod3"	, PALETTERGB (205, 190, 112) },
  {"LightGoldenrod4"	, PALETTERGB (139, 129, 76) },
  {"LightYellow1"	, PALETTERGB (255, 255, 224) },
  {"LightYellow2"	, PALETTERGB (238, 238, 209) },
  {"LightYellow3"	, PALETTERGB (205, 205, 180) },
  {"LightYellow4"	, PALETTERGB (139, 139, 122) },
  {"yellow1"		, PALETTERGB (255, 255, 0) },
  {"yellow2"		, PALETTERGB (238, 238, 0) },
  {"yellow3"		, PALETTERGB (205, 205, 0) },
  {"yellow4"		, PALETTERGB (139, 139, 0) },
  {"gold1"		, PALETTERGB (255, 215, 0) },
  {"gold2"		, PALETTERGB (238, 201, 0) },
  {"gold3"		, PALETTERGB (205, 173, 0) },
  {"gold4"		, PALETTERGB (139, 117, 0) },
  {"goldenrod1"		, PALETTERGB (255, 193, 37) },
  {"goldenrod2"		, PALETTERGB (238, 180, 34) },
  {"goldenrod3"		, PALETTERGB (205, 155, 29) },
  {"goldenrod4"		, PALETTERGB (139, 105, 20) },
  {"DarkGoldenrod1"	, PALETTERGB (255, 185, 15) },
  {"DarkGoldenrod2"	, PALETTERGB (238, 173, 14) },
  {"DarkGoldenrod3"	, PALETTERGB (205, 149, 12) },
  {"DarkGoldenrod4"	, PALETTERGB (139, 101, 8) },
  {"RosyBrown1"		, PALETTERGB (255, 193, 193) },
  {"RosyBrown2"		, PALETTERGB (238, 180, 180) },
  {"RosyBrown3"		, PALETTERGB (205, 155, 155) },
  {"RosyBrown4"		, PALETTERGB (139, 105, 105) },
  {"IndianRed1"		, PALETTERGB (255, 106, 106) },
  {"IndianRed2"		, PALETTERGB (238, 99, 99) },
  {"IndianRed3"		, PALETTERGB (205, 85, 85) },
  {"IndianRed4"		, PALETTERGB (139, 58, 58) },
  {"sienna1"		, PALETTERGB (255, 130, 71) },
  {"sienna2"		, PALETTERGB (238, 121, 66) },
  {"sienna3"		, PALETTERGB (205, 104, 57) },
  {"sienna4"		, PALETTERGB (139, 71, 38) },
  {"burlywood1"		, PALETTERGB (255, 211, 155) },
  {"burlywood2"		, PALETTERGB (238, 197, 145) },
  {"burlywood3"		, PALETTERGB (205, 170, 125) },
  {"burlywood4"		, PALETTERGB (139, 115, 85) },
  {"wheat1"		, PALETTERGB (255, 231, 186) },
  {"wheat2"		, PALETTERGB (238, 216, 174) },
  {"wheat3"		, PALETTERGB (205, 186, 150) },
  {"wheat4"		, PALETTERGB (139, 126, 102) },
  {"tan1"		, PALETTERGB (255, 165, 79) },
  {"tan2"		, PALETTERGB (238, 154, 73) },
  {"tan3"		, PALETTERGB (205, 133, 63) },
  {"tan4"		, PALETTERGB (139, 90, 43) },
  {"chocolate1"		, PALETTERGB (255, 127, 36) },
  {"chocolate2"		, PALETTERGB (238, 118, 33) },
  {"chocolate3"		, PALETTERGB (205, 102, 29) },
  {"chocolate4"		, PALETTERGB (139, 69, 19) },
  {"firebrick1"		, PALETTERGB (255, 48, 48) },
  {"firebrick2"		, PALETTERGB (238, 44, 44) },
  {"firebrick3"		, PALETTERGB (205, 38, 38) },
  {"firebrick4"		, PALETTERGB (139, 26, 26) },
  {"brown1"		, PALETTERGB (255, 64, 64) },
  {"brown2"		, PALETTERGB (238, 59, 59) },
  {"brown3"		, PALETTERGB (205, 51, 51) },
  {"brown4"		, PALETTERGB (139, 35, 35) },
  {"salmon1"		, PALETTERGB (255, 140, 105) },
  {"salmon2"		, PALETTERGB (238, 130, 98) },
  {"salmon3"		, PALETTERGB (205, 112, 84) },
  {"salmon4"		, PALETTERGB (139, 76, 57) },
  {"LightSalmon1"	, PALETTERGB (255, 160, 122) },
  {"LightSalmon2"	, PALETTERGB (238, 149, 114) },
  {"LightSalmon3"	, PALETTERGB (205, 129, 98) },
  {"LightSalmon4"	, PALETTERGB (139, 87, 66) },
  {"orange1"		, PALETTERGB (255, 165, 0) },
  {"orange2"		, PALETTERGB (238, 154, 0) },
  {"orange3"		, PALETTERGB (205, 133, 0) },
  {"orange4"		, PALETTERGB (139, 90, 0) },
  {"DarkOrange1"	, PALETTERGB (255, 127, 0) },
  {"DarkOrange2"	, PALETTERGB (238, 118, 0) },
  {"DarkOrange3"	, PALETTERGB (205, 102, 0) },
  {"DarkOrange4"	, PALETTERGB (139, 69, 0) },
  {"coral1"		, PALETTERGB (255, 114, 86) },
  {"coral2"		, PALETTERGB (238, 106, 80) },
  {"coral3"		, PALETTERGB (205, 91, 69) },
  {"coral4"		, PALETTERGB (139, 62, 47) },
  {"tomato1"		, PALETTERGB (255, 99, 71) },
  {"tomato2"		, PALETTERGB (238, 92, 66) },
  {"tomato3"		, PALETTERGB (205, 79, 57) },
  {"tomato4"		, PALETTERGB (139, 54, 38) },
  {"OrangeRed1"		, PALETTERGB (255, 69, 0) },
  {"OrangeRed2"		, PALETTERGB (238, 64, 0) },
  {"OrangeRed3"		, PALETTERGB (205, 55, 0) },
  {"OrangeRed4"		, PALETTERGB (139, 37, 0) },
  {"red1"		, PALETTERGB (255, 0, 0) },
  {"red2"		, PALETTERGB (238, 0, 0) },
  {"red3"		, PALETTERGB (205, 0, 0) },
  {"red4"		, PALETTERGB (139, 0, 0) },
  {"DeepPink1"		, PALETTERGB (255, 20, 147) },
  {"DeepPink2"		, PALETTERGB (238, 18, 137) },
  {"DeepPink3"		, PALETTERGB (205, 16, 118) },
  {"DeepPink4"		, PALETTERGB (139, 10, 80) },
  {"HotPink1"		, PALETTERGB (255, 110, 180) },
  {"HotPink2"		, PALETTERGB (238, 106, 167) },
  {"HotPink3"		, PALETTERGB (205, 96, 144) },
  {"HotPink4"		, PALETTERGB (139, 58, 98) },
  {"pink1"		, PALETTERGB (255, 181, 197) },
  {"pink2"		, PALETTERGB (238, 169, 184) },
  {"pink3"		, PALETTERGB (205, 145, 158) },
  {"pink4"		, PALETTERGB (139, 99, 108) },
  {"LightPink1"		, PALETTERGB (255, 174, 185) },
  {"LightPink2"		, PALETTERGB (238, 162, 173) },
  {"LightPink3"		, PALETTERGB (205, 140, 149) },
  {"LightPink4"		, PALETTERGB (139, 95, 101) },
  {"PaleVioletRed1"	, PALETTERGB (255, 130, 171) },
  {"PaleVioletRed2"	, PALETTERGB (238, 121, 159) },
  {"PaleVioletRed3"	, PALETTERGB (205, 104, 137) },
  {"PaleVioletRed4"	, PALETTERGB (139, 71, 93) },
  {"maroon1"		, PALETTERGB (255, 52, 179) },
  {"maroon2"		, PALETTERGB (238, 48, 167) },
  {"maroon3"		, PALETTERGB (205, 41, 144) },
  {"maroon4"		, PALETTERGB (139, 28, 98) },
  {"VioletRed1"		, PALETTERGB (255, 62, 150) },
  {"VioletRed2"		, PALETTERGB (238, 58, 140) },
  {"VioletRed3"		, PALETTERGB (205, 50, 120) },
  {"VioletRed4"		, PALETTERGB (139, 34, 82) },
  {"magenta1"		, PALETTERGB (255, 0, 255) },
  {"magenta2"		, PALETTERGB (238, 0, 238) },
  {"magenta3"		, PALETTERGB (205, 0, 205) },
  {"magenta4"		, PALETTERGB (139, 0, 139) },
  {"orchid1"		, PALETTERGB (255, 131, 250) },
  {"orchid2"		, PALETTERGB (238, 122, 233) },
  {"orchid3"		, PALETTERGB (205, 105, 201) },
  {"orchid4"		, PALETTERGB (139, 71, 137) },
  {"plum1"		, PALETTERGB (255, 187, 255) },
  {"plum2"		, PALETTERGB (238, 174, 238) },
  {"plum3"		, PALETTERGB (205, 150, 205) },
  {"plum4"		, PALETTERGB (139, 102, 139) },
  {"MediumOrchid1"	, PALETTERGB (224, 102, 255) },
  {"MediumOrchid2"	, PALETTERGB (209, 95, 238) },
  {"MediumOrchid3"	, PALETTERGB (180, 82, 205) },
  {"MediumOrchid4"	, PALETTERGB (122, 55, 139) },
  {"DarkOrchid1"	, PALETTERGB (191, 62, 255) },
  {"DarkOrchid2"	, PALETTERGB (178, 58, 238) },
  {"DarkOrchid3"	, PALETTERGB (154, 50, 205) },
  {"DarkOrchid4"	, PALETTERGB (104, 34, 139) },
  {"purple1"		, PALETTERGB (155, 48, 255) },
  {"purple2"		, PALETTERGB (145, 44, 238) },
  {"purple3"		, PALETTERGB (125, 38, 205) },
  {"purple4"		, PALETTERGB (85, 26, 139) },
  {"MediumPurple1"	, PALETTERGB (171, 130, 255) },
  {"MediumPurple2"	, PALETTERGB (159, 121, 238) },
  {"MediumPurple3"	, PALETTERGB (137, 104, 205) },
  {"MediumPurple4"	, PALETTERGB (93, 71, 139) },
  {"thistle1"		, PALETTERGB (255, 225, 255) },
  {"thistle2"		, PALETTERGB (238, 210, 238) },
  {"thistle3"		, PALETTERGB (205, 181, 205) },
  {"thistle4"		, PALETTERGB (139, 123, 139) },
  {"gray0"		, PALETTERGB (0, 0, 0) },
  {"grey0"		, PALETTERGB (0, 0, 0) },
  {"gray1"		, PALETTERGB (3, 3, 3) },
  {"grey1"		, PALETTERGB (3, 3, 3) },
  {"gray2"		, PALETTERGB (5, 5, 5) },
  {"grey2"		, PALETTERGB (5, 5, 5) },
  {"gray3"		, PALETTERGB (8, 8, 8) },
  {"grey3"		, PALETTERGB (8, 8, 8) },
  {"gray4"		, PALETTERGB (10, 10, 10) },
  {"grey4"		, PALETTERGB (10, 10, 10) },
  {"gray5"		, PALETTERGB (13, 13, 13) },
  {"grey5"		, PALETTERGB (13, 13, 13) },
  {"gray6"		, PALETTERGB (15, 15, 15) },
  {"grey6"		, PALETTERGB (15, 15, 15) },
  {"gray7"		, PALETTERGB (18, 18, 18) },
  {"grey7"		, PALETTERGB (18, 18, 18) },
  {"gray8"		, PALETTERGB (20, 20, 20) },
  {"grey8"		, PALETTERGB (20, 20, 20) },
  {"gray9"		, PALETTERGB (23, 23, 23) },
  {"grey9"		, PALETTERGB (23, 23, 23) },
  {"gray10"		, PALETTERGB (26, 26, 26) },
  {"grey10"		, PALETTERGB (26, 26, 26) },
  {"gray11"		, PALETTERGB (28, 28, 28) },
  {"grey11"		, PALETTERGB (28, 28, 28) },
  {"gray12"		, PALETTERGB (31, 31, 31) },
  {"grey12"		, PALETTERGB (31, 31, 31) },
  {"gray13"		, PALETTERGB (33, 33, 33) },
  {"grey13"		, PALETTERGB (33, 33, 33) },
  {"gray14"		, PALETTERGB (36, 36, 36) },
  {"grey14"		, PALETTERGB (36, 36, 36) },
  {"gray15"		, PALETTERGB (38, 38, 38) },
  {"grey15"		, PALETTERGB (38, 38, 38) },
  {"gray16"		, PALETTERGB (41, 41, 41) },
  {"grey16"		, PALETTERGB (41, 41, 41) },
  {"gray17"		, PALETTERGB (43, 43, 43) },
  {"grey17"		, PALETTERGB (43, 43, 43) },
  {"gray18"		, PALETTERGB (46, 46, 46) },
  {"grey18"		, PALETTERGB (46, 46, 46) },
  {"gray19"		, PALETTERGB (48, 48, 48) },
  {"grey19"		, PALETTERGB (48, 48, 48) },
  {"gray20"		, PALETTERGB (51, 51, 51) },
  {"grey20"		, PALETTERGB (51, 51, 51) },
  {"gray21"		, PALETTERGB (54, 54, 54) },
  {"grey21"		, PALETTERGB (54, 54, 54) },
  {"gray22"		, PALETTERGB (56, 56, 56) },
  {"grey22"		, PALETTERGB (56, 56, 56) },
  {"gray23"		, PALETTERGB (59, 59, 59) },
  {"grey23"		, PALETTERGB (59, 59, 59) },
  {"gray24"		, PALETTERGB (61, 61, 61) },
  {"grey24"		, PALETTERGB (61, 61, 61) },
  {"gray25"		, PALETTERGB (64, 64, 64) },
  {"grey25"		, PALETTERGB (64, 64, 64) },
  {"gray26"		, PALETTERGB (66, 66, 66) },
  {"grey26"		, PALETTERGB (66, 66, 66) },
  {"gray27"		, PALETTERGB (69, 69, 69) },
  {"grey27"		, PALETTERGB (69, 69, 69) },
  {"gray28"		, PALETTERGB (71, 71, 71) },
  {"grey28"		, PALETTERGB (71, 71, 71) },
  {"gray29"		, PALETTERGB (74, 74, 74) },
  {"grey29"		, PALETTERGB (74, 74, 74) },
  {"gray30"		, PALETTERGB (77, 77, 77) },
  {"grey30"		, PALETTERGB (77, 77, 77) },
  {"gray31"		, PALETTERGB (79, 79, 79) },
  {"grey31"		, PALETTERGB (79, 79, 79) },
  {"gray32"		, PALETTERGB (82, 82, 82) },
  {"grey32"		, PALETTERGB (82, 82, 82) },
  {"gray33"		, PALETTERGB (84, 84, 84) },
  {"grey33"		, PALETTERGB (84, 84, 84) },
  {"gray34"		, PALETTERGB (87, 87, 87) },
  {"grey34"		, PALETTERGB (87, 87, 87) },
  {"gray35"		, PALETTERGB (89, 89, 89) },
  {"grey35"		, PALETTERGB (89, 89, 89) },
  {"gray36"		, PALETTERGB (92, 92, 92) },
  {"grey36"		, PALETTERGB (92, 92, 92) },
  {"gray37"		, PALETTERGB (94, 94, 94) },
  {"grey37"		, PALETTERGB (94, 94, 94) },
  {"gray38"		, PALETTERGB (97, 97, 97) },
  {"grey38"		, PALETTERGB (97, 97, 97) },
  {"gray39"		, PALETTERGB (99, 99, 99) },
  {"grey39"		, PALETTERGB (99, 99, 99) },
  {"gray40"		, PALETTERGB (102, 102, 102) },
  {"grey40"		, PALETTERGB (102, 102, 102) },
  {"gray41"		, PALETTERGB (105, 105, 105) },
  {"grey41"		, PALETTERGB (105, 105, 105) },
  {"gray42"		, PALETTERGB (107, 107, 107) },
  {"grey42"		, PALETTERGB (107, 107, 107) },
  {"gray43"		, PALETTERGB (110, 110, 110) },
  {"grey43"		, PALETTERGB (110, 110, 110) },
  {"gray44"		, PALETTERGB (112, 112, 112) },
  {"grey44"		, PALETTERGB (112, 112, 112) },
  {"gray45"		, PALETTERGB (115, 115, 115) },
  {"grey45"		, PALETTERGB (115, 115, 115) },
  {"gray46"		, PALETTERGB (117, 117, 117) },
  {"grey46"		, PALETTERGB (117, 117, 117) },
  {"gray47"		, PALETTERGB (120, 120, 120) },
  {"grey47"		, PALETTERGB (120, 120, 120) },
  {"gray48"		, PALETTERGB (122, 122, 122) },
  {"grey48"		, PALETTERGB (122, 122, 122) },
  {"gray49"		, PALETTERGB (125, 125, 125) },
  {"grey49"		, PALETTERGB (125, 125, 125) },
  {"gray50"		, PALETTERGB (128, 128, 128) },	/* Adjusted */
  {"grey50"		, PALETTERGB (128, 128, 128) },	/* Adjusted */
  {"gray51"		, PALETTERGB (130, 130, 130) },
  {"grey51"		, PALETTERGB (130, 130, 130) },
  {"gray52"		, PALETTERGB (133, 133, 133) },
  {"grey52"		, PALETTERGB (133, 133, 133) },
  {"gray53"		, PALETTERGB (135, 135, 135) },
  {"grey53"		, PALETTERGB (135, 135, 135) },
  {"gray54"		, PALETTERGB (138, 138, 138) },
  {"grey54"		, PALETTERGB (138, 138, 138) },
  {"gray55"		, PALETTERGB (140, 140, 140) },
  {"grey55"		, PALETTERGB (140, 140, 140) },
  {"gray56"		, PALETTERGB (143, 143, 143) },
  {"grey56"		, PALETTERGB (143, 143, 143) },
  {"gray57"		, PALETTERGB (145, 145, 145) },
  {"grey57"		, PALETTERGB (145, 145, 145) },
  {"gray58"		, PALETTERGB (148, 148, 148) },
  {"grey58"		, PALETTERGB (148, 148, 148) },
  {"gray59"		, PALETTERGB (150, 150, 150) },
  {"grey59"		, PALETTERGB (150, 150, 150) },
  {"gray60"		, PALETTERGB (153, 153, 153) },
  {"grey60"		, PALETTERGB (153, 153, 153) },
  {"gray61"		, PALETTERGB (156, 156, 156) },
  {"grey61"		, PALETTERGB (156, 156, 156) },
  {"gray62"		, PALETTERGB (158, 158, 158) },
  {"grey62"		, PALETTERGB (158, 158, 158) },
  {"gray63"		, PALETTERGB (161, 161, 161) },
  {"grey63"		, PALETTERGB (161, 161, 161) },
  {"gray64"		, PALETTERGB (163, 163, 163) },
  {"grey64"		, PALETTERGB (163, 163, 163) },
  {"gray65"		, PALETTERGB (166, 166, 166) },
  {"grey65"		, PALETTERGB (166, 166, 166) },
  {"gray66"		, PALETTERGB (168, 168, 168) },
  {"grey66"		, PALETTERGB (168, 168, 168) },
  {"gray67"		, PALETTERGB (171, 171, 171) },
  {"grey67"		, PALETTERGB (171, 171, 171) },
  {"gray68"		, PALETTERGB (173, 173, 173) },
  {"grey68"		, PALETTERGB (173, 173, 173) },
  {"gray69"		, PALETTERGB (176, 176, 176) },
  {"grey69"		, PALETTERGB (176, 176, 176) },
  {"gray70"		, PALETTERGB (179, 179, 179) },
  {"grey70"		, PALETTERGB (179, 179, 179) },
  {"gray71"		, PALETTERGB (181, 181, 181) },
  {"grey71"		, PALETTERGB (181, 181, 181) },
  {"gray72"		, PALETTERGB (184, 184, 184) },
  {"grey72"		, PALETTERGB (184, 184, 184) },
  {"gray73"		, PALETTERGB (186, 186, 186) },
  {"grey73"		, PALETTERGB (186, 186, 186) },
  {"gray74"		, PALETTERGB (189, 189, 189) },
  {"grey74"		, PALETTERGB (189, 189, 189) },
  {"gray75"		, PALETTERGB (192, 192, 192) },	/* Adjusted */
  {"grey75"		, PALETTERGB (192, 192, 192) },	/* Adjusted */
  {"gray76"		, PALETTERGB (194, 194, 194) },
  {"grey76"		, PALETTERGB (194, 194, 194) },
  {"gray77"		, PALETTERGB (196, 196, 196) },
  {"grey77"		, PALETTERGB (196, 196, 196) },
  {"gray78"		, PALETTERGB (199, 199, 199) },
  {"grey78"		, PALETTERGB (199, 199, 199) },
  {"gray79"		, PALETTERGB (201, 201, 201) },
  {"grey79"		, PALETTERGB (201, 201, 201) },
  {"gray80"		, PALETTERGB (204, 204, 204) },
  {"grey80"		, PALETTERGB (204, 204, 204) },
  {"gray81"		, PALETTERGB (207, 207, 207) },
  {"grey81"		, PALETTERGB (207, 207, 207) },
  {"gray82"		, PALETTERGB (209, 209, 209) },
  {"grey82"		, PALETTERGB (209, 209, 209) },
  {"gray83"		, PALETTERGB (212, 212, 212) },
  {"grey83"		, PALETTERGB (212, 212, 212) },
  {"gray84"		, PALETTERGB (214, 214, 214) },
  {"grey84"		, PALETTERGB (214, 214, 214) },
  {"gray85"		, PALETTERGB (217, 217, 217) },
  {"grey85"		, PALETTERGB (217, 217, 217) },
  {"gray86"		, PALETTERGB (219, 219, 219) },
  {"grey86"		, PALETTERGB (219, 219, 219) },
  {"gray87"		, PALETTERGB (222, 222, 222) },
  {"grey87"		, PALETTERGB (222, 222, 222) },
  {"gray88"		, PALETTERGB (224, 224, 224) },
  {"grey88"		, PALETTERGB (224, 224, 224) },
  {"gray89"		, PALETTERGB (227, 227, 227) },
  {"grey89"		, PALETTERGB (227, 227, 227) },
  {"gray90"		, PALETTERGB (229, 229, 229) },
  {"grey90"		, PALETTERGB (229, 229, 229) },
  {"gray91"		, PALETTERGB (232, 232, 232) },
  {"grey91"		, PALETTERGB (232, 232, 232) },
  {"gray92"		, PALETTERGB (235, 235, 235) },
  {"grey92"		, PALETTERGB (235, 235, 235) },
  {"gray93"		, PALETTERGB (237, 237, 237) },
  {"grey93"		, PALETTERGB (237, 237, 237) },
  {"gray94"		, PALETTERGB (240, 240, 240) },
  {"grey94"		, PALETTERGB (240, 240, 240) },
  {"gray95"		, PALETTERGB (242, 242, 242) },
  {"grey95"		, PALETTERGB (242, 242, 242) },
  {"gray96"		, PALETTERGB (245, 245, 245) },
  {"grey96"		, PALETTERGB (245, 245, 245) },
  {"gray97"		, PALETTERGB (247, 247, 247) },
  {"grey97"		, PALETTERGB (247, 247, 247) },
  {"gray98"		, PALETTERGB (250, 250, 250) },
  {"grey98"		, PALETTERGB (250, 250, 250) },
  {"gray99"		, PALETTERGB (252, 252, 252) },
  {"grey99"		, PALETTERGB (252, 252, 252) },
  {"gray100"		, PALETTERGB (255, 255, 255) },
  {"grey100"		, PALETTERGB (255, 255, 255) },
  {"DarkGrey"		, PALETTERGB (169, 169, 169) },
  {"DarkGray"		, PALETTERGB (169, 169, 169) },
  {"DarkBlue"		, PALETTERGB (0, 0, 128) },	/* Adjusted == Navy */
  {"DarkCyan"		, PALETTERGB (0, 128, 128) },	/* Adjusted */
  {"DarkMagenta"	, PALETTERGB (128, 0, 128) },	/* Adjusted */
  {"DarkRed"		, PALETTERGB (128, 0, 0) },	/* Adjusted */
  {"LightGreen"		, PALETTERGB (144, 238, 144) },
  /* Added to match values in the default Windows palette: */
  {"DarkYellow"		, PALETTERGB (128, 128, 0) },
  {"PaleYellow"		, PALETTERGB (255, 255, 128) }
};

/************************************************************************/
/*                               helpers                                */
/************************************************************************/

static int
hexval (char c)
{
  /* assumes ASCII and isxdigit (c) */
  if (c >= 'a')
    return c - 'a' + 10;
  else if (c >= 'A')
    return c - 'A' + 10;
  else
    return c - '0';
}

static COLORREF
mswindows_string_to_color (const char *name)
{
  int i;

  if (*name == '#')
    {
      /* numeric names look like "#RRGGBB", "#RRRGGGBBB" or "#RRRRGGGGBBBB"
	 or "rgb:rrrr/gggg/bbbb" */
      unsigned int r, g, b;

      for (i = 1; i < strlen (name); i++)
	{
	  if (!isxdigit ((int) name[i]))
	    return (COLORREF) -1;
	}
      if (strlen (name) == 7)
	{
	  r = hexval (name[1]) * 16 + hexval (name[2]);
	  g = hexval (name[3]) * 16 + hexval (name[4]);
	  b = hexval (name[5]) * 16 + hexval (name[6]);
	  return (PALETTERGB (r, g, b));
	}
      else if (strlen (name) == 10)
	{
	  r = hexval (name[1]) * 16 + hexval (name[2]);
	  g = hexval (name[4]) * 16 + hexval (name[5]);
	  b = hexval (name[7]) * 16 + hexval (name[8]);
	  return (PALETTERGB (r, g, b));
	}
      else if (strlen (name) == 13)
	{
	  r = hexval (name[1]) * 16 + hexval (name[2]);
	  g = hexval (name[5]) * 16 + hexval (name[6]);
	  b = hexval (name[9]) * 16 + hexval (name[10]);
	  return (PALETTERGB (r, g, b));
	}
    }
  else if (!strncmp (name, "rgb:", 4))
    {
      unsigned int r, g, b;

      if (sscanf ((const char *) name, "rgb:%04x/%04x/%04x", &r, &g, &b) == 3)
	{
	  int len = strlen (name);
	  if (len == 18)
	    {
	      r /= 257;
	      g /= 257;
	      b /= 257;
	    }
	  else if (len == 15)
	    {
	      r /= 17;
	      g /= 17;
	      b /= 17;
	    }
	  return (PALETTERGB (r, g, b));
	}
      else
	return (COLORREF) -1;
    }
  else if (*name)	/* Can't be an empty string */
    {
      char *nospaces = (char*)malloc(strlen (name) + 1);
      char *c = nospaces;
      while (*name)
	if (*name != ' ')
	  *c++ = *name++;
	else
	  name++;
      *c = '\0';

      for (i = 0; i < sizeof (mswindows_X_color_map) / sizeof(colormap_t); i++)
	if (!strcasecmp (nospaces, mswindows_X_color_map[i].name))
          {
            free (nospaces);
	    return (mswindows_X_color_map[i].colorref);
          }
      free (nospaces);
    }
  return (COLORREF) -1;
}

/* =============================================================
 * End of helper functions taken from XEmacs objects-msw.c
 * =============================================================
 */
