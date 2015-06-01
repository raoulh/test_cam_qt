#include "MainWindow.h"
#include "ui_MainWindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    cam = new CamView(this);

    connect(cam, &CamView::newFrameAvailable, [=](const uchar *data, uint len)
    {
        QPixmap pix;
        pix.loadFromData(data, len);
        ui->label->setPixmap(pix);
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButtonPlay_clicked()
{
    cam->setUrl(ui->lineEdit->text());
    cam->play();
}

void MainWindow::on_pushButtonStop_clicked()
{
    cam->stop();
}

void MainWindow::on_pushButtonRestart_clicked()
{
    cam->restart();
}

void MainWindow::on_checkBox_stateChanged(int)
{
    cam->setAutoRestart(ui->checkBox->isChecked());
}
