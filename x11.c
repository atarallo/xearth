/*
 * x11.c
 * kirk johnson
 * july 1993
 *
 * Copyright (C) 1989, 1990, 1993-1995, 1999 Kirk Lauritz Johnson
 *
 * Parts of the source code (as marked) are:
 *   Copyright (C) 1989, 1990, 1991 by Jim Frost
 *   Copyright (C) 1992 by Jamie Zawinski <jwz@lucid.com>
 *
 * Permission to use, copy, modify and freely distribute xearth for
 * non-commercial and not-for-profit purposes is hereby granted
 * without fee, provided that both the above copyright notice and this
 * permission notice appear in all copies and in supporting
 * documentation.
 *
 * The author makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, INDIRECT
 * OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "xearth.h"
#ifdef FRAMEBUFFER
#include "framebuffer.h"
#include <linux/fb.h>
#else
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#endif
#include <signal.h>
#include "earthquake.h"

#define RETAIN_PROP_NAME "_XSETROOT_ID"

#define MONO_1   (0)
#define MONO_8   (1)
#define COLOR_8  (2)
#define MONO_16  (3)
#define COLOR_16 (4)
#define MONO_32  (5)
#define COLOR_32 (6)

#define LABEL_LEFT_FLUSH (1<<0)
#define LABEL_TOP_FLUSH  (1<<1)

#ifdef FRAMEBUFFER
  #define BLACK     0x0000
  #define WHITE     0xffff
  #define RED       0xf800
  #define GREEN     0x07e0
  #define BLUE      0x001f
  #define YELLOW    0xffe0
  #define MAGENTA   0xf81f
  #define LIGHTBLUE 0x07ff
  #define GREY      0x7bef
  #define LIGHTGREY 0xadf7


  #define ScreenWidth  (vinfo.xres)
  #define ScreenHeight (vinfo.yres)
  #define Scanline (vinfo.xres*vinfo.bits_per_pixel/8)

  #define Pixel int
  #define Display void;
  extern struct fb_var_screeninfo vinfo;
  extern struct fb_fix_screeninfo finfo;
#endif


static void         init_x_general _P((int, char *[]));
static void         process_opts _P((void));
static void         init_x_colors _P((void));
static void         init_x_pixmaps _P((void));
static void         init_x_root_window _P((void));
static void         init_x_separate_window _P((void));
static void         wakeup _P((int));
static int          get_bits_per_pixel _P((int));
#ifndef FRAMEBUFFER
static XFontStruct *load_x_font _P((Display *, char *));
#endif
static void         get_proj_type _P((void));
static void         get_viewing_position _P((void));
static void         get_sun_position _P((void));
static void         get_rotation _P((void));
static void         get_size _P((void));
static void         get_shift _P((void));
static void         get_labelpos _P((void));
static void         get_geometry _P((void));
static void         get_ncolors _P ((void));
static void			x11_setup _P((void));
#ifndef FRAMEBUFFER
static void         pack_mono_1 _P((u16or32 *, u_char *));
static void         pack_8 _P((u16or32 *, Pixel *, u_char *));
static void         pack_16 _P((u16or32 *, Pixel *, u_char *));
/*******************************************************************/
/* static void         pack_24 _P((u16or32 *, Pixel *, u_char *)); */
/*                                                                 */
/* WORK IN PROGRESS                                                */
/*******************************************************************/
static void         pack_24 _P((u16or32 *, Pixel *));
/*******************************************************************/
/* static void         pack_32 _P((u16or32 *, Pixel *, u_char *)); */
/*                                                                 */
/* WORK IN PROGRESS                                                */
/*******************************************************************/
static void         pack_32 _P((u16or32 *, Pixel *));
#endif
static int          x11_row _P((u_char *));
static void         x11_cleanup _P((void));
#ifdef FRAMEBUFFER
void         draw_label();
static void         mark_location( MarkerInfo *);
static void         draw_outlined_string(Pixel, Pixel,int, int, char *, int);
#else
static void         draw_label _P((Display *));
static void         mark_location _P((Display *, MarkerInfo *));
static void         draw_outlined_string _P((Display *, Pixmap, Pixel, Pixel,
                      int, int, char *, int));
static Window       GetVRoot _P((Display *));
static void         updateProperty _P((Display *, Window, const char *, Atom,
                      int, int, int));
static void         preserveResource _P((Display *, Window));
static void         freePrevious _P((Display *, Window));
static int          xkill_handler _P((Display *, XErrorEvent *));
#endif
static void draw_earthquake_location (Display *dpy, earthquake_list_t *list);
static void draw_earthquake_mark (Display *dpy, int radius, int x, int y,
                                  Pixel fill_color);
static void draw_earthquake_legend (Display *dpy, int y);

static int bpp;
static u16or32 *dith;
static u_char  *xbuf;
static int      idx;
#ifndef FRAMEBUFFER
static XImage  *xim;
static Pixmap   work_pix;
static Pixmap   disp_pix;
static int    (*orig_error_handler) _P((Display *, XErrorEvent *));
#endif

#ifdef DEBUG
static int frame = 0;
#endif /* DEBUG */

char        *progclass;
#ifndef FRAMEBUFFER
Widget       app_shell;
XtAppContext app_context;
XrmDatabase  db;

Display     *dsply;             /* display connection  */
int          scrn;              /* screen number       */
Window       root;              /* root window         */
Window       xearth_window;     /* xearth window       */
Colormap     cmap;              /* default colormap    */
Visual      *visl;              /* default visual      */
#endif
int          dpth;              /* default depth       */
Pixel        white;             /* white pixel         */
Pixel        black;             /* black pixel         */
Pixel        hlight;            /* highlight pixel     */
#ifndef FRAMEBUFFER
GC           gc;                /* graphics context    */
#endif
Pixel       *pels;              /* allocated colors    */
char        *font_name;         /* text font name      */
#ifndef FRAMEBUFFER
XFontStruct *font;              /* basic text font     */
#endif

static int   do_once;           /* only render once?   */
static int   mono;              /* render in mono?     */
static int   use_root;          /* render into root?   */
static int   window_pos_flag;   /* spec'ed window pos? */
static int   window_xvalue;     /* window x position   */
static int   window_yvalue;     /* window y position   */
static int   window_gravity;    /* window gravity      */

static int   label_xvalue;      /* label x position    */
static int   label_yvalue;      /* label y position    */
static int   label_orient;      /* label orientation   */

static Pixel get_color (Display *dpy, char *color_name);

/* earthquake color */
static char *past_time_color_name[NUM_PAST_TIME_CLASS];

static Pixel past_time_color[NUM_PAST_TIME_CLASS];

static int radius_unit;

static char *defaults[] = {
    "*proj:       orthographic",
    "*pos:        sunrel 0 0",
    "*rot:        0",
    "*shift:      0 0",
    "*mag:        1.0",
    "*shade:      on",
    "*label:      off",
    "*labelpos:   -5-5",
    "*markers:    on",
    "*markerfile: built-in",
    "*wait:       300",
    "*timewarp:   1",
    "*day:        100",
    "*night:      5",
    "*term:       1",
    "*twopix:     on",
    "*ncolors:    64",
    "*fork:       off",
    "*once:       off",
    "*nice:       0",
    "*stars:      on",
    "*starfreq:   0.002",
    "*bigstars:   0",
    "*grid:       off",
    "*grid1:      6",
    "*grid2:      15",
    "*overlayfile: none",
    "*earthquake_info: off",
    "*past_time_class1_color: red",
    "*past_time_class2_color: gold",
    "*past_time_class3_color: darkgoldenrod",
    "*past_time_class4_color: darkslategray",
    "*past_time_class5_color: dimgrey",
    "*gamma:      1.0",
    "*font:       variable",
    "*title:      xearth",
    "*iconname:   xearth",
    NULL
};
#ifndef FRAMEBUFFER
static XrmOptionDescRec options[] = {
    {"-proj", ".proj", XrmoptionSepArg, 0},
    {"-pos", ".pos", XrmoptionSepArg, 0},
    {"-rot", ".rot", XrmoptionSepArg, 0},
    {"-mag", ".mag", XrmoptionSepArg, 0},
    {"-shade", ".shade", XrmoptionNoArg, "on"},
    {"-noshade", ".shade", XrmoptionNoArg, "off"},
    {"-sunpos", ".sunpos", XrmoptionSepArg, 0},
    {"-size", ".size", XrmoptionSepArg, 0},
    {"-shift", ".shift", XrmoptionSepArg, 0},
    {"-label", ".label", XrmoptionNoArg, "on"},
    {"-nolabel", ".label", XrmoptionNoArg, "off"},
    {"-labelpos", ".labelpos", XrmoptionSepArg, 0},
    {"-markers", ".markers", XrmoptionNoArg, "on"},
    {"-nomarkers", ".markers", XrmoptionNoArg, "off"},
    {"-markerfile", ".markerfile", XrmoptionSepArg, 0},
    {"-showmarkers", ".showmarkers", XrmoptionNoArg, "on"},
    {"-overlayfile", ".overlayfile", XrmoptionSepArg, 0},
    {"-earthquake_info", ".earthquake_info", XrmoptionNoArg, "on"},
    {"-noearthquake_info", ".earthquake_info", XrmoptionNoArg, "off"},
    {"-wait", ".wait", XrmoptionSepArg, 0},
    {"-timewarp", ".timewarp", XrmoptionSepArg, 0},
    {"-day", ".day", XrmoptionSepArg, 0},
    {"-night", ".night", XrmoptionSepArg, 0},
    {"-term", ".term", XrmoptionSepArg, 0},
    {"-onepix", ".twopix", XrmoptionNoArg, "off"},
    {"-twopix", ".twopix", XrmoptionNoArg, "on"},
    {"-ncolors", ".ncolors", XrmoptionSepArg, 0},
    {"-fork", ".fork", XrmoptionNoArg, "on"},
    {"-nofork", ".fork", XrmoptionNoArg, "off"},
    {"-once", ".once", XrmoptionNoArg, "on"},
    {"-noonce", ".once", XrmoptionNoArg, "off"},
    {"-nice", ".nice", XrmoptionSepArg, 0},
    {"-version", ".version", XrmoptionNoArg, "on"},
    {"-stars", ".stars", XrmoptionNoArg, "on"},
    {"-nostars", ".stars", XrmoptionNoArg, "off"},
    {"-starfreq", ".starfreq", XrmoptionSepArg, 0},
    {"-bigstars", ".bigstars", XrmoptionSepArg, 0},
    {"-grid", ".grid", XrmoptionNoArg, "on"},
    {"-nogrid", ".grid", XrmoptionNoArg, "off"},
    {"-grid1", ".grid1", XrmoptionSepArg, 0},
    {"-grid2", ".grid2", XrmoptionSepArg, 0},
    {"-time", ".time", XrmoptionSepArg, 0},
    {"-gamma", ".gamma", XrmoptionSepArg, 0},
    {"-font", ".font", XrmoptionSepArg, 0},
    {"-mono", ".mono", XrmoptionNoArg, "on"},
    {"-nomono", ".mono", XrmoptionNoArg, "off"},
    {"-root", ".root", XrmoptionNoArg, "on"},
    {"-noroot", ".root", XrmoptionNoArg, "off"},
    {"-geometry", ".geometry", XrmoptionSepArg, 0},
    {"-title", ".title", XrmoptionSepArg, 0},
    {"-iconname", ".iconname", XrmoptionSepArg, 0},
};

#endif
void command_line_x(argc, argv)
     int   argc;
     char *argv[];
{
  init_x_general(argc, argv);
  process_opts();

  init_x_colors();
  init_x_pixmaps();
#ifndef FRAMEBUFFER
  font = load_x_font(dsply, font_name);

  if (use_root)
    init_x_root_window();
  else
    init_x_separate_window();
#endif
}


static void init_x_general(argc, argv)
     int   argc;
     char *argv[];
{
#ifdef FRAMEBUFFER
  progname  = "TTXEARTH";
#else
  progname  = argv[0];
#endif
  progclass = "XEarth";
#ifndef FRAMEBUFFER
  app_shell = XtAppInitialize(&app_context, progclass,
                              options, XtNumber(options),
                              &argc, argv, defaults, 0, 0);
#endif
  if (argc > 1) usage(NULL);
#ifndef FRAMEBUFFER
  dsply = XtDisplay(app_shell);
  scrn  = DefaultScreen(dsply);
  db    = XtDatabase(dsply);

  XtGetApplicationNameAndClass(dsply, &progname, &progclass);

  root   = RootWindow(dsply, scrn);
  cmap   = DefaultColormap(dsply, scrn);
  visl   = DefaultVisual(dsply, scrn);
  dpth   = DefaultDepth(dsply, scrn);
  wdth   = DisplayWidth(dsply, scrn);
  hght   = DisplayHeight(dsply, scrn);
  white  = WhitePixel(dsply, scrn);
  black  = BlackPixel(dsply, scrn);
  gc     = XCreateGC(dsply, root, 0, NULL);
  hlight = white;
  XSetState(dsply, gc, white, black, GXcopy, AllPlanes);

  bpp = get_bits_per_pixel(dpth);
#else
  dpth=vinfo.bits_per_pixel;
  wdth=(vinfo.xres);
  white=0xffff;
  black=0;
  hlight=white;
  bpp= vinfo.bits_per_pixel;
#endif
}


static void process_opts()
{
  if (get_boolean_resource("version", "Version"))
    version_info(1);

    if (get_boolean_resource ("showmarkers", "Showmarkers")) {
        markerfile = get_string_resource ("markerfile", "Markerfile");
        show_marker_info (markerfile);
    }

    /* process complex resources
     */
    get_proj_type ();
    get_viewing_position ();
    get_sun_position ();
    get_size ();
    get_shift ();
    get_labelpos ();
    get_rotation ();
    get_geometry ();
    get_ncolors ();

  /* process simple resources
   */
  view_mag        = get_float_resource("mag", "Mag");
  do_shade        = get_boolean_resource("shade", "Shade");
  do_label        = get_boolean_resource("label", "Label");
  do_markers      = get_boolean_resource("markers", "Markers");
  markerfile      = get_string_resource("markerfile", "Markerfile");
  wait_time       = get_integer_resource("wait", "Wait");
  time_warp       = get_float_resource("timewarp", "Timewarp");
  day             = get_integer_resource("day", "Day");
  night           = get_integer_resource("night", "Night");
  terminator      = get_integer_resource("term", "Term");
  use_two_pixmaps = get_boolean_resource("twopix", "Twopix");
#ifndef FRAMEBUFFER
  num_colors      = get_integer_resource("ncolors", "Ncolors");
#else
  num_colors      = 64;
#endif
  do_fork         = get_boolean_resource("fork", "Fork");
  do_once         = get_boolean_resource("once", "Once");
  priority        = get_integer_resource("nice", "Nice");
  do_stars        = get_boolean_resource("stars", "Stars");
  star_freq       = get_float_resource("starfreq", "Starfreq");
  big_stars       = get_integer_resource("bigstars", "Bigstars");
  do_grid         = get_boolean_resource("grid", "Grid");
#ifndef FRAMEBUFFER
  grid_big        = get_integer_resource("grid1", "Grid1");
  grid_small      = get_integer_resource("grid2", "Grid2");
#else
  grid_big        = 15;
  grid_small      = 6;
#endif
  fixed_time      = get_integer_resource("time", "Time");
  xgamma          = get_float_resource("gamma", "Gamma");
#ifndef FRAMEBUFFER
  font_name       = get_string_resource("font", "Font");
#endif
  mono            = get_boolean_resource("mono", "Mono");
    overlayfile = get_string_resource ("overlayfile", "Overlayfile");
    earthquake_info = get_boolean_resource ("earthquake_info", "Quake_info");
    past_time_color_name[past_time_class1] =
        get_string_resource ("past_time_class1_color",
                             "Past_time_class1_color");
    past_time_color_name[past_time_class2] =
        get_string_resource ("past_time_class2_color",
                             "Past_time_class2_color");
    past_time_color_name[past_time_class3] =
        get_string_resource ("past_time_class3_color",
                             "Past_time_class3_color");
    past_time_color_name[past_time_class4] =
        get_string_resource ("past_time_class4_color",
                             "Past_time_class4_color");
    past_time_color_name[past_time_class5] =
        get_string_resource ("past_time_class5_color",
                             "Past_time_class5_color");

  /* various sanity checks on simple resources
   */
  if ((view_rot < -180) || (view_rot > 360))
    fatal("viewing rotation must be between -180 and 360");
  if (view_mag <= 0)
    fatal("viewing magnification must be positive");
  if (wait_time < 0)
    fatal("arg to -wait must be non-negative");
  if (time_warp <= 0)
    fatal("arg to -timewarp must be positive");
  if ((num_colors < 3) || (num_colors > 1024))
    fatal("arg to -ncolors must be between 3 and 1024");
  if ((star_freq < 0) || (star_freq > 1))
    fatal("arg to -starfreq must be between 0 and 1");
  if ((big_stars < 0) || (big_stars > 100))
    fatal("arg to -bigstars must be between 0 and 100");
  if (grid_big <= 0)
    fatal("arg to -grid1 must be positive");
  if (grid_small <= 0)
    fatal("arg to -grid2 must be positive");
  if ((day > 100) || (day < 0))
    fatal("arg to -day must be between 0 and 100");
  if ((night > 100) || (night < 0))
    fatal("arg to -night must be between 0 and 100");
  if ((terminator > 100) || (terminator < 0))
    fatal("arg to -term must be between 0 and 100");
  if (xgamma <= 0)
    fatal("arg to -gamma must be positive");
    if (strcmp (overlayfile, "none") == 0)
        overlayfile = NULL;

  /* if we're only rendering once, make sure we don't
   * waste memory by allocating two pixmaps
   */
  if (do_once)
    use_two_pixmaps = 0;

  /* if we're working with a one-bit display, force -mono mode
   */
  if (dpth == 1)
    mono = 1;
}


static void init_x_colors()
{
  int     i;
#ifndef FRAMEBUFFER
  XColor  xc, junk;
#else
  int r,g,b;
#endif
  u_char *tmp;
  double  inv_xgamma;

  if (mono)
  {
    mono_dither_setup();
    pels = (Pixel *) malloc((unsigned) sizeof(Pixel) * 2);
    assert(pels != NULL);
    pels[0] = black;
    pels[1] = white;
  }
  else
  {
#ifndef FRAMEBUFFER
  
    if (XAllocNamedColor(dsply, cmap, "red", &xc, &junk) != 0)
      hlight = xc.pixel;
#else
      hlight = RED;
#endif
    dither_setup(num_colors);
    pels = (Pixel *) malloc((unsigned) sizeof(Pixel) * dither_ncolors);
    assert(pels != NULL);

    tmp = dither_colormap;
    inv_xgamma = 1.0 / xgamma;
    for (i=0; i<dither_ncolors; i++)
    {
#ifndef FRAMEBUFFER
      xc.red   = ((1<<16)-1) * pow(((double) tmp[0] / 255), inv_xgamma);
      xc.green = ((1<<16)-1) * pow(((double) tmp[1] / 255), inv_xgamma);
      xc.blue  = ((1<<16)-1) * pow(((double) tmp[2] / 255), inv_xgamma);

      if (XAllocColor(dsply, cmap, &xc) == 0)
        fatal("unable to allocate enough colors");
      pels[i] = xc.pixel;
#else
      r=((1<<16)-1) * pow(((double) tmp[0] / 255), inv_xgamma);
      g=((1<<16)-1) * pow(((double) tmp[1] / 255), inv_xgamma);
      b=((1<<16)-1) * pow(((double) tmp[2] / 255), inv_xgamma);
      pels[i] =((((r>>3)&0x1f)<<11)|(((g>>2)&0x3f)<<5)|((b>>3)&0x1f));
#endif

      tmp += 3;
    }

        for (i = 0; i < NUM_PAST_TIME_CLASS; ++i)
            past_time_color[i] = get_color (dsply, past_time_color_name[i]);

        radius_unit = wdth > hght ? hght / 500 : wdth / 500;
        if (radius_unit == 0)
            radius_unit = 1;    // minmum 1 pixel unit
    }
}


static void init_x_pixmaps()
{
#ifndef FRAMEBUFFER
  work_pix = XCreatePixmap(dsply, root, (unsigned) wdth,
                           (unsigned) hght, (unsigned) dpth);
  if (use_two_pixmaps)
    disp_pix = XCreatePixmap(dsply, root, (unsigned) wdth,
                             (unsigned) hght, (unsigned) dpth);
#endif
}


static void init_x_root_window()
{
#ifndef FRAMEBUFFER
  xearth_window = GetVRoot(dsply);

  /* try to free any resources retained by any previous clients that
   * scribbled in the root window (also deletes the _XSETROOT_ID
   * property from the root window, if it was there)
   */
  freePrevious(dsply, xearth_window);
#endif
  /* 18 may 1994
   *
   * setting the _XSETROOT_ID property is dangerous if xearth might be
   * killed by other means (e.g., from the shell), because some other
   * client might allocate a resource with the same resource ID that
   * xearth had stored in the _XSETROOT_ID property, so subsequent
   * attempts to free any resources retained by a client that had
   * scribbled on the root window via XKillClient() may end up killing
   * the wrong client.
   *
   * this possibility could be eliminated by setting the closedown
   * mode for the display connection to RetainPermanent, but this
   * seemed to be causing core dumps in an R5pl26 server -- i submitted
   * a bug report to the X consortium about this. i _think_ the server
   * core dumps were related to the fact that xearth can sleep for a
   * _long_ time between protocol requests, perhaps longer than it
   * takes for one server to die (e.g., when somebody logs out) and a
   * new server to be restarted, and somehow exercising the display
   * connection from the old server was causing the new one to crash?
   *
   * possible fixes:
   *
   * - replace the big sleep() with a loop that interleaves sleep(1)
   *   and, say, calls to XNoOp() to test the display connection;
   *   presumably one second is short enough to avoid the possibility
   *   of one server dying and another restarting before a call to
   *   XNoOp() catches the fact that the connection to the old server
   *   died.
   *
   * - use RetainTemporary mode instead of RetainPermanent? need to
   *   check the X documentation and figure out exactly what this
   *   would mean.
   *
   * it would be nice to install the _XSETROOT_ID property so xearth
   * interoperates gracefully with other things that try to scribble
   * on the root window (e.g., xsetroot, xloadimage, xv), but until i
   * figure out a fix to the problems described above, probably best
   * not to bother.
   */
  /* preserveResource(dsply, xearth_window); */
}


static void init_x_separate_window()
{
#ifndef FRAMEBUFFER
  XSizeHints *xsh;
  char       *title;
  char       *iname;

  xearth_window = XCreateSimpleWindow(dsply,
                                      root,
                                      window_xvalue,
                                      window_yvalue,
                                      wdth,
                                      hght,
                                      DefaultBorderWidth,
                                      white,
                                      black);

  xsh = XAllocSizeHints();
  xsh->width       = wdth;
  xsh->height      = hght;
  xsh->min_width   = wdth;
  xsh->min_height  = hght;
  xsh->max_width   = wdth;
  xsh->max_height  = hght;
  xsh->base_width  = wdth;
  xsh->base_height = hght;
  xsh->flags       = (PSize|PMinSize|PMaxSize|PBaseSize);

    if (window_pos_flag) {
        xsh->x = window_xvalue;
        xsh->y = window_yvalue;
        xsh->win_gravity = window_gravity;
        xsh->flags |= (USPosition | PWinGravity);
    }

  title = get_string_resource("title", "Title");
  iname = get_string_resource("iconname", "Iconname");
  if ((title == NULL) || (iname == NULL))
    fatal("title or iconname is NULL (this shouldn't happen)");

  XSetWMNormalHints(dsply, xearth_window, xsh);
  XStoreName(dsply, xearth_window, title);
  XSetIconName(dsply, xearth_window, iname);

  XMapRaised(dsply, xearth_window);
  XSync(dsply, False);

  XFree((char *) xsh);
  free(title);
  free(iname);
#endif
}


void x11_output() {
    while (1) {
        compute_positions();

        /* load earthquake data */
        if (earthquake_info) {
            get_earthquake_data ();
        }               // end if
        /* if we were really clever, we'd only
         * do this if the position has changed
         */
        scan_map ();
        do_dots ();

    /* for now, go ahead and reload the marker info every time
     * we redraw, but maybe change this in the future?
     */
    load_marker_info(markerfile);

    x11_setup();
    render(x11_row);
    x11_cleanup();

    if (do_once) {
#ifndef FRAMEBUFFER
      if (use_root)
        preserveResource(dsply, xearth_window);
      XSync(dsply, True);
#endif
      return;
    }

        /* schedule an alarm for wait_time seconds and pause. alarm() and
         * pause() are used instead of sleep so that if xearth is sent a
         * SIGSTOP and SIGCONT separated by more than wait_time, it will
         * refresh the screen as soon as the SIGCONT is received. this
         * facilitates graceful interaction with things like FvwmBacker.
         * (thanks to Richard Everson for passing this along.)
         */
        signal (SIGALRM, wakeup);
        signal (SIGCONT, wakeup);
        if (wait_time > 0) {
            /* only do the alarm()/pause() stuff if wait_time is non-zero,
             * else alarm() will not behave as desired.
             */
            alarm (wait_time);
            pause ();
        }
    }
}

/* no-op signal handler for catching SIGALRM and SIGCONT
 * (used to wake up from pause() system call)
 */
static void wakeup(int junk)
{
  /* nothing */
}

#ifndef FRAMEBUFFER
/* determine bits_per_pixel value for pixmaps of specified depth */
static int get_bits_per_pixel(int depth) {
  int                  i;
  int                  cnt;
  XPixmapFormatValues *pmf;
  int                  rslt;

  pmf = XListPixmapFormats(dsply, &cnt);
  if (pmf == NULL)
    fatal("unable to get pixmap format list");

  rslt = 0;
  for (i=0; i < cnt; i++)
    if (pmf[i].depth == depth) {
      rslt = pmf[i].bits_per_pixel;
      break;
    }

  if (rslt == 0)
    fatal("unable to determine pixmap format");

  XFree(pmf);

  return rslt;
}

static XFontStruct *load_x_font(dpy, fontname)
     Display *dpy;
     char    *fontname;
{
  XFontStruct *rslt;

  rslt = XLoadQueryFont(dpy, fontname);
  if (rslt == NULL) {
    rslt = XQueryFont(dpy, XGContextFromGC(gc));
    if (rslt == NULL)
      fatal("completely unable to load fonts");
    else
      warning("unable to load font, reverting to default");
  } else {
    XSetFont(dpy, gc, rslt->fid);
  }

  return rslt;
}
#endif

/* fetch and decode 'proj' resource (projection type) */
static void get_proj_type() {
  char *res;

  res = get_string_resource("proj", "Proj");
  if (res != NULL) {
    decode_proj_type(res);
    free(res);
  }
}

/* fetch and decode 'pos' resource (viewing position specifier) */
static void get_viewing_position() {
  char *res;

  res = get_string_resource("pos", "Pos");
  if (res != NULL) {
    decode_viewing_pos(res);
    free(res);
  }
}

/* fetch and decode 'sunpos' resource (sun position specifier) */
static void get_sun_position() {
  char *res;

  res = get_string_resource("sunpos", "Sunpos");
  if (res != NULL) {
    decode_sun_pos(res);
    free(res);
  }
}

/* fetch and decode 'rot' resource (rotation specifier) */
static void get_rotation() {
  char *res;

  res = get_string_resource("rot", "Rotation");
  if (res != NULL) {
    decode_rotation(res);
    free(res);
  }
}

/* fetch and decode 'size' resource (size specifier) */
static void get_size() {
  char *res;

  res = get_string_resource("size", "Size");
  if (res != NULL) {
    decode_size(res);
    free(res);
  }
}

/* fetch and decode 'shift' resource (shift specifier) */
static void get_shift() {
  char *res;

  res = get_string_resource("shift", "Shift");
  if (res != NULL)
  {
    decode_shift(res);
    free(res);
  }
}

/* fetch and decode 'ncolors' resource (color depth) */
static void get_ncolors () {
    char *res;

    res = get_string_resource ("ncolors", "Ncolors");
    if (res != NULL) {
        decode_colors (res);
        free (res);
    }
}

/* fetch and decode 'labelpos' resource (label position) */
static void get_labelpos() {
  char    *res;
  int      mask;
  int      x, y;
  unsigned w, h;

  /* it's somewhat brute-force ugly to hard-code these here,
   * duplicating information contained in defaults[], but such it is.
   */
  label_orient = 0;
  label_xvalue = wdth - 5;
  label_yvalue = hght - 5;
#ifndef FRAMEBUFFER
  res = get_string_resource("labelpos", "Labelpos");
  if (res != NULL) {
    mask = XParseGeometry(res, &x, &y, &w, &h);

    if (mask & (WidthValue | HeightValue))
      warning("width and height ignored in label position");

    if ((mask & XValue) && (mask & YValue)) {
      label_xvalue = x;
      label_yvalue = y;

      if ((mask & XNegative) == 0)
        label_orient |= LABEL_LEFT_FLUSH;

      if((mask & YNegative)==0)
        label_orient|=LABEL_TOP_FLUSH;
    } else {
      warning("label position must specify x and y offsets");
    }

    free(res);
  }
  #endif
}

/* fetch and decode 'root' and 'geometry' resource (whether to render
 * into root or separate window; if separate window, position of that
 * window) [this is pretty ugly code, but it gets the job done ...]
 */
static void get_geometry() {
  int check_geom;
  char *res;
  int mask;
  int x, y;
  unsigned int w, h;

#ifndef FRAMEBUFFER
  res = get_string_resource("root", "Root");
  if (res != NULL) {
    free(res);
    if(get_boolean_resource("root", "Root")) {
      /* user specified -root; render into the root window
       * (ignore any -geometry, if provided)
       */
      use_root   = 1;
      check_geom = 0;
    } else {
      /* user specified -noroot; render into separate window
       */
      use_root   = 0;
      check_geom = 1;
    }
  } else {
    /* user specified neither -root nor -noroot; if -geometry is
     * provided, render into separate window, else render into root
     */
    use_root   = 1;
    check_geom = 1;
  }

  /* if check_geom isn't set, nothing more to do
   */
  if (check_geom == 0) return;
#endif
  /* look for -geometry and try to make sense of it
   */
#ifndef FRAMEBUFFER
  res = get_string_resource("geometry", "Geometry");
  if (res != NULL) {
    /* if -geometry is specified, assume -noroot and set default width
     * and height (which get overridden by -geometry width and height,
     * if provided)
     */
    use_root = 0;
    wdth = DefaultWdthHght;
    hght = DefaultWdthHght;

    mask = XParseGeometry(res, &x, &y, &w, &h);

    /* extract width and height information
     */
    if ((mask & WidthValue) && (mask & HeightValue)) {
      wdth = w;
      hght = h;
    } else {
      if ((mask & WidthValue) || (mask & HeightValue))
        warning("geometry must specify both width and height");
    }

    /* extract position information
     */
    if ((mask & XValue) && (mask & YValue)) {
      window_pos_flag = 1;
      window_xvalue   = x;
      window_yvalue   = y;

      if (mask & XNegative) {
        window_xvalue += (DisplayWidth(dsply, scrn) - wdth);
        window_xvalue -= (2 * DefaultBorderWidth);
      }

      if (mask & YNegative) {
        window_yvalue += (DisplayHeight(dsply, scrn) - hght);
        window_yvalue -= (2 * DefaultBorderWidth);
      }

      if (mask & XNegative)
        if (mask & YNegative)
          window_gravity = SouthEastGravity;
        else
          window_gravity = NorthEastGravity;
      else if (mask & YNegative)
          window_gravity = SouthWestGravity;
        else
          window_gravity = NorthWestGravity;
    } else {
      if ((mask & XValue) || (mask & YValue))
        warning("geometry must specify both x and y offsets");

      window_pos_flag = 0;
      window_xvalue   = 0;
      window_yvalue   = 0;
      window_gravity  = 0;
    }
    free(res);
  } else if (use_root == 0) {
    /* if -noroot was specified but no -geometry was provided, assume
     * defaults
     */
    wdth            = DefaultWdthHght;
    hght            = DefaultWdthHght;
    window_pos_flag = 0;
    window_xvalue   = 0;
    window_yvalue   = 0;
  }
#else
  wdth            = 100;
  hght            = 100;
#endif 
}


static void x11_setup() {
  unsigned dith_size;
  unsigned xbuf_size;
#ifdef FRAMEBUFFER
  bpp=16;
  wdth=ScreenWidth;
#endif
  switch (bpp)
  {
  case 1:
    dith_size = wdth + 7;
    break;

  case 8:
  case 16:
  case 24:
  case 32:
#ifdef FRAMEBUFFER  
    dith_size = wdth+4;
#else
    dith_size = wdth;
#endif
    break;

  default:
    dith_size = 0; /* keep lint happy */
    fprintf(stderr,
            "xearth %s: fatal - unsupported pixmap format (%d bits/pixel)\n",
            VersionString, bpp);
    exit(1);
  }

  xbuf_size = (dith_size * bpp) >> 3;

  dith = (u16or32 *) malloc((unsigned) sizeof(u16or32) * dith_size);
  assert(dith != NULL);

  xbuf = (u_char *) malloc((unsigned) xbuf_size);
  assert(xbuf != NULL);
#ifndef FRAMEBUFFER
  xim = XCreateImage(dsply, visl, (unsigned) dpth, ZPixmap, 0,
                     (char *) xbuf, (unsigned) wdth, 1, 8, xbuf_size);

  if (xim->bits_per_pixel != bpp) {
    fprintf(stderr,
            "xearth %s: fatal - unexpected bits/pixel for depth %d\n",
            VersionString, dpth);
    fprintf(stderr,
            "  (expected %d bits/pixel, actual value is %d)\n",
            bpp, xim->bits_per_pixel);
    exit(1);
  }
  if (bpp == 1) {
    /* force MSBFirst bitmap_bit_order and byte_order
     */
    xim->bitmap_bit_order = MSBFirst;
    xim->byte_order       = MSBFirst;
  }
#else
  init_x_general(0,NULL);
  init_x_colors();
#endif

  idx = 0;
}

#ifndef FRAMEBUFFER

/* pack pixels into ximage format (assuming bits_per_pixel == 1,
 * bitmap_bit_order == MSBFirst, and byte_order == MSBFirst)
 */
static void pack_mono_1(src, dst)
     u16or32 *src;
     u_char  *dst;
{
  int      i, i_lim;
  unsigned val;

  i_lim = wdth;
  for (i=0; i<i_lim; i+=8) {
    val = ((src[0] << 7) | (src[1] << 6) | (src[2] << 5) |
           (src[3] << 4) | (src[4] << 3) | (src[5] << 2) |
           (src[6] << 1) | (src[7] << 0));

    /* if white is pixel 0, need to toggle the bits
     */
    dst[i>>3] = (white == 0) ? (~ val) : val;
    src += 8;
  }
}

/* pack pixels into ximage format (assuming bits_per_pixel == 8) */
static void pack_8(src, map, dst)
     u16or32 *src;
     Pixel   *map;
     u_char  *dst;
{
    int i, i_lim;
    unsigned val;

  i_lim = wdth;
  for (i=0; i<i_lim; i++) {
    val = map[src[i]];
    dst[i] = val;
  }
}
#endif

/* pack pixels into ximage format (assuming bits_per_pixel == 16) */
static void pack_16(src, map, dst)
     u16or32 *src;
     Pixel   *map;
     u_char  *dst;
{
  int      i, i_lim;
  unsigned int val;

  i_lim = wdth;
#ifdef FRAMEBUFFER
  if (0) {
#else
    if (xim->byte_order == MSBFirst) {
#endif
    for (i=0; i<i_lim; i++) {
      val    = map[src[i]];
      dst[0] = (val >> 8) & 0xff;
      dst[1] = val & 0xff;
      dst   += 2;
    }
  } else { /* (xim->byte_order == LSBFirst) */
    for (i=0; i<i_lim; i++) {
      val    = map[src[i]];
      dst[0] = val & 0xff;
      dst[1] = (val >> 8) & 0xff;
      dst   += 2;
    }
  }
}
#ifndef FRAMEBUFFER

/* pack pixels into ximage format (assuming bits_per_pixel == 24) */
static void pack_24 (u16or32 *src, Pixel *dst) {
  int      i, i_lim;
 // unsigned val;

  i_lim = wdth;

  if (xim->byte_order == MSBFirst) {
    for (i=0; i<i_lim; i++) {
      dst[0] = src[i * 3 + 0];
      dst[1] = src[i * 3 + 1];
      dst[2] = src[i * 3 + 2];
      dst   += 3;
    }
  } else { /* (xim->byte_order == LSBFirst) */
    for(i=0;i<i_lim; i++) {
      dst[0] = src[i * 3 + 2];
      dst[1] = src[i * 3 + 1];
      dst[2] = src[i * 3 + 0];
      dst   += 3;
    }
  }
}

/* pack pixels into ximage format (assuming bits_per_pixel == 32) */
static void pack_32 (u16or32 * src,Pixel *dst) {
  int i, i_lim;

  i_lim = wdth;

  if (xim->byte_order == MSBFirst) {
    for (i=0; i<i_lim; i++) {
      dst[0] = 0;
      dst[1] = src[i * 3 + 0];
      dst[2] = src[i * 3 + 1];
      dst[3] = src[i * 3 + 2];
      dst   += 4;
    }
  } else { /* (xim->byte_order == LSBFirst) */

    for (i=0; i<i_lim; i++) {
      dst[0] = src[i * 3 + 2];
      dst[1] = src[i * 3 + 1];
      dst[2] = src[i * 3 + 0];
      dst[3] = 0;
      dst   += 4;
    }
  }
}

#endif
#ifdef FRAMEBUFFER
extern char *fbp;
#endif
static int x11_row(row)
     u_char *row;
{
#ifndef FRAMEBUFFER
//int i;
//printf("now in row %d  idx=%d\n",*row,idx);
//for(i=0;i<wdth;i++) {
//  printf("%d: %d %d %d\n",i,row[3*i],row[3*i+1],row[3*i+2]);
//}
#endif
  if(bpp<24) {
    if(mono) mono_dither_row(row, dith);
    else dither_row(row, dith);
  }

  switch (bpp) {
#ifndef FRAMEBUFFER
  case 1:
    pack_mono_1(dith, xbuf);
    break;
  case 8:
    pack_8(dith, pels, xbuf);
    break;
#endif
  case 16:
    pack_16(dith, pels, xbuf);
    break;
#ifndef FRAMEBUFFER
  case 24:
    pack_24 (row, xbuf);
    break;

  case 32:
        pack_32 (row, xbuf);
    break;
#endif
  default:
    fprintf(stderr,
            "xearth %s: fatal - unsupported pixmap format (%d bits/pixel)\n",
            VersionString, bpp);
    exit(1);
  }

#ifndef FRAMEBUFFER
  XPutImage(dsply, work_pix, gc, xim, 0, 0, 0, idx, (unsigned) wdth, 1);
#else
  memmove(fbp+idx*Scanline,xbuf,Scanline);
#endif
  idx += 1;

  return 0;
}


static void x11_cleanup()
{
  MarkerInfo *minfo;
#ifndef FRAMEBUFFER
  Display    *dpy;
  Pixmap      tmp;

  XDestroyImage(xim);
#endif
  free(dith);

#ifndef FRAMEBUFFER
  dpy = dsply;
#endif

    if (earthquake_info)
        draw_earthquake_location (dpy, get_earthquake_list ());
  if(do_markers) {
    minfo = marker_info;
    while (minfo->label != NULL) {
    #ifdef FRAMEBUFFER
      mark_location( minfo);
    #else
      mark_location(dpy, minfo);
    #endif
      minfo += 1;
    }
  }

#ifdef FRAMEBUFFER
  if (do_label) draw_label();
#else
  if (do_label) draw_label(dpy);
#endif

#ifndef FRAMEBUFFER
  XSetWindowBackgroundPixmap(dpy, xearth_window, work_pix);
  XClearWindow(dpy, xearth_window);
  XSync(dpy, True);

  if (use_two_pixmaps) {
    tmp      = work_pix;
    work_pix = disp_pix;
    disp_pix = tmp;
  }
#endif
}

#ifdef FRAMEBUFFER
void draw_label() {
#else
static void draw_label(dpy)
     Display *dpy;
{
#endif
  int         dy;
  int         x, y;
  int         len;
  int         direction;
  int         ascent;
  int         descent;
  char        buf[128];
#ifndef FRAMEBUFFER
  XCharStruct extents;

  dy = font->ascent + font->descent + 1;
#else 
  dy=8;
#endif

#ifndef FRAMEBUFFER
  if (label_orient & LABEL_TOP_FLUSH) {
    y = label_yvalue + font->ascent;
  } else {
    y = (hght + label_yvalue) - font->descent;
#ifdef DEBUG
    y -= 3 * dy;    /* 4 lines of text */
#else
    y -= 2 * dy;    /* 3 lines of text */
#endif
        // include earthquake legend and update time
        if (earthquake_info) {
            y -= dy + radius_unit * radius_factor8 * 2 + 1;
        }
    }

    if (earthquake_info) {
        // draw the earthquake legend first
        draw_earthquake_legend (dpy, y);
        y += radius_unit * radius_factor8 * 2 + dy + 1;
    }                   // end if
#else
  y = label_yvalue;
#endif
#ifdef DEBUG
  frame += 1;
  sprintf(buf, "frame %d", frame);
  len = strlen(buf);
  XTextExtents(font, buf, len, &direction, &ascent, &descent, &extents);
  if(label_orient & LABEL_LEFT_FLUSH)
    x=label_xvalue-extents.lbearing;
  else
    x = (wdth + label_xvalue) - extents.rbearing;
#ifdef FRAMEBUFFER
  draw_outlined_string( white, black, x, y, buf, len);
#else
  draw_outlined_string(dpy, work_pix, white, black, x, y, buf, len);
#endif
  y += dy;
#endif /* DEBUG */

  strftime(buf, sizeof (buf), "%Y-%m-%d %H:%M %Z",
              localtime (&current_time));
  len = strlen(buf);
#ifndef FRAMEBUFFER
  XTextExtents(font, buf, len, &direction, &ascent, &descent, &extents);
  if (label_orient & LABEL_LEFT_FLUSH)
    x = label_xvalue - extents.lbearing;
  else
    x = (wdth + label_xvalue) - extents.rbearing;
  draw_outlined_string(dpy, work_pix, white, black, x, y, buf, len);
#else
  draw_outlined_string( white, black, x, y, buf, len);
#endif
  y += dy;

  sprintf(buf, "view %.1f %c %.1f %c",
          fabs(view_lat), ((view_lat < 0) ? 'S' : 'N'),
          fabs(view_lon), ((view_lon < 0) ? 'W' : 'E'));
  len = strlen(buf);
#ifndef FRAMEBUFFER
  XTextExtents(font, buf, len, &direction, &ascent, &descent, &extents);
  if (label_orient & LABEL_LEFT_FLUSH)
    x = label_xvalue - extents.lbearing;
  else
    x = (wdth + label_xvalue) - extents.rbearing;
  draw_outlined_string(dpy, work_pix, white, black, x, y, buf, len);
#else
  draw_outlined_string( white, black, x, y, buf, len);
#endif
  y += dy;

  sprintf(buf, "sun %.1f %c %.1f %c",
          fabs(sun_lat), ((sun_lat < 0) ? 'S' : 'N'),
          fabs(sun_lon), ((sun_lon < 0) ? 'W' : 'E'));
  len = strlen(buf);
#ifndef FRAMEBUFFER
  XTextExtents(font, buf, len, &direction, &ascent, &descent, &extents);
  if (label_orient & LABEL_LEFT_FLUSH)
    x = label_xvalue - extents.lbearing;
  else
    x = (wdth + label_xvalue) - extents.rbearing;
  draw_outlined_string(dpy, work_pix, white, black, x, y, buf, len);
#else 
    x = (wdth + label_xvalue);
  draw_outlined_string( white, black, x, y, buf, len);
#endif
  y += dy;
}

#ifdef FRAMEBUFFER
static void mark_location(MarkerInfo *info) {
#else
static void mark_location(dpy, info)
     Display    *dpy;
     MarkerInfo *info;
{
#endif
  int         x, y;
  int         len;
  double      lat, lon;
  double      pos[3];
  char       *text;
  int         direction;
  int         ascent;
  int         descent;
#ifndef FRAMEBUFFER
  XCharStruct extents;
#endif

  lat = info->lat * (M_PI/180);
  lon = info->lon * (M_PI/180);

  pos[0] = sin(lon) * cos(lat);
  pos[1] = sin(lat);
  pos[2] = cos(lon) * cos(lat);

  XFORM_ROTATE(pos, view_pos_info);

  if(proj_type == ProjTypeOrthographic) {
    /* if the marker isn't visible, return immediately
     */
    if (pos[2] <= 0) return;
  } else if (proj_type == ProjTypeMercator) {
    /* apply mercator projection
     */
    pos[0] = MERCATOR_X(pos[0], pos[2]);
    pos[1] = MERCATOR_Y(pos[1]);
  } else { /* (proj_type == ProjTypeCylindrical) */

    /* apply cylindrical projection
     */
    pos[0] = CYLINDRICAL_X(pos[0], pos[2]);
    pos[1] = CYLINDRICAL_Y(pos[1]);
  }

  x = XPROJECT(pos[0]);
  y = YPROJECT(pos[1]);

  XSetForeground(dpy, gc, black);
  XDrawArc(dpy, work_pix, gc, x-3, y-3, 6, 6, 0, 360*64);
  XDrawArc(dpy, work_pix, gc, x-1, y-1, 2, 2, 0, 360*64);
  XSetForeground(dpy, gc, hlight);
  XDrawArc(dpy, work_pix, gc, x-2, y-2, 4, 4, 0, 360*64);

  text = info->label;
  if (text != NULL) {
    len = strlen(text);
#ifndef FRAMEBUFFER
    XTextExtents(font, text, len, &direction, &ascent, &descent, &extents);

    switch (info->align) {
    case MarkerAlignLeft:
      x -= (extents.rbearing + 4);
      y += (font->ascent + font->descent) / 3;
      break;

    case MarkerAlignRight:
    case MarkerAlignDefault:
      x += (extents.lbearing + 3);
      y += (font->ascent + font->descent) / 3;
      break;

    case MarkerAlignAbove:
      x -= (extents.rbearing - extents.lbearing) / 2;
      y -= (extents.descent + 4);
      break;

    case MarkerAlignBelow:
      x -= (extents.rbearing - extents.lbearing) / 2;
      y += (extents.ascent + 5);
      break;

    default:
      assert(0);
    }
    draw_outlined_string(dpy, work_pix, hlight, black, x, y, text, len);
#else
    draw_outlined_string(hlight, black, x, y, text, len);
#endif
  }

  XSetForeground(dpy, gc, white);
}

#ifdef FRAMEBUFFER
void draw_outlined_string(Pixel fg, Pixel bg, int x, int y, char *text, int len) {
#else
static void draw_outlined_string(dpy, pix, fg, bg, x, y, text, len)
     Display *dpy;
     Pixmap   pix;
     Pixel    fg;
     Pixel    bg;
     int      x;
     int      y;
     char    *text;
     int      len;
{
#endif
  XSetForeground(dpy, gc, bg);
  XDrawString(dpy, pix, gc, x+1, y, text, len);
  XDrawString(dpy, pix, gc, x-1, y, text, len);
  XDrawString(dpy, pix, gc, x, y+1, text, len);
  XDrawString(dpy, pix, gc, x, y-1, text, len);
  XSetForeground(dpy, gc, fg);
  XDrawString(dpy, pix, gc, x, y, text, len);
}

#ifndef FRAMEBUFFER
/* Function Name: GetVRoot
 * Description: Gets the root window, even if it's a virtual root
 * Arguments: the display and the screen
 * Returns: the root window for the client
 *
 * (taken nearly verbatim from the june 1993 comp.windows.x FAQ, item 148)
 */
static Window GetVRoot(dpy)
     Display *dpy;
{
  int          i;
  Window       rootReturn, parentReturn, *children;
  unsigned int numChildren;
  Atom         __SWM_VROOT = None;
  Window       rslt = root;

  __SWM_VROOT = XInternAtom(dpy, "__SWM_VROOT", False);
  XQueryTree(dpy, root, &rootReturn, &parentReturn, &children, &numChildren);
  for (i=0; i<numChildren; i++) {
    Atom          actual_type;
    int           actual_format;
    unsigned long nitems, bytesafter;
    Window       *newRoot = NULL;

    /* item 148 in the FAQ neglects to mention that there is a race
     * condition here; consider a child of the root window that
     * existed when XQueryTree() was called, but has disappeared
     * before XGetWindowProperty() gets called for that window ...
     */
    if((XGetWindowProperty(dpy, children[i], __SWM_VROOT, 0, 1,
                            False, XA_WINDOW, &actual_type,
                            &actual_format, &nitems, &bytesafter,
                            (unsigned char **) &newRoot) == Success)
        && newRoot) {
      rslt = *newRoot;
      break;
    }
  }

  /* item 148 in the FAQ also neglects to mention that we probably
   * want to free the list of children after we're done with it ...
   */
  XFree((void *) children);

  return rslt;
}

#endif
/*
 * the following code is lifted nearly verbatim from jim frost's
 * xloadimage code (version 3.00). that code includes a note
 * indicating that the changes to allow proper freeing of previously
 * allocated resources made by Deron Dann Johnson (dj@eng.sun.com),
 * thus he may well be the author of this code.
 *
 * Copyright (C) 1989, 1990, 1991 by Jim Frost.
 *
 * xkill_handler() and the XSetErrorHandler() code in freePrevious()
 * were not in the original xloadimage code; this is new as of xearth
 * version 0.91.
 */
#ifndef FRAMEBUFFER
static void updateProperty(dpy, w, name, type, format, data, nelem)
     Display    *dpy;
     Window      w;
     const char *name;
     Atom        type;
     int         format;
     int         data;
     int         nelem;
{
  /* intern the property name */
  Atom atom = XInternAtom(dpy, name, 0);

  /* create or replace the property */
  XChangeProperty(dpy, w, atom, type, format, PropModeReplace,
                  (unsigned char *)&data, nelem);
}
#endif

/* Sets the close-down mode of the client to 'RetainPermanent'
 * so all client resources will be preserved after the client
 * exits.  Puts a property on the default root window containing
 * an XID of the client so that the resources can later be killed.
 */
#ifndef FRAMEBUFFER
static void preserveResource(dpy, w)
     Display *dpy;
     Window   w;
{
  /* create dummy resource */
  Pixmap pm = XCreatePixmap(dpy, w, 1, 1, 1);

  /* create/replace the property */
  updateProperty(dpy, w, RETAIN_PROP_NAME, XA_PIXMAP, 32, (int)pm, 1);

  /* retain all client resources until explicitly killed */
  XSetCloseDownMode(dpy, RetainPermanent);
}

/* Flushes any resources previously retained by the client,
 * if any exist.
 */
static void freePrevious(dpy, w)
     Display *dpy;
     Window   w;
{
  Pixmap       *pm;
  Atom          actual_type;
  int           format;
  unsigned long nitems;
  unsigned long bytes_after;

  /* intern the property name */
  Atom atom = XInternAtom(dpy, RETAIN_PROP_NAME, 0);

  /* look for existing resource allocation */
  if ((XGetWindowProperty(dpy, w, atom, 0, 1, 1 /*delete*/,
                          AnyPropertyType, &actual_type,
                          &format, &nitems, &bytes_after,
                          (unsigned char **) &pm) == Success) && (nitems == 1)) {
    if ((actual_type == XA_PIXMAP) && (format == 32) &&
        (nitems == 1) && (bytes_after == 0)) {
      /* blast it away, but first provide new X error handler in case
       * the client that installed the RETAIN_PROP_NAME (_XSETROOT_ID)
       * property on the root window has already terminated
       */
      orig_error_handler = XSetErrorHandler(xkill_handler);
      XKillClient(dpy, (XID) *pm);
      XSync(dpy, False);
      XSetErrorHandler(orig_error_handler);
      XFree((void *) pm);
    } else if (actual_type != None) {
      fprintf(stderr,
              "%s: warning: invalid format encountered for property %s\n",
              RETAIN_PROP_NAME, progname);
    }
    }
}


static int xkill_handler(dpy, xev)
     Display     *dpy;
     XErrorEvent *xev;
{
  /* ignore any BadValue errors from the call to XKillClient() in
   * freePrevious(); they should only happen if the client that
   * installed the RETAIN_PROP_NAME (_XSETROOT_ID) property on the
   * root window has already terminated
   */
  if ((xev->error_code == BadValue) && (xev->request_code == X_KillClient)) {
    fprintf(stderr, "ignoring BadValue error from XKillClient()\n");
    fflush(stderr);
    return 0;
  }

  /* pass any other errors get on to the original error handler
   */
  return orig_error_handler(dpy, xev);
}

static Pixel get_color (Display *dpy, char *color_name)
{
    XColor near_color, true_color;

    XAllocNamedColor (dpy, cmap, color_name, &near_color, &true_color);
    return near_color.pixel;
}                       // end get_color;

static void draw_earthquake_location (Display *dpy, earthquake_list_t *list)
{
    int x, y;
    double pos[3];
    int i;
    int radius;
    Pixel fill_color;
    int count = list->count - 1;

    for (i = count; i >= 0; --i) {

        memcpy (pos, list->item[i].pos, sizeof (pos));

        XFORM_ROTATE (pos, view_pos_info);

        if (proj_type == ProjTypeOrthographic) {
            // if location isn't visible, check next one
            if (pos[2] <= 0)
                continue;
        } else if (proj_type == ProjTypeMercator) {
            // apply mercator projection
            pos[0] = MERCATOR_X (pos[0], pos[2]);
            pos[1] = MERCATOR_Y (pos[1]);
        } else {        // proj_type == ProjTypeCylindrical
            // apply cylindrical projection
            pos[0] = CYLINDRICAL_X (pos[0], pos[2]);
            pos[1] = CYLINDRICAL_Y (pos[1]);
        }               // end if

        x = XPROJECT (pos[0]);
        y = YPROJECT (pos[1]);

        fill_color = past_time_color[list->item[i].past_time_class];

        // calcule the radius to draw
        radius = radius_unit * list->item[i].radius_factor;

        draw_earthquake_mark (dpy, radius, x - radius, y - radius, fill_color);
    }                   // end for
}                       // end draw_earthquake_location

static void draw_earthquake_mark (Display *dpy, int radius, int x, int y, Pixel fill_color)
{
    // fill a circle
    XSetForeground (dpy, gc, fill_color);
    XFillArc (dpy, work_pix, gc, x, y, radius * 2, radius * 2, 0, 360 * 64);
    if (radius > 1) {
        // then draw a border
        XSetForeground (dpy, gc, black);
        XDrawArc (dpy, work_pix, gc, x, y, radius * 2, radius * 2, 0,
                  360 * 64);
    }                   // end if
    XSetForeground (dpy, gc, white);
}                       // end draw_earthquake_mark

static void draw_earthquake_legend (Display *dpy, int y)
{
    int x;
    int radius;
    char buf[128];
    int len;
    int direction;
    int ascent;
    int descent;
    XCharStruct extents;
    int dy;

    dy = font->ascent + font->descent + 1;
    Pixel fill_color = past_time_color[past_time_class1];

    if (label_orient & LABEL_LEFT_FLUSH)
        x = label_xvalue + 5;
    else
        x = (wdth + label_xvalue * 2) - radius_unit * radius_factor8 * 7 / 2;

    radius = radius_unit * radius_factor8;
    y -= dy - 1;
    draw_earthquake_mark (dpy, radius, x - radius, y, fill_color);

    x += radius_unit * radius_factor8 / 2;
    y += radius_unit * (radius_factor8 - radius_factor7) * 2;
    radius = radius_unit * radius_factor7;
    fill_color = past_time_color[past_time_class2];
    draw_earthquake_mark (dpy, radius, x - radius, y, fill_color);

    x += radius_unit * radius_factor8 / 2;
    y += radius_unit * (radius_factor7 - radius_factor6) * 2;
    radius = radius_unit * radius_factor6;
    fill_color = past_time_color[past_time_class3];
    draw_earthquake_mark (dpy, radius, x - radius, y, fill_color);

    x += radius_unit * radius_factor8 / 2;
    y += radius_unit * (radius_factor6 - radius_factor5) * 2;
    radius = radius_unit * radius_factor5;
    fill_color = past_time_color[past_time_class4];
    draw_earthquake_mark (dpy, radius, x - radius, y, fill_color);

    x += radius_unit * radius_factor8 / 2;
    y += radius_unit * (radius_factor5 - radius_factor4) * 2;
    radius = radius_unit * radius_factor4;
    fill_color = past_time_color[past_time_class5];
    draw_earthquake_mark (dpy, radius, x - radius, y, fill_color);

    x += radius_unit * radius_factor8 / 2;
    y += radius_unit * (radius_factor4 - radius_factor3) * 2;
    radius = radius_unit * radius_factor3;
    fill_color = past_time_color[past_time_class5];
    draw_earthquake_mark (dpy, radius, x - radius, y, fill_color);

    x += radius_unit * radius_factor8 / 2;
    y += radius_unit * (radius_factor3 - radius_factor2) * 2;
    radius = radius_unit * radius_factor2;
    fill_color = past_time_color[past_time_class5];
    draw_earthquake_mark (dpy, radius, x - radius, y, fill_color);

    x += radius_unit * radius_factor8 / 2;
    y += radius_unit * (radius_factor2 - radius_factor1) * 2;
    radius = radius_unit * radius_factor1;
    fill_color = past_time_color[past_time_class5];
    draw_earthquake_mark (dpy, radius, x - radius, y, fill_color);

    y += radius + 1 + dy;
    strftime (buf, sizeof (buf), "quake %Y-%m-%d %H:%M %Z",
              localtime (&cur_earth_dat_file_mtime));
    len = strlen (buf);
    XTextExtents (font, buf, len, &direction, &ascent, &descent, &extents);
    if (label_orient & LABEL_LEFT_FLUSH)
        x = label_xvalue - extents.lbearing;
    else
        x = (wdth + label_xvalue) - extents.rbearing;
    draw_outlined_string (dpy, work_pix, white, black, x, y, buf, len);

}                       // end draw_earthquake_legend
#endif
