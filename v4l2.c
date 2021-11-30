#include "v4l2.h"
#include <jpeglib.h>
#include "sys/time.h"
#include "libv4l2.h"

#ifdef __cplusplus
extern "C"
{
#endif

static struct buffer
{
    void *start;
    unsigned int length;
} * buffers;
int buffers_length;

char runningDev[15] = "";
char devName[15] = "";
char camName[32] = "";
char devFmtDesc[4] = "";

int fd = -1;
int videoIsRun = -1;
int deviceIsOpen = -1;
unsigned char *rgb24 = NULL;
int WIDTH = 1280, HEIGHT = 720;

//V4l2相关结构体
static struct v4l2_fmtdesc fmtdesc;
static struct v4l2_frmsizeenum frmsizeenum;
static struct v4l2_format format;
static struct v4l2_requestbuffers reqbuf;
static struct v4l2_buffer buffer;

struct _control *in_parameters;
int parametercount;
struct v4l2_jpegcompression jpegcomp;

void StartVideoPrePare();
void StartVideoStream();
void EndVideoStream();
void EndVideoStreamClear();
int test_device_exist(char *devName);

long getTimeUsec()
{
    struct timeval t;
    gettimeofday(&t, 0);
    return (long)((long)t.tv_sec * 1000 * 1000 + t.tv_usec);
}

//convert mjpeg frame to RGB24
int MJPEG2RGB(unsigned char *data_frame, unsigned char *rgb, int bytesused)
{
    // variables:
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    // data points to the mjpeg frame received from v4l2.
    unsigned char *data = data_frame;
    size_t data_size = bytesused;

    // all the pixels after conversion to RGB.
    int pixel_size = 0; //size of one pixel
    if (data == NULL || data_size <= 0)
    {
        printf("Empty data!\n");
        return -1;
    }

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, data, data_size);
    int rc = jpeg_read_header(&cinfo, TRUE);
    if (!(1 == rc))
    {
        //printf("Not a jpg frame.\n");
        //return -2;
    }
    jpeg_start_decompress(&cinfo);
    pixel_size = cinfo.output_components;

    // ... Every frame:
    while (cinfo.output_scanline < cinfo.output_height)
    {
        unsigned char *temp_array[] = {rgb + (cinfo.output_scanline) * WIDTH * pixel_size};
        jpeg_read_scanlines(&cinfo, temp_array, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return 0;
}

void StartVideoPrePare()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = WIDTH;
    format.fmt.pix.height = HEIGHT;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    ioctl(fd, VIDIOC_S_FMT, &format);

    //申请帧缓存区
    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.count = 4;

    if (-1 == ioctl(fd, VIDIOC_REQBUFS, &reqbuf))
    {
        if (errno == EINVAL)
            printf("Video capturing or mmap-streaming is not supported\n");
        else
            perror("VIDIOC_REQBUFS");
        return;
    }

    //分配缓存区
    buffers = calloc(reqbuf.count, sizeof(*buffers));
    if (buffers == NULL)
        perror("buffers is NULL");
    else
        assert(buffers != NULL);

    //mmap内存映射
    int i;
    for (i = 0; i < (int)reqbuf.count; i++)
    {
        memset(&buffer, 0, sizeof(buffer));
        buffer.type = reqbuf.type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;

        if (-1 == ioctl(fd, VIDIOC_QUERYBUF, &buffer))
        {
            perror("VIDIOC_QUERYBUF");
            return;
        }

        buffers[i].length = buffer.length;

        buffers[i].start = mmap(NULL, buffer.length,
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED,
                                fd, buffer.m.offset);

        if (MAP_FAILED == buffers[i].start)
        {
            perror("mmap");
            return;
        }
    }

    //将缓存帧放到队列中等待视频流到来
    unsigned int ii;
    for (ii = 0; ii < reqbuf.count; ii++)
    {
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = ii;
        if (ioctl(fd, VIDIOC_QBUF, &buffer) == -1)
        {
            perror("VIDIOC_QBUF failed");
        }
    }

    rgb24 = (unsigned char *)malloc(WIDTH * HEIGHT * 3 * sizeof(char));
    assert(rgb24 != NULL);
}

void StartVideoStream()
{
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) == -1)
    {
        perror("VIDIOC_STREAMON failed");
    }
}

void EndVideoStream()
{
    //关闭视频流
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) == -1)
    {
        perror("VIDIOC_STREAMOFF failed");
    }
}

void EndVideoStreamClear()
{
    //手动释放分配的内存
    int i;
    for (i = 0; i < (int)reqbuf.count; i++)
        munmap(buffers[i].start, buffers[i].length);
    //free(rgb24);
    rgb24 = NULL;
}

int test_device_exist(char *devName)
{
    struct stat st;
    if (-1 == stat(devName, &st))
        return -1;

    if (!S_ISCHR(st.st_mode))
        return -1;

    return 0;
}

int GetDeviceCount()
{
    char devname[15] = "";
    int count = 0;
    int i;
    for (i = 0; i < 100; i++)
    {
        sprintf(devname, "%s%d", "/dev/video", i);
        if (test_device_exist(devname) == 0)
            count++;

        memset(devname, 0, sizeof(devname));
    }

    return count;
}

//根据索引获取设备名称
char *GetDeviceName(int index)
{
    memset(devName, 0, sizeof(devName));

    int count = 0;
    char devname[15] = "";
    int i;
    for (i = 0; i < 100; i++)
    {
        sprintf(devname, "%s%d", "/dev/video", i);
        if (test_device_exist(devname) == 0)
        {
            if (count == index)
                break;
            count++;
        }
        else
        {
            memset(devname, 0, sizeof(devname));
        }
    }

    strcpy(devName, devname);

    return devName;
}

//运行指定索引的视频
int StartRun(int index)
{
    if (videoIsRun > 0)
        return -1;

    char *devname = GetDeviceName(index);
    fd = open(devname, O_RDWR);
    if (fd == -1)
        return -1;

    deviceIsOpen = 1;

    StartVideoPrePare();
    StartVideoStream();

    strcpy(runningDev, devname);
    videoIsRun = 1;

    //debug
    struct v4l2_capability cap;
    int ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
    if (ret < 0)
    {
        printf("VIDIOC_QUERYCAP error\n");
    }

    if (V4L2_CAP_META_CAPTURE & cap.device_caps)
    {
        fprintf(stderr, "Not Video Capture\n");
    }
    else
    {
        fprintf(stderr, "Video Capture\n");
    }

    return 0;
}

int GetFrame()
{
    if (videoIsRun > 0)
    {
        fd_set fds;
        struct timeval tv;
        int r;

        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        tv.tv_sec = 7;
        tv.tv_usec = 0;

        r = select(fd + 1, &fds, NULL, NULL, &tv);
        if (0 == r)
        {
            return -1;
        }
        else if (-1 == r)
        {
            return errno;
        }

        memset(&buffer, 0, sizeof(buffer));
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;

        if (ioctl(fd, VIDIOC_DQBUF, &buffer) == -1)
        {
            //perror("GetFrame VIDIOC_DQBUF Failed");//tdodo
            return errno;
        }
        else
        {
            //convert_yuv_to_rgb_buffer((unsigned char*)buffers[buffer.index].start, rgb24, WIDTH, HEIGHT);
            MJPEG2RGB((unsigned char *)buffers[buffer.index].start, rgb24, buffer.bytesused);
            if (ioctl(fd, VIDIOC_QBUF, &buffer) < 0)
            {
                //perror("GetFrame VIDIOC_QBUF Failed");//todo
                return errno;
            }
            return 0;
        }
    }

    return 0;
}

int StopRun()
{
    printf("stop run\n");
    if (videoIsRun > 0)
    {
        EndVideoStream();
        EndVideoStreamClear();
    }
    memset(runningDev, 0, sizeof(runningDev));
    videoIsRun = -1;
    deviceIsOpen = -1;

    if (close(fd) != 0)
    {
        return -1;
    }

    return 0;
}

char *GetDevFmtDesc(int index)
{
    memset(devFmtDesc, 0, sizeof(devFmtDesc));

    fmtdesc.index = index;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) != -1)
    {
        char fmt[5] = "";
        sprintf(fmt, "%c%c%c%c",
                (__u8)(fmtdesc.pixelformat & 0XFF),
                (__u8)((fmtdesc.pixelformat >> 8) & 0XFF),
                (__u8)((fmtdesc.pixelformat >> 16) & 0XFF),
                (__u8)((fmtdesc.pixelformat >> 24) & 0XFF));

        strncpy(devFmtDesc, fmt, 4);
    }

    return devFmtDesc;
}

int GetCurResWidth()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_FMT, &format) == -1)
        return -1;
    return format.fmt.pix.width;
}

int GetCurResHeight()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_FMT, &format) == -1)
        return -1;
    return format.fmt.pix.height;
}

void control_readed(struct v4l2_queryctrl *ctrl)
{
    struct v4l2_control c;
    memset(&c, 0, sizeof(struct v4l2_control));
    c.id = ctrl->id;

    if (in_parameters == NULL)
    {
        in_parameters = (control *)calloc(1, sizeof(control));
    }
    else
    {
        in_parameters =
            (control *)realloc(in_parameters, (parametercount + 1) * sizeof(control));
    }

    if (in_parameters == NULL)
    {
        printf("Calloc failed\n");
        return;
    }

    memcpy(&in_parameters[parametercount].ctrl, ctrl, sizeof(struct v4l2_queryctrl));
    in_parameters[parametercount].group = IN_CMD_V4L2;
    in_parameters[parametercount].value = c.value;
    if (ctrl->type == V4L2_CTRL_TYPE_MENU)
    {
        in_parameters[parametercount].menuitems =
            (struct v4l2_querymenu *)malloc((ctrl->maximum + 1) * sizeof(struct v4l2_querymenu));
        int i;
        for (i = ctrl->minimum; i <= ctrl->maximum; i++)
        {
            struct v4l2_querymenu qm;
            memset(&qm, 0, sizeof(struct v4l2_querymenu));
            qm.id = ctrl->id;
            qm.index = i;
            if (ioctl(fd, VIDIOC_QUERYMENU, &qm) == 0)
            {
                memcpy(&in_parameters[parametercount].menuitems[i], &qm, sizeof(struct v4l2_querymenu));
                printf("Menu item %d: %s\n", qm.index, qm.name);
            }
            else
            {
                printf("Unable to get menu item for %s, index=%d\n", ctrl->name, qm.index);
            }
        }
    }
    else
    {
        in_parameters[parametercount].menuitems = NULL;
    }

    in_parameters[parametercount].value = 0;
    in_parameters[parametercount].class_id = (ctrl->id & 0xFFFF0000);
#ifndef V4L2_CTRL_FLAG_NEXT_CTRL
    in_parameters[parametercount].class_id = V4L2_CTRL_CLASS_USER;
#endif

    int ret = -1;
    if (in_parameters[parametercount].class_id == V4L2_CTRL_CLASS_USER)
    {
        printf("V4L2 parameter found: %s value %d Class: USER \n", ctrl->name, c.value);
        ret = ioctl(fd, VIDIOC_G_CTRL, &c);
        if (ret == 0)
        {
            in_parameters[parametercount].value = c.value;
        }
        else
        {
            printf("Unable to get the value of %s retcode: %d  %s\n", ctrl->name, ret, strerror(errno));
        }
    }
    else
    {
        printf("V4L2 parameter found: %s value %d Class: EXTENDED \n", ctrl->name, c.value);
        struct v4l2_ext_controls ext_ctrls = {0};
        struct v4l2_ext_control ext_ctrl = {0};
        ext_ctrl.id = ctrl->id;
#ifdef V4L2_CTRL_TYPE_STRING
        ext_ctrl.size = 0;
        if (ctrl.type == V4L2_CTRL_TYPE_STRING)
        {
            ext_ctrl.size = ctrl->maximum + 1;
            // FIXMEEEEext_ctrl.string = control->string;
        }
#endif
        ext_ctrls.count = 1;
        ext_ctrls.controls = &ext_ctrl;
        ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, &ext_ctrls);
        if (ret)
        {
            switch (ext_ctrl.id)
            {
            case V4L2_CID_PAN_RESET:
                in_parameters[parametercount].value = 1;
                printf("Setting PAN reset value to 1\n");
                break;
            case V4L2_CID_TILT_RESET:
                in_parameters[parametercount].value = 1;
                printf("Setting the Tilt reset value to 2\n");
                break;
            default:
                printf("control id: 0x%08x failed to get value (error %i)\n", ext_ctrl.id, ret);
            }
        }
        else
        {
            switch (ctrl->type)
            {
#ifdef V4L2_CTRL_TYPE_STRING
            case V4L2_CTRL_TYPE_STRING:
                //string gets set on VIDIOC_G_EXT_CTRLS
                //add the maximum size to value
                in_parameters[parametercount].value = ext_ctrl.size;
                break;
#endif
            case V4L2_CTRL_TYPE_INTEGER64:
                in_parameters[parametercount].value = ext_ctrl.value64;
                break;
            default:
                in_parameters[parametercount].value = ext_ctrl.value;
                break;
            }
        }
    }

    parametercount++;
}

int isv4l2Control(int control, struct v4l2_queryctrl *queryctrl)
{
    int err = 0;

    queryctrl->id = control;
    if ((err = ioctl(fd, VIDIOC_QUERYCTRL, queryctrl)) < 0)
    {
        //fprintf(stderr, "ioctl querycontrol error %d \n",errno);
        return -1;
    }

    if (queryctrl->flags & V4L2_CTRL_FLAG_DISABLED)
    {
        //fprintf(stderr, "control %s disabled \n", (char *) queryctrl->name);
        return -1;
    }

    if (queryctrl->type & V4L2_CTRL_TYPE_BOOLEAN)
    {
        return 1;
    }

    if (queryctrl->type & V4L2_CTRL_TYPE_INTEGER)
    {
        return 0;
    }

    fprintf(stderr, "contol %s unsupported  \n", (char *)queryctrl->name);
    return -1;
}

#ifdef __cplusplus
}
#endif
