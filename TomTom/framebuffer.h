
/* framebuffer.h      Framebuffer (c) Markus Hoffmann    */


/* This file is part of X11BASIC, the basic interpreter for Unix/X
 * ======================================================================
 * X11BASIC is free software and comes with NO WARRANTY - read the file
 * COPYING for details
 */
#define FB_DEVICE_NAME "/dev/ts"
#ifndef FALSE
  #define FALSE    0
  #define TRUE     (!FALSE)
#endif
#define FRAMEBUFFER
#define PI       3.141592653589793
#define E        2.718281828459
#define round(a) ((int)(a+0.5))
#define min(a,b) ((a<b)?a:b)
#define max(a,b) ((a>b)?a:b)
#define sgn(x)   ((x<0)?-1:1)
#define rad(x)   (PI*x/180)
#define deg(x)   (180*x/PI)

#ifdef FRAMEBUFFER
  #define XDrawArc(a,b,c,d,e,f,g,h,i) FB_Arc(d,e,f,g,h,i)
  #define XFillArc(a,b,c,d,e,f,g,h,i) FB_pArc(d,e,f,g,h,i)
  #define XDrawPoints(a,b,c,d,e,f)    FB_points(d,e,f)
  #define XDrawLines(a,b,c,d,e,f)     FB_lines(d,e,f)
  #define XFillPolygon(a,b,c,d,e,f,g) FB_pPolygon(d,e,f,g)
  #define XSetLineAttributes(a,b,c,d,e,f) ; 
  #define XWindowEvent(a,b,c,d) FB_event(c,d)
  #define XCheckWindowEvent(a,b,c,d) FB_check_event(c,d)
  #define XNextEvent(a,b)            FB_next_event(b)
  
  #define XQueryPointer(a,b,c,d,e,f,g,h,i) FB_Query_pointer(e,f,g,h,i)
  #define XUnmapWindow(a,b) ;
  #define XMapWindow(a,b) ;
  #define XrmGetResource(a,b,c,d,e) 0
  #define XSetForeground(a,b,c) FB_set_color(c)
  #define XSetBackground(a,b,c) FB_set_bcolor(c)
  #define XDrawString(a,b,c,d,e,f,g) FB_DrawString(d,e,f,g) 
  #define CharWidth    5
  #define CharHeight    (7+1)
#define ScreenWidth  (vinfo.xres)
#define ScreenHeight (vinfo.yres)
#define Scanline (vinfo.xres*vinfo.bits_per_pixel/8)

  typedef struct {
    int x;
    int y;
  } XPoint;
  typedef struct {
    int type;
    struct {
      int x;
      int y;
      int button;
      int x_root;
      int y_root;
      int state;
    } xbutton;
    struct {
      int x;
      int y;
      int x_root;
      int y_root;
      int state;
    } xmotion;
    struct {
      int x;
      int y;
      int x_root;
      int y_root;
      int state;
      int keycode;
      int ks;
      char buf[4];
    } xkey;
  } XEvent;
  #define ExposureMask 3
  #define NoExpose 1
  #define Expose 2
  #define GraphicsExpose 3

  #define ButtonPressMask   0x08
  #define ButtonReleaseMask   0x04
  #define ButtonPress 8
  #define ButtonRelease 4

  #define PointerMotionMask 0x30
  #define MotionNotify      0x10

  #define KeyPressMask      0x80
  #define KeyPress          0x80
  #define KeyRelease        0x40
  #define KeyReleaseMask    0x40

  #define CirculateRequest 0
  #define ConfigureRequest 0
  #define ResizeRequest    0

  extern int global_mousex,global_mousey,global_mousek,global_mouses;
#endif

void FbRender_Open();
void FbRender_Close();
void FbRender_Flush();

char *FB_get_image(int x, int y, int w,int h, int *len);
void FB_put_image(char *,int, int);
void FB_get_geometry(int *x, int *y, int *w, int *h, int *b, int *d);
void FB_mtext(int x, int y, char *t);

void FB_DrawString(int x, int y, char *t,int len);
void FB_DrawLine(int x0, int y0, int x1, int y1,unsigned short color);
