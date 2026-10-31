/* SMS-S3 port 2026-10-31. SMSPlus / Retro-Go core, standalone platform adapter.
 * Z80 retains its original non-commercial license; see third-party notices.
 * No ROM or BIOS image is embedded. State format is local to this port.
 */
#include "sms_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <stddef.h>
#ifdef ARDUINO_ARCH_ESP32
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
extern "C" {
#include "src/smsplus/shared.h"
int sms_audio_submit_stereo(const int16_t*,const int16_t*,size_t);
}
// shared.h's allocation macros apply only to the C core, not C++ standard APIs.
#undef malloc
#undef calloc
#undef free

namespace SmsCore {
static uint16_t *rgb=nullptr;
static uint8_t *indexed=nullptr;
static uint16_t palette[256];
static bool ready=false,running=false,render=true;
static size_t sourceBytes=0;
static int ioError=0;
static void release(void *p){sms_port_free(p);}
static void yieldLoader(){
#ifdef ARDUINO_ARCH_ESP32
  vTaskDelay(1);
#endif
}
static void *frameAlloc(size_t n){
#ifdef ARDUINO_ARCH_ESP32
  void *p=heap_caps_malloc(n,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  return p?p:heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
  return malloc(n);
#endif
}
static void *romAlloc(size_t n){
#ifdef ARDUINO_ARCH_ESP32
  return heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
  return malloc(n);
#endif
}
bool beginPlatform(uint16_t *frame,size_t bytes){
  if(!frame||bytes<FRAME_BYTES)return false;
  if(ready)return rgb==frame;
  indexed=(uint8_t*)frameAlloc(FRAME_PIXELS);
  if(!indexed)return false;
  if(!sms_port_prepare_tables()){release(indexed);indexed=nullptr;sms_port_release_tables();return false;}
  rgb=frame;memset(rgb,0,FRAME_BYTES);memset(indexed,0,FRAME_PIXELS);
  memset(&smsplus,0,sizeof smsplus);memset(&sms,0,sizeof sms);memset(&vdp,0,sizeof vdp);
  memset(&snd,0,sizeof snd);memset(&bios,0,sizeof bios);memset(&coleco,0,sizeof coleco);
  bitmap.data=indexed;bitmap.width=WIDTH;bitmap.height=HEIGHT;bitmap.pitch=WIDTH;bitmap.granularity=1;
  ready=true;return true;
}
void unloadRom(){
  if(running)system_shutdown();
  // Partial allocation paths also reach here before poweron.
  if(!running){if(sms.wram){release(sms.wram);sms.wram=nullptr;}if(vdp.vram){release(vdp.vram);vdp.vram=nullptr;}}
  release(cart.rom);release(cart.sram);cart.rom=nullptr;cart.sram=nullptr;cart.loaded=0;
  running=false;sourceBytes=0;
  if(rgb)memset(rgb,0,FRAME_BYTES);
}
void endPlatform(){unloadRom();sms_port_release_tables();release(indexed);indexed=nullptr;rgb=nullptr;ready=false;}
bool loadRomPath(const char *path){
  ioError=0;if(!ready||!path||!path[0])return false;
  unloadRom();errno=0;FILE *f=fopen(path,"rb");
  if(!f){ioError=errno?errno:EIO;return false;}
  if(fseek(f,0,SEEK_END)!=0){ioError=errno?errno:EIO;fclose(f);return false;}
  const long fileBytes=ftell(f);
  const size_t header=fileBytes>512 && ((size_t)fileBytes%8192U)==512U?512U:0U;
  if(fileBytes<0||fileBytes<(long)(header+8192U)||fileBytes>(long)(4U*1024U*1024U+header)){
    fclose(f);fprintf(stderr,"[SMS][ROM][REJECT] size=%ld\n",fileBytes);return false;
  }
  const size_t actual=(size_t)fileBytes-header;
  const size_t padded=actual<32768U?32768U:(actual+16383U)&~16383U;
  uint8_t *rom=(uint8_t*)romAlloc(padded);
  if(!rom){fclose(f);return false;}
  if(fseek(f,(long)header,SEEK_SET)!=0){ioError=errno?errno:EIO;fclose(f);release(rom);return false;}
  size_t done=0;uint32_t crc=0;
  while(done<actual){
    const size_t n=actual-done>4096?4096:actual-done;errno=0;
    const size_t got=fread(rom+done,1,n,f);
    if(got!=n||ferror(f)){ioError=errno?errno:EIO;fprintf(stderr,"[SMS][ROM][READ_FAIL] offset=%u/%u errno=%d\n",(unsigned)done,(unsigned)actual,ioError);fclose(f);release(rom);return false;}
    crc=sms_port_crc32(crc,rom+done,n);done+=n;yieldLoader();
  }
  errno=0;if(fclose(f)!=0){ioError=errno?errno:EIO;release(rom);return false;}
  // Small ROMs mirror; larger non-bank-aligned dumps read as open bus padding.
  if(actual<=16384U){for(size_t i=actual;i<padded;++i)rom[i]=rom[i%actual];}
  else if(actual<padded)memset(rom+actual,0xff,padded-actual);
  // Reject Game Gear content with a .sms suffix instead of changing system.
  const size_t offsets[]={0x1ff0,0x3ff0,0x7ff0};
  for(size_t off:offsets){
    if(off+16<=actual&&!memcmp(rom+off,"TMR SEGA",8)){
      unsigned region=rom[off+15]>>4;
      if(region>=5&&region<=7){release(rom);fprintf(stderr,"[SMS][ROM][REJECT] Game Gear header\n");return false;}
    }
  }
  system_reset_config();option.sndrate=22050;option.overscan=0;option.extra_gg=0;option.use_bios=0;option.fm=SND_NONE;
  cart.rom=rom;cart.size=(uint32_t)padded;cart.crc=crc;cart.pages=(uint16_t)(padded/16384U);cart.loaded=1;
  cart.sram=(uint8_t*)sms_port_calloc(1,32768);
  sms.wram=(uint8_t*)sms_port_calloc(1,8192);vdp.vram=(uint8_t*)sms_port_calloc(1,16384);
  if(!cart.sram||!sms.wram||!vdp.vram){unloadRom();return false;}
  set_rom_config();
  if(!(sms.console==CONSOLE_SMS||sms.console==CONSOLE_SMS2)){unloadRom();return false;}
  // SDL, peripherals, BIOS and unsupported systems are deliberately not enabled.
  sms.device[0]=sms.device[1]=DEVICE_PAD2B;
  bitmap.data=indexed;bitmap.width=WIDTH;bitmap.height=HEIGHT;bitmap.pitch=WIDTH;bitmap.granularity=1;
  system_poweron();running=true;sourceBytes=actual;render=true;input.pad[0]=input.pad[1]=input.system=0;
  return true;
}
bool loaded(){return running;}
void reset(){if(running){system_reset();render=true;memset(rgb,0,FRAME_BYTES);}}
int runFrame(){
  if(!running)return -1;
  system_frame(render?0:1);
  if(snd.enabled&&snd.sample_count>0)sms_audio_submit_stereo(snd.stream[0],snd.stream[1],(size_t)snd.sample_count);
  return 0;
}
void setFrameRendering(bool enabled){render=enabled;}
void setButtons(uint16_t b){
  input.pad[0]=0;input.pad[1]=0;input.system=0;
  if(b&(1<<0))input.pad[0]|=INPUT_BUTTON1;
  if(b&(1<<1))input.pad[0]|=INPUT_BUTTON2;
  if(b&(1<<4))input.pad[0]|=INPUT_UP;
  if(b&(1<<5))input.pad[0]|=INPUT_DOWN;
  if(b&(1<<6))input.pad[0]|=INPUT_LEFT;
  if(b&(1<<7))input.pad[0]|=INPUT_RIGHT;
  if(b&(1<<3))input.system|=INPUT_PAUSE;
}
uint16_t *framebuffer(){return rgb;}
const uint8_t *indexedFramebuffer(){return indexed;}
void copyPalette(uint16_t out[256]){
  uint16_t be[256];
  if(render_copy_palette(be))for(unsigned i=0;i<256;++i)palette[i]=(uint16_t)((be[i]<<8)|(be[i]>>8));
  if(out)memcpy(out,palette,sizeof palette);
}
void copyRgbFrame(){
  if(!running||!rgb||!indexed)return;
  copyPalette(nullptr);
  for(size_t i=0;i<(size_t)WIDTH*activeHeight();++i)rgb[i]=palette[indexed[i]];
}
unsigned activeHeight(){return running&&(vdp.height==224||vdp.height==240)?vdp.height:192;}
uint32_t framePeriodUs(){return running&&sms.display==DISPLAY_PAL?20120U:16688U;}
size_t romSize(){return sourceBytes;}
uint32_t romHash(){return running?cart.crc:0;}
int lastLoadIoError(){return ioError;}
size_t stateRamSize(){return running?32768:0;}
uint8_t *cartRamData(){return running?cart.sram:nullptr;}
size_t cartRamSize(){return running?32768:0;}
bool hasBatteryRam(){return running&&sms.save!=0;}
bool soundMasterEnabled(){return running&&snd.enabled;}

// Fields contain no live pointers or callbacks. ABI is explicitly checked before
// interpreting context bytes, and all input is validated before changing state.
static constexpr size_t SMS_BYTES=sizeof(sms_t)-offsetof(sms_t,paused);
static constexpr size_t VDP_BYTES=sizeof(vdp_t)-offsetof(vdp_t,cram);
static constexpr size_t CPU_BYTES=offsetof(Z80_Regs,daisy);
struct LocalState {
  uint32_t magic,version,abi,romCrc;
  uint8_t wram[8192],vram[16384],sram[32768];
  uint8_t smsBytes[SMS_BYTES],vdpBytes[VDP_BYTES],cpuBytes[CPU_BYTES],fcr[4];
  int32_t cycles;
  SN76489_Context psg;
  uint32_t checksum;
};
static constexpr uint32_t STATE_MAGIC=0x53534d53U,LOCAL_STATE_VERSION=1;
static uint32_t stateAbi(){return (uint32_t)(sizeof(LocalState)^(SMS_BYTES<<16)^(VDP_BYTES<<8)^CPU_BYTES);}
size_t stateSize(){return sizeof(LocalState);}
bool saveState(void *buffer,size_t bytes){
  if(!running||!buffer||bytes!=sizeof(LocalState))return false;
  uint8_t *out=(uint8_t*)buffer;memset(out,0,bytes);
#define PUT(member,source,count) memcpy(out+offsetof(LocalState,member),source,count)
  const uint32_t header[]={STATE_MAGIC,LOCAL_STATE_VERSION,stateAbi(),cart.crc};memcpy(out,header,sizeof header);
  PUT(wram,sms.wram,8192);PUT(vram,vdp.vram,16384);PUT(sram,cart.sram,32768);
  PUT(smsBytes,&sms.paused,SMS_BYTES);PUT(vdpBytes,vdp.cram,VDP_BYTES);PUT(cpuBytes,&Z80,CPU_BYTES);PUT(fcr,cart.fcr,4);
  const int32_t cycles=z80_cycle_count;PUT(cycles,&cycles,sizeof cycles);
  PUT(psg,SN76489_GetContextPtr(0),sizeof(SN76489_Context));
  const uint32_t checksum=sms_port_crc32(0,out,offsetof(LocalState,checksum));PUT(checksum,&checksum,4);
#undef PUT
  return true;
}
bool loadState(const void *buffer,size_t bytes){
  if(!running||!buffer||bytes!=sizeof(LocalState))return false;
  const uint8_t *in=(const uint8_t*)buffer;uint32_t header[4],crc=0;memcpy(header,in,sizeof header);
  if(header[0]!=STATE_MAGIC||header[1]!=LOCAL_STATE_VERSION||header[2]!=stateAbi()||header[3]!=cart.crc)return false;
  memcpy(&crc,in+offsetof(LocalState,checksum),4);
  if(crc!=sms_port_crc32(0,in,offsetof(LocalState,checksum)))return false;
  sms_t savedSms={};vdp_t savedVdp={};Z80_Regs savedCpu={};SN76489_Context savedPsg={};int32_t cycles=0;
#define GET(target,member,count) memcpy(target,in+offsetof(LocalState,member),count)
  GET(&savedSms.paused,smsBytes,SMS_BYTES);GET(savedVdp.cram,vdpBytes,VDP_BYTES);GET(&savedCpu,cpuBytes,CPU_BYTES);
  GET(&savedPsg,psg,sizeof savedPsg);GET(&cycles,cycles,sizeof cycles);
  if(savedSms.console!=sms.console||savedSms.display!=sms.display||savedSms.territory>1||savedSms.paused>1||savedSms.save>1||savedSms.use_fm||
     savedSms.device[0]!=DEVICE_PAD2B||savedSms.device[1]!=DEVICE_PAD2B)return false;
  if(savedVdp.lpf!=(sms.display?313:262)||savedVdp.code>3||savedVdp.extended>2||savedVdp.mode>15||savedVdp.pending>1||
     (savedVdp.height!=192&&savedVdp.height!=224&&savedVdp.height!=240)||savedVdp.addr>0x3fff||savedVdp.bd>15||
     savedVdp.line<0||savedVdp.line>savedVdp.lpf||savedVdp.irq>1||savedVdp.vint_pending>1||savedVdp.hint_pending>1)return false;
  if(savedVdp.ct!=((savedVdp.reg[3]<<6)&0x3fc0)||savedVdp.pg!=((savedVdp.reg[4]<<11)&0x3800)||
     savedVdp.sa!=((savedVdp.reg[5]<<7)&0x3f80)||savedVdp.sg!=((savedVdp.reg[6]<<11)&0x3800)||
     savedVdp.satb!=((savedVdp.reg[5]<<7)&0x3f00)||savedVdp.left < -1 ||savedVdp.left>255)return false;
  const int32_t addresses[]={savedVdp.pn,savedVdp.ct,savedVdp.pg,savedVdp.sa,savedVdp.sg,savedVdp.ntab,savedVdp.satb};
  for(int32_t a:addresses)if(a<0||a>=0x4000)return false;
  if(savedCpu.im>2||savedCpu.iff1>1||savedCpu.iff2>1||savedCpu.halt>1||savedCpu.nmi_pending>1||savedCpu.after_ei>1||cycles<0||cycles>64)return false;
  const uint32_t pairs[]={savedCpu.pc.d,savedCpu.sp.d,savedCpu.af.d,savedCpu.bc.d,savedCpu.de.d,savedCpu.hl.d,
    savedCpu.ix.d,savedCpu.iy.d,savedCpu.wz.d,savedCpu.af2.d,savedCpu.bc2.d,savedCpu.de2.d,savedCpu.hl2.d};
  for(uint32_t value:pairs)if(value>0xffffU)return false;
  if(savedCpu.nmi_state>3||savedCpu.irq_state>3)return false;
  if(!isfinite(savedPsg.Clock)||savedPsg.Clock<0||savedPsg.Clock>=1.01f||!isfinite(savedPsg.dClock)||savedPsg.dClock<=0||savedPsg.dClock>100||
     savedPsg.Mute<0||savedPsg.Mute>15||savedPsg.VolumeArray<0||savedPsg.VolumeArray>1||savedPsg.BoostNoise<0||savedPsg.BoostNoise>1||
     savedPsg.LatchedRegister<0||savedPsg.LatchedRegister>7||savedPsg.NumClocksForSample<0||savedPsg.NumClocksForSample>100||
     (savedPsg.NoiseFreq!=0x10&&savedPsg.NoiseFreq!=0x20&&savedPsg.NoiseFreq!=0x40&&savedPsg.NoiseFreq!=0x80))return false;
  for(unsigned i=0;i<8;++i)if(savedPsg.Registers[i]>(i&1?15:i==6?15:1023))return false;
  for(unsigned i=0;i<3;++i)if(savedPsg.Registers[i*2]==0)return false;
  for(unsigned i=0;i<4;++i){
    if(savedPsg.ToneFreqPos[i]!=1&&savedPsg.ToneFreqPos[i]!=-1)return false;
    if(savedPsg.IntermediatePos[i]!=INT32_MIN && (savedPsg.IntermediatePos[i]<-131072 || savedPsg.IntermediatePos[i]>131072))return false;
    if(savedPsg.ToneFreqVals[i]<-100||savedPsg.ToneFreqVals[i]>2048)return false;
  }
  // Validation complete. Live RAM ownership and Z80 callbacks are retained.
  system_reset();
  GET(sms.wram,wram,8192);GET(vdp.vram,vram,16384);GET(cart.sram,sram,32768);
  memcpy(&sms.paused,&savedSms.paused,SMS_BYTES);memcpy(vdp.cram,savedVdp.cram,VDP_BYTES);
  memcpy(&Z80,&savedCpu,CPU_BYTES);z80_cycle_count=cycles;
  GET(cart.fcr,fcr,4);memcpy(SN76489_GetContextPtr(0),&savedPsg,sizeof savedPsg);
#undef GET
  slot.rom=cart.rom;slot.pages=cart.pages;slot.mapper=cart.mapper;slot.fcr=cart.fcr;mapper_reset();
  cpu_readmap[0]=slot.rom;
  for(unsigned i=0;i<4;++i){if(slot.mapper==MAPPER_KOREA_MSX)mapper_8k_w(i,cart.fcr[i]);else mapper_16k_w(i,cart.fcr[i]);}
  sms_port_w(0x3e,sms.memctrl);
  vdp_init();render_reset();
  // vdp_init derives safe viewport/table addresses from the restored registers.
  memset(rgb,0,FRAME_BYTES);render=true;input.pad[0]=input.pad[1]=input.system=0;
  return true;
}
}

