

/* framebuffer.c      Grafik-Routinen fuer Framebuffer-Systeme (c) Markus Hoffmann    */


/* This file is part of X11BASIC, the basic interpreter for Unix/X
 * ======================================================================
 * X11BASIC is free software and comes with NO WARRANTY - read the file
 * COPYING for details
 */



#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <linux/ioctl.h>
#include "framebuffer.h"



  #undef BLACK
  #undef WHITE
  #undef RED
  #undef GREEN
  #undef BLUE     
  #undef YELLOW   
  #undef MAGENTA  
  
/* Some colors (16 Bit color screen assumed) */

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


void FillBox (int x, int y, int w, int h, unsigned short color);

void FbRender_Clear(int,int, unsigned short);
extern struct fb_var_screeninfo vinfo;
extern struct fb_fix_screeninfo finfo;
extern long int screensize;

int fbfd = -1;
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
long int screensize = 0;
char *fbp = 0;
/* char *fbbackp = 0; */

void FbRender_Open() {
  fbfd=open("/dev/fb", O_RDWR);
  if (!fbfd) {
    printf("ERROR: could not open framebufferdevice.\n");
    exit(1);
  }
#if DEBUG
  printf("Framebuffer device now opened.\n");
#endif
  // Get fixed screen information
  if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
    printf("Fout bij het lezen van de vaste informatie.\n");
    exit(2);
  }
  // Get variable screen information
  if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
    printf("Fout bij het lezen van de variabele informatie.\n");
    exit(3);
  }
#if DEBUG
  printf("%dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel );
#endif

  // Figure out the size of the screen in bytes
  screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

  // Map the device to memory
  fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
  if((int)fbp==-1) {
    printf("Fout: gefaald bij het mappen van de framebuffer device in het geheugen.\n");
    exit(4);
  }
 /* fbbackp = (char*)malloc(screensize); */
 /* fb_var.yoffset=fb_var.yres; 
  vinfo.yoffset=0;
  ioctl(fb,FBIOPAN_DISPLAY,&vinfo))*/
}

void FbRender_Close() {
  if(fbfd > 0) {
    munmap(fbp, screensize);
    close(fbfd);
  }
 /* if(fbbackp) free(fbbackp); */
  fbfd = -1;
}

/* This is a low-level Function, need to be fast, but does noch check 
   clipping */

inline void FB_PutPixel(int x, int y, unsigned short color) {
  unsigned short *ptr  = (unsigned short*)(fbp+x*2+y*Scanline);
  *ptr = color;
}
inline void Fb_Scroll(int target_y, int source_y, int height) {
  memmove(fbp+target_y*Scanline,fbp+source_y*Scanline,height*Scanline);
}


int global_color=YELLOW;
int global_bcolor=BLACK;

void FB_set_color(unsigned short color) {
  global_color=color;
}
void FB_set_bcolor(unsigned short color) {
  global_bcolor=color;
}

void FB_plot(int x, int y) {
  if(x<0 || y<0 || x>=ScreenWidth || y>=ScreenHeight) return;
  FB_PutPixel(x,y,global_color);
}
unsigned short FB_point(int x, int y) {
  if(x<0 || y<0 || x>=ScreenWidth || y>=ScreenHeight) return;
  unsigned short *ptr  = (unsigned short*)(fbp+x*2+y*Scanline);
  return(*ptr);
}
void DrawHorizontalLine(int X, int Y, int width, unsigned short color) {
  register int w = width;  // in pixels
#define iClipTop 0
#define iClipBottom ScreenHeight
#define iClipMin 0
#define iClipMax ScreenWidth

  if (Y<iClipTop) return;
  if (Y>=iClipBottom) return;

  if (iClipMin-X>0)      // clip left margin
    { w-=(iClipMin-X); X=iClipMin; }
  if (w>iClipMax-X)    // clip right margin
    w=iClipMax-X;

  // put the pixels...  
  {
    while ( w-- > 0 )
      FB_PutPixel(X++,Y,color);
  }
}
void fillLine(int x1,int x2,int y,unsigned short color) {
 if(x2>=x1) DrawHorizontalLine(x1,y,x2-x1,color);
 else DrawHorizontalLine(x1,y,x1-x2,color);
}

/* Bresenham's line drawing algorithm */

void FB_DrawLine(int x0, int y0, int x1, int y1,unsigned short color) {
  int dy = y1 - y0;
  int dx = x1 - x0;
  int stepx, stepy;

  if (dy < 0) { dy = -dy;  stepy = -1; } else { stepy = 1; }
  if (dx < 0) { dx = -dx;  stepx = -1; } else { stepx = 1; }
  dy <<= 1;						     // dy is now 2*dy
  dx <<= 1;						     // dx is now 2*dx

  FB_PutPixel(x0, y0,color);
  if (dx > dy) {
      int fraction = dy - (dx >> 1);			     // same as 2*dy - dx
      while (x0 != x1) {
	  if (fraction >= 0) {
	      y0 += stepy;
	      fraction -= dx;				     // same as fraction -= 2*dx
	  }
	  x0 += stepx;
	  fraction += dy;				     // same as fraction -= 2*dy
	  FB_PutPixel(x0, y0,color);
      }
  } else {
      int fraction = dx - (dy >> 1);
      while (y0 != y1) {
	  if (fraction >= 0) {
	      x0 += stepx;
	      fraction -= dy;
	  }
	  y0 += stepy;
	  fraction += dx;
	  FB_PutPixel(x0, y0,color);
    }
  }
}
void FB_line(int x1,int y1,int x2,int y2) {
  if(y1==y2) {
    if(x2>=x1) DrawHorizontalLine(x1,y1,x2-x1,global_color);
    else DrawHorizontalLine(x2,y1,x1-x2,global_color);
  } else FB_DrawLine(x1,y1,x2,y2,global_color);
}



void FB_box(int x1,int y1,int x2,int y2) {
  DrawHorizontalLine(x1,y1,x2-x1,global_color);
  DrawHorizontalLine(x1,y2,x2-x1,global_color);
  FB_DrawLine(x1,y1,x1,y2,global_color);
  FB_DrawLine(x2,y1,x2,y2,global_color);
}


void FillBox (int x, int y, int w, int h, unsigned short color) {
  int i;
  for (i=y; i<=y+h; i++) {
    DrawHorizontalLine(x, i, w, color);
  }  
}
void FB_pbox(int x1, int y1, int x2, int y2) {
  FillBox(x1,y1,x2-x1,y2-y1,global_color);
}





void Fb_inverse(int x, int y,int w,int h){

  if(x<0||y<0||w<=0||h<=0|| x+w>ScreenWidth|| y+h>ScreenHeight) return;

  register unsigned short *ptr  = (unsigned short*)(fbp+y*Scanline);
  ptr+=x;
  register unsigned short *endp  = ptr+h*ScreenWidth;
  register int i;
  while (ptr<endp) {
    for(i=0;i<w;i++) *ptr++=~*ptr;
    ptr+=ScreenWidth-w;
  }
}
unsigned short int mean_color(unsigned short int a, unsigned short int b) {
  return(
  (((((a>>11)&0x1f)+((b>>11)&0x1f))>>2)<<11)|
  (((((a>>5)&0x3f)+((b>>5)&0x3f))>>2)<<5)|
  (((a&0x1f)+(b&0x1f))>>2));
}

void FB_copyarea(int x,int y,int w, int h, int tx,int ty) {
  if(x<0||y<0||w<=0||h<=0|| x+w>ScreenWidth|| y+h>ScreenHeight||
  tx<0||ty<0||tx+w>ScreenWidth|| ty+h>ScreenHeight) return;
  register unsigned short *ptr1  = (unsigned short*)(fbp+y*Scanline);
  register unsigned short *ptr2  = (unsigned short*)(fbp+ty*Scanline);
  ptr1+=x;
  ptr2+=tx;

  register int i;
if(y>ty) {
  for(i=0;i<h;i++) {
     memmove(ptr2,ptr1,w*sizeof(short));
     ptr1+=ScreenWidth;
     ptr2+=ScreenWidth;
  }

} else {
  for(i=h-1;i>=0;i--) {
     memmove(&ptr2[i*ScreenWidth],&ptr1[i*ScreenWidth],w*sizeof(short));
  }
}
}
int FB_get_color(int r, int g, int b) {
  return((((r>>11)&0x1f)<<11)|(((g>>10)&0x3f)<<5)|(((b>>11)&0x1f)));
}
/* Character set  (it is a 5x7 Pixel font)*/
  #define CharWidth    5
  #define CharHeight    (7+1)

#define FL_NORMAL        0
#define FL_BOLD          2
#define FL_DIM           4
#define FL_UNDERLINE  0x10
#define FL_BLINK      0x20
#define FL_REVERSE    0x80
#define FL_HIDDEN    0x100


const unsigned char asciiTable[]={
	0x00,0x00,0x00,0x00,0x00,//0x00,
	0x00,0x00,0x5F,0x00,0x00,//0x00,
	0x00,0x07,0x00,0x07,0x00,//0x00,
	0x14,0x7F,0x14,0x7F,0x14,//0x00,
	0x24,0x2A,0x7F,0x2A,0x12,//0x00,
	0x23,0x13,0x08,0x64,0x62,//0x00,
	0x36,0x49,0x55,0x22,0x50,//0x00,
	0x00,0x05,0x03,0x00,0x00,//0x00,
	0x00,0x1C,0x22,0x41,0x00,//0x00,
	0x00,0x41,0x22,0x1C,0x00,//0x00,
	0x14,0x08,0x3E,0x08,0x14,//0x00,
	0x08,0x08,0x3E,0x08,0x08,//0x00,
	0x00,0x50,0x30,0x00,0x00,//0x00,
	0x08,0x08,0x08,0x08,0x08,//0x00,
	0x00,0x60,0x60,0x00,0x00,//0x00,
	0x20,0x10,0x08,0x04,0x02,//0x00,
	0x3E,0x51,0x49,0x45,0x3E,//0x00,
	0x00,0x42,0x7F,0x40,0x00,//0x00,
	0x42,0x61,0x51,0x49,0x46,//0x00,
	0x21,0x41,0x45,0x4B,0x31,//0x00,
	0x18,0x14,0x12,0x7F,0x10,//0x00,
	0x27,0x45,0x45,0x45,0x39,//0x00,
	0x3C,0x4A,0x49,0x49,0x30,//0x00,
	0x01,0x71,0x09,0x05,0x03,//0x00,
	0x36,0x49,0x49,0x49,0x36,//0x00,
	0x06,0x49,0x49,0x29,0x1E,//0x00,
	0x00,0x36,0x36,0x00,0x00,//0x00,
	0x00,0x56,0x36,0x00,0x00,//0x00,
	0x08,0x14,0x22,0x41,0x00,//0x00,
	0x14,0x14,0x14,0x14,0x14,//0x00,
	0x00,0x41,0x22,0x14,0x08,//0x00,
	0x02,0x01,0x51,0x09,0x06,//0x00,
	0x32,0x49,0x79,0x41,0x3E,//0x00,
	0x7E,0x11,0x11,0x11,0x7E,//0x00,
	0x7F,0x49,0x49,0x49,0x36,//0x00,
	0x3E,0x41,0x41,0x41,0x22,//0x00,
	0x7F,0x41,0x41,0x22,0x1C,//0x00,
	0x7F,0x49,0x49,0x49,0x41,//0x00,
	0x7F,0x09,0x09,0x09,0x01,//0x00,
	0x3E,0x41,0x49,0x49,0x7A,//0x00,
	0x7F,0x08,0x08,0x08,0x7F,//0x00,
	0x00,0x41,0x7F,0x41,0x00,//0x00,
	0x20,0x40,0x41,0x3F,0x01,//0x00,
	0x7F,0x08,0x14,0x22,0x41,//0x00,
	0x7F,0x40,0x40,0x40,0x40,//0x00,
	0x7F,0x02,0x0C,0x02,0x7F,//0x00,
	0x7F,0x04,0x08,0x10,0x7F,//0x00,
	0x3E,0x41,0x41,0x41,0x3E,//0x00,
	0x7F,0x09,0x09,0x09,0x06,//0x00,
	0x3E,0x41,0x51,0x21,0x5E,//0x00,
	0x7F,0x09,0x19,0x29,0x46,//0x00,
	0x46,0x49,0x49,0x49,0x31,//0x00,
	0x01,0x01,0x7F,0x01,0x01,//0x00,
	0x3F,0x40,0x40,0x40,0x3F,//0x00,
	0x1F,0x20,0x40,0x20,0x1F,//0x00,
	0x3F,0x40,0x38,0x40,0x3F,//0x00,
	0x63,0x14,0x08,0x14,0x63,//0x00,
	0x07,0x08,0x70,0x08,0x07,//0x00,
	0x61,0x51,0x49,0x45,0x43,//0x00,
	0x00,0x7F,0x41,0x41,0x00,//0x00,
	0x02,0x04,0x08,0x10,0x20,//0x00,
	0x00,0x41,0x41,0x7F,0x00,//0x00,
	0x04,0x02,0x01,0x02,0x04,//0x00,
	0x40,0x40,0x40,0x40,0x40,//0x40,
	0x00,0x01,0x02,0x04,0x00,//0x00,
	0x20,0x54,0x54,0x54,0x78,//0x00,
	0x7F,0x48,0x44,0x44,0x38,//0x00,
	0x38,0x44,0x44,0x44,0x20,//0x00,
	0x38,0x44,0x44,0x48,0x7F,//0x00,
	0x38,0x54,0x54,0x54,0x18,//0x00,
	0x08,0x7E,0x09,0x01,0x02,//0x00,
	0x0C,0x52,0x52,0x52,0x3E,//0x00,
	0x7F,0x08,0x04,0x04,0x78,//0x00,
	0x00,0x44,0x7D,0x40,0x00,//0x00,
	0x20,0x40,0x44,0x3D,0x00,//0x00,
	0x7F,0x10,0x28,0x44,0x00,//0x00,
	0x00,0x41,0x7F,0x40,0x00,//0x00,
	0x7C,0x04,0x18,0x04,0x78,//0x00,
	0x7C,0x08,0x04,0x04,0x78,//0x00,
	0x38,0x44,0x44,0x44,0x38,//0x00,
	0x7C,0x14,0x14,0x14,0x08,//0x00,
	0x08,0x14,0x14,0x18,0x7C,//0x00,
	0x7C,0x08,0x04,0x04,0x08,//0x00,
	0x48,0x54,0x54,0x54,0x20,//0x00,
	0x04,0x3F,0x44,0x40,0x20,//0x00,
	0x3C,0x40,0x40,0x20,0x7C,//0x00,
	0x1C,0x20,0x40,0x20,0x1C,//0x00,
	0x3C,0x40,0x30,0x40,0x3C,//0x00,
	0x44,0x28,0x10,0x28,0x44,//0x00,
	0x0C,0x50,0x50,0x50,0x3C,//0x00,
	0x44,0x64,0x54,0x4C,0x44,//0x00,
	0x00,0x08,0x36,0x41,0x00,//0x00,
	0x00,0x00,0x7F,0x00,0x00,//0x00,
	0x00,0x41,0x36,0x08,0x00,//0x00,
	0x10,0x08,0x08,0x10,0x08,//0x00,
	0x78,0x46,0x41,0x46,0x78,//0x00
};

void Fb_BlitCharacter(int x, int y, unsigned short aColor, unsigned short aBackColor, char character, int flags){
  unsigned char data0,data1,data2,data3,data4;

  if (x<0||y<0|| x>ScreenWidth-CharWidth || y>ScreenHeight-CharHeight) return;
  if ((character < 32) || (character > 126)) character = '.';

  const unsigned char *aptr = &asciiTable[(character-32)*5];
  data0 = *aptr++;
  data1 = *aptr++;
  data2 = *aptr++;
  data3 = *aptr++;
  data4 = *aptr;

  register unsigned short *ptr  = (unsigned short*)(fbp+y*Scanline);
  ptr+=x;
  register unsigned short *endp  = ptr+CharHeight*ScreenWidth;

  if(flags&FL_REVERSE) { /* reverse */
#if 1
    unsigned short t=aBackColor;
    aBackColor=aColor;
    aColor=t;
#else
   int i;
   for(i=0;i<16;i++) {
    if(flags&(1<<i))  FillCircle (10+i*10, 150, 5, 0xf000);
    else  FillCircle (10+i*10, 150, 5, 0x0700);
   }
#endif
  }
#if 0
  if(flags&FL_BOLD) {/* bold */
    data0|=data1;
    data1|=data2;
    data2|=data3;
    data3|=data4;
  }
#endif  
  if(flags&FL_UNDERLINE) {/* underline */
    data0|=0x80;
    data1|=0x80;
    data2|=0x80;
    data3|=0x80;
    data4|=0x80;
  }
#if 0  
  
  while (ptr<endp) {
    if (data0 & 1) *ptr++ = aColor; else *ptr++ = aBackColor; data0 >>= 1;    
    if (data1 & 1) *ptr++ = aColor; else *ptr++ = aBackColor; data1 >>= 1;
    if (data2 & 1) *ptr++ = aColor; else *ptr++ = aBackColor; data2 >>= 1;
    if (data3 & 1) *ptr++ = aColor; else *ptr++ = aBackColor; data3 >>= 1;
    if (data4 & 1) *ptr++ = aColor; else *ptr++ = aBackColor; data4 >>= 1;
    ptr+=ScreenWidth-CharWidth;
  }
#else

  while (ptr<endp) {
    if (data0 & 1) *ptr++ = aColor; else ptr++; data0 >>= 1;	
    if (data1 & 1) *ptr++ = aColor; else ptr++; data1 >>= 1;
    if (data2 & 1) *ptr++ = aColor; else ptr++; data2 >>= 1;
    if (data3 & 1) *ptr++ = aColor; else ptr++; data3 >>= 1;
    if (data4 & 1) *ptr++ = aColor; else ptr++; data4 >>= 1;
    ptr+=ScreenWidth-CharWidth;
  }
#endif
  
}

void Fb_BlitText(int x, int y, unsigned short aColor, unsigned short aBackColor, char *str) {
  while (*str) {
    Fb_BlitCharacter(x, y, aColor, aBackColor, *str,0);
    x+=CharWidth;
    str++;
  }
}

void FB_mtext(int x, int y, char *t) {
  Fb_BlitText(x,y,global_color, global_bcolor,t);
}
void FB_DrawString(int x, int y, char *t,int len) {
  char buf[len+1];
  memcpy(buf,t,len);
  buf[len]=0;
  Fb_BlitText(x,y,global_color, global_bcolor,buf);
}



void FB_get_geometry(int *x, int *y, int *w, int *h, int *b, int *d) {
  *b=*x=*y=0;
  *w=ScreenWidth;
  *h=ScreenHeight;
  *d=vinfo.bits_per_pixel;
}



void DrawCircle(int x, int y, int r, unsigned short color) {
  int i, row, col, px, py;
  long int sum;
  x+=r;
  y+=r;

  py = r<<1;
  px = 0;
  sum = 0;
  while (px <= py) {
    if (!(px & 1)) {
      col = x + (px>>1);
      row = y + (py>>1);FB_PutPixel(col,row,color);
      row = y - (py>>1);FB_PutPixel(col,row,color);
      col = x - (px>>1);FB_PutPixel(col,row,color);
      row = y + (py>>1);FB_PutPixel(col,row,color);
      col = x + (py>>1);
      row = y + (px>>1);FB_PutPixel(col,row,color);
      row = y - (px>>1);FB_PutPixel(col,row,color);
      col = x - (py>>1);FB_PutPixel(col,row,color);
      row = y + (px>>1);FB_PutPixel(col,row,color);
    }
    sum+=px++;
    sum += px;
    if (sum >= 0) {
	sum-=py--;
	sum-=py;
    }
  }
}
void FB_Arc(int x, int y, int r1, int r2, int a1, int a2) {
  if(r1>1) DrawCircle(x,y,r1/2,global_color);
  else FB_plot(x, y);
}

void FillCircle(int x, int y, int r, unsigned short color) {
  int i, row, col_start, col_end, t_row, t_col, px, py;
  long int sum;
  x+=r;
  y+=r;
  
  py = r<<1;
  px = 0;
  sum = 0;
  while (px <= py) {
    col_start=x-(px>>1);
    col_end=x+(px>>1);
    row =y+(py>>1);
    fillLine(col_start, col_end, row, color);
    col_start =x - (py>>1);
    col_end =x + (py>>1);
    row =y+(px>>1);
    fillLine(col_start, col_end, row, color);

    col_start = x - (px>>1);
    col_end = x + (px>>1);
    row =y - (py>>1);
    fillLine(col_start, col_end, row, color);

    col_start = x - (py>>1);
    col_end = x + (py>>1);
    row =y - (px>>1);
    fillLine(col_start, col_end, row, color);

    sum +=px++;
    sum += px;
    if (sum >= 0) {
       sum-=py--;
       sum -=py;
    }
  }
}
void FB_pArc(int x, int y, int r1, int r2, int a1, int a2) {
  if(r1>1) FillCircle(x,y,r1/2,global_color);
  else FB_plot(x, y);
}

/*
  ���������������������������������������������������������ͻ
  �                                                         �
  �                                                         �
  � fil2Poly() = fills a polygon in specified color by      �
  �              filling in boundaries resulting from       �
  �              connecting specified points in the         �
  �              order given and then connecting last       �
  �              point to first.  Uses an array to          �
  �              store coordinates.                         �
  �                                                         �
  ���������������������������������������������������������ͼ
*/
int sort_function (const long int *a, const long int *b)
{
	if (*a < *b)
	return(-1);
	if (*a == *b)
	return(0);
	if (*a > *b)
	return(1);
}


void fill2Poly(unsigned short color,int *point, int num) {
  #define sign(x) ((x) > 0 ? 1:  ((x) == 0 ? 0:  (-1)))

  int dx, dy, dxabs, dyabs, i=0, index=0, j, k, px, py, sdx, sdy, x, y,
      xs,xe,ys,ye,toggle=0, old_sdy, sy0;
  long int coord[4000];
  if (num<3) return;
  i=2*num+1;
  px = point[0];
  py = point[1];
  if (point[1] == point[3]) {
    coord[index++] = px | (long)py << 16;
  }
  for (j=0; j<=i-3; j+=2) {
		xs = point[j];
		ys = point[j+1];
		if ((j == (i-3)) || (j == (i-4)))
		{
			xe = point[0];
			ye = point[1];
		}
		else
		{
			xe = point[j+2];
			ye = point[j+3];
		}
    dx = xe - xs;
    dy = ye - ys;
    sdx = sign(dx);
    sdy = sign(dy);
    if (j==0) {
      old_sdy = sdy;
      sy0 = sdy;
    }
    dxabs = abs(dx);
    dyabs = abs(dy);
    x = 0;
    y = 0;
    if (dxabs >= dyabs) {
      for (k=0; k<dxabs; k++) {
	y += dyabs;
	if (y>=dxabs) {
	  y -= dxabs;
	  py += sdy;
	  if (old_sdy != sdy) {
	    old_sdy = sdy;
	    index--;
	  }
	  coord[index++] = (px + sdx) | (long)py << 16;
	}
	px += sdx;
	FB_PutPixel(px,py,color);
      }
    } else {
      for (k=0; k<dyabs; k++) {
	x += dxabs;
	if (x>=dyabs) {
	  x -= dyabs;
	  px += sdx;
	}
	py += sdy;
	if (old_sdy != sdy) {
	  old_sdy = sdy;
	  if (sdy != 0) index--;
        }
	FB_PutPixel(px,py,color);
	coord[index] = px | (long)py << 16;
	index++;
      }
    }
  }
  if(sy0 + sdy == 0) index--;
  qsort(coord,index,sizeof(coord[0]),(int(*)
		(const void *, const void *))sort_function);
  for (i=0; i<index; i++) {
    xs = min(ScreenWidth-1,(max(0,(int)((signed short)(coord[i])))));
    xe = min(ScreenWidth-1,(max(0,(int)((signed short)(coord[i + 1])))));
    ys = min(ScreenHeight-1,(max(0,(int)((signed short)(coord[i] >> 16)))));
    ye = min(ScreenHeight-1,(max(0,(int)((signed short)(coord[i + 1] >> 16)))));
    if ((ys == ye) && (toggle == 0)) {
      fillLine(xs, xe, ys, color);
      toggle = 1;
    } else toggle = 0;
  }
}




