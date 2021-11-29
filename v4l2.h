#ifndef V4L2_H
#define V4L2_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned char *rgb24;
extern int videoIsRun;

enum _cmd_group {
    IN_CMD_GENERIC = 0, // if you use non V4L2 input plugin you not need to deal the groups.
    IN_CMD_V4L2 = 1,
    IN_CMD_RESOLUTION = 2,
    IN_CMD_JPEG_QUALITY = 3,
    IN_CMD_PWC = 4,
};

struct _control {
    struct v4l2_queryctrl ctrl;
    int value;
    struct v4l2_querymenu* menuitems;
    /*  In the case the control a V4L2 ctrl this variable will specify
        that the control is a V4L2_CTRL_CLASS_USER control or not.
        For non V4L2 control it is not acceptable, leave it 0.
    */
    int class_id;
    int group;
};

typedef struct _control control;

long getTimeUsec();
int GetDeviceCount();
char *GetDeviceName(int index);
int StartRun(int index);
int GetFrame();
int StopRun();

int GetDevFmtWidth();
int GetDevFmtHeight();
int GetDevFmtSize();
int GetDevFmtBytesLine();

int GetResolutinCount();
int GetResolutionWidth(int index);
int GetResolutionHeight(int index);
int GetCurResWidth();
int GetCurResHeight();

void enumerateControls();
int v4l2GetControl(int control);
int v4l2SetControl(unsigned int control_id, int value);

#ifdef  __cplusplus
}
#endif

#endif // V4L2_H
