#pragma once
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>
#include <QPointer>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <deque>
#include <memory>
#include "stream.h"
#include "anthropic.h"

// One Qt network thread and connection pool for the lifetime of the plugin.
// It keeps draining the response while the caller renders/draws incoming text.
inline QObject *diaryNetworkContext() {
    static QObject *context=[] {
        auto thread=new QThread;auto object=new QObject;
        object->moveToThread(thread);thread->start();return object;
    }();return context;
}
struct DiaryNetworkJob {
    std::mutex mutex;std::condition_variable changed;
    std::deque<QString> deltas;
    QJsonObject result;QString error;
    bool done=false;std::atomic<bool> abort{false};
};
inline QJsonObject diaryApiRequest(const QJsonObject &payload,const QString &key,
        const std::function<bool()> &valid,bool imageGeneration=false,
        const std::function<void(const QString&)> &onDelta={},const QString &provider="openai") {
    if(provider!="openai"&&provider!="anthropic")throw std::runtime_error("Unknown AI provider");
    bool claude=provider=="anthropic";
    if(claude&&imageGeneration)throw std::runtime_error("Claude does not generate raster images; use OpenAI or ask for a diagram");
    auto job=std::make_shared<DiaryNetworkJob>();bool streaming=bool(onDelta)&&!imageGeneration&&!claude;
    QJsonObject body=claude?tomClaudePayload(payload):payload;if(streaming)body.insert("stream",true);
    QMetaObject::invokeMethod(diaryNetworkContext(),[job,body,key,imageGeneration,streaming,claude] {
        static auto manager=new QNetworkAccessManager(diaryNetworkContext());
        QNetworkRequest request(QUrl(claude ? "https://api.anthropic.com/v1/messages" : imageGeneration ? "https://api.openai.com/v1/images/generations" : "https://api.openai.com/v1/responses"));
#ifdef DIARY_API_TEST_URL
        request.setUrl(QUrl(DIARY_API_TEST_URL));
#endif
        request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
        if(claude){request.setRawHeader("x-api-key",key.toUtf8());request.setRawHeader("anthropic-version","2023-06-01");}
        else request.setRawHeader("Authorization","Bearer "+key.toUtf8());
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
        auto reply=manager->post(request,QJsonDocument(body).toJson(QJsonDocument::Compact));
        reply->setReadBufferSize(65536);
        auto timeout=new QTimer(reply);timeout->setSingleShot(true);
        QObject::connect(timeout,&QTimer::timeout,reply,&QNetworkReply::abort);
        timeout->start(imageGeneration?300000:120000);
        auto cancel=new QTimer(reply);
        QObject::connect(cancel,&QTimer::timeout,reply,[job,reply]{if(job->abort)reply->abort();});cancel->start(25);
        auto bytes=std::make_shared<QByteArray>();auto parser=std::make_shared<DiaryEventStream>();
        auto received=std::make_shared<qint64>(0);
        auto drain=[job,reply,bytes,parser,received,imageGeneration,streaming] {
            auto chunk=reply->readAll();*received+=chunk.size();
            try {
                if(*received>(imageGeneration?32*1024*1024:4*1024*1024))throw std::runtime_error("Response too large");
                if(streaming)parser->feed(chunk,[job](const QString &delta){
                    std::lock_guard<std::mutex> lock(job->mutex);job->deltas.push_back(delta);job->changed.notify_one();
                });else *bytes+=chunk;
            } catch(const std::exception &e) {
                {std::lock_guard<std::mutex> lock(job->mutex);job->error=QString::fromUtf8(e.what());}
                job->abort=true;
            }
        };
        QObject::connect(reply,&QNetworkReply::readyRead,reply,drain);
        QObject::connect(reply,&QNetworkReply::finished,reply,[job,reply,bytes,parser,streaming,drain,claude] {
            drain();std::lock_guard<std::mutex> lock(job->mutex);
            int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if(job->error.isEmpty()) {
                if(code!=200||reply->error()!=QNetworkReply::NoError)
                    job->error=QString("AI request failed (HTTP %1); check Wi-Fi, key and model/tool support").arg(code);
                else if(streaming && (parser->failed||parser->completed.isEmpty()))job->error="Answer stream incomplete; partial ink may remain. Retry when ready";
                else {
                    job->result=streaming?parser->completed:QJsonDocument::fromJson(*bytes).object();
                    if(job->result.isEmpty())job->error="Invalid API response";
                    else if(claude)try{job->result=tomClaudeResponse(job->result);}catch(const std::exception &e){job->error=QString::fromUtf8(e.what());}
                }
            }
            job->done=true;job->changed.notify_one();reply->deleteLater();
        });
    },Qt::QueuedConnection);
    std::exception_ptr callbackError;
    for(;;) {
        std::deque<QString> deltas;bool done;
        {std::unique_lock<std::mutex> lock(job->mutex);
            job->changed.wait_for(lock,std::chrono::milliseconds(25),[&]{return job->done||!job->deltas.empty();});
            deltas.swap(job->deltas);done=job->done;
        }
        if(done && !job->error.isEmpty() && !callbackError)
            callbackError=std::make_exception_ptr(std::runtime_error(job->error.toStdString()));
        if(!callbackError)try {
            if(!valid())throw std::runtime_error("Cancelled or page changed; partial ink may remain");
            if(onDelta)for(const auto &delta:deltas)onDelta(delta);
        }catch(...){callbackError=std::current_exception();job->abort=true;}
        if(done)break;
    }
    if(callbackError)std::rethrow_exception(callbackError);
    if(!job->error.isEmpty())throw std::runtime_error(job->error.toStdString());
    return job->result;
}
