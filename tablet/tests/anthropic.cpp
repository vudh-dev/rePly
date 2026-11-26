#include "../src/anthropic.h"
#include "../src/response.h"
#include <cassert>
#include <iostream>
int main(){
 QJsonObject p{{"model","claude-test"},{"store",false},{"instructions","Tutor"},{"max_output_tokens",100},{"input",QJsonArray{QJsonObject{{"role","user"},{"content",QJsonArray{QJsonObject{{"type","input_text"},{"text","read"}},QJsonObject{{"type","input_image"},{"image_url","data:image/png;base64,YWJj"}}}}}}},{"text",QJsonObject{{"format",QJsonObject{{"schema",QJsonObject{{"type","object"}}}}}}}};
 auto c=tomClaudePayload(p);assert(!c.contains("store")&&!c.contains("reasoning"));assert(c["system"]=="Tutor");assert(c["messages"].toArray()[0].toObject()["content"].toArray()[1].toObject()["source"].toObject()["data"]=="YWJj");assert(c.contains("output_config"));
 auto r=tomClaudeResponse(QJsonObject{{"stop_reason","end_turn"},{"content",QJsonArray{QJsonObject{{"type","text"},{"text","answer"}}}}});assert(responseText(r)=="answer");
 bool rejected=false;try{tomClaudeResponse(QJsonObject{{"stop_reason","max_tokens"}});}catch(...){rejected=true;}assert(rejected);
 std::cout<<"Claude payload and completion normalization passed\n";
}
