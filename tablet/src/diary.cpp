// Ink Diary prototype. GPL-3.0-only. See THIRD_PARTY.md.
#include <atomic>
#include <mutex>
#include <poll.h>
#include <QSaveFile>
#include <thread>
#include <stdexcept>
#include <QBuffer>
#include <QDir>
#include <QEventLoop>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QTemporaryDir>
#include <QUuid>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QTimer>
#include "inserter.inc"
#include "layout.h"
#include "response.h"
#include "plot.h"
#include "api.h"
#include "images.h"
#include "diagrams.h"
#include "webanswer.h"
#include "markdown.h"
#include "freshness.h"
#include "letterpaths.h"
#include "inkpagination.h"
#include "tutor.h"
static bool diaryCancellationRequested();
#include "pathwriter.inc"
#include "zigzag.h"
#include "feedback.h"
#include "penaway.h"
extern "C" char *diary_start(const char *);

namespace {
std::atomic<bool> busy{false}, cancelled{false};
QMutex stateMutex;
QString message = "ChatGPT";
QPointer<QObject> originController, originView;
QString originPage;
const QString base = "/home/root/.config/reply";
void state(const QString &text) { QMutexLocker lock(&stateMutex); message = text; }
void require(bool valid, const char *why) { if (!valid) throw std::runtime_error(why); }

// GUI-thread-owned feedback geometry remains fixed throughout a request,
// even while animation frames change or the overlay has already disappeared.
QRect feedbackRect;
void feedbackShow(bool,bool=false) {} // Public edition: no gesture animation.

#include "autosend.inc"
#include "liveink.inc"

struct DiaryFramebufferConfig { void *address; int width,height,type,bpl; bool reload; };
QImage memoryCapture() {
    if(!framebuffer_spy$getFramebufferConfig || !framebuffer_spy$refreshFramebuffer)return {};
    auto config=((DiaryFramebufferConfig(*)())framebuffer_spy$getFramebufferConfig)();
    if(!config.address || config.width!=1620 || config.height!=2160 || config.type!=2 || config.bpl<6480 || config.bpl>8192)return {};
    ((void(*)())framebuffer_spy$refreshFramebuffer)();
    // RGB32 deliberately ignores the framebuffer's alpha, exactly as rm-shot does.
    return QImage(static_cast<const uchar*>(config.address),1620,2160,config.bpl,QImage::Format_RGB32).convertToFormat(QImage::Format_RGB888);
}
QImage capture(const QString &path, bool forceLegacy=false) {
    require(diaryGuard(), "Cancelled or page changed");

    if(!forceLegacy && QFile::exists(base+"/fast-capture-verified")) {
        auto image=memoryCapture();if(!image.isNull())return image;
    }
    QFile::remove(path);
    QByteArray arg = (path + ",0,0").toUtf8();
    char *result = rm_shot$screenshotHandler(arg.data());
    bool started = result && QByteArray(result) == "success";
    free(result);
    require(started, "Screenshot failed to start");
    // rm-shot writes asynchronously. Decode rather than accepting file existence.
    for (int i=0; i<500; ++i) {
        require(diaryGuard(), "Cancelled or page changed");
        QThread::msleep(20);
        QImage image(path);
        if (!image.isNull()) return image;
    }
    throw std::runtime_error("Screenshot timed out");
}

// Exclude the native toolbar from question pixels and page-change comparisons.
// Its busy icon can update immediately after a fast trigger, without changing ink.
QImage questionPixels(const QImage &screen) {
    const QRect area=diaryCaptureArea();QImage image=screen.copy(area);
    runOnGuiSync([&]{
        // No overlay to mask in the public edition.
        QPainter painter(&image);
        for(auto object:runtimeObjects()) {
            auto item=qobject_cast<QQuickItem*>(object);
            if(!item || !item->isVisible())continue;
            bool ownButton=item->objectName()=="reply-answer-button" || item->objectName()=="reply-auto-button";
            if(!ownButton && item->objectName()!="toolbarLayout")continue;
            if(!ownButton) {
                auto ancestor=item->parentItem();
                while(ancestor && ancestor->metaObject()->indexOfProperty("expanded")<0)ancestor=ancestor->parentItem();
                if(!ancestor || !ancestor->property("expanded").toBool())continue;
            }
            QRectF rect=item->mapRectToScene(QRectF(0,0,item->width(),item->height()));
            painter.fillRect(rect.translated(-area.x(),-area.y()).adjusted(-4,-4,4,4),Qt::white);
        }
    });return image;
}

QString answer(const QImage &image, const QJsonObject &config, int /*lines*/, DiaryTutorSession &tutor, const std::function<void(const QString&)> &delta={}) {
    QString key = config.value("api_key").toString().trimmed();
    QString model = config.value("model").toString().trimmed();
    require(!key.isEmpty() && !model.isEmpty(), "Set api_key and model in config.json");
    QByteArray png;
    QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly); image.save(&buffer,"PNG");
    QString prompt = QString(R"PROMPT(Read the latest unanswered handwritten question. Ignore prior answered questions and the trailing zigzag send gesture. Ask briefly if handwriting is unclear. Reply in the question's language, directly, without repeating the question, introductions, filler or follow-up offers.
Match length to the task: for a simple who/what/where/when factual lookup, return only the requested name, entity, place, date or value, not a sentence restating the question. For example, "Who invented the telephone?" -> "Alexander Graham Bell."; "Capital of France?" -> "Paris."; "3+3?" -> "6". Do not add titles, biographies, background, or an as-of date unless requested or necessary to resolve ambiguity. Definitions may use one concise sentence. Give brief results for trivial arithmetic; complete worked solutions for maths/physics/engineering, including necessary assumptions, method, steps, units and final result. Explain important steps. Do not shorten a solution to fit the screen. Respect requests for only the final answer. Use plain maths, not LaTeX.
For ordinary prose and worked solutions prefer plain paragraphs separated by blank lines, each containing a complete thought or step, so the tablet can start writing before you finish. Use GitHub Markdown when formatting is actually needed. No HTML or external image links. Tables at most five columns. Code fences only for programming code or explicitly requested raw Markdown, never around an entire answer or table.
Return the schema. command transcribes the latest question with context needed for follow-ups. kind=text for ordinary answers, text contains the answer. Empty unused text/expression; default xmin=-5,xmax=5, template={title:"",count:0,labels:[],columns:[],rows:[],cell_lines:1}, calendar={year:2000,month:1,week_start:7}.
kind=search for explicit search/look-up/online verification/source requests OR changing/current facts (news, weather, prices, schedules, officeholders, recent events). text is the standalone contextual query, not an answer. Never answer current facts from training data. Resolve relative dates using the supplied clock; ask if necessary location/date is missing. Timeless questions and fiction need no search.
kind=image only for explicit picture/comic/manga generation; text is a faithful detailed standalone image prompt including requested panels, characters and dialogue. Questions about pictures use text. Do not invent unreadable details.
kind=plot only when latest request starts with 'plot' and specifies one real y=f(x). expression uses x, decimal numbers, pi,e,+,-,*,/,^,parentheses,sin,cos,tan,sqrt,abs,exp,ln,log; explicit multiplication, radians and natural log. Use requested xmin/xmax or -5/5. Clarify unsupported/unclear equations instead of changing them.
kind=checklist or table for a requested single template: template.count is exact item/body-row count; labels exactly count entries or [] for blank checklist; columns contains headings, rows exactly count cell arrays or [] for blank body. cell_lines=1..10 controls cell height, not row count; default1. No Markdown in template cells. Defaults:10 checklist items,5 pros/cons rows. For task lists in ordinary Markdown use one '- [ ] label' per line; blank items '- [ ]' individually.
kind=calendar for monthly calendars: calendar.year/month, week_start=7 Sunday default or1 Monday. Resolve relative dates from clock, clarify missing dates. template.cell_lines defaults3. App calculates weekdays and dates; never calculate them yourself.
kind=mermaid for flowcharts/sequence diagrams: text is bare source. Only flowchart TD/graph TD with separate node declarations A["Label"], B("Label"), C{"Decision"} then separate edges A --> B or A -->|Yes| B; max12 nodes/24 edges, decision/edge labels max15 chars. Or sequenceDiagram,2-5 'participant A as Name' declarations and max20 A->>B: Message / B-->>A: Reply lines. No subgraphs, styling, directives, HTML, links, loops, notes or activation. Explain unsupported diagram types with kind=text.
kind=venn: text serialized JSON {"sets":["A","B"],"regions":[{"mask":1,"text":"A only"},{"mask":2,"text":"B only"},{"mask":3,"text":"Both"}]}. 2 or3 sets, include all masks1..3 or1..7 exactly once (1=A,2=B,4=C). Empty text for blank regions. Areas do not represent counts. Labels appear in a numbered legend.
)PROMPT");
    const QString clockContext=diaryCurrentContext(QDateTime::currentDateTimeUtc(),config.value("timezone").toString("UTC"));
    QJsonObject payload{{"model",model},{"reasoning",QJsonObject{{"effort","low"}}},{"store",false},{"max_output_tokens",8192},{"instructions",prompt},
        {"input",QJsonArray{QJsonObject{{"role","user"},{"content",QJsonArray{
            QJsonObject{{"type","input_text"},{"text",clockContext+(tutor.enabled ? "Read the latest handwritten contribution and continue this notebook conversation. It may be an answer, drawing, statement or question." : "Read the latest unanswered question on this notebook page.")}},
            QJsonObject{{"type","input_image"},{"detail","high"},
                {"image_url","data:image/png;base64," + QString::fromLatin1(png.toBase64())}}
        }}}}}};
    payload.insert("service_tier",config.value("service_tier").toString("default"));
    payload.insert("text",QJsonObject{{"format",diaryAnswerFormat()}});
    payload.insert("reasoning",QJsonObject{{"effort",config.value("reasoning_effort").toString("low")}});
    tutor.prepare(payload);
    const QString provider=config.value("provider").toString("openai");
    if(provider=="anthropic")payload.insert("instructions",payload.value("instructions").toString()+"\nProvider capability override: No web search or raster-image generation is available in this edition with Claude. For current facts requiring verification, say briefly that web verification requires OpenAI; do not guess. For picture requests explain that raster generation requires OpenAI and offer a supported diagram. Never return kind=search or kind=image. Other supported diagram kinds remain available.");
    auto reply=diaryApiRequest(payload,key,diaryGuard,false,delta,provider);
    auto text=responseText(reply);tutor.apiReply=reply;return text;
}

void work() {
    QElapsedTimer timing;timing.start();
    bool completed=false;
    try {
        QTemporaryDir temp("/tmp/reply-XXXXXX");
        require(temp.isValid(), "Cannot create private temporary directory");
        QFile file(base+"/config.json");
        require(file.open(QIODevice::ReadOnly), "Create reply/config.json first");
        QJsonObject config=QJsonDocument::fromJson(file.readAll()).object();
        require(!config.isEmpty(), "Invalid config.json");
        QString notebookId;
        runOnGuiSync([&]{if(auto document=objectProperty(originView,"document"))notebookId=document->property("id").toString();});
        require(!notebookId.isEmpty(),"Cannot identify notebook for agent routing");
        DiaryTutorSession tutor(base,"/home/root/.local/share/reply",notebookId);
        runOnGuiSync([&]{
            for(auto object:runtimeObjects())if(isVisibleObject(object) && controllerProperty(object,"controller")==originController && QString(object->metaObject()->className()).contains("DeviceSceneView")) {
                inkViewportObject=object;inkViewport=diaryInkMapping(object);break;
            }
        });
        require(!inkViewport.isEmpty(),"Cannot establish page viewport");
        state("Reading page...");
        auto screen=capture(temp.path()+"/question.png");
        qInfo("[reply] Capture ready at %lld ms",timing.elapsed());
        require(screen.size()==diaryCanvasSize(),
                "Paper Pro port requires portrait (1620 x 2160)");
        QSize canvas;
        runOnGuiSync([&] {canvas=activeCanvasSize();});
        require(canvas==screen.size(), "Screen/input coordinates differ; refusing to draw");
        const QRect area=diaryCaptureArea();
        QImage question=questionPixels(screen);
        int last=lastInkRow(question);
        runOnGuiSync([&]{if(!feedbackRect.isEmpty())last=qMax(last,feedbackRect.bottom()-area.y());});
        require(last>=0, "Write a question on a blank page first");
        int top=area.y()+last+45;
        // Native scene images can extend below the current viewport. Render at
        // a fixed readable size, independent of empty space on screen.
        int width=area.width(), available=1825;
        state("Thinking...");
        bool demo=config.value("demo_mode").toBool(false);
        bool pageChecked=false;QString streamRaw,streamedText;QVector<QImage> streamImages;int streamHeight=0;
        auto checkPage=[&] {
            if(pageChecked)return;
            state("Checking page...");
            auto current=capture(temp.path()+"/check.png");auto checked=questionPixels(current);
            if(current.size()!=screen.size() || checked!=question) {
                question.save("/tmp/reply-question-before.png");checked.save("/tmp/reply-question-after.png");
                throw std::runtime_error("Page changed while thinking; tap the wand again");
            }
            require(diaryGuard(),"Cancelled or page changed");pageChecked=true;
            qInfo("[reply] Page checked at %lld ms",timing.elapsed());
        };
        auto renderText=[&](const QString &part) {
            DiaryMarkdownImage rendered;QString error;
            runOnGuiSync([&]{try {
                static int font=QFontDatabase::addApplicationFont(base+"/Caveat.ttf");
                static int mono=QFontDatabase::addApplicationFont(base+"/DejaVuSansMono.ttf");
                require(font>=0&&mono>=0,"Answer fonts missing");
                rendered=renderDiaryMarkdown(part,QFontDatabase::applicationFontFamilies(font).first(),width);
            }catch(const std::exception &e){error=QString::fromUtf8(e.what());}});
            if(!error.isEmpty())throw std::runtime_error(error.toStdString());
            return rendered;
        };
        auto writeChunk=[&](const QString &part) {
            if(part.trimmed().isEmpty())return;
            auto rendered=renderText(part);
            checkPage();require(diaryGuard(),"Cancelled or page changed");
            qInfo("[reply] Streaming ink starts at %lld ms",timing.elapsed());
            state("Writing black ink... tap stop to cancel");
            auto result=writeLiveInk(rendered.image,QPointF(area.x(),top+streamHeight),screen.size());
            if(!result.value("ok").toBool())throw std::runtime_error(result.value("message").toString().toStdString());
            streamImages.append(rendered.image);streamHeight+=rendered.image.height();
        };
        auto streamDelta=[&](const QString &delta) {
            streamRaw+=delta;require(streamRaw.size()<=64000,"Oversized answer stream");
            auto partial=diaryPartialAnswer(streamRaw);
            if(partial.fields.value("kind")!="text" || !partial.fields.contains("command"))return;
            DiaryAnswer check;check.command=partial.fields.value("command");diaryRequireFreshSearch(check);if(check.search)return;
            // Only immutable, plain paragraphs stream early. Markdown tables,
            // code, references and list layouts wait for their complete content.
            int end=partial.text.lastIndexOf("\n\n");if(end<0)return;end+=2;
            if(end<=streamedText.size())return;
            QString prefix=partial.text.left(end);
            if(prefix.contains(QRegularExpression(R"([`|\[\]#*_<>]|(?m)^\s*(?:[-+>] |[0-9]+[.)] ))")))return;
            if(!prefix.startsWith(streamedText))throw std::runtime_error("Answer prefix changed; partial ink may remain");
            writeChunk(prefix.mid(streamedText.size()).trimmed());streamedText=prefix;
        };
        DiaryAnswer resultAnswer;
        if(demo) resultAnswer.text="This is a handwriting test. Live answers will work after you configure an OpenAI API key.";
        else resultAnswer=decodeDiaryAnswer(answer(question.copy(0,0,question.width(),qMin(question.height(),last+32)),config,qMin(9,available/60),tutor,streamDelta));
        if(!demo)diaryRequireFreshSearch(resultAnswer);
        DiaryWebAnswer web;
        if(resultAnswer.search) {
            require(config.value("provider").toString("openai")=="openai","Web search requires OpenAI in this release");
            state("Searching the web...");
            auto searchPayload=diarySearchPayload(resultAnswer.text,config.value("model").toString(),config.value("timezone").toString("UTC"));
            searchPayload.insert("service_tier",config.value("service_tier").toString("default"));
            searchPayload.insert("reasoning",QJsonObject{{"effort",config.value("reasoning_effort").toString("low")}});
            if(tutor.enabled)searchPayload.insert("instructions",searchPayload.value("instructions").toString()+"\nFor this learner, use concise teaching steps, verify resource links before assigning them, and do not claim to have watched a video.\n"+tutor.profile);
            web=parseDiarySearch(diaryApiRequest(searchPayload,
                config.value("api_key").toString(),diaryGuard));
            web.checkedAt=diaryLocalClock(QDateTime::currentDateTimeUtc(),config.value("timezone").toString("UTC"));
            resultAnswer.text=web.text;
        }
        qInfo("[reply] Answer ready at %lld ms",timing.elapsed());
        QString text=resultAnswer.text;
        // Keep citation metadata in the saved HTML, not in the handwritten answer.
        if(resultAnswer.search) {
            for(int i=0;i<web.sources.size();++i)text.replace(QString("[%1]").arg(i+1),QString());
            text=text.trimmed();
        }
        QImage ink; QString renderError, renderedHtml;
        bool streamed=!streamedText.isEmpty();
        if(streamed) {
            require(!resultAnswer.search && text.startsWith(streamedText.trimmed()),"Answer changed after streaming; partial ink may remain");
            QString remainder=text.mid(qMin(text.size(),streamedText.size()));
            writeChunk(remainder);
            ink=QImage(width,streamHeight,QImage::Format_ARGB32_Premultiplied);ink.fill(Qt::transparent);
            QPainter painter(&ink);int y=0;for(const auto &part:streamImages){painter.drawImage(0,y,part);y+=part.height();}painter.end();
            renderedHtml=renderText(text).html;
        }
        if(resultAnswer.image) {
            require(config.value("provider").toString("openai")=="openai","Image generation requires OpenAI in this release");
            state("Generating grayscale image...");
            ink=diaryDecodeImage(diaryApiRequest(diaryImagePayload(text,config),config.value("api_key").toString(),diaryGuard,true),width);
        }
        runOnGuiSync([&] {
            if(resultAnswer.image || streamed)return;
            if(!resultAnswer.diagram.isEmpty()) {
                try {ink=renderDiaryDiagram(resultAnswer.diagram,text,width);}
                catch(const std::exception &error){renderError=QString::fromUtf8(error.what());}
                return;
            }
            if(resultAnswer.plot) {
                try {ink=renderPlot(resultAnswer,width,available);}
                catch(const std::exception &error){renderError=QString::fromUtf8(error.what());}
                return;
            }
            static int fontId=QFontDatabase::addApplicationFont(base+"/Caveat.ttf");
            static int monoId=QFontDatabase::addApplicationFont(base+"/DejaVuSansMono.ttf");
            if(monoId<0) {renderError="Code font missing; reinstall Ink Diary";return;}
            if(fontId<0) {renderError="Caveat font missing";return;}
            try {
                auto rendered=renderDiaryMarkdown(text,QFontDatabase::applicationFontFamilies(fontId).first(),width,
                    QString(),QStringList(),resultAnswer.cellLines);
                ink=rendered.image;renderedHtml=resultAnswer.search?diarySourcesHtml(web):rendered.html;
            } catch(const std::exception &error) {renderError=QString::fromUtf8(error.what());}
        });
        if(!renderError.isEmpty()) throw std::runtime_error(renderError.toStdString());
        qInfo("[reply] Layout ready at %lld ms",timing.elapsed());
        checkPage();
        require(diaryGuard(), "Cancelled or page changed");
        const QString answerDir="/home/root/.local/share/reply/answers";
        require(QDir().mkpath(answerDir), "Cannot create answer image directory");
        QFile::setPermissions(answerDir,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);
        QString imagePath=answerDir+"/"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".png";
        require(ink.save(imagePath), "Cannot save answer image");
        QFile::setPermissions(imagePath,QFileDevice::ReadOwner|QFileDevice::WriteOwner);
        QString htmlPath=imagePath+".html";
        if(!renderedHtml.isEmpty()) {
            QFile html(htmlPath);
            require(html.open(QIODevice::WriteOnly),"Cannot save source links");
            html.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
            QByteArray bytes=renderedHtml.toUtf8();
            require(html.write(bytes)==bytes.size(),"Cannot save source links");
        }
        QString sourcePath=imagePath+(resultAnswer.image || !resultAnswer.diagram.isEmpty() ? ".source.txt" : ".md");
        if(!resultAnswer.plot) {
            QFile raw(sourcePath);
            require(raw.open(QIODevice::WriteOnly),"Cannot save answer Markdown");
            raw.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
            auto bytes=text.toUtf8();require(raw.write(bytes)==bytes.size(),"Cannot save answer Markdown");
        }
        require(diaryGuard(), "Cancelled or page changed");
        QJsonObject result;
        bool live=!resultAnswer.image && resultAnswer.diagram.isEmpty() && !resultAnswer.plot;
        if(streamed) result=QJsonObject{{"ok",true}};
        else if(live) {
            qInfo("[reply] Writing starts at %lld ms",timing.elapsed());
            state("Writing black ink... tap stop to cancel");
            result=writeLiveInk(ink,QPointF(area.x(),top),screen.size());
        } else {
        state("Inserting image...");
        result=insertSceneImage(QJsonObject{{"path",imagePath},{"coordinateSpace","screen"},
            {"x",area.x()+width/2.0},{"y",top+ink.height()/2.0},{"appendBelowContent",true}});
        }
        if(!result.value("ok").toBool()) {
            QFile::remove(imagePath);
            QFile::remove(htmlPath);
            QFile::remove(sourcePath);
            throw std::runtime_error(result.value("message").toString().toStdString());
        }
        completed=true;
        try {tutor.commit(resultAnswer.command,text,resultAnswer.image?"image":resultAnswer.search?"search":resultAnswer.plot?"plot":!resultAnswer.diagram.isEmpty()?"diagram":"text");}
        catch(const std::exception &e){qWarning("[reply] Tutor memory save failed: %s",e.what());state("Answer written; tutor memory could not be saved");throw;}
        qInfo("[reply] Finished at %lld ms",timing.elapsed());
        state(live ? "Answer written in black ink" : resultAnswer.image ? "Grayscale image added; scroll down if needed" : !resultAnswer.diagram.isEmpty() ? "Diagram added; scroll down if needed" : resultAnswer.plot ? "Plot added; scroll down if needed" : resultAnswer.search ? (web.sources.isEmpty() ? "Web answer added; source links unavailable" : "Web answer + sources added") : "Answer added; scroll down if needed");

    } catch(const std::exception &error) { state(continuedWriting ? "Writing continued; finish with a zigzag" : QString::fromUtf8(error.what())); }
    feedbackShow(false,completed);
    busy=false;
}
}

static bool diaryCancellationRequested() { return cancelled.load(); }

static bool diaryGuard() {
    if(cancelled) return false;
    bool valid=false;
    runOnGuiSync([&] {
        auto now=findActivePage();
        valid=originController && originView && now.controller==originController.data()
            && now.documentView==originView.data() && now.pageId==originPage
            && !originView->property("subDialogOpen").toBool()
            && !originView->property("inSuspend").toBool();
        if(valid && !inkViewport.isEmpty())valid=inkViewportObject && diaryInkMapping(inkViewportObject)==inkViewport;
    });
    if(!valid) cancelled=true;
    return valid;
}
extern "C" char *diary_start(const char *arg) {
    ++gestureGeneration;
    if(busy.exchange(true)) return response(QJsonObject{{"ok",false},{"message","Already busy"}});
    feedbackShow(false);
    if(!arg || QByteArray(arg)!="gesture")runOnGuiSync([]{feedbackRect={};});
    cancelled=false;continuedWriting=false;
    runOnGuiSync([]{inkViewportObject.clear();inkViewport.clear();inkScrollPixels=0;});
    bool valid=false;
    runOnGuiSync([&] {
        auto context=findActivePage();
        originController=context.controller; originView=context.documentView; originPage=context.pageId;
        valid=originController && originView && !originPage.isEmpty();
    });
    if(!valid) {busy=false; state("Open a notebook page first"); return response(QJsonObject{{"ok",false}});}
    feedbackShow(true);
    state("Starting...");
    std::thread(work).detach();
    return response(QJsonObject{{"ok",true}});
}
extern "C" char *diary_cancel(const char *) { ++gestureGeneration; cancelled=true; feedbackShow(false); return response(QJsonObject{{"ok",true}}); }
extern "C" char *diary_status(const char *) {
    startPenMonitor();
    QFile config(base+"/config.json");
    bool demo=config.open(QIODevice::ReadOnly) && QJsonDocument::fromJson(config.readAll()).object().value("demo_mode").toBool();
    QMutexLocker lock(&stateMutex);
    return response(QJsonObject{{"busy",busy.load()},{"autoSend",autoSend.load()},{"monitorReady",monitorReady.load()},{"autoDiagnostic",autoDiagnostic()},{"message",message=="ChatGPT" && demo ? "ChatGPT (demo)" : message}});
}
extern "C" char *diary_auto(const char *) {
    startPenMonitor();++gestureGeneration;autoSend=!autoSend.load();
    QSaveFile file(base+"/interaction.json");
    if(file.open(QIODevice::WriteOnly)) {
        file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
        file.write(QJsonDocument(QJsonObject{{"auto_send",autoSend.load()}}).toJson());file.commit();
    }
    return response(QJsonObject{{"autoSend",autoSend.load()}});
}
extern "C" char _xovi_shouldLoad() {
    if (QSysInfo::currentCpuArchitecture() != "arm64") { qWarning() << "[reply] Unsupported CPU" << QSysInfo::currentCpuArchitecture(); return false; }
    QFile hardware("/sys/firmware/devicetree/base/model");
    if (!hardware.open(QIODevice::ReadOnly) || !hardware.readAll().startsWith("reMarkable Ferrari")) { qWarning("[reply] Hardware identity check failed"); return false; }
    QFile version("/etc/os-release");
    if (!version.open(QIODevice::ReadOnly)) { qWarning("[reply] Cannot read firmware identity"); return false; }
    const auto firmwareLines=version.readAll().split('\n');
    if (!firmwareLines.contains("IMG_VERSION=\"3.29.0.153\"") && !firmwareLines.contains("IMG_VERSION=\"3.29.1.0\"")) { qWarning("[reply] Firmware identity check failed"); return false; }
    const bool supported = dlsym(RTLD_DEFAULT,"_Z21qRegisterResourceDataiPKhS0_S0_")!=nullptr;
    qInfo("[reply] Paper Pro runtime check: Qt resources %s", supported ? "available" : "unavailable");
    return supported;
}
extern "C" char *diary_probe(const char *arg) {
    if(arg && QByteArray(arg)=="capture-check") {
        if(busy.exchange(true))return response(failure("busy","Already busy"));
        cancelled=false;
        runOnGuiSync([&]{auto context=findActivePage();originController=context.controller;originView=context.documentView;originPage=context.pageId;inkViewportObject.clear();inkViewport.clear();});
        std::thread([]{try {
            QTemporaryDir temp;QElapsedTimer timer;timer.start();
            auto direct=memoryCapture();auto directMs=timer.elapsed();
            auto legacy=capture(temp.path()+"/reference.png",true);auto legacyMs=timer.elapsed()-directMs;
            auto after=memoryCapture();
            bool matches=!direct.isNull() && questionPixels(direct)==questionPixels(legacy.convertToFormat(QImage::Format_RGB888)) && questionPixels(after)==questionPixels(legacy.convertToFormat(QImage::Format_RGB888));
            direct.save("/tmp/reply-memory.png");legacy.save("/tmp/reply-reference.png");
            if(matches){QFile marker(base+"/fast-capture-verified");require(marker.open(QIODevice::WriteOnly),"Cannot enable verified capture");marker.write("3.29.0.153\n");}
            else QFile::remove(base+"/fast-capture-verified");
            QString result=QString("Capture comparison: %1; memory %2 ms, PNG %3 ms").arg(matches?"MATCH":"MISMATCH").arg(directMs).arg(legacyMs);
            qInfo().noquote()<<"[reply]"<<result;state(result);
        }catch(const std::exception &e){state(QString::fromUtf8(e.what()));}busy=false;}).detach();
        return response(QJsonObject{{"ok",true}});
    }

    if(arg && QByteArray(arg)=="tutor-check") {
        if(busy.exchange(true))return response(failure("busy","Already busy"));
        std::thread([]{QJsonObject report;try {
            QFile f(base+"/config.json");require(f.open(QIODevice::ReadOnly),"Missing configuration");auto config=QJsonDocument::fromJson(f.readAll()).object();
            QTemporaryDir temp;require(temp.isValid(),"Missing test folder");
            auto turn=[&](const QString &message,bool compact) {
                DiaryTutorSession session(base,temp.path()+"/session");require(session.enabled,"Tutor profile not installed");
                QJsonObject p{{"model",config.value("model")},{"store",false},{"service_tier","default"},{"reasoning",QJsonObject{{"effort","low"}}},{"max_output_tokens",8192},{"instructions","Return the required rendering schema. Use kind=text; text is the short tutor reply. command transcribes the learner message. Empty unused strings and arrays; template.count=0, cell_lines=1; xmin=-5,xmax=5; calendar year=2000,month=1,week_start=7. Do not repeat the learner message in text."},{"input",QJsonArray{QJsonObject{{"role","user"},{"content",message}}}}};
                p.insert("text",QJsonObject{{"format",diaryAnswerFormat()}});
                session.prepare(p);if(compact)p.insert("context_management",QJsonArray{QJsonObject{{"type","compaction"},{"compact_threshold",2000}}});
                auto r=diaryApiRequest(p,config.value("api_key").toString(),[]{return true;});
                report.insert("lastStatus",r.value("status"));report.insert("incompleteDetails",r.value("incomplete_details"));report.insert("lastUsage",r.value("usage"));
                auto text=decodeDiaryAnswer(responseText(r)).text;session.apiReply=r;session.commit(message,text,"text");
                return qMakePair(text,r);
            };
            auto first=turn("For this test remember my check word: orbit-seven. Start our physics course with one short diagnostic question. Do not answer it.",false);
            report.insert("diagnostic",first.first);
            QString filler;for(int i=0;i<500;++i)filler+="Irrelevant scratch record "+QString::number(i)+": no learning evidence, discard when summarizing.\n";
            auto second=turn("The following is test padding, not a learner contribution. Do not transcribe it into command.\n"+filler+"\nLatest learner contribution (transcribe only this): Reply only Ready.",true);
            bool compacted=false;for(auto item:second.second.value("output").toArray())if(item.toObject().value("type")=="compaction")compacted=true;
            report.insert("compactionReturned",compacted);
            auto third=turn("What was my check word? Give only that word.",false);
            report.insert("recall",third.first);report.insert("memoryPassed",third.first.contains("orbit-seven",Qt::CaseInsensitive));
            report.insert("ok",compacted&&third.first.contains("orbit-seven",Qt::CaseInsensitive));
        }catch(const std::exception &e){report.insert("error",QString::fromUtf8(e.what()));report.insert("ok",false);}
        try{diaryPrivateSave("/tmp/reply-tutor-check.json",QJsonDocument(report).toJson());}catch(...){}
        state(report.value("ok").toBool()?"Tutor check passed":"Tutor check failed; inspect diagnostic file");busy=false;
        }).detach();return response(QJsonObject{{"ok",true}});
    }
    if(arg && QByteArray(arg)=="new-page-test") {
        if(busy.exchange(true))return response(failure("busy","Already busy"));
        cancelled=false;++gestureGeneration;
        runOnGuiSync([&]{auto context=findActivePage();originController=context.controller;originView=context.documentView;originPage=context.pageId;inkViewportObject.clear();inkViewport.clear();inkScrollPixels=0;});
        std::thread([]{try {
            drawingInk=true;
            require(diaryContinueOnNewPage(220),"Native new-page test failed");
            QImage ink;runOnGuiSync([&]{int id=QFontDatabase::addApplicationFont(base+"/Caveat.ttf");ink=renderDiaryMarkdown("Continued on a new page.",QFontDatabase::applicationFontFamilies(id).first(),1200).image;});
            auto r=writeLiveInk(ink,QPointF(174,220),diaryCanvasSize());
            require(r.value("ok").toBool(),"New-page ink failed");state("New-page test passed");
        }catch(const std::exception &e){state(QString::fromUtf8(e.what()));}
        syntheticInkUntil=QDateTime::currentMSecsSinceEpoch()+400;drawingInk=false;busy=false;
        }).detach();return response(QJsonObject{{"ok",true}});
    }
    if(arg && QByteArray(arg)=="scroll-info") {
        QJsonObject out;QJsonArray objects;
        runOnGuiSync([&]{for(auto obj:runtimeObjects()) {
            QString cls=obj->metaObject()->className();
            if(!(cls.contains("DeviceSceneView") || cls=="SceneView" || cls.startsWith("Navigation_QML")))continue;
            QJsonArray methods;QJsonObject props;
            auto meta=obj->metaObject();
            for(int i=0;i<meta->methodCount();++i)methods.append(QString::fromLatin1(meta->method(i).methodSignature()));
            for(int i=0;i<meta->propertyCount();++i){auto p=meta->property(i);props.insert(p.name(),QString(p.typeName())+" writable="+QString::number(p.isWritable())+" value="+p.read(obj).toString());}
            objects.append(QJsonObject{{"class",cls},{"methods",methods},{"props",props}});
        }});
        out.insert("objects",objects);
        for(const char *path:{":/qml/device/view/documentview/DeviceSceneView.qml",":/qml/device/view/documentview/Navigation.qml",":/qml/device/view/documentview/DocumentView.qml",":/qml/device/view/documentview/PagesActions.qml"}){QFile f(path);if(f.open(QIODevice::ReadOnly))out.insert(path,QString::fromUtf8(f.readAll()));}
        return response(out);
    }
    if(arg && QByteArray(arg)=="toolbar-layout") {
        QJsonObject out;
        for(const char *path:{":/qt/qml/xofm/libs/toolbar/qml/Toolbar.qml",":/ark-imports/ark/controls/Toolbar.qml"}) {
            QFile f(QString::fromUtf8(path));if(f.open(QIODevice::ReadOnly))out.insert(QString::fromUtf8(path),QString::fromUtf8(f.readAll()));
        }
        return response(out);
    }

    if(arg && QByteArray(arg)=="line-types") {
        QJsonArray types;
        runOnGuiSync([&]{for(const char *name:{"Line","PenPoint","QList<PenPoint>","ScenePenInputHandler","Line::Point"}) {
            auto type=QMetaType::fromName(name);QJsonObject entry{{"type",name},{"valid",type.isValid()},{"size",int(type.sizeOf())}};
            if(auto meta=type.metaObject()) {
                QJsonArray props,methods;
                for(int i=0;i<meta->propertyCount();++i){auto p=meta->property(i);props.append(QString(p.name())+":"+p.typeName()+":"+QString::number(p.isWritable()));}
                for(int i=0;i<meta->methodCount();++i)methods.append(QString::fromLatin1(meta->method(i).methodSignature()));
                entry.insert("properties",props);entry.insert("methods",methods);
            }types.append(entry);
        }});return response(QJsonObject{{"types",types}});
    }
    if(arg && QByteArray(arg)=="ink-test") {
        if(busy.exchange(true))return response(failure("busy","Already busy"));
        cancelled=false;++gestureGeneration;
        runOnGuiSync([&]{auto context=findActivePage();originController=context.controller;originView=context.documentView;originPage=context.pageId;});
        std::thread([]{
            try {
                QTemporaryDir temp;auto screenshot=capture(temp.path()+"/blank.png");
                require(screenshot.size()==diaryCanvasSize(),"Portrait required");
                int top=qMax(250,lastInkRow(screenshot.copy(diaryCaptureArea()))+45);
                QImage ink;runOnGuiSync([&]{int id=QFontDatabase::addApplicationFont(base+"/Caveat.ttf");
                    ink=renderDiaryMarkdown("Alexander Graham Bell.\n\nThe answer is 42.",QFontDatabase::applicationFontFamilies(id).first(),900).image;});
                require(top+ink.height()<1930,"Scroll to empty space or open a blank page for the Fineliner sample");
                state("Writing medium Fineliner sample...");auto result=writeLiveInk(ink,QPointF(250,top),screenshot.size());
                state(result.value("ok").toBool()?QString("Medium Fineliner sample written (%1 ms drawing)").arg(result.value("elapsedMs").toInt()):result.value("message").toString());
            }catch(const std::exception &e){state(QString::fromUtf8(e.what()));}busy=false;
        }).detach();return response(QJsonObject{{"ok",true}});
    }
    if(arg && QByteArray(arg)=="tools") {
        QJsonArray objects;
        runOnGuiSync([&] {
            auto inspectObjects=runtimeObjects();
            for(auto obj:QList<QObject *>(inspectObjects))for(const char *key:{"sceneController","activePen","primaryPen","secondaryPen","penHandler","penInput"})if(auto ref=objectProperty(obj,key))if(!inspectObjects.contains(ref))inspectObjects.append(ref);
            for(auto obj:inspectObjects) {
                QString cls=obj->metaObject()->className();
                if(!cls.contains("Scene") && !cls.contains("Tool") && !cls.contains("DocumentView") && !cls.contains("Pen"))continue;
                QJsonObject props; QJsonArray methods;
                auto meta=obj->metaObject();
                for(int i=0;i<meta->propertyCount();++i) {
                    auto prop=meta->property(i);QString name=prop.name();
                    if(cls=="PenSetting" || cls=="ScenePenInputHandler" || cls=="PenInput" || name.contains("color",Qt::CaseInsensitive)||name.contains("tool",Qt::CaseInsensitive)||name.contains("pen",Qt::CaseInsensitive)||name.contains("zoom",Qt::CaseInsensitive)||name.contains("transform",Qt::CaseInsensitive)) {
                        auto v=prop.read(obj);props.insert(name,QString::fromLatin1(prop.typeName())+"="+v.toString()+" writable="+QString::number(prop.isWritable()));
                    }
                }
                for(int i=meta->methodOffset();i<meta->methodCount();++i)methods.append(QString::fromLatin1(meta->method(i).methodSignature()));
                if(!props.isEmpty() || cls.contains("SceneController"))objects.append(QJsonObject{{"class",cls},{"visible",isVisibleObject(obj)},{"props",props},{"methods",methods}});
            }
        });return response(QJsonObject{{"objects",objects}});
    }
    if(arg && QByteArray(arg)=="checklist") {
        QJsonObject check;
        runOnGuiSync([&] {
            try {
                int id=QFontDatabase::addApplicationFont(base+"/Caveat.ttf");
                QString text;for(int i=0;i<10;++i)text+="- [ ]\n";
                auto rendered=renderDiaryMarkdown(text,QFontDatabase::applicationFontFamilies(id).first(),1174);
                check.insert("checkboxes",rendered.html.count("class=\"unchecked\""));
                check.insert("height",rendered.image.height());
            } catch(const std::exception &e){check.insert("error",QString::fromUtf8(e.what()));}
        });
        return response(check);
    }
    auto result=capabilities();
    bool imageSupport=false,mapping=false;
    runOnGuiSync([&] {
        auto context=findActivePage();
        if(!context.controller)return;
        for(int i=0;i<context.controller->metaObject()->methodCount();++i)
            if(context.controller->metaObject()->method(i).name()=="insertImageFileAsSceneItem")imageSupport=true;
        for(auto object:runtimeObjects()) {
            if(!qobject_cast<QQuickItem *>(object) || !isVisibleObject(object) || controllerProperty(object,"controller")!=context.controller)continue;
            for(int i=0;i<object->metaObject()->methodCount();++i)
                if(object->metaObject()->method(i).name()=="viewToScene")mapping=true;
        }
    });
    result.insert("nativeImageInsertAvailable",imageSupport);result.insert("screenMappingAvailable",mapping);
    return response(result);
}
