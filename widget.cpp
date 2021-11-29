#include "widget.h"
#include "ui_widget.h"
#include <QMutex>
#include <QDebug>
#include "v4l2.h"

QMutex mutexSetR;

extern struct _control* in_parameters;
extern int parametercount;

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    InitWidget();
}

Widget::~Widget()
{
    delete ui;
}

void Widget::InitWidget()
{
    err11 = err19 = 0;
    imageprocessthread = new MajorImageProcessingThread;

    setWindowTitle("camera demo");
    setFixedSize(1000,600);

    int camcount = GetDeviceCount();

    if(camcount > 0)
    {
        //获取所有设备
        for(int i = 0;i < camcount;i++)
            ui->cb_camera_name->addItem(QString::fromLatin1(GetCameraName(i)));

        //启动默认视频
        StartRun(2);
        imageprocessthread->init(ui->cb_camera_name->currentIndex());
        imageprocessthread->start();



        //获取所有分辨率
        ui->cb_resolution->clear();
        int rescount = GetResolutinCount();
        int curW = GetCurResWidth();
        int curH = GetCurResHeight();
        for(int i = 0; i < rescount; i++)
        {
            QString rW = QString::number(GetResolutionWidth(i));
            QString rH = QString::number(GetResolutionHeight(i));
            QString res = rW + "X" + rH;
            ui->cb_resolution->addItem(res);
            bool ok;
            if(curW == rW.toInt(&ok, 10) && curH == rH.toInt(&ok, 10))
                ui->cb_resolution->setCurrentIndex(i);
        }

        enumerateControls();
    }

    connect(imageprocessthread, SIGNAL(SendMajorImageProcessing(QImage, int)),
            this, SLOT(ReceiveMajorImage(QImage, int)));

    connect(ui->cb_resolution, SIGNAL(currentIndexChanged(QString)), this, SLOT(SetResolution()));
}

void Widget::ReceiveMajorImage(QImage image, int result)
{
    //超时后关闭视频
    //超时代表着VIDIOC_DQBUF会阻塞，直接关闭视频即可
    if(result == -1)
    {
        imageprocessthread->stop();
        imageprocessthread->wait();
        StopRun();

        ui->image_label->clear();
        ui->image_label->setText("获取设备图像超时！");
    }

    if(!image.isNull())
    {
        ui->image_label->clear();
        switch(result)
        {
        case 0:     //Success
            err11 = err19 = 0;
            if(image.isNull())
                ui->image_label->setText("画面丢失！");
            else
                ui->image_label->setPixmap(QPixmap::fromImage(image.scaled(ui->image_label->size())));

            break;
        case 11:    //Resource temporarily unavailable
            err11++;
            if(err11 == 10)
            {
                ui->image_label->clear();
                ui->image_label->setText("设备已打开，但获取视频失败！\n请尝试切换USB端口后断开重试！");
            }
            break;
        case 19:    //No such device
            err19++;
            if(err19 == 10)
            {
                ui->image_label->clear();
                ui->image_label->setText("设备丢失！");
            }
            break;
        }
    }
}

int Widget::SetResolution()
{
    int width, height;
    QString res = ui->cb_resolution->currentText();
    QStringList list = res.split("X");
    width = list[0].toInt();
    height = list[1].toInt();
    printf("set resolution width = %d, height = %d\n",width, height);

    mutexSetR.lock();
    V4L2SetResolution(width, height);
    mutexSetR.unlock();
    return 0;
}
