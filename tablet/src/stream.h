#pragma once
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <functional>
#include <stdexcept>

// SSE frames may be split anywhere, including in UTF-8 or JSON escapes.
class DiaryEventStream {
    QByteArray pending, data;
public:
    QJsonObject completed;
    bool failed=false;
    void feed(const QByteArray &bytes,const std::function<void(const QString&)> &delta) {
        pending+=bytes;
        if(pending.size()>1024*1024)throw std::runtime_error("Oversized streaming event");
        int end;
        while((end=pending.indexOf('\n'))>=0) {
            QByteArray line=pending.left(end);pending.remove(0,end+1);
            if(line.endsWith('\r'))line.chop(1);
            if(line.startsWith("data:")){if(!data.isEmpty())data+='\n';data+=line.mid(5).trimmed();}
            else if(line.isEmpty() && !data.isEmpty()) {
                auto frame=data;data.clear();if(frame=="[DONE]")continue;
                QJsonParseError error;auto doc=QJsonDocument::fromJson(frame,&error);
                if(error.error!=QJsonParseError::NoError || !doc.isObject())throw std::runtime_error("Invalid streaming event");
                auto event=doc.object();auto type=event.value("type").toString();
                if(type=="response.output_text.delta")delta(event.value("delta").toString());
                else if(type=="response.completed")completed=event.value("response").toObject();
                else if(type=="response.failed" || type=="response.incomplete" || type=="error")failed=true;
            }
            if(data.size()>1024*1024)throw std::runtime_error("Oversized streaming event");
        }
    }
};

// Only expose top-level JSON string fields. No regex matching inside quoted text
// or nested template data. An unfinished escape is held until the next delta.
struct DiaryPartialAnswer {QMap<QString,QString> fields;QString text;bool textStarted=false,textComplete=false;};
inline DiaryPartialAnswer diaryPartialAnswer(const QString &json) {
    DiaryPartialAnswer out;int p=0;
    auto ws=[&]{while(p<json.size() && json[p].isSpace())++p;};
    auto string=[&](QString &value)->bool {
        if(p>=json.size() || json[p]!='"')return false;
        ++p;value.clear();
        while(p<json.size()) {
            QChar c=json[p++];if(c=='"')return true;
            if(c!='\\'){value+=c;continue;}
            if(p>=json.size())return false;
            c=json[p++];
            if(c=='u') {
                if(p+4>json.size())return false;
                bool ok=false;ushort code=json.mid(p,4).toUShort(&ok,16);if(!ok)return false;p+=4;value+=QChar(code);
            } else if(c=='n')value+='\n';else if(c=='r')value+='\r';else if(c=='t')value+='\t';
            else if(c=='b')value+='\b';else if(c=='f')value+='\f';
            else if(c=='"'||c=='\\'||c=='/')value+=c;else return false;
        }return false;
    };
    ws();if(p>=json.size()||json[p++]!='{')return out;
    while(p<json.size()) {
        ws();QString key,value;if(!string(key))return out;ws();if(p>=json.size()||json[p++]!=':')return out;ws();
        if(p>=json.size())return out;
        if(json[p]=='"') {
            bool complete=string(value);
            if(key=="text"){out.text=value;out.textStarted=true;out.textComplete=complete;}
            if(!complete)return out;
            out.fields.insert(key,value);
        } else {
            int depth=0;bool quoted=false,escaped=false;
            for(;p<json.size();++p) {
                QChar c=json[p];
                if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;continue;}
                if(c=='"'){quoted=true;continue;}
                if(c=='{'||c=='[')++depth;
                else if(c=='}'||c==']'){if(depth==0)break;--depth;}
                else if(c==','&&depth==0)break;
            }
        }
        ws();if(p>=json.size()||json[p++]!=',')return out;
    }return out;
}
