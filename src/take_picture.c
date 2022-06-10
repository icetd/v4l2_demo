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

#define DEVIDE_NAME		"/dev/video0"
#define CAPTURE_WIDTH	1920
#define CAPTURE_HEIGTH	1080

int main(int argc, char **argv)
{
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

	format.type			= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.width		= CAPTURE_WIDTH;
	format.fmt.pix.height		= CAPTURE_HEIGTH;
	format.fmt.pix.pixelformat	= V4L2_PIX_FMT_MJPEG; 
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
	struct v4l2_buffer readbuffer;
	readbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = ioctl(fd, VIDIOC_DQBUF, &readbuffer);
	if(ret < 0) {
		perror("extract data failed`");
	}

	FILE *file = fopen("my.jpg", "w+");
	fwrite(mptr[readbuffer.index], readbuffer.length, 1, file);
	fclose(file);
	
	ret = ioctl(fd, VIDIOC_QBUF, &readbuffer);
	if(ret < 0) {
		perror("put back failed.");
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
