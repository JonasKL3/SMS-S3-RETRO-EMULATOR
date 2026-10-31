#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <thread>
#include <chrono>
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <freertos/task.h>
#include "firmware/SMS_S3/sms_video_output.h"
#include "firmware/SMS_S3/sms_video_scale.h"
SerialClass Serial;thread_local TestTask *currentTestTask=nullptr;
static void verify(const TFT_eSPI &panel,const std::vector<uint8_t> &frame,const std::vector<uint16_t>&pal,bool enlarged,unsigned height){
  const unsigned width=enlarged?320:256,dy=enlarged?240:height;
  const unsigned x0=enlarged?0:32,y0=enlarged?0:(240-height)/2;
  for(unsigned y=0;y<dy;++y)for(unsigned x=0;x<width;++x){
    unsigned sx=enlarged?x*256/320:x,sy=enlarged?y*height/240:y;
    assert(panel.pixels[(y0+y)*320+x0+x]==pal[frame[sy*256+sx]]);
  }
}
static void send(const std::vector<uint8_t>&frame,const std::vector<uint16_t>&pal,bool enlarged,unsigned h){
  assert(SmsVideoOutput::submitIndexed(frame.data(),pal.data(),enlarged,h));SmsVideoOutput::drain();
}
static void verifyCaptures(const TFT_eSPI&panel,size_t first,const std::vector<uint8_t>&frame,const std::vector<uint16_t>&pal){
  for(unsigned block=0;block<24;++block){
    const auto &capture=panel.captures[first+block];assert(capture.size()==256*8);
    for(unsigned i=0;i<capture.size();++i)assert(capture[i]==pal[frame[block*256*8+i]]);
  }
}
int main(){
#ifdef _WIN32
  _set_error_mode(1);_set_abort_behavior(0,3);
#endif
  TFT_eSPI panel;std::vector<uint16_t> stripe(320*8),pal(256);std::vector<uint8_t> frame(256*240);
  for(unsigned i=0;i<pal.size();++i)pal[i]=(uint16_t)(0x1200+i*137);
  for(unsigned i=0;i<frame.size();++i)frame[i]=(uint8_t)((i*7)^(i>>9));
  const auto original=frame;const auto originalPal=pal;
  assert(!SmsVideoOutput::begin(panel,nullptr));assert(SmsVideoOutput::begin(panel,stripe.data()));
  assert(!SmsVideoOutput::submitIndexed(frame.data(),pal.data(),true,0));
  assert(!SmsVideoOutput::submitIndexed(frame.data(),nullptr,true,192));
  panel.blocked=true;assert(SmsVideoOutput::submitIndexed(frame.data(),pal.data(),false,192));
  for(unsigned wait=0;panel.entered.load()==0&&wait<1000;++wait)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(panel.entered.load()!=0);
  for(auto &p:frame)p^=0x11;for(auto &p:pal)p^=0x2222;
  assert(SmsVideoOutput::submitIndexed(frame.data(),pal.data(),false,192));
  for(auto &p:frame)p^=0x22;for(auto &p:pal)p^=0x4444;
  const auto latest=frame;const auto latestPal=pal;
  assert(SmsVideoOutput::submitIndexed(frame.data(),pal.data(),false,192));
  for(auto &p:frame)p=0xee;for(auto &p:pal)p=0xeeee;
  std::thread unblock([&](){std::this_thread::sleep_for(std::chrono::milliseconds(20));panel.blocked=false;});
  SmsVideoOutput::drain();unblock.join();verify(panel,latest,latestPal,false,192);
  assert(panel.pushes==48&&panel.captures.size()==48);
  verifyCaptures(panel,0,original,originalPal);verifyCaptures(panel,24,latest,latestPal);
  
  frame=latest;pal=latestPal;
  unsigned before=panel.pushes.load();send(frame,pal,false,192);assert(panel.pushes==before);
  for(unsigned h:{192,224,240}){
    before=panel.pushes.load();send(frame,pal,true,h);verify(panel,frame,pal,true,h);
    assert(panel.pushes==before+30);
    before=panel.pushes.load();send(frame,pal,true,h);assert(panel.pushes==before);
  }
  send(frame,pal,true,192);before=panel.pushes.load();
  for(unsigned x=0;x<256;++x)frame[44*256+x]^=0x7f;
  send(frame,pal,true,192);verify(panel,frame,pal,true,192);
  assert(panel.pushes==before+2);
  before=panel.pushes.load();pal[0]^=0xffff;send(frame,pal,true,192);verify(panel,frame,pal,true,192);
  assert(panel.pushes==before+30);
  before=panel.pushes.load();frame[230*256]^=0x12;send(frame,pal,true,192);
  assert(panel.pushes==before);
  before=panel.pushes.load();SmsVideoOutput::invalidate();panel.fillScreen(0xeeee);
  send(frame,pal,true,192);verify(panel,frame,pal,true,192);assert(panel.pushes==before+30);
  send(frame,pal,false,224);verify(panel,frame,pal,false,224);
  send(frame,pal,false,192);verify(panel,frame,pal,false,192);
  assert(panel.pixels[14*320+40]==0);
  frame[0]^=1;panel.blocked=true;assert(SmsVideoOutput::submitIndexed(frame.data(),pal.data(),false,192));
  std::thread unblockAgain([&](){std::this_thread::sleep_for(std::chrono::milliseconds(20));panel.blocked=false;});
  SmsVideoOutput::invalidate();unblockAgain.join();verify(panel,frame,pal,false,192);
  before=panel.pushes.load();send(frame,pal,false,192);assert(panel.pushes==before+24);
  const size_t captureStart=panel.captures.size();panel.delayMs=1;
  for(auto &p:frame)p=1;
  for(unsigned sequence=1000;sequence<1200;++sequence){
    pal[1]=(uint16_t)sequence;assert(SmsVideoOutput::submitIndexed(frame.data(),pal.data(),false,192));
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  SmsVideoOutput::drain();panel.delayMs=0;unsigned previous=0;
  for(size_t i=captureStart;i<panel.captures.size();++i){
    const auto &pixels=panel.captures[i];assert(pixels[0]>=previous);previous=pixels[0];
    for(auto color:pixels)assert(color==pixels[0]);
  }
  assert(previous==1199);
  puts("SMS indexed video PASS: 192/224/240 exact RGB565, immutable index+palette snapshots, palette-only repaint, pending replacement, dirty boundary/native stripes, padding, borders, fences, 200-frame coherent monotonic queue");
  fflush(stdout);std::_Exit(0);
}
