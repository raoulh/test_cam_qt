#include "CamView.h"
#include <QDebug>
#include <string.h>

CamView::CamView(QObject *parent) :
    QObject(parent)
{
    accessManager = new QNetworkAccessManager(this);
    connect(accessManager, SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError> &)),
            this, SLOT(sslErrors(QNetworkReply*, const QList<QSslError> &)));

    connect(this, &CamView::statusChanged, [=](int status)
    {
        qDebug() << "Status changed: " << (status == StatusPlaying?"Playing":"Stopped");
    });
}

void CamView::setUrl(QString _url)
{
    url = _url;
}

void CamView::play()
{
    if (url.isEmpty())
    {
        qWarning() << "CamView: Empty url";
        status = StatusStopped;
        emit statusChanged(status);
        return;
    }

    if (status != StatusStopped)
        return;

    qDebug() << "Start request to " << url;

    if (reply)
    {
        reply->abort();
        reply->deleteLater();
    }

    QUrl u(url);
    QNetworkRequest request(u);
    reply = accessManager->get(request);
    reply->setReadBufferSize(10*1024);

    single_frame = false;
    buffer.clear();
    formatDetected = false;
    format_error = false;
    nextContentLength = -1;

    connect(reply, SIGNAL(readyRead()), this, SLOT(slotDataRead()));
    connect(reply, SIGNAL(finished()), this, SLOT(finished()));
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(slotError(QNetworkReply::NetworkError)));

    status = StatusPlaying;
    emit statusChanged(status);
}

void CamView::disconnectSignals()
{
    disconnect(reply, SIGNAL(readyRead()), this, SLOT(slotDataRead()));
    disconnect(reply, SIGNAL(finished()), this, SLOT(finished()));
    disconnect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
               this, SLOT(slotError(QNetworkReply::NetworkError)));
}

void CamView::stop()
{
    if (reply)
    {
        disconnectSignals();
        reply->abort();
        reply->deleteLater();
        reply = nullptr;
    }

    status = StatusStopped;
    emit statusChanged(status);
}

void CamView::restart()
{
    qDebug() << "Restarting stream...";
    stop();
    play();
}

void CamView::slotError(QNetworkReply::NetworkError)
{
    qWarning() << "Error: " << reply->errorString();
    emit errorOccured();
}

void CamView::sslErrors(QNetworkReply *reply, const QList<QSslError> &)
{
    reply->ignoreSslErrors();
}

void CamView::finished()
{
    if (single_frame)
        emit newFrameAvailable(&buffer[0], buffer.size());

    if (reply)
    {
        disconnectSignals();
        reply->abort();
        reply->deleteLater();
        reply = nullptr;
    }

    if (format_error)
        return;

    if (auto_restart && status != StatusStopped)
        QTimer::singleShot(0, this, SLOT(restart()));
    else
    {
        status = StatusStopped;
        emit statusChanged(status);
    }
}

void CamView::slotDataRead()
{
    int pos = buffer.size();
    buffer.resize(pos + reply->bytesAvailable());
    reply->read((char *)&buffer[pos], reply->bytesAvailable());

    processData();
}

bool CamView::readEnd(int pos, int &lineend, int &nextstart)
{
    bool foundr = false;

    for (uint i = pos;i < buffer.size();i++)
    {
        if (buffer[i] == '\r')
            foundr = true;
        else if (buffer[i] == '\n')
        {
            lineend = i - (foundr?1:0);
            nextstart = i + 1;
            return true;
        }
    }

    return false;
}

bool CamView::strStartsWith(const string &str, const string &needle, CaseSensitivity cs)
{
    if (needle.empty())
        return true;

    if (needle.length() > str.length())
        return false;

    if (cs == CaseSensitive)
        return memcmp(str.c_str(), needle.c_str(), needle.length()) == 0;

    for (uint i = 0;i < needle.length();i++)
    {
        if (tolower(str[i]) != tolower(needle[i]))
            return false;
    }

    return true;
}

void CamView::processData()
{
    if (!formatDetected)
    {
        //first frame, we need to look for the boundary
        //and for content-type/content-length if present
        if (buffer.size() < 4)
            return; //need more data

        if (buffer[0] != '-' || buffer[1] != '-')
        {
            //try to detect of the data is directly the jpeg picture
            if (buffer[0] == 0xFF && buffer[1] == 0xD8)
            {
                qWarning() << "Data seems to be a single frame.";
                single_frame = true;
            }
            else
            {
                qWarning() << "Wrong start of frame, give up!";
                format_error = true;
            }

            formatDetected = true;
            return;
        }

        //search for the line end after the boundary to get the boundary text
        int end, next;
        if (!readEnd(2, end, next))
        {
            if (buffer.size() > 500)
            {
                qWarning() << "Boundary not found, give up!";
                format_error = true;
            }

            return; //need more data;
        }

        //get boundary
        boundary = string((char *)&buffer[0], end);

        //qDebug() << "Found boundary \"" << QString::fromStdString(boundary) << "\"";

        int i = next;
        while (readEnd(next, end, next))
        {
            int len = end - i;

            if (len == 0)
            {
                //line is empty, data starts now
                nextDataStart = next;
                formatDetected = true;
                scanpos = 0;
                break;
            }

            if (len > 15)
            {
                string s((char *)&buffer[i], len);
                if (CamView::strStartsWith(s, "Content-Length", CaseInsensitive))
                {
                    CamView::from_string(s.substr(15), nextContentLength);
                    //qDebug() << "Found content length header: \"" << nextContentLength << "\"";
                    //nextContentLength = -1; //to test code without content-length header
                }
            }

            i = next;
        }

        if (!formatDetected)
        {
            qWarning() << "Something is wrong in the data, give up!";
            format_error = true;
        }
    }

    if (formatDetected && !single_frame)
    {
        //we should be positionned at the start of data
        //small check to be sure
        if (!(buffer[nextDataStart] == 0xFF && buffer[nextDataStart + 1] == 0xD8))
        {
            qWarning() << "Wrong image data.";
            format_error = true;

            qDebug() << "Cancel stream";
            reply->abort();
            reply->deleteLater();
            reply = nullptr;

            return;
        }

        if (nextContentLength >= 0)
        {
            //the content-length is known, fast path
            if (buffer.size() < nextContentLength + nextDataStart + 2)
                return; //need more data

            //qDebug() << "Set new frame";

            emit newFrameAvailable(&buffer[nextDataStart], nextContentLength);

            if (buffer[nextDataStart + nextContentLength] == '\r') //assume a \n always follows \r
                nextContentLength += 2;
            else if (buffer[nextDataStart + nextContentLength] == '\n')
                nextContentLength += 1;

            //remove unused data from buffer
            auto iter = buffer.begin();
            buffer.erase(iter, iter + (nextDataStart + nextContentLength));

            //reset for next frame
            nextContentLength = -1;
            formatDetected = false;
            nextDataStart = 0;
            scanpos = 0;
        }
        else
        {
            uint i;
            //qDebug() << "scanpos: " << scanpos;
            scanpos = 0;
            for (i = nextDataStart + scanpos;
                 i < buffer.size() - boundary.length();i++)
            {
                if (buffer[i] == '-' && buffer[i + 1] == '-' &&
                    !boundary.compare(0, boundary.length(), (const char *)&buffer[i], boundary.length()))
                {
                    //boundary found
                    //check for newline between boundary and data
                    nextContentLength = i - nextDataStart;
                    /*if (buffer[i - 2] == '\r')
                        nextContentLength -= 2;
                    else if (buffer[i - 1] == '\n')
                        nextContentLength -= 1;*/

                    emit newFrameAvailable(&buffer[nextDataStart], nextContentLength);

                    //remove unused data from buffer
                    auto iter = buffer.begin();
                    buffer.erase(iter, iter + (nextDataStart + nextContentLength));

                    //reset for next frame
                    nextContentLength = -1;
                    formatDetected = false;
                    nextDataStart = 0;
                    scanpos = 0;
                    return;
                }
            }

            scanpos += i;
        }
    }
}
