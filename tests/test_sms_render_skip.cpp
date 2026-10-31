// Real Mode4 renderer: skipped background must preserve all VDP machine flags.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "../firmware/SMS_S3/sms_core.h"
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
extern "C" int sms_audio_submit_stereo(const int16_t*,const int16_t*,size_t){return 1;}

static void reg(unsigned index,unsigned value){
  vdp_write(1,(uint8_t)value);vdp_write(1,(uint8_t)(0x80|index));
}
static std::vector<uint8_t> save(){
  std::vector<uint8_t> bytes(SmsCore::stateSize());assert(SmsCore::saveState(bytes.data(),bytes.size()));return bytes;
}
static void fixture(unsigned variant){
  SmsCore::reset();
  reg(0,0x04|((variant&1)?0x20:0)|((variant&2)?0x08:0));
  reg(1,0x40|((variant&4)?0x01:0));reg(2,0x0e);reg(5,0x7e);reg(6,0);
  reg(8,(variant&8)?5:0);reg(9,(variant&16)?19:0);
  // RAM program has no VDP writes; NMI/IRQ disabled. Execute harmless RAM loop.
  sms.wram[0]=0x18;sms.wram[1]=0xfe;Z80.pc.d=0xc000;Z80.iff1=Z80.iff2=0;
  memset(vdp.vram,0,16384);
  // Tile0/1 are solid opaque pixels. Background tile1 may have priority/palette.
  for(unsigned row=0;row<8;++row){vdp.vram[row*4]=0xff;vdp.vram[32+row*4+1]=0xff;}
  for(unsigned i=0;i<32*28;++i){vdp.vram[0x3800+i*2]=1;vdp.vram[0x3800+i*2+1]=(variant&32)?0x10:0;}
  const unsigned sat=(0x7e<<7)&0x3f00;
  // Overlap at the left boundary, with more than8 sprites on the same line.
  // A termination marker follows. Two last sprites exercise right clipping.
  for(unsigned i=0;i<10;++i){vdp.vram[sat+i]=20;vdp.vram[sat+0x80+i*2]=(i<8?4:252);vdp.vram[sat+0x81+i*2]=0;}
  vdp.vram[sat+10]=208;
  // Mark all cached patterns dirty through the real VRAM write interface.
  for(unsigned address=0;address<64;++address){
    const uint8_t value=vdp.vram[address];vdp_write(1,(uint8_t)address);vdp_write(1,0x40);vdp_write(0,value^0xff);
    vdp_write(1,(uint8_t)address);vdp_write(1,0x40);vdp_write(0,value);
  }
}

int main(int argc,char **argv){
#ifdef _WIN32
  _set_error_mode(1);_set_abort_behavior(0,3);
#endif
  assert(argc==2);std::vector<uint16_t> rgb(SmsCore::FRAME_PIXELS);
  assert(SmsCore::beginPlatform(rgb.data(),SmsCore::FRAME_BYTES));assert(SmsCore::loadRomPath(argv[1]));
  unsigned collisionCases=0,overflowCases=0;
  for(unsigned variant=0;variant<64;++variant){
    fixture(variant);const auto initial=save();
    SmsCore::setFrameRendering(true);assert(SmsCore::runFrame()==0);const auto drawn=save();
    const unsigned status=vdp.status,collision=vdp.spr_col;
    collisionCases+=(status&0x20)!=0;overflowCases+=(status&0x40)!=0;
    assert(SmsCore::loadState(initial.data(),initial.size()));
    SmsCore::setFrameRendering(false);assert(SmsCore::runFrame()==0);const auto skipped=save();
    assert(vdp.status==status&&vdp.spr_col==collision);assert(skipped==drawn);
  }
  assert(collisionCases>0&&overflowCases>0);
  SmsCore::endPlatform();
  puts("Mode4 skip PASS: 64 overlapping/priority/scroll/left-mask/zoom/overflow scenes preserve complete CPU/VDP/PSG serialized state");
}
