#define DIARY_API_TEST_URL qEnvironmentVariable("DIARY_TEST_URL")
#include "../src/api.h"
#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <thread>
#include <cassert>
#include <iostream>
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);QTcpServer server;assert(server.listen(QHostAddress::LocalHost,0));
    qputenv("DIARY_TEST_URL",QString("http://127.0.0.1:%1/responses").arg(server.serverPort()).toUtf8());
    int connections=0,requests=0;
    QObject::connect(&server,&QTcpServer::newConnection,&app,[&]{
        auto socket=server.nextPendingConnection();++connections;
        auto buffer=std::make_shared<QByteArray>();
        QObject::connect(socket,&QTcpSocket::readyRead,socket,[&,socket,buffer]{
            *buffer+=socket->readAll();int end=buffer->indexOf("\r\n\r\n");if(end<0)return;
            int length=0;for(auto line:buffer->left(end).split('\n'))if(line.toLower().startsWith("content-length:"))length=line.mid(15).trimmed().toInt();
            if(buffer->size()<end+4+length)return;
            auto body=QJsonDocument::fromJson(buffer->mid(end+4,length)).object();buffer->remove(0,end+4+length);++requests;
            QByteArray data="{\"status\":\"completed\"}";
            if(body.value("stream").toBool())data="data: {\"type\":\"response.output_text.delta\",\"delta\":\"first\"}\n\ndata: {\"type\":\"response.output_text.delta\",\"delta\":\" second\"}\n\ndata: {\"type\":\"response.completed\",\"response\":{\"status\":\"completed\"}}\n\n";
            if(body.value("hang").toBool())return;
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: "+QByteArray(body.value("stream").toBool()?"text/event-stream":"application/json")+"\r\nContent-Length: "+QByteArray::number(data.size())+"\r\nConnection: keep-alive\r\n\r\n"+data);
        });
    });
    std::thread worker([&]{
        auto valid=[] {return true;};
        assert(diaryApiRequest({},"test-only",valid).value("status")=="completed");
        QString streamed;
        assert(diaryApiRequest({},"test-only",valid,false,[&](const QString &d){QThread::msleep(30);streamed+=d;}).value("status")=="completed");
        assert(streamed=="first second");
        bool failed=false;try {diaryApiRequest({},"test-only",valid,false,[](const QString&){throw std::runtime_error("drawing cancelled");});}catch(const std::exception&){failed=true;}assert(failed);
        QElapsedTimer time;time.start();failed=false;
        try{diaryApiRequest(QJsonObject{{"hang",true}},"test-only",[&]{return time.elapsed()<100;});}catch(const std::exception&){failed=true;}
        assert(failed&&time.elapsed()<2000);
        QMetaObject::invokeMethod(&app,&QCoreApplication::quit,Qt::QueuedConnection);
    });
    QTimer::singleShot(10000,&app,[]{std::abort();});app.exec();worker.join();
    assert(requests==4);assert(connections==1);
    std::cout<<"PASS: persistent HTTP connection, streaming during slow consumption, callback failure, cancelled stalled request\n";
}
