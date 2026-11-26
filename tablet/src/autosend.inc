// Included inside diary.cpp's anonymous namespace, after its state helpers.
std::atomic<bool> autoSend{true}, monitorReady{false}, drawingInk{false};
std::atomic<unsigned long> gestureGeneration{0};
std::atomic<bool> continuedWriting{false};
std::atomic<qint64> syntheticInkUntil{0};
std::once_flag monitorOnce;
QMutex autoInfoMutex;
QString autoInfo="Waiting for a zigzag";
void autoNote(const QString &note) { QMutexLocker lock(&autoInfoMutex);autoInfo=note; }
QString autoDiagnostic() { QMutexLocker lock(&autoInfoMutex);return autoInfo; }

void monitorPen() {
    int fd=::open("/dev/input/event2",O_RDONLY|O_NONBLOCK|O_CLOEXEC);
    int touch=::open("/dev/input/event3",O_RDONLY|O_NONBLOCK|O_CLOEXEC);
    input_absinfo ax{},ay{};char name[128]{};
    if(fd<0 || ioctl(fd,EVIOCGNAME(sizeof(name)),name)<0 || QByteArray(name)!="Elan marker input" ||
       ioctl(fd,EVIOCGABS(ABS_X),&ax)<0 || ioctl(fd,EVIOCGABS(ABS_Y),&ay)<0 || ax.maximum<=ax.minimum || ay.maximum<=ay.minimum) {
        if(fd>=0)::close(fd);
        if(touch>=0)::close(touch);
        return;
    }
    monitorReady=true;
    QElapsedTimer clock;clock.start();
    int x=0,y=0;bool down=false,wasDown=false,eraser=false,penNear=false;
    QRectF acceptedBounds;
    DiaryPenAwayGate penAway;
    QPolygonF stroke; qint64 lastPenAt=0; bool pendingGesture=false,invalidStroke=false;
    QString page;QPointer<QObject> controller,view;
    unsigned long generation=gestureGeneration.load();
    auto clear=[&]{stroke.clear();pendingGesture=false;};
    for(;;) {
        pollfd fds[2]={{fd,POLLIN,0},{touch,POLLIN,0}};
        if(::poll(fds,touch>=0?2:1,10)<0 && errno!=EINTR)break;
        if(generation!=gestureGeneration.load() || !autoSend) {clear();generation=gestureGeneration.load();}
        input_event events[128];ssize_t bytes;
        if(fds[0].revents&POLLIN)while((bytes=::read(fd,events,sizeof(events)))>0) {
            for(int i=0;i<bytes/int(sizeof(input_event));++i) {
                auto e=events[i];
                if(e.type==EV_SYN && e.code==SYN_DROPPED){clear();down=wasDown=false;continue;}
                if(e.type==EV_ABS && e.code==ABS_X)x=e.value;
                if(e.type==EV_ABS && e.code==ABS_Y)y=e.value;
                if(e.type==EV_KEY && e.code==BTN_TOOL_PEN)penNear=e.value;
                if(penNear || down)lastPenAt=clock.elapsed();
                if(e.type==EV_KEY && e.code==BTN_TOUCH)down=e.value;
                if(e.type==EV_KEY && e.code==BTN_TOOL_RUBBER)eraser=e.value;
                penAway.update(clock.elapsed(),penNear,down);
                if(e.type!=EV_SYN || e.code!=SYN_REPORT)continue;
                if(!drawingInk && QDateTime::currentMSecsSinceEpoch()>=syntheticInkUntil.load() && down && !wasDown && busy) {
                    continuedWriting=true;cancelled=true;
                    autoNote("Writing continued; waiting for final zigzag");
                    qInfo("[reply] Continued writing cancelled early request");
                }
                if(drawingInk || QDateTime::currentMSecsSinceEpoch()<syntheticInkUntil.load() || !autoSend || eraser){clear();wasDown=down;continue;}
                QPointF point((x-ax.minimum)*1620.0/(ax.maximum-ax.minimum),(y-ay.minimum)*2160.0/(ay.maximum-ay.minimum));
                if(down && !wasDown) {
                    pendingGesture=false;stroke.clear();invalidStroke=false;
                    runOnGuiSync([&]{auto context=findActivePage();
                        page=context.pageId;controller=context.controller;view=context.documentView;
                    });
                }
                if(down) {
                    if(!QRectF(30,30,1560,1980).contains(point) || stroke.size()>5000){invalidStroke=true;clear();}
                    else if(!invalidStroke && (stroke.isEmpty() || QLineF(stroke.last(),point).length()>=.8))stroke<<point;
                }
                if(!down && wasDown && !stroke.isEmpty()) {
                    QElapsedTimer detection;detection.start();
                    bool match=!invalidStroke && diaryZigzag(stroke);
                    if(match) {
                        acceptedBounds=stroke.boundingRect();
                        pendingGesture=true;autoNote("Zigzag candidate; waiting for pen away");
                        qInfo("[reply] Zigzag detected in %.3f ms",detection.nsecsElapsed()/1000000.);
                    } else autoNote("Waiting for a zigzag");
                    stroke.clear();
                }
                wasDown=down;
            }
        }
        // Ignore palm motion and contacts already resting on the display.
        // Only a fresh touch while the marker is away cancels the gesture.
        if(touch>=0 && (fds[1].revents&POLLIN)) {
            while((bytes=::read(touch,events,sizeof(events)))>0)
                for(int i=0;i<bytes/int(sizeof(input_event));++i) {
                    if(events[i].type==EV_KEY && events[i].code==BTN_TOUCH && events[i].value==1 && !penNear && !down && clock.elapsed()-lastPenAt>500) {
                        if(busy){cancelled=true;qInfo("[reply] Cancelled by deliberate touch with marker away");}
                        if(pendingGesture)autoNote("Pending send cancelled by touch");
                        clear();
                    }
                }
        }
        if(pendingGesture && penAway.ready(clock.elapsed()) && !penNear && !down && !busy && autoSend) {
            pendingGesture=false;bool valid=false;
            runOnGuiSync([&]{auto context=findActivePage();valid=controller && view && context.controller==controller && context.documentView==view && context.pageId==page
                && !view->property("subDialogOpen").toBool() && !view->property("inSuspend").toBool();});
            if(valid && generation==gestureGeneration.load()) {autoNote("Auto-send triggered");qInfo("[reply] Zigzag auto-send triggered");char *result=diary_start("gesture");free(result);}
            else autoNote("Pending send cancelled by page change");
            clear();
        }
    }
    monitorReady=false;::close(fd);if(touch>=0)::close(touch);
}
void startPenMonitor() {
    std::call_once(monitorOnce,[]{
        QFile preferences(base+"/interaction.json");
        if(preferences.open(QIODevice::ReadOnly))autoSend=QJsonDocument::fromJson(preferences.readAll()).object().value("auto_send").toBool(true);
        std::thread(monitorPen).detach();
    });
}
