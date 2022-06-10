#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <jpeglib.h>
#include <linux/fb.h>

#define DEVIDE_NAME		"/dev/video0"
#define CAPTURE_WIDTH	1280
#define CAPTURE_HEIGTH	720
#define RGBDATA  (CAPTURE_WIDTH * CAPTURE_HEIGTH *3)

int lcdfd = 0;	
unsigned int *lcdptr = NULL;
int lcd_w, lcd_h;


void yuyv_to_rgb(unsigned char *yuyvdata, unsigned char *rgbdata, int w, int h)
{
    //码流Y0 U0 Y1 V1 Y2 U2 Y3 V3 --> YUYV像素[Y0 U0 V1] [Y1 U0 V1] [Y2 U2 V3] [Y3 U2 V3]--> RGB像素
	int r1, g1, b1;
	int r2, g2, b2;
	for(int i=0; i<w*h/2; i++)
	{
	    char data[4];
	    memcpy(data, yuyvdata+i*4, 4);
	    unsigned char Y0=data[0];
	    unsigned char U0=data[1];
	    unsigned char Y1=data[2];
	    unsigned char V1=data[3];
		//Y0U0Y1V1  -->[Y0 U0 V1] [Y1 U0 V1]
	    r1 = Y0+1.4075*(V1-128); if(r1>255)r1=255; if(r1<0)r1=0;
	    g1 =Y0- 0.3455 * (U0-128) - 0.7169*(V1-128); if(g1>255)g1=255; if(g1<0)g1=0;
	    b1 = Y0 + 1.779 * (U0-128);  if(b1>255)b1=255; if(b1<0)b1=0;

	    r2 = Y1+1.4075*(V1-128);if(r2>255)r2=255; if(r2<0)r2=0;
	    g2 = Y1- 0.3455 * (U0-128) - 0.7169*(V1-128); if(g2>255)g2=255; if(g2<0)g2=0;
	    b2 = Y1 + 1.779 * (U0-128);  if(b2>255)b2=255; if(b2<0)b2=0;

		/*rgb or bgr depend on your camera*/
	    rgbdata[i*6+0]=b1;
	    rgbdata[i*6+1]=g1;
	    rgbdata[i*6+2]=r1;
	    rgbdata[i*6+3]=b2;
	    rgbdata[i*6+4]=g2;
	    rgbdata[i*6+5]=r2;
	}
}

void lcd_show_rgb(unsigned char *rgbdata, int w, int h)
{
	unsigned int *ptr = lcdptr;
	for(int i = 0; i < h; ++i) {
		for(int j = 0; j < w; ++j) {
			memcpy(ptr + j, rgbdata + j*3, 3);
		}
		ptr += lcd_w;
		rgbdata += w*3;
	}
}


int main(int argc, char **argv)
{
	lcdfd = open("/dev/fb0", O_RDWR);
	if(lcdfd < 0) {
		perror("open fd failed");
		return -1;
	}
	/*获取LCD信息*/
	struct fb_var_screeninfo info;
	int lret = ioctl(lcdfd, FBIOGET_VSCREENINFO, &info);

	lcd_w = info.xres_virtual; //虚拟机和物理机不同
	lcd_h = info.yres_virtual;
	
	printf("%d * %d \n", lcd_w, lcd_h);
	lcdptr = (unsigned int *)mmap(NULL, lcd_w*lcd_h*4, PROT_READ|PROT_WRITE, MAP_SHARED, lcdfd, 0);

	/*1. open device  */
	int fd = open(DEVIDE_NAME, O_RDWR);
	if(fd < 0) {
		perror("open video device 0 failed");
		return -1;
	}

	/* is camera? */
	struct v4l2_capability cap;
	
	int ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
	if (ret < 0) {
		perror("ioctl VIDIOC_QUERYCAP");
		close(fd);
	}
	if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {	
		printf("Driver Name: %s\n", cap.driver);         
	} else {
		printf("open file is not video\n");
		close(fd);
		return -2;
	}

	/*2. get support format */
	struct v4l2_fmtdesc fmtdesc;
	
	for(int i = 0; ;++i) {
		fmtdesc.index	= i;
		fmtdesc.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;

		int ret = ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc);
		if(ret < 0) {
			break;
		}
		printf("index:%d ", fmtdesc.index);
		printf("Picture Format:%s\n", fmtdesc.description);

		struct v4l2_frmsizeenum frmsize;
		frmsize.pixel_format = fmtdesc.pixelformat;
		for(int j = 0; ; ++j) {
			frmsize.index = j;
			ret = ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
			if(ret < 0) {
				break;
			}
			printf("width: %d height: %d\n",
					frmsize.discrete.width, frmsize.discrete.height);
		}
	}
	
	/*3. set the capture format*/
	struct v4l2_format format;

	format.type					= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.width		= CAPTURE_WIDTH;
	format.fmt.pix.height		= CAPTURE_HEIGTH;
	format.fmt.pix.pixelformat	= V4L2_PIX_FMT_YUYV; 
	ret = ioctl(fd, VIDIOC_S_FMT, &format);
	if(ret < 0) {
		perror("set format failed.");
	} 


	/*4. applay for kernel space*/
	struct v4l2_requestbuffers reqbuffer;
	reqbuffer.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	reqbuffer.count		= 4;
	reqbuffer.memory	= V4L2_MEMORY_MMAP; //映射方法
	ret = ioctl(fd, VIDIOC_REQBUFS, &reqbuffer);
	if(ret < 0) {
		perror("applay queue failed.");
	}

	/*5. map */
	unsigned char *mptr[4]; //保存映射后用户空间的首地址
	unsigned int size[4];
	struct v4l2_buffer mapbuffer;
	mapbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	for (int i = 0; i < 4; ++i) {
		mapbuffer.index = i;
		ret = ioctl(fd, VIDIOC_QUERYBUF, &mapbuffer);
		if(ret < 0) {
			perror("query kernel space queue failed.");		
		}
		
		mptr[i] = mmap(NULL, mapbuffer.length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, mapbuffer.m.offset);
		size[i] = mapbuffer.length;

		ret = ioctl(fd, VIDIOC_QBUF, &mapbuffer);
		if(ret < 0) {
			perror("put back failed.");
		}
	}

	/*6 start collecting */

	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = ioctl(fd, VIDIOC_STREAMON, &type);
	if(ret < 0) {
		perror("stream on failed");
	}

	/*7. collect data */
	unsigned char rgbdata[RGBDATA];
	while(1)
	{
		struct v4l2_buffer readbuffer;
		readbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		ret = ioctl(fd, VIDIOC_DQBUF, &readbuffer);
		if(ret < 0) {
			perror("extract data failed");
		}
		
		yuyv_to_rgb(mptr[readbuffer.index], rgbdata, CAPTURE_WIDTH, CAPTURE_HEIGTH);
		lcd_show_rgb(rgbdata, CAPTURE_WIDTH, CAPTURE_HEIGTH);	

		ret = ioctl(fd, VIDIOC_QBUF, &readbuffer);
		if(ret < 0) {
			perror("put back failed.");
		}
	}
	/*8 stop collecting */
	ret = ioctl(fd, VIDIOC_STREAMOFF, &type);

	/*9 free map*/
	for (int i = 0; i < 4; ++i)
		munmap(mptr[i], size[i]);
	
	/*10. close device */
	close(fd);

	return 0;
}
