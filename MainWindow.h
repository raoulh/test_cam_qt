#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "CamView.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_pushButtonPlay_clicked();
    void on_pushButtonStop_clicked();

private:
    Ui::MainWindow *ui;

    CamView *cam;
};

#endif // MAINWINDOW_H
