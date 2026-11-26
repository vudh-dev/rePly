#include "../src/stream.h"
#include <cassert>
#include <iostream>
int main() {
    QString answer="Greek α and escaped \"quotes\".\n\nSecond step: 6 × 7 = 42.";
    QByteArray wire;
    for(const auto &piece:QStringList{answer.left(11),answer.mid(11)}) {
        wire+="event: response.output_text.delta\r\ndata: "+QJsonDocument(QJsonObject{{"type","response.output_text.delta"},{"delta",piece}}).toJson(QJsonDocument::Compact)+"\r\n\r\n";
    }
    wire+="data: {\"type\":\"response.completed\",\"response\":{\"status\":\"completed\"}}\n\n";
    for(int split=1;split<100;++split) {
        DiaryEventStream stream;QString text;
        for(int i=0;i<wire.size();i+=split)stream.feed(wire.mid(i,split),[&](const QString &d){text+=d;});
        assert(text==answer);assert(stream.completed.value("status")=="completed");assert(!stream.failed);
    }
    DiaryEventStream failed;failed.feed("data: {\"type\":\"response.incomplete\"}\n\n",[](const QString&){});assert(failed.failed&&failed.completed.isEmpty());
    QString json=QString::fromUtf8(QJsonDocument(QJsonObject{{"command","Explain α"},{"kind","text"},{"template",QJsonObject{{"text","not the answer"}}},{"text",answer}}).toJson(QJsonDocument::Compact));
    for(int i=0;i<=json.size();++i) {
        auto part=diaryPartialAnswer(json.left(i));assert(answer.startsWith(part.text));
        if(part.textStarted){assert(part.fields.value("kind")=="text");assert(part.fields.value("command")=="Explain α");}
    }
    auto all=diaryPartialAnswer(json);assert(all.text==answer&&all.textComplete);
    auto escaped=diaryPartialAnswer(R"({"command":"question","kind":"text","text":"a\u03b1\n\nq\"x"})");assert(escaped.text=="aα\n\nq\"x");
    auto fake=diaryPartialAnswer(R"({"command":"say \"kind\":\"text\"","kind":"search","text":"latest news"})");assert(fake.fields.value("kind")=="search");
    std::cout<<"PASS: SSE arbitrary packet splits/UTF-8, terminal failures, partial structured strings and escaped/nested fields\n";
}
