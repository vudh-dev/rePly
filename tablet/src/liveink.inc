QPointer<QObject> inkViewportObject;
QVariantList inkViewport;
double inkScrollPixels=0; // worker-owned, cumulative screen-pixel displacement for this answer

QVariantList diaryInkMapping(QObject *view) {
    if(!view)return {};
    QVariant a,b;
    if(!QMetaObject::invokeMethod(view,"viewToScene",Q_RETURN_ARG(QVariant,a),Q_ARG(QVariant,QVariant::fromValue(QPointF(0,0)))) ||
       !QMetaObject::invokeMethod(view,"viewToScene",Q_RETURN_ARG(QVariant,b),Q_ARG(QVariant,QVariant::fromValue(QPointF(100,100)))) ||
       !a.canConvert<QPointF>() || !b.canConvert<QPointF>())return {};
    return {a,b};
}

// Use the notebook's own add-page path. Do not edit .rm/.content files.
bool diaryContinueOnNewPage(double nextInkY) {
    if(!diaryGuard())return false;
    QPointer<QObject> view=originView,document;
    QString documentId,oldPage=originPage;int count=-1,index=-1;bool requested=false;
    runOnGuiSync([&]{
        if(!view || !view->property("notePage").toBool())return;
        document=objectProperty(view,"document");if(!document)return;
        documentId=document->property("id").toString();
        count=view->property("pageCount").toInt();index=view->property("currentPage").toInt()+1;
        if(count<1||index<1||index>count||view->property("addingPage").toBool())return;
        requested=QMetaObject::invokeMethod(view,"addPage",Q_ARG(QVariant,view->property("document")),Q_ARG(QVariant,QVariant(index)));
    });
    if(!requested)return false;
    QElapsedTimer timer;timer.start();QVariantList candidateMapping;QPointer<QObject> candidateView; qint64 stableSince=0;
    while(timer.elapsed()<8000&&!cancelled) {
        bool ready=false,wrong=false;
        runOnGuiSync([&]{
            if(!view||!document||objectProperty(view,"document")!=document||document->property("id").toString()!=documentId||view->property("subDialogOpen").toBool()||view->property("inSuspend").toBool()){wrong=true;return;}
            auto now=findActivePage();
            if(now.documentView!=view){wrong=true;return;}
            if(now.pageId==oldPage||now.pageId.isEmpty()||!now.controller)return;
            if(view->property("currentPage").toInt()!=index||view->property("pageCount").toInt()!=count+1){wrong=true;return;}
            if(!now.controller->property("itemsBoundingRect").toRectF().isEmpty()){wrong=true;return;}
            for(auto object:runtimeObjects())if(isVisibleObject(object)&&controllerProperty(object,"controller")==now.controller&&QString(object->metaObject()->className()).contains("DeviceSceneView")) {
                auto mapping=diaryInkMapping(object);if(mapping.size()!=2)continue;
                if(view->property("isLoading").toBool()||candidateView!=object||candidateMapping!=mapping){candidateView=object;candidateMapping=mapping;stableSince=timer.elapsed();return;}
                if(timer.elapsed()-stableSince<250)return;
                originController=now.controller;originPage=now.pageId;inkViewportObject=object;inkViewport=mapping;
                inkScrollPixels=nextInkY-220.;ready=true;break;
            }
        });
        if(wrong){cancelled=true;return false;}
        if(ready){feedbackShow(false);qInfo("[reply] Continued on new native notebook page");QThread::msleep(150);return diaryGuard();}
        QThread::msleep(40);
    }
    cancelled=true;return false;
}

struct DiaryPenScope {
    QPointer<QObject> settings,pen,controller;
    QVariant tool,color,thickness,active,forceMove;
    bool changed=false;
    ~DiaryPenScope() {
        if(!changed)return;
        runOnGuiSync([&]{
            // DocumentToolSettings is reused when opening another notebook.
            // Never apply the old notebook's settings to a new document.
            if(!settings || !pen || findActivePage().controller!=controller)return;
            pen->setProperty("tool",tool);pen->setProperty("color",color);pen->setProperty("thickness",thickness);
            settings->setProperty("activeTool",active);settings->setProperty("forceMoveTool",forceMove);
        });
    }
    bool selectBlack() {
        bool ok=false;
        runOnGuiSync([&]{
            auto context=findActivePage();if(!context.controller)return;
            controller=context.controller;
            for(auto object:runtimeObjects())if(QByteArray(object->metaObject()->className())=="DocumentToolSettings") {settings=object;break;}
            if(!settings)return;
            pen=objectProperty(settings,"primaryPen");if(!pen)return;
            auto meta=pen->metaObject();
            auto tp=meta->property(meta->indexOfProperty("tool"));
            auto cp=meta->property(meta->indexOfProperty("color"));
            bool toolFound=false,colorFound=false;
            int black=cp.enumerator().keyToValue("Black",&colorFound);
            int fineliner=tp.enumerator().keyToValue("Finelinerv2",&toolFound);
            if(!toolFound || !colorFound)return;
            tool=pen->property("tool");color=pen->property("color");thickness=pen->property("thickness");
            active=settings->property("activeTool");forceMove=settings->property("forceMoveTool");changed=true;
            ok=settings->setProperty("activeTool","primary") && settings->setProperty("forceMoveTool",false)
                && pen->setProperty("tool",fineliner) && pen->setProperty("color",black) && pen->setProperty("thickness",2.0);
            ok=ok && pen->property("color").toInt()==black && pen->property("tool").toInt()==fineliner && pen->property("thickness").toDouble()==2.0;
        });return ok;
    }
};
QJsonObject writeLiveInk(const QImage &image,const QPointF &offset,const QSize &canvas) {
    auto paths=letterPaths(image);
    if(!diaryGuard())return failure("cancelled","Cancelled before writing");
    bool stableViewport=false;
    runOnGuiSync([&]{
        auto context=findActivePage();
        for(auto object:runtimeObjects())if(isVisibleObject(object) && controllerProperty(object,"controller")==context.controller && QString(object->metaObject()->className()).contains("DeviceSceneView")) {
            inkViewportObject=object;inkViewport=diaryInkMapping(object);stableViewport=!inkViewport.isEmpty();break;
        }
    });
    if(!stableViewport)return failure("viewport","Cannot establish the live-ink viewport");
    DiaryPenScope pen;
    if(!pen.selectBlack())return failure("pen-settings","Cannot select black medium Fineliner ink on this page");
    drawingInk=true;
    struct Reset {~Reset(){syntheticInkUntil=QDateTime::currentMSecsSinceEpoch()+400;drawingInk=false;}} reset;
    QThread::msleep(200);
    auto cuts=diaryInkCuts(image);
    for(size_t band=1;band<cuts.size();++band) {
        auto section=diaryInkBand(paths,cuts[band-1],cuts[band]);
        if(section.empty())continue;
        double top=1e9,bottom=-1e9;
        for(const auto &path:section)for(auto point:path){top=std::min(top,point.y());bottom=std::max(bottom,point.y());}
        for(int attempt=0;offset.y()+bottom-inkScrollPixels>=1930 && attempt<12;++attempt) {
            if(!diaryGuard())return failure("cancelled","Cancelled before scrolling");
            bool moved=false,atEnd=false;
            runOnGuiSync([&]{
                auto view=inkViewportObject.data();if(!view)return;
                auto before=diaryInkMapping(view);
                if(before!=inkViewport || before.size()!=2)return;
                double unit=(before[1].toPointF().y()-before[0].toPointF().y())/100.;
                if(unit<=0)return;
                double distance=std::max(1.,offset.y()+top-inkScrollPixels-220.);
                QVariant old=view->property("verticalJumpDistance");
                if(!view->setProperty("verticalJumpDistance",distance))return;
                bool invoked=QMetaObject::invokeMethod(view,"scrollDown");
                view->setProperty("verticalJumpDistance",old);
                auto after=diaryInkMapping(view);
                if(!invoked||after.size()!=2)return;
                QPointF oldScale=before[1].toPointF()-before[0].toPointF(),newScale=after[1].toPointF()-after[0].toPointF();
                double shift=(after[0].toPointF().y()-before[0].toPointF().y())/unit;
                if(QLineF(oldScale,newScale).length()>0.001 || std::abs(after[0].toPointF().x()-before[0].toPointF().x())>0.01 )return;
                if(std::abs(shift)<=0.1){atEnd=true;return;}
                if(shift<0)return;
                inkViewport=after;inkScrollPixels+=shift;moved=true;
                for(auto obj:runtimeObjects())if(obj->objectName()=="reply-gesture-feedback")if(auto item=qobject_cast<QQuickItem*>(obj))item->setY(item->y()-shift);
            });
            if(!moved) {
                if(atEnd&&diaryContinueOnNewPage(offset.y()+top)) {
                    runOnGuiSync([&]{pen.controller=originController;});
                    break;
                }
                return failure("scroll","Could not advance the page safely");
            }
            qInfo("[reply] Auto-scroll displacement %.1f px",inkScrollPixels);
            QThread::msleep(120);
        }
        QPointF position=offset-QPointF(0,inkScrollPixels);
        if(position.y()+top<80 || position.y()+bottom>=1930)return failure("scroll","Unable to fit this ink section safely");
        auto result=drawLetterPaths(section,position,canvas);
        if(!result.value("ok").toBool())return result;
    }
    return QJsonObject{{"ok",true}};
}
