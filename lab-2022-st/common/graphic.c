#include "common.h"
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>

static int LCD_FB_FD;
static int *LCD_FB_BUF = NULL;
static int DRAW_BUF[SCREEN_WIDTH*SCREEN_HEIGHT];

static struct area {
	int x1, x2, y1, y2;
} update_area = {0,0,0,0};

#define AREA_SET_EMPTY(pa) do {\
	(pa)->x1 = SCREEN_WIDTH;\
	(pa)->x2 = 0;\
	(pa)->y1 = SCREEN_HEIGHT;\
	(pa)->y2 = 0;\
} while(0)

void fb_init(char *dev)
{
	int fd;
	struct fb_fix_screeninfo fb_fix;
	struct fb_var_screeninfo fb_var;

	if(LCD_FB_BUF != NULL) return; /*already done*/

	//进入终端图形模式
	fd = open("/dev/tty0",O_RDWR,0);
	ioctl(fd, KDSETMODE, KD_GRAPHICS);
	close(fd);

	//First: Open the device
	if((fd = open(dev, O_RDWR)) < 0){
		printf("Unable to open framebuffer %s, errno = %d\n", dev, errno);
		return;
	}
	if(ioctl(fd, FBIOGET_FSCREENINFO, &fb_fix) < 0){
		printf("Unable to FBIOGET_FSCREENINFO %s\n", dev);
		return;
	}
	if(ioctl(fd, FBIOGET_VSCREENINFO, &fb_var) < 0){
		printf("Unable to FBIOGET_VSCREENINFO %s\n", dev);
		return;
	}

	printf("framebuffer info: bits_per_pixel=%u,size=(%d,%d),virtual_pos_size=(%d,%d)(%d,%d),line_length=%u,smem_len=%u\n",
		fb_var.bits_per_pixel, fb_var.xres, fb_var.yres, fb_var.xoffset, fb_var.yoffset,
		fb_var.xres_virtual, fb_var.yres_virtual, fb_fix.line_length, fb_fix.smem_len);

	//Second: mmap
	void *addr = mmap(NULL, fb_fix.smem_len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if(addr == (void *)-1){
		printf("failed to mmap memory for framebuffer.\n");
		return;
	}

	if((fb_var.xoffset != 0) ||(fb_var.yoffset != 0))
	{
		fb_var.xoffset = 0;
		fb_var.yoffset = 0;
		if(ioctl(fd, FBIOPAN_DISPLAY, &fb_var) < 0) {
			printf("FBIOPAN_DISPLAY framebuffer failed\n");
		}
	}

	LCD_FB_FD = fd;
	LCD_FB_BUF = addr;

	//set empty
	AREA_SET_EMPTY(&update_area);
	return;
}

static void _copy_area(int *dst, int *src, struct area *pa)
{
	int x, y, w, h;
	x = pa->x1; w = pa->x2-x;
	y = pa->y1; h = pa->y2-y;
	src += y*SCREEN_WIDTH + x;
	dst += y*SCREEN_WIDTH + x;
	while(h-- > 0){
		memcpy(dst, src, w*4);
		src += SCREEN_WIDTH;
		dst += SCREEN_WIDTH;
	}
}

static int _check_area(struct area *pa)
{
	if(pa->x2 == 0) return 0; //is empty

	if(pa->x1 < 0) pa->x1 = 0;
	if(pa->x2 > SCREEN_WIDTH) pa->x2 = SCREEN_WIDTH;
	if(pa->y1 < 0) pa->y1 = 0;
	if(pa->y2 > SCREEN_HEIGHT) pa->y2 = SCREEN_HEIGHT;

	if((pa->x2 > pa->x1) && (pa->y2 > pa->y1))
		return 1; //no empty

	//set empty
	AREA_SET_EMPTY(pa);
	return 0;
}

void fb_update(void)
{
	if(_check_area(&update_area) == 0) return; //is empty
	_copy_area(LCD_FB_BUF, DRAW_BUF, &update_area);
	AREA_SET_EMPTY(&update_area); //set empty
	return;
}

/*======================================================================*/

void * _begin_draw(int x, int y, int w, int h)
{
	int x2 = x+w;
	int y2 = y+h;
	if(update_area.x1 > x) update_area.x1 = x;
	if(update_area.y1 > y) update_area.y1 = y;
	if(update_area.x2 < x2) update_area.x2 = x2;
	if(update_area.y2 < y2) update_area.y2 = y2;
	return DRAW_BUF;
}

inline void fb_draw_pixel(int x, int y, int color)
{
	if(x<0 || y<0 || x>=SCREEN_WIDTH || y>=SCREEN_HEIGHT) return;
	int *buf = _begin_draw(x,y,1,1);
/*---------------------------------------------------*/
	*(buf + y*SCREEN_WIDTH + x) = color;
/*---------------------------------------------------*/
	return;
}
// #define FB_DRAW_PIEXL(buf,x,y,color) (*(buf + y*SCREEN_WIDTH + x) = color)
void fb_draw_rect(int x, int y, int w, int h, int color)
{
	if(x < 0) { w += x; x = 0;}
	if(x+w > SCREEN_WIDTH) { w = SCREEN_WIDTH-x;}
	if(y < 0) { h += y; y = 0;}
	if(y+h >SCREEN_HEIGHT) { h = SCREEN_HEIGHT-y;}
	if(w<=0 || h<=0) return;
	int *buf = _begin_draw(x,y,w,h);
/*---------------------------------------------------*/
	// printf("you need implement fb_draw_rect()\n"); exit(0);
    for(int i=x;i<x+w;i++){
        for(int j=y;j<y+h;j++){
            FB_DRAW_PIEXL(buf,i,j,color);
        }
    }

/*---------------------------------------------------*/
	return;
}

void fb_draw_line(int x1, int y1, int x2, int y2, int color)
{
/*---------------------------------------------------*/
	// printf("you need implement fb_draw_line()\n"); exit(0);
    int *buf = _begin_draw(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    int dx= (x1>x2)?-1:1;
    int dy= (y1>y2)?-1:1;
    if(x2==x1){
        for(int y=y1;y!=(y2+dy);y+=dy){
            FB_DRAW_PIEXL(buf,x1,y,color);
        }
    }else{
        double gradient= 1.0* (y2-y1)/ (x2-x1);
        if(abs(gradient)<=0.5){
            for(int x=x1;x!=(x2+dx);x+=dx){
                int y= (x-x1)*(y2-y1)/(x2-x1)+y1;
                FB_DRAW_PIEXL(buf,x,y,color);
            }
        }else{
            for(int y=y1;y!=(y2+dy);y+=dy){
                int x= (y-y1)*(x2-x1)/(y2-y1)+x1;
                FB_DRAW_PIEXL(buf,x,y,color);
            }
        }
    }    
/*---------------------------------------------------*/
	return;
}

void fb_draw_image(int x, int y, fb_image *image, int color)
{
	if(image == NULL) return;

	int ix = 0; //image x
	int iy = 0; //image y
	int w = image->pixel_w; //draw width
	int h = image->pixel_h; //draw height

	if(x<0) {w+=x; ix-=x; x=0;}
	if(y<0) {h+=y; iy-=y; y=0;}
	
	if(x+w > SCREEN_WIDTH) {
		w = SCREEN_WIDTH - x;
	}
	if(y+h > SCREEN_HEIGHT) {
		h = SCREEN_HEIGHT - y;
	}
	if((w <= 0)||(h <= 0)) return;

	int *buf = _begin_draw(x,y,w,h);
/*---------------------------------------------------------------*/
	char *dst = (char *)(buf + y*SCREEN_WIDTH + x);
	char *src; //不同的图像颜色格式定位不同
/*---------------------------------------------------------------*/

	int alpha;
	// int ww;

	if(image->color_type == FB_COLOR_RGB_8880) /*lab3: jpg*/
	{
        src=image->content;
        for(int j=iy;j<iy+h;j++){
            memcpy(dst,src,w*4);
            dst+=SCREEN_WIDTH*4;
            src+=image->line_byte;
        }
		return;
	}
	else if(image->color_type == FB_COLOR_RGBA_8888) /*lab3: png*/
	{
        src=image->content;
        for(int j=iy;j<iy+h;j++){
            for(int i=ix*4;i<w*4;i+=4){
                alpha=(unsigned char)src[i+3];
                if(alpha==0)continue;
                else if(alpha==255){
                    dst[i+2]=src[i+2];
                    dst[i+1]=src[i+1];
                    dst[i]=src[i];
                }else {
                    dst[i+2] += (((src[i+2]-dst[i+2])*alpha)>>8);
                    dst[i+1] += (((src[i+1]-dst[i+1])*alpha)>>8);
                    dst[i] += (((src[i]-dst[i])*alpha)>>8);
                }
            }
            dst+=SCREEN_WIDTH*4;
            src+=image->line_byte;
        }
		return;
	}
	else if(image->color_type == FB_COLOR_ALPHA_8) /*lab3: font*/
	{   
        src=image->content;
        char* pcolor= (char*)&color;
        for(int j=iy;j<iy+h;j++){
            char* t=dst;
            for(int i=ix;i<image->line_byte;i++){
                alpha=(unsigned char)src[i];
                if(alpha==0);
                else if(alpha==255){
                    *(int*)t=color;
                }else{
                    t[2] += (((pcolor[2]-t[2])*alpha)>>8);
                    t[1] += (((pcolor[1]-t[1])*alpha)>>8);
                    t[0] += (((pcolor[0]-t[0])*alpha)>>8);
                }
                t+=4;
            }
            dst+=SCREEN_WIDTH*4;
            src+=image->line_byte;
        }
		return;
	}
/*---------------------------------------------------------------*/
	return;
}

void fb_draw_border(int x, int y, int w, int h, int color)
{
	if(w<=0 || h<=0) return;
	fb_draw_rect(x, y, w, 1, color);
	if(h > 1) {
		fb_draw_rect(x, y+h-1, w, 1, color);
		fb_draw_rect(x, y+1, 1, h-2, color);
		if(w > 1) fb_draw_rect(x+w-1, y+1, 1, h-2, color);
	}
}

/** draw a text string **/
void fb_draw_text(int x, int y, char *text, int font_size, int color)
{
	fb_image *img;
	fb_font_info info;
	int i=0;
	int len = strlen(text);
	while(i < len)
	{
		img = fb_read_font_image(text+i, font_size, &info);
		if(img == NULL) break;
		fb_draw_image(x+info.left, y-info.top, img, color);
		fb_free_image(img);

		x += info.advance_x;
		i += info.bytes;
	}
	return;
}

