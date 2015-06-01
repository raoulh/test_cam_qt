#ifndef CAMVIEW_H
#define CAMVIEW_H

#include <QtCore>
#include <QtNetwork>
#include <iostream>
#include <sstream>
#include <vector>

using namespace std;

class CamView : public QObject
{
    Q_OBJECT
public:
    explicit CamView(QObject *parent = 0);

    void setUrl(QString url);
    int getStatus() { return status; }
    bool getAutoRestart() { return auto_restart; }
    void setAutoRestart(bool restart) { auto_restart = restart; }

    enum
    {
        StatusStopped = 0,
        StatusPlaying,
    };

signals:
    void newFrameAvailable(const uchar *data, uint len);
    void statusChanged(int status);
    void errorOccured();

public slots:
    void play();
    void stop();
    void restart();

private slots:
    void slotDataRead();
    void finished();
    void slotError(QNetworkReply::NetworkError e);
    void sslErrors(QNetworkReply *reply, const QList<QSslError> &);

private:
    QNetworkAccessManager *accessManager = nullptr;
    QNetworkReply *reply = nullptr;

    int status = StatusStopped;
    bool auto_restart = true;

    QString url;

    vector<unsigned char> buffer;

    bool formatDetected;
    bool single_frame;
    bool format_error;
    string boundary;
    int nextContentLength;
    int nextDataStart;
    int scanpos;

    void disconnectSignals();

    void processData();
    bool readEnd(int pos, int &lineend, int &nextstart);

    enum CaseSensitivity { CaseInsensitive, CaseSensitive };
    bool strStartsWith(const string &str, const string &needle, CaseSensitivity cs = CaseSensitive);

    template<typename T>
    bool from_string(const std::string &str, T &dest)
    {
        std::istringstream iss(str);
        iss >> dest;
        return iss.eof();
    }
};

#endif // CAMVIEW_H
