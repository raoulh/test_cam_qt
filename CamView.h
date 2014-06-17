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

signals:
    void newFrameAvailable(const uchar *data, uint len);

private slots:
    void slotDataRead();
    void finished();
    void restart();
    void slotError(QNetworkReply::NetworkError e);
    void sslErrors(QNetworkReply *reply, const QList<QSslError> &);
    void downloadProgress(qint64 received, qint64 total);

private:
    QNetworkAccessManager *accessManager = nullptr;
    QNetworkReply *reply = nullptr;

    QString url;

    vector<unsigned char> buffer;

    bool formatDetected;
    bool single_frame;
    bool format_error;
    string boundary;
    int nextContentLength;
    int nextDataStart;
    int scanpos;

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
