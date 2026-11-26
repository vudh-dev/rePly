#pragma once
#include <QFile>
#include <QSaveFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QUuid>
#include <QRegularExpression>
#include <stdexcept>

inline void diaryPrivateSave(const QString &path,const QByteArray &bytes) {
    QSaveFile file(path);file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly))throw std::runtime_error("Cannot save tutor memory");
    file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    if(file.write(bytes)!=bytes.size()||!file.commit())throw std::runtime_error("Cannot commit tutor memory");
}
inline QJsonArray diaryTutorCarry(QJsonArray prior,const QJsonObject &reply) {
    for(auto item:reply.value("output").toArray())prior.append(item);
    int last=-1;
    for(int i=0;i<prior.size();++i)if(prior[i].toObject().value("type")=="compaction")last=i;
    if(last>=0){QJsonArray compact;for(int i=last;i<prior.size();++i)compact.append(prior[i]);return compact;}
    return prior;
}
class DiaryTutorSession {
    QString folder;
public:
    bool enabled=false;QString profile;QJsonArray history;QJsonObject apiReply;
    DiaryTutorSession(const QString &configDir,const QString &stateDir,const QString &documentId=QString()):folder(stateDir) {
        QString profileDir=configDir;
        if(!documentId.isNull()) {
            QFile routes(configDir+"/agents.json");
            if(!routes.exists())return; // Notebook mode never falls back to global history.
            if(!routes.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot read notebook agents");
            QJsonParseError error;auto mapping=QJsonDocument::fromJson(routes.readAll(),&error);
            if(error.error!=QJsonParseError::NoError||!mapping.isObject())throw std::runtime_error("Invalid notebook agents");
            QString agent=mapping.object().value(documentId).toString();if(agent.isEmpty())return;
            if(!QRegularExpression("^[a-z][a-z0-9_-]{0,63}$").match(agent).hasMatch())throw std::runtime_error("Invalid notebook agent name");
            profileDir=configDir+"/agents/"+agent;folder=stateDir+"/agents/"+agent;
            if(!QFile::exists(profileDir+"/tutor-context.md"))throw std::runtime_error("Notebook agent profile missing");
        }
        QFile context(profileDir+"/tutor-context.md");if(!context.exists())return;
        if(!context.open(QIODevice::ReadOnly)||context.size()>128*1024)throw std::runtime_error("Cannot read tutor context");
        profile=QString::fromUtf8(context.readAll());
        QFile project(profileDir+"/tutor-project-context.md");
        if(project.exists()){if(!project.open(QIODevice::ReadOnly)||project.size()>128*1024)throw std::runtime_error("Cannot read project context");profile+="\n\n"+QString::fromUtf8(project.readAll());}
        if(profile.trimmed().isEmpty())throw std::runtime_error("Tutor context is empty");
        enabled=true;
        QFile state(folder+"/session.json");
        if(state.exists()) {
            if(!state.open(QIODevice::ReadOnly)||state.size()>16*1024*1024)throw std::runtime_error("Cannot read tutor memory");
            QJsonParseError error;auto doc=QJsonDocument::fromJson(state.readAll(),&error);
            if(error.error!=QJsonParseError::NoError||!doc.isObject()||!doc.object().value("input").isArray())throw std::runtime_error("Tutor memory is damaged; preserved for recovery");
            history=doc.object().value("input").toArray();
        }
    }
    void prepare(QJsonObject &payload) const {
        if(!enabled)return;
        QString instructions=payload.value("instructions").toString();
        instructions+="\nPERSONAL NOTEBOOK ASSISTANT MODE: The following notebook policy overrides generic complete-solution, no-follow-up and latest-question rules. Read the latest LEARNER CONTRIBUTION, including an attempted answer without a question mark. command must transcribe that contribution faithfully (including errors) with only enough context to identify the exercise. Do not turn a wrong learner answer into a corrected transcription. Use the persistent history to identify the outstanding question. Old assistant messages may be structured rendering envelopes; their text field was the lesson. Historical images are represented by their transcribed contribution; ask if a missing visual detail matters.\n"+profile;
        payload.insert("instructions",instructions);
        QJsonArray input=history;for(auto item:payload.value("input").toArray())input.append(item);
        payload.insert("input",input);
        payload.insert("context_management",QJsonArray{QJsonObject{{"type","compaction"},{"compact_threshold",20000}}});
    }
    void commit(const QString &learner,const QString &displayed,const QString &kind) {
        if(!enabled||apiReply.isEmpty())return;
        if(!QDir().mkpath(folder+"/turns"))throw std::runtime_error("Cannot create tutor memory folder");
        QFile::setPermissions(folder,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);
        QFile::setPermissions(folder+"/turns",QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);
        QString stamp=QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        QJsonArray next=history;
        next.append(QJsonObject{{"role","user"},{"content",stamp+"\n"+learner}});
        next=diaryTutorCarry(next,apiReply);
        if(kind!="text")next.append(QJsonObject{{"role","assistant"},{"content","Delivered on the tablet ("+kind+"): "+displayed}});
        QJsonObject entry{{"time",stamp},{"learner",learner},{"displayed",displayed},{"kind",kind},{"response",apiReply}};
        QString id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        diaryPrivateSave(folder+"/turns/"+id+".json",QJsonDocument(entry).toJson(QJsonDocument::Compact));
        diaryPrivateSave(folder+"/session.json",QJsonDocument(QJsonObject{{"version",1},{"last_turn",id},{"updated",stamp},{"input",next}}).toJson(QJsonDocument::Compact));
        history=next;
    }
};
