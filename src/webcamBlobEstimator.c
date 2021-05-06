/*
	Simple capture program tries to capture a specified number of
	frames and stores them into a series of target files

  This program is *not* implemented to be used in any production
  setting, it's just written as a simple example on how to use the
  V4L2 API to capture frames using the webcam. This also is the
  reason for it's structure that's far of from any clean software
  development pattern.
*/

#include <linux/videodev2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/event.h>

#include <jpeglib.h>
#include <jerror.h>

#include "./webcamBlobEstimator.h"

#ifndef __cplusplus
	typedef int bool;
	#define true 1
	#define false 0
#endif

static void printUsage(char* argv[]) {
	printf("Usage: %s CAPDEV TARGETFILE\n", argv[0]);
	printf("\n");
	printf("Captures into a specified failename. Also runs blob detection and exports / prints blob information\n");
	printf("\n");
	printf("Arguments:\n");
	printf("\tCAPDEV\n\t\tCapture device (for example /dev/video0)\n");
	printf("\tTARGETFILE\n\t\tTarget filename prefix excluding the extension\n");
}


static void drawRect(
	struct imgRawImage* lpImage,
	unsigned long int xStart,
	unsigned long int xEnd,
	unsigned long int yStart,
	unsigned long int yEnd,
	unsigned long int lineWidth
) {
	unsigned long int x,y;
	for(x = xStart; x <= xEnd; x=x+1) {
		for(y = yStart; (y < yStart + lineWidth) && (y < lpImage->height); y=y+1) {
			lpImage->lpData[(x + y * lpImage->width) * lpImage->numComponents] = 255;
		}
	}

	for(x = xStart; x <= xEnd; x=x+1) {
		for(y = yEnd; (y < yEnd + lineWidth) && (y < lpImage->height); y=y+1) {
			lpImage->lpData[(x + y * lpImage->width) * lpImage->numComponents] = 255;
		}
	}

	for(y = yStart; y <= yEnd; y=y+1) {
		for(x = xStart; (x < xStart + lineWidth) && (x < lpImage->width); x=x+1) {
			lpImage->lpData[(x + y * lpImage->width) * lpImage->numComponents] = 255;
		}
	}

	for(y = yStart; y <= yEnd; y=y+1) {
		for(x = xEnd; (x < xEnd + lineWidth) && (x < lpImage->width); x=x+1) {
			lpImage->lpData[(x + y * lpImage->width) * lpImage->numComponents] = 255;
		}
	}

}

static void greyscale(
	struct imgRawImage* lpImage
) {
	unsigned long int i;

	for(i = 0; i < (lpImage->width * lpImage->height); i=i+1) {
		double grey = 0.2126 * lpImage->lpData[i * lpImage->numComponents + 0]
						+ 0.7152 * lpImage->lpData[i * lpImage->numComponents + 1]
						+ 0.0722 * lpImage->lpData[i * lpImage->numComponents + 2];

		lpImage->lpData[i * lpImage->numComponents + 0] = (unsigned char)grey;
		lpImage->lpData[i * lpImage->numComponents + 1] = (unsigned char)grey;
		lpImage->lpData[i * lpImage->numComponents + 2] = (unsigned char)grey;
	}
}

static int createHistograms(
	struct imgRawImage* lpImage,
	char* lpFilenamePrefix,
	struct histogramBuffer** lpHistXOut,
	struct histogramBuffer** lpHistYOut,
	struct rectBound* lpRegion
) {
	struct histogramBuffer* lpNewHistX;
	struct histogramBuffer* lpNewHistY;
	unsigned long int i;
	unsigned long int x,y,c;

	struct rectBound bounds;

	if(lpRegion != NULL) {
		bounds.xMin = lpRegion->xMin;
		bounds.xMax = lpRegion->xMax;
		bounds.yMin = lpRegion->yMin;
		bounds.yMax = lpRegion->yMax;
	} else {
		bounds.xMin = 0;
		bounds.yMin = 0;
		bounds.xMax = lpImage->width;
		bounds.yMin = lpImage->height;
	}

	/*
		Create histogram X and histogram Y
	*/
	lpNewHistX = malloc(sizeof(struct histogramBuffer) + sizeof(double)*(lpImage->width));
	if(lpNewHistX == NULL) {
		return 1;
	}
	lpNewHistY = malloc(sizeof(struct histogramBuffer) + sizeof(double)*(lpImage->height));
	if(lpNewHistY == NULL) {
		free(lpNewHistX);
		return 1;
	}

	lpNewHistX->sLen = lpImage->width;
	lpNewHistY->sLen = lpImage->height;

	for(i = 0; i < lpImage->width; i=i+1)  { lpNewHistX->dValues[i] = 0; }
	for(i = 0; i < lpImage->height; i=i+1) { lpNewHistY->dValues[i] = 0; }

	for(x = 0; x < lpImage->width; x=x+1) {
		for(y = 0; y < lpImage->height; y=y+1) {
			for(c = 1; c < lpImage->numComponents; c=c+1) {
				lpNewHistX->dValues[x] = lpNewHistX->dValues[x] + ((double)lpImage->lpData[(x + y * lpImage->width)*lpImage->numComponents + c])/255.0;
				lpNewHistY->dValues[y] = lpNewHistY->dValues[y] + ((double)lpImage->lpData[(x + y * lpImage->width)*lpImage->numComponents + c])/255.0;
			}
		}
	}

	/*
		Dump raw histogram data
	*/
	{
		char* lpFilename = NULL;
		if(asprintf(&lpFilename, "%s-histrawx.dat", lpFilenamePrefix) < 0) {
			free(lpNewHistX);
			free(lpNewHistY);
			return 1;
		}
		FILE* fHandle = fopen(lpFilename, "w");
		if(fHandle == NULL) {
			free(lpFilename);
			free(lpNewHistX);
			free(lpNewHistY);
			return 1;
		}
		for(i = 0; i < lpNewHistX->sLen; i=i+1) {
			fprintf(fHandle, "%lu\t%lf\n", i, lpNewHistX->dValues[i]);
		}
		fclose(fHandle);
		free(lpFilename);
	}
	{
		char* lpFilename = NULL;
		if(asprintf(&lpFilename, "%s-histrawy.dat", lpFilenamePrefix) < 0) {
			free(lpNewHistX);
			free(lpNewHistY);
			return 1;
		}
		FILE* fHandle = fopen(lpFilename, "w");
		if(fHandle == NULL) {
			free(lpFilename);
			free(lpNewHistX);
			free(lpNewHistY);
			return 1;
		}
		for(i = 0; i < lpNewHistY->sLen; i=i+1) {
			fprintf(fHandle, "%lu\t%lf\n", i, lpNewHistY->dValues[i]);
		}
		fclose(fHandle);
		free(lpFilename);
	}

	/*
		Normalize histograms (for peak search)
		and perform absolute peak search in parallel
	*/
	unsigned long int absPeakX = 0;
	unsigned long int absPeakY = 0;
	double dAvgX = 0;
	double dAvgY = 0;
	{
		double dMin = lpNewHistX->dValues[0];
		double dMax = lpNewHistX->dValues[0];
		for(i = 0; i < lpNewHistX->sLen; i=i+1) {
			if(lpNewHistX->dValues[i] < dMin) { dMin = lpNewHistX->dValues[i]; }
			if(lpNewHistX->dValues[i] > dMax) { dMax = lpNewHistX->dValues[i]; absPeakX = i; }
			dAvgX = dAvgX + lpNewHistX->dValues[i];
		}
		dAvgX = dAvgX / ((double)lpNewHistX->sLen);

/*		double dRange = (dMax - dMin);

		for(i = 0; i < lpNewHistX->sLen; i=i+1) {
			lpNewHistX->dValues[i] = (lpNewHistX->dValues[i] - dMin) / (dRange);
		} */
	}
	{
		double dMin = lpNewHistY->dValues[0];
		double dMax = lpNewHistY->dValues[0];
		for(i = 0; i < lpNewHistY->sLen; i=i+1) {
			if(lpNewHistY->dValues[i] < dMin) { dMin = lpNewHistY->dValues[i]; }
			if(lpNewHistY->dValues[i] > dMax) { dMax = lpNewHistY->dValues[i]; absPeakY = i; }
			dAvgY = dAvgY + lpNewHistY->dValues[i];
		}
		dAvgY = dAvgY / ((double)lpNewHistY->sLen);

/*		double dRange = (dMax - dMin);

		for(i = 0; i < lpNewHistY->sLen; i=i+1) {
			lpNewHistY->dValues[i] = (lpNewHistY->dValues[i] - dMin) / (dRange);
		} */
	}

/*	{
		char* lpFilename = NULL;
		if(asprintf(&lpFilename, "%s-histnormx.dat", lpFilenamePrefix) < 0) {
			free(lpNewHistX);
			free(lpNewHistY);
			return 1;
		}
		FILE* fHandle = fopen(lpFilename, "w");
		if(fHandle == NULL) {
			free(lpFilename);
			free(lpNewHistX);
			free(lpNewHistY);
			return 1;
		}
		for(i = 0; i < lpNewHistX->sLen; i=i+1) {
			fprintf(fHandle, "%lu\t%lf\n", i, lpNewHistX->dValues[i]);
		}
		fclose(fHandle);
		free(lpFilename);
	}
	{
		char* lpFilename = NULL;
		if(asprintf(&lpFilename, "%s-histnormy.dat", lpFilenamePrefix) < 0) {
			free(lpNewHistX);
			free(lpNewHistY);
			return 1;
		}
		FILE* fHandle = fopen(lpFilename, "w");
		if(fHandle == NULL) {
			free(lpFilename);
			free(lpNewHistX);
			free(lpNewHistY);
			return 1;
		}
		for(i = 0; i < lpNewHistY->sLen; i=i+1) {
			fprintf(fHandle, "%lu\t%lf\n", i, lpNewHistY->dValues[i]);
		}
		fclose(fHandle);
		free(lpFilename);
	} */

	/*
		Calculate width of peaks

		The seed is given by the local max.
		The width is determined by associating every pixel with the peak that's above
		average (?), expanding from the seed ...
	*/
	{
		unsigned long int peakXMin = absPeakX;
		unsigned long int peakXMax = absPeakX;
		unsigned long int peakYMin = absPeakY;
		unsigned long int peakYMax = absPeakY;

		double dPeakSumX = 0;
		double dPeakSumY = 0;

		while((peakXMin > 0) && ((double)lpNewHistX->dValues[peakXMin-1] > dAvgX)) { peakXMin = peakXMin - 1; }
		while((peakXMax < (lpNewHistX->sLen-1)) && ((double)lpNewHistX->dValues[peakXMax+1] > dAvgX)) { peakXMax = peakXMax + 1; }

		while((peakYMin > 0) && ((double)lpNewHistY->dValues[peakYMin-1] > dAvgY)) { peakYMin = peakYMin - 1; }
		while((peakYMax < (lpNewHistY->sLen-1)) && ((double)lpNewHistY->dValues[peakYMax+1] > dAvgY)) { peakYMax = peakYMax + 1; }

		for(i = peakXMin; i <= peakXMax; i=i+1) {
			dPeakSumX = dPeakSumX + ((double)lpNewHistX->dValues[i]);
		}
		for(i = peakYMin; i <= peakYMax; i=i+1) {
			dPeakSumY = dPeakSumY + ((double)lpNewHistY->dValues[i]);
		}

		printf("# Estimated peak\n#\tx: %lu %lu\n#\ty : %lu %lu\n#\tWidths: %lu %lu\n#\tSums: %lf %lf\n%lu %lu %lu %lu %lu %lu %lf %lf\n", peakXMin, peakXMax, peakYMin, peakYMax, peakXMax-peakXMin, peakYMax-peakYMin, dPeakSumX, dPeakSumY, peakXMin, peakXMax, peakYMin, peakYMax, peakXMax-peakXMin, peakYMax-peakYMin, dPeakSumX, dPeakSumY);

		/*
			Plot estimated peak location into image (2 pixel wide red if possible)...
		*/
		drawRect(lpImage, peakXMin, peakXMax, peakYMin, peakYMax, 2);
	}

	return 0;
}

/*
  Write one image into a target file

  See https://www.tspi.at/2020/03/20/libjpegexample.html
*/
static int storeJpegImageFile(struct imgRawImage* lpImage, char* lpFilename) {
	struct jpeg_compress_struct info;
	struct jpeg_error_mgr err;

	unsigned char* lpRowBuffer[1];

	FILE* fHandle;

	fHandle = fopen(lpFilename, "wb");
	if(fHandle == NULL) {
		#ifdef DEBUG
			fprintf(stderr, "%s:%u Failed to open output file %s\n", __FILE__, __LINE__, lpFilename);
		#endif
		return 1;
	}

	info.err = jpeg_std_error(&err);
	jpeg_create_compress(&info);

	jpeg_stdio_dest(&info, fHandle);

	info.image_width = lpImage->width;
	info.image_height = lpImage->height;
	info.input_components = 3;
	info.in_color_space = JCS_RGB;

	jpeg_set_defaults(&info);
	jpeg_set_quality(&info, 100, TRUE);

	jpeg_start_compress(&info, TRUE);

	/* Write every scanline ... */
	while(info.next_scanline < info.image_height) {
		lpRowBuffer[0] = &(lpImage->lpData[info.next_scanline * (lpImage->width * 3)]);
		jpeg_write_scanlines(&info, lpRowBuffer, 1);
	}

	jpeg_finish_compress(&info);
	fclose(fHandle);

	jpeg_destroy_compress(&info);
	return 0;
}

/*
  Wrapper around ioctl that repeats the calls in case
  they are interrupted by a signal (i.e. restarts) until
  they succeed or fail.
*/
static int xioctl(int fh, int request, void* arg) {
	int r;
	do {
		r = ioctl(fh, request, arg);
	} while((r == -1) && (errno == EINTR));
	return r;
}

/*
  Open the device file (first check it exists, is
  a device file, etc.)
*/
static char* deviceOpen_DefaultFilename = "/dev/video0";
static enum cameraError deviceOpen(
	int* lpDeviceOut,
	char* deviceName
) {
	struct stat st;
	int hHandle;

	if(lpDeviceOut == NULL) {
		return cameraE_InvalidParam;
	}
	(*lpDeviceOut) = -1;

	if(deviceName == NULL) {
		deviceName = deviceOpen_DefaultFilename;
	}

	/*
		First check that the file exists and that we
		are really seeing a device file
	*/
	if(stat(deviceName, &st) == -1) {
		return cameraE_UnknownDevice;
	}

	if(!S_ISCHR(st.st_mode)) {
		return cameraE_UnknownDevice;
	}

	hHandle = open(deviceName, O_RDWR|O_NONBLOCK, 0);
	if(hHandle < 0) {
		switch(errno) {
			case EACCES:	return cameraE_PermissionDenied;
			case EPERM:		return cameraE_PermissionDenied;
			default:		return cameraE_Failed;
		}
	}

	(*lpDeviceOut) = hHandle;
	return cameraE_Ok;
}

static enum cameraError deviceClose(
	int hHandle
) {
	if(hHandle < 0) { return cameraE_InvalidParam; }
	close(hHandle);
	return cameraE_Ok;
}




int main(int argc, char* argv[]) {
	enum cameraError e;
	int hHandle;
	int kq = -1;

	struct imgRawImage* lpRawImg;

	if(argc < 3) { printUsage(argv); return 1; }
	if(argc > 4) { printUsage(argv); return 1; }

	/*
		Try to open the camera
	*/
	e = deviceOpen(&hHandle, argv[1]);
	if(e != cameraE_Ok) {
		printf("Failed to open camera\n");
		return 2;
	}

	kq = kqueue();
	if(kq == -1) {
		printf("%s:%u Failed to create kqueue\n", __FILE__, __LINE__);
		return 3;
	}

	/*
		Query capabilities
	*/
	bool bReadWriteSupported = false;
	bool bStreamingSupported = false;
	{
		struct v4l2_capability cap;

		memset(&cap, 0, sizeof(cap));

		if(xioctl(hHandle, VIDIOC_QUERYCAP, &cap) == -1) {
			printf("%s:%u Failed to query capabilities\n", __FILE__, __LINE__);
			deviceClose(hHandle);
			return 2;
		}

		if((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0) {
			printf("%s:%u Device does not support video capture\n", __FILE__, __LINE__);
			deviceClose(hHandle);
			return 2;
		}

		if((cap.capabilities & V4L2_CAP_READWRITE) != 0) {
			bReadWriteSupported = true;
		}
		if((cap.capabilities & V4L2_CAP_STREAMING) != 0) {
			bStreamingSupported = true;
		}
	}


	#ifdef DEBUG
		printf("%s:%u Read/Write interface supported: %s\n", __FILE__, __LINE__, (bReadWriteSupported == true) ? "yes" : "no");
		printf("%s:%u Streaming interface supported: %s\n", __FILE__, __LINE__, (bStreamingSupported == true) ? "yes" : "no");
	#endif

	/*
		Query cropping capabilities and set cropping rectangle
	*/
	int defaultWidth 	= 640;
	int defaultHeight	= 480;
	for(;;) {
		struct v4l2_cropcap cropcap;

		memset(&cropcap, 0, sizeof(cropcap));
		cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if(xioctl(hHandle, VIDIOC_CROPCAP, &cropcap) == -1) {
			printf("%s:%u Failed to query cropping capabilities, continuing anyways\n", __FILE__, __LINE__);
			break;
		}

		#ifdef DEBUG
			printf("Cropping capabilities:\n");
			printf("\tDefault boundaries: %d, %d, %d, %d\n", cropcap.defrect.left, cropcap.defrect.top, cropcap.defrect.width, cropcap.defrect.height);
			printf("\tBoundaries (left, top, width, height): %d, %d, %d, %d\n", cropcap.bounds.left, cropcap.bounds.top, cropcap.bounds.width, cropcap.bounds.height);
			printf("\tAspect ratio: %u : %u\n", cropcap.pixelaspect.numerator, cropcap.pixelaspect.denominator);
		#endif

		printf("Setting default cropping rectangle ... ");

		struct v4l2_crop crop;

		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect;

		if(xioctl(hHandle, VIDIOC_S_CROP, &crop) == -1) {
			printf("failed\n");
		} else {
			printf("ok\n");
		}

		break;
	}

	/*
		Enumerate all supported formats (even though we'll request 640 x 480
		YUYV later on anyways)
	*/
	{
		printf("Doing format negotiation\n");

		int idx = 0;
		for(idx = 0;; idx = idx + 1) {
			struct v4l2_fmtdesc fmt;

			fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			fmt.index = idx;

			if(xioctl(hHandle, VIDIOC_ENUM_FMT, &fmt) == -1) {
				printf("Done enumeration after %u formats\n", idx);
				break;
			}

			printf("\tFormat %u with code %08x (compressed: %s): %s\n", idx, fmt.pixelformat, ((fmt.flags & V4L2_FMT_FLAG_COMPRESSED) != 0) ? "yes" : "no", fmt.description);
		}
	}

	/*
		v4l2_format negotiation, we just request the default ones or 640x480
	*/
	{
		struct v4l2_format fmt;

		memset(&fmt, 0, sizeof(fmt));

		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmt.fmt.pix.width = defaultWidth;
		fmt.fmt.pix.height = defaultHeight;
		fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
		fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

		if(xioctl(hHandle, VIDIOC_S_FMT, &fmt) == -1) {
			printf("%s:%u Format negotiation (S_FMT) failed!\n", __FILE__, __LINE__);
		}

		/* Now one should query the real size ... */
		defaultWidth = fmt.fmt.pix.width;
		defaultHeight = fmt.fmt.pix.height;
	}

	printf("Negotiated width and height: %d x %d\n", defaultWidth, defaultHeight);

	/*
		Setup buffers
	*/
	int bufferCount = 2;
	{
		struct v4l2_requestbuffers rqBuffers;

		/*
			Request 1 buffer (simple but not seamless, usually use 3+) ...
		*/

		memset(&rqBuffers, 0, sizeof(rqBuffers));
		rqBuffers.count = bufferCount;
		rqBuffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		rqBuffers.memory = V4L2_MEMORY_MMAP;

		if(xioctl(hHandle, VIDIOC_REQBUFS, &rqBuffers) == -1) {
			printf("%s:%u Requesting buffers failed!\n", __FILE__, __LINE__);
			deviceClose(hHandle);
			return 2;
		}

		bufferCount = rqBuffers.count;
	}
	printf("Requested %d buffers\n", bufferCount);

	/*
		Map buffers
	*/
	struct imageBuffer* lpBuffers;
	{
		lpBuffers = calloc(bufferCount, sizeof(struct imageBuffer));
		if(lpBuffers == NULL) {
			printf("%s:%u Out of memory\n", __FILE__, __LINE__);
			deviceClose(hHandle);
			return 2;
		}

		int iBuf;
		for(iBuf = 0; iBuf < bufferCount; iBuf = iBuf + 1) {
			struct v4l2_buffer vBuffer;

			memset(&vBuffer, 0, sizeof(struct v4l2_buffer));

			vBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			vBuffer.memory = V4L2_MEMORY_MMAP;
			vBuffer.index = iBuf;

			if(xioctl(hHandle, VIDIOC_QUERYBUF, &vBuffer) == -1) {
				printf("%s:%u Failed to query buffer %d\n", __FILE__, __LINE__, iBuf);
				deviceClose(hHandle);
				return 2;
			}

			lpBuffers[iBuf].lpBase = mmap(NULL, vBuffer.length, PROT_READ|PROT_WRITE, MAP_SHARED, hHandle, vBuffer.m.offset);
			lpBuffers[iBuf].sLen = vBuffer.length;

			if(lpBuffers[iBuf].lpBase == MAP_FAILED) {
				printf("%s:%u Failed to map buffer %d\n", __FILE__, __LINE__, iBuf);
				deviceClose(hHandle);
				return 2;
			}
		}
	}

	/*
		First we queue all buffers
	*/
	{
		int iBuf;
		for(iBuf = 0; iBuf < bufferCount; iBuf = iBuf + 1) {
			struct v4l2_buffer buf;
			memset(&buf, 0, sizeof(struct v4l2_buffer));

			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			buf.index = iBuf;

			if(xioctl(hHandle, VIDIOC_QBUF, &buf) == -1) {
				printf("%s:%u Queueing buffer %d failed ...\n", __FILE__, __LINE__, iBuf);
				deviceClose(hHandle);
				return 2;
			}
		}
	}

	/*
		Add to kqueue ...
	*/
	{
		struct kevent kev;

		EV_SET(&kev, hHandle, EVFILT_READ, EV_ADD|EV_ENABLE|EV_CLEAR, 0, 0, NULL);
		kevent(kq, &kev, 1, NULL, 0, NULL);
	}

	/*
		Run streaming loop
	*/
	{
		/* Enable streaming */
		enum v4l2_buf_type type;

		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if(xioctl(hHandle, VIDIOC_STREAMON, &type) == -1) {
			printf("%s:%u Stream on failed\n", __FILE__, __LINE__);
			deviceClose(hHandle);
			return 2;
		}
	}

	/*
		Capture specified number of frames ...
	*/
	for(;;) {
		struct kevent kev;
		struct v4l2_buffer buf;

		int r = kevent(kq, NULL, 0, &kev, 1, NULL);
		if(r < 0) {
			printf("%s:%u kevent failed\n", __FILE__, __LINE__);
			deviceClose(hHandle);
			return 2;
		}

		if(r > 0) {
			/* We got our frame or EOF ... try to dqueue */
			memset(&buf, 0, sizeof(struct v4l2_buffer));

			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;

			if(xioctl(hHandle, VIDIOC_DQBUF, &buf) == -1) {
				if(errno == EAGAIN) { continue; }

				printf("%s:%u DQBUF failed\n", __FILE__, __LINE__);
				deviceClose(hHandle);
				return 2;
			}

			printf("%s:%u Dequeued buffer %d\n", __FILE__, __LINE__, buf.index);

			/* ToDo: Process image ... */
			{
				lpRawImg = malloc(sizeof(struct imgRawImage));
				if(lpRawImg == NULL) {
					printf("%s:%u Out of memory\n", __FILE__, __LINE__);
					deviceClose(hHandle);
					return 2;
				}
				lpRawImg->lpData = malloc(sizeof(unsigned char)*defaultWidth*defaultHeight*3);
				if(lpRawImg->lpData == NULL) {
					free(lpRawImg);
					printf("%s:%u Out of memory\n", __FILE__, __LINE__);
					deviceClose(hHandle);
					return 2;
				}
				/*
					Convert the previously requested YUYV (YUV422) image into RGB (RGB888)

					YUV422:
						4 Byte -> 2 Pixel

					RGB888
						3 Byte -> 1 Pixel
				*/

				lpRawImg->numComponents = 3;
				lpRawImg->width = defaultWidth;
				lpRawImg->height = defaultHeight;

				unsigned long int row,col;
				for(row = 0; row < defaultHeight; row=row+1) {
					for(col = 0; col < defaultWidth; col=col+1) {
						unsigned char y0, y1, y;
						unsigned char u0, v0;

						signed int c,d,e;
						unsigned char r,g,b;
						signed int rtmp,gtmp, btmp;

						y0 = ((unsigned char*)(lpBuffers[buf.index].lpBase))[((col + row * defaultWidth) >> 1)*4 + 0];
						u0 = ((unsigned char*)(lpBuffers[buf.index].lpBase))[((col + row * defaultWidth) >> 1)*4 + 1];
						y1 = ((unsigned char*)(lpBuffers[buf.index].lpBase))[((col + row * defaultWidth) >> 1)*4 + 2];
						v0 = ((unsigned char*)(lpBuffers[buf.index].lpBase))[((col + row * defaultWidth) >> 1)*4 + 3];

						if((col + row * defaultWidth) % 2 == 0) {
							y = y0;
						} else {
							y = y1;
						}

						c = ((signed int)y) - 16;
						d = ((signed int)u0) - 128;
						e = ((signed int)v0) - 128;

						rtmp = ((298 * c + 409 * e + 128) >> 8);
						gtmp = ((298 * c - 100 * d - 208 * e + 128) >> 8);
						btmp = ((298 * c + 516 * d + 128) >> 8);

						if(rtmp < 0) { r = 0; }
						else if(rtmp > 255) { r = 255; }
						else { r = (unsigned char)rtmp; }

						if(gtmp < 0) { g = 0; }
						else if(gtmp > 255) { g = 255; }
						else { g = (unsigned char)gtmp; }

						if(btmp < 0) { b = 0; }
						else if(btmp > 255) { b = 255; }
						else { b = (unsigned char)btmp; }

						lpRawImg->lpData[(col + row*defaultWidth)*3 + 0] = r;
						lpRawImg->lpData[(col + row*defaultWidth)*3 + 1] = g;
						lpRawImg->lpData[(col + row*defaultWidth)*3 + 2] = b;
					}
				}

	        	char* lpFilename = NULL;
	        	if(asprintf(&lpFilename, "%s-raw.jpg", argv[2]) < 0) {
					printf("%s:%u Out of memory, skipping frame\n", __FILE__, __LINE__);
	        	} else {
	  				printf("%s:%u Writing %s\n", __FILE__, __LINE__, lpFilename);
					greyscale(lpRawImg);
					createHistograms(lpRawImg, argv[2], NULL, NULL, NULL);
		  			storeJpegImageFile(lpRawImg, lpFilename);
	          		free(lpFilename);
				}

				free(lpRawImg->lpData);
				free(lpRawImg);
				lpRawImg = NULL;
			}

			/* Re-enqueue */
			if(xioctl(hHandle, VIDIOC_QBUF, &buf) == -1) {
				printf("%s:%u Queueing buffer %d failed ...\n", __FILE__, __LINE__, buf.index);
				deviceClose(hHandle);
				return 2;
			}
		}
		break;
	}





	/*
		Stop streaming
	*/
	{
		/* Disable streaming */
		enum v4l2_buf_type type;

		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if(xioctl(hHandle, VIDIOC_STREAMOFF, &type) == -1) {
			printf("%s:%u Stream off failed\n", __FILE__, __LINE__);
			deviceClose(hHandle);
			return 2;
		}
	}

	/*
		Release buffers ...
	*/
	{
		int iBuf;
		for(iBuf = 0; iBuf < bufferCount; iBuf = iBuf + 1) {
			munmap(lpBuffers[iBuf].lpBase, lpBuffers[iBuf].sLen);
		}
	}

	{
		struct v4l2_requestbuffers rqBuffers;

		/*
			Request 1 buffer (simple but not seamless, usually use 3+) ...
		*/

		memset(&rqBuffers, 0, sizeof(rqBuffers));
		rqBuffers.count = 0;
		rqBuffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		rqBuffers.memory = V4L2_MEMORY_MMAP;

		if(xioctl(hHandle, VIDIOC_REQBUFS, &rqBuffers) == -1) {
			printf("%s:%u Releasing buffers failed!\n", __FILE__, __LINE__);
			deviceClose(hHandle);
			return 2;
		}
	}

	/*
		Close camera at the end
	*/
	deviceClose(hHandle);
	return 0;
}
