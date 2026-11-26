#include "../src/tutor.h"
#include <QTemporaryDir>
#include <cassert>
#include <iostream>
int main(){
 QTemporaryDir d;QString cfg=d.path()+"/cfg",state=d.path()+"/state";QDir().mkpath(cfg);
 DiaryTutorSession absent(cfg,state);assert(!absent.enabled);
 diaryPrivateSave(cfg+"/tutor-context.md","Teach the learner one question at a time.");
 DiaryTutorSession tutor(cfg,state);assert(tutor.enabled&&tutor.history.empty());
 QJsonObject p{{"instructions","base"},{"input",QJsonArray{QJsonObject{{"role","user"},{"content","image placeholder"}}}}};tutor.prepare(p);
 assert(p["instructions"].toString().contains("Teach the learner"));assert(p["context_management"].toArray()[0].toObject()["compact_threshold"].toInt()==20000);
 tutor.apiReply=QJsonObject{{"output",QJsonArray{QJsonObject{{"type","message"},{"role","assistant"},{"content",QJsonArray{QJsonObject{{"type","output_text"},{"text","One question"}}}}}}}};
 tutor.commit("My answer","One question","text");
 DiaryTutorSession reopened(cfg,state);assert(reopened.history.size()==2);assert(reopened.history[0].toObject()["content"].toString().contains("My answer"));
 QJsonObject opaque{{"type","compaction"},{"encrypted_content","untouched"},{"unknown_field",42}};
 auto next=diaryTutorCarry(reopened.history,QJsonObject{{"output",QJsonArray{opaque,QJsonObject{{"type","message"}}}}});
 assert(next.size()==2&&next[0].toObject()==opaque);
 diaryPrivateSave(state+"/session.json","broken");bool failed=false;try{DiaryTutorSession invalid(cfg,state);}catch(...){failed=true;}assert(failed);
 QFile f(state+"/session.json");assert(f.open(QIODevice::ReadOnly)&&f.readAll()=="broken");
 // Routing is by notebook identity, with no global-history fallback.
 diaryPrivateSave(cfg+"/agents.json",R"({"physics-id":"physics","personal-id":"personal"})");
 for(auto name:{"physics","personal"}){QDir().mkpath(cfg+"/agents/"+name);diaryPrivateSave(cfg+"/agents/"+name+"/tutor-context.md",QByteArray(name));}
 DiaryTutorSession physics(cfg,d.path()+"/routed","physics-id"),personal(cfg,d.path()+"/routed","personal-id"),other(cfg,state,"other-id");
 assert(physics.enabled&&personal.enabled&&!other.enabled);
 physics.apiReply=tutor.apiReply;physics.commit("physics only","question","text");
 DiaryTutorSession physicsAgain(cfg,d.path()+"/routed","physics-id"),personalAgain(cfg,d.path()+"/routed","personal-id");
 assert(physicsAgain.history.size()==2&&personalAgain.history.empty());
 assert(physicsAgain.profile=="physics"&&personalAgain.profile=="personal");
 diaryPrivateSave(cfg+"/agents.json",R"({"bad":"../physics"})");
 failed=false;try{DiaryTutorSession bad(cfg,state,"bad");}catch(...){failed=true;}assert(failed);
 std::cout<<"Tutor persistence and compaction tests passed\n";
}
