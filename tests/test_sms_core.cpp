#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <Arduino.h>
#include <SD.h>
#include "../firmware/SMS_S3/sms_core.h"
#include "../firmware/SMS_S3/sms_savestates.h"
extern "C" {
#include "../firmware/SMS_S3/src/smsplus/shared.h"
}
#undef malloc
#undef calloc
#undef free
#undef bitmap
#undef cart
#undef input
#undef option
SerialClass Serial;SDClass SD;
static unsigned audioPackets=0;
static bool audible=false;
extern "C" int sms_audio_submit_stereo(const int16_t *l,const int16_t *r,size_t count){assert(l&&r&&count);for(size_t i=0;i<count;++i)if(l[i]||r[i])audible=true;++audioPackets;return 1;}
int main(int argc,char **argv){
#ifdef _WIN32
  _set_error_mode(1);_set_abort_behavior(0,3);
#endif
  assert(argc==2);std::vector<uint16_t> frame(SmsCore::FRAME_PIXELS,0);
  assert(SmsCore::beginPlatform(frame.data(),SmsCore::FRAME_BYTES));
  assert(!SmsCore::loadRomPath("/missing_sms_s3/no_rom.sms"));assert(SmsCore::lastLoadIoError()!=0);
  assert(SmsCore::loadRomPath(argv[1]));assert(SmsCore::loaded());
  assert(SmsCore::activeHeight()==192);assert(SmsCore::framePeriodUs()>16000&&SmsCore::framePeriodUs()<17000);
  for(unsigned n=0;n<4;++n){SmsCore::setFrameRendering(true);assert(SmsCore::runFrame()==0);}
  assert(audioPackets>0&&audible);assert(sms.wram[0]!=0);
  SmsCore::copyRgbFrame();
  unsigned red=0;for(unsigned i=0;i<256*192;++i)red+=frame[i]==0xf800;
  assert(red>256*160); // Test ROM explicitly sets CRAM[0] red and enables Mode4.
  std::vector<uint8_t> saved(SmsCore::stateSize());assert(saved.size()>32768);
  assert(SmsCore::saveState(saved.data(),saved.size()));
  const uint8_t ramBefore=sms.wram[0];sms.wram[0]^=0x5a;
  assert(SmsCore::loadState(saved.data(),saved.size()));assert(sms.wram[0]==ramBefore);
  SmsSaveState::selectGame("/JOGOS/SMS/jogos/homebrew.sms",SmsCore::romHash(),(uint32_t)SmsCore::romSize());
  assert(!SmsSaveState::enabled());assert(SmsSaveState::save(0));
  sms.wram[0]^=0x33;assert(SmsSaveState::load(0));assert(sms.wram[0]==ramBefore);
  const auto good=saved;saved[saved.size()/2]^=1;
  assert(!SmsCore::loadState(saved.data(),saved.size()));assert(sms.wram[0]==ramBefore);
  assert(!SmsCore::loadState(good.data(),good.size()-1));
  // Pause must be a console NMI edge, not a reset or continuous retrigger.
  unsigned before=sms.wram[2];SmsCore::setButtons(1U<<3);SmsCore::runFrame();
  assert((uint8_t)(sms.wram[2]-before)==1);
  before=sms.wram[2];SmsCore::setButtons(1U<<3);SmsCore::runFrame();assert(sms.wram[2]==before);
  SmsCore::setButtons(0);SmsCore::runFrame();SmsCore::setButtons(1U<<3);SmsCore::runFrame();
  assert((uint8_t)(sms.wram[2]-before)==1);SmsCore::setButtons(0);
  SmsCore::setButtons(1);assert(smsplus.input.pad[0]&INPUT_BUTTON1);
  SmsCore::setButtons(2);assert(smsplus.input.pad[0]&INPUT_BUTTON2);
  SmsCore::setButtons(0);
  // Pixel skipping still runs CPU/audio; it must not suppress the machine.
  SmsCore::setFrameRendering(false);before=audioPackets;SmsCore::runFrame();assert(audioPackets>before);
  SmsCore::setFrameRendering(true);SmsCore::runFrame();
  // Verify all 64 SMS CRAM codes without applying a display color filter.
  for(unsigned code=0;code<64;++code){
    vdp.cram[0]=(uint8_t)code;palette_sync(0);uint16_t colors[256];render_copy_palette(colors);
    unsigned r=(code&3)*85,g=((code>>2)&3)*85,b=((code>>4)&3)*85;
    const uint16_t expected=((r>>3)<<11)|((g>>2)<<5)|(b>>3);
    assert((uint16_t)((colors[0]<<8)|(colors[0]>>8))==expected);
  }
  SmsCore::unloadRom();assert(!SmsCore::loaded());assert(SmsCore::loadRomPath(argv[1]));
  SmsCore::runFrame();SmsCore::endPlatform();
  puts("SMS core PASS: real Z80 homebrew, Mode4 red pixels, PSG packets, pause NMI edges, buttons, state roundtrip/corruption, skipped drawing runs audio, unload/reload");
}
