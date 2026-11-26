#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <stdexcept>
// Convert our internal Responses-shaped request into Anthropic Messages.
inline QJsonObject tomClaudePayload(const QJsonObject &p) {
    QJsonArray messages;
    auto input=p.value("input");
    QJsonArray items=input.isArray()?input.toArray():QJsonArray{QJsonObject{{"role","user"},{"content",input}}};
    for(auto value:items){auto item=value.toObject();QString role=item.value("role").toString();
        if(role!="user"&&role!="assistant")continue;
        QJsonArray content;auto raw=item.value("content");
        if(raw.isString())content.append(QJsonObject{{"type","text"},{"text",raw.toString()}});
        else for(auto cv:raw.toArray()){auto c=cv.toObject();auto type=c.value("type").toString();
            if(type=="input_text"||type=="output_text")content.append(QJsonObject{{"type","text"},{"text",c.value("text")}});
            else if(type=="input_image") {QString url=c.value("image_url").toString(),prefix="data:image/png;base64,";
                if(!url.startsWith(prefix))throw std::runtime_error("Unsupported Claude image format");
                content.append(QJsonObject{{"type","image"},{"source",QJsonObject{{"type","base64"},{"media_type","image/png"},{"data",url.mid(prefix.size())}}}});
            }
        }
        if(!content.isEmpty())messages.append(QJsonObject{{"role",role},{"content",content}});
    }
    QJsonObject result{{"model",p.value("model")},{"max_tokens",p.value("max_output_tokens").toInt(8192)},
        {"system",p.value("instructions")},{"messages",messages}};
    auto schema=p.value("text").toObject().value("format").toObject().value("schema").toObject();
    if(!schema.isEmpty())result.insert("output_config",QJsonObject{{"format",QJsonObject{{"type","json_schema"},{"schema",schema}}}});
    return result;
}
inline QJsonObject tomClaudeResponse(const QJsonObject &p) {
    if(p.value("stop_reason").toString()!="end_turn")throw std::runtime_error("Claude response incomplete or refused; retry with a shorter request");
    QString text;for(auto v:p.value("content").toArray()){auto c=v.toObject();if(c.value("type")=="text")text+=c.value("text").toString();}
    if(text.trimmed().isEmpty())throw std::runtime_error("Claude returned no answer");
    return {{"status","completed"},{"usage",p.value("usage")},{"output",QJsonArray{QJsonObject{{"type","message"},{"role","assistant"},{"content",QJsonArray{QJsonObject{{"type","output_text"},{"text",text}}}}}}}};
}
