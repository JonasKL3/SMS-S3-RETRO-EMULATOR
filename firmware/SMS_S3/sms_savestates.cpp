// SMS-S3 state wrapper v1. Synchronous producer-task use only: drain display
// and pause audio before calling. The embedded core validates its own payload.
#include "sms_savestates.h"
#include "sms_paths.h"
#include "sms_core.h"
#include <Arduino.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {
static constexpr uint16_t VERSION=1;
// Pinned SMSPlus 4ced1206 + checked SMS-S3 memory-state format revision 1.
// Bump this if the core's serialization layout/ABI changes.
static constexpr uint32_t CORE_FORMAT=0x4ced0001UL;
static constexpr size_t MAX_PAYLOAD=256U*1024U;
static constexpr size_t CHUNK=4096;
static constexpr char MAGIC[8]={'S','M','S','S','3','S','T','1'};
#pragma pack(push,1)
struct Header {
  char magic[8];
  uint16_t version,headerSize;
  uint32_t coreFormat;
  uint8_t slot,reserved[3];
  uint32_t pathHash,romHash,romSize,payloadSize,payloadCrc,headerCrc;
};
#pragma pack(pop)
static_assert(sizeof(Header)==44,"SMS state file header layout");
static uint8_t scratch[CHUNK];
static uint32_t pathId=0,romId=0,romBytes=0,lastAutoAt=0;
static bool haveGame=false,autoEnabled=false;
static bool slotKnown[10]={},slotExists[10]={};
static char key[40]={};

static bool validSlot(int slot){return (slot>=0&&slot<=3)||slot==9;}
static uint32_t crcUpdate(uint32_t crc,const uint8_t *data,size_t size){
  for(size_t i=0;i<size;++i){
    crc^=data[i];
    for(unsigned b=0;b<8;++b)crc=(crc>>1)^(0xedb88320UL&(uint32_t)-(int32_t)(crc&1U));
  }
  return crc;
}
static uint32_t crc32(const void *data,size_t size){return crcUpdate(0xffffffffUL,(const uint8_t*)data,size)^0xffffffffUL;}
static uint32_t hashPath(const char *path){
  uint32_t hash=2166136261UL;
  if(path)while(*path)hash=(hash^(uint8_t)*path++)*16777619UL;
  return hash;
}
static size_t expectedSize(){
  const size_t n=SmsCore::stateSize();
  return n>0&&n<=MAX_PAYLOAD?n:0;
}
static bool makePath(char *out,size_t capacity,int slot,const char *suffix){
  char sdPath[160];
  return haveGame&&SmsPaths::makeStatePath(sdPath,sizeof sdPath,pathId,romId,slot,suffix)&&
      SmsPaths::makeVfsPath(out,capacity,sdPath);
}
// Return 1 if present, 0 if absent, -1 on an I/O failure. Never interpret a
// transient error as absence and rename over a possibly good state.
static int exists(const char *path){
  struct stat info={};errno=0;
  if(stat(path,&info)==0)return 1;
  return errno==ENOENT?0:-1;
}
static bool removeIfPresent(const char *path){
  errno=0;if(remove(path)==0||errno==ENOENT)return true;
  Serial.printf("[SMS][STATE][FAIL] remove errno=%d path=%s\n",errno,path);return false;
}
static bool readExact(FILE *file,uint8_t *bytes,size_t size){
  for(size_t offset=0;offset<size;){
    const size_t count=size-offset>CHUNK?CHUNK:size-offset;
    if(fread(bytes+offset,1,count,file)!=count)return false;
    offset+=count;delay(1);
  }
  return !ferror(file);
}
static bool writeExact(FILE *file,const uint8_t *bytes,size_t size){
  for(size_t offset=0;offset<size;){
    const size_t count=size-offset>CHUNK?CHUNK:size-offset;
    if(fwrite(bytes+offset,1,count,file)!=count)return false;
    offset+=count;delay(1);
  }
  return !ferror(file);
}
static bool closeWritten(FILE *file,bool ok){
  errno=0;const int flushed=fflush(file),flushErr=errno;
  if(flushed!=0||ferror(file)){
    Serial.printf("[SMS][STATE][FAIL] flush=%d errno=%d ferror=%d\n",flushed,flushErr,ferror(file));ok=false;
  }
  if(ok){
    errno=0;const int fd=fileno(file);
#ifdef _WIN32
    const int synced=fd<0?-1:_commit(fd);
#else
    const int synced=fd<0?-1:fsync(fd);
#endif
    if(synced!=0){Serial.printf("[SMS][STATE][FAIL] fsync errno=%d\n",errno);ok=false;}
  }
  errno=0;const int closed=fclose(file),closeErr=errno;
  if(closed!=0){Serial.printf("[SMS][STATE][FAIL] close=%d errno=%d\n",closed,closeErr);ok=false;}
  return ok;
}
static bool validHeader(const Header &h,int slot,size_t expected){
  return expected&&memcmp(h.magic,MAGIC,sizeof MAGIC)==0&&h.version==VERSION&&
      h.headerSize==sizeof h&&h.coreFormat==CORE_FORMAT&&validSlot(slot)&&
      h.slot==(uint8_t)slot&&h.reserved[0]==0&&h.reserved[1]==0&&h.reserved[2]==0&&
      h.pathHash==pathId&&h.romHash==romId&&h.romSize==romBytes&&
      h.payloadSize==expected&&h.headerCrc==crc32(&h,offsetof(Header,headerCrc));
}
// Optional dst permits a full CRC/length check for present() without allocating
// a second complete payload. No emulated machine state is changed here.
static bool readState(const char *path,int slot,uint8_t *dst=nullptr,size_t capacity=0){
  const size_t expected=expectedSize();
  if(!expected||(dst&&capacity!=expected))return false;
  FILE *file=fopen(path,"rb");if(!file)return false;
  Header h={};
  bool ok=readExact(file,(uint8_t*)&h,sizeof h)&&validHeader(h,slot,expected);
  uint32_t crc=0xffffffffUL;
  for(size_t offset=0;ok&&offset<expected;){
    const size_t count=expected-offset>CHUNK?CHUNK:expected-offset;
    uint8_t *target=dst?dst+offset:scratch;
    ok=readExact(file,target,count);
    if(ok)crc=crcUpdate(crc,target,count);
    offset+=count;
  }
  if(ok)ok=(crc^0xffffffffUL)==h.payloadCrc&&fgetc(file)==EOF&&!ferror(file);
  if(fclose(file)!=0)ok=false;
  return ok;
}
static bool preserveInvalid(const char *path){
  const int present=exists(path);
  if(present<=0)return present==0;
  char bad[360];
  for(unsigned i=0;i<10;++i){
    const int count=snprintf(bad,sizeof bad,"%s.bad%u",path,i);
    if(count<0||(size_t)count>=sizeof bad)return false;
    const int taken=exists(bad);if(taken<0)return false;if(taken)continue;
    if(rename(path,bad)!=0)return false;
    Serial.printf("[SMS][STATE][PRESERVE] %s\n",bad);return true;
  }
  return false;
}
static bool recoverMain(int slot,const char *path,const char *backup){
  if(readState(path,slot))return true;
  // Explicit stat disambiguates ENOENT from I/O failure before any mutation.
  if(exists(path)<0||exists(backup)<0)return false;
  if(readState(backup,slot)){
    if(!preserveInvalid(path)||rename(backup,path)!=0)return false;
    return readState(path,slot);
  }
  return preserveInvalid(path);
}
static bool apply(const char *path,int slot){
  const size_t expected=expectedSize();if(!expected)return false;
  uint8_t *payload=(uint8_t*)heap_caps_malloc(expected,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!payload)return false;
  const bool valid=readState(path,slot,payload,expected);
  const bool restored=valid&&SmsCore::loadState(payload,expected);
  heap_caps_free(payload);return restored;
}
} // namespace

namespace SmsSaveState {
void selectGame(const char *romPath,uint32_t romHash,uint32_t romSize){
  pathId=hashPath(romPath);romId=romHash;romBytes=romSize;
  haveGame=romPath&&romPath[0]&&romSize>0;
  snprintf(key,sizeof key,"%08lX_%08lX",(unsigned long)pathId,(unsigned long)romId);
  memset(slotKnown,0,sizeof slotKnown);memset(slotExists,0,sizeof slotExists);
  lastAutoAt=millis();
  Serial.printf("[SMS][STATE][GAME] key=%s dir=%s\n",key,SmsPaths::SAVE);
}
bool enabled(){return autoEnabled;}
void setEnabled(bool value){autoEnabled=value;lastAutoAt=millis();}
void resetAutoTimer(){lastAutoAt=millis();}
bool autoDue(){return haveGame&&autoEnabled&&(uint32_t)(millis()-lastAutoAt)>=AUTO_INTERVAL_MS;}
const char *gameKey(){return key;}
bool present(int slot){
  if(!haveGame||!validSlot(slot))return false;
  if(slotKnown[slot])return slotExists[slot];
  char path[320],backup[320];
  const bool ok=makePath(path,sizeof path,slot,"")&&makePath(backup,sizeof backup,slot,".bak")&&
      (readState(path,slot)||readState(backup,slot));
  slotKnown[slot]=true;slotExists[slot]=ok;return ok;
}
bool save(int slot){
  if(!haveGame||!SmsCore::loaded()||!validSlot(slot))return false;
  slotKnown[slot]=false;
  if(!SD.exists(SmsPaths::SAVE)&&!SD.mkdir(SmsPaths::SAVE))return false;
  char path[320],temporary[320],backup[320];
  if(!makePath(path,sizeof path,slot,"")||!makePath(temporary,sizeof temporary,slot,".tmp")||
      !makePath(backup,sizeof backup,slot,".bak")||!recoverMain(slot,path,backup))return false;
  const size_t size=expectedSize();if(!size)return false;
  uint8_t *payload=(uint8_t*)heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!payload)return false;
  bool ok=SmsCore::saveState(payload,size);
  Header h={};memcpy(h.magic,MAGIC,sizeof MAGIC);
  h.version=VERSION;h.headerSize=sizeof h;h.coreFormat=CORE_FORMAT;h.slot=(uint8_t)slot;
  h.pathHash=pathId;h.romHash=romId;h.romSize=romBytes;h.payloadSize=(uint32_t)size;
  if(ok)h.payloadCrc=crc32(payload,size);
  h.headerCrc=crc32(&h,offsetof(Header,headerCrc));
  if(ok)ok=removeIfPresent(temporary);
  FILE *out=ok?fopen(temporary,"wb"):nullptr;
  if(!out)ok=false;
  if(out){ok=writeExact(out,(const uint8_t*)&h,sizeof h)&&writeExact(out,payload,size);ok=closeWritten(out,ok);}
  heap_caps_free(payload);
  if(!ok||!readState(temporary,slot)){
    removeIfPresent(temporary);Serial.printf("[SMS][STATE][SAVE][FAIL] slot=%d; principal preservado\n",slot);return false;
  }
  const int hadMain=exists(path);if(hadMain<0)return false;
  if(hadMain&&(!removeIfPresent(backup)||rename(path,backup)!=0))return false;
  if(rename(temporary,path)!=0){
    if(hadMain&&exists(path)==0)rename(backup,path);
    return false;
  }
  slotKnown[slot]=true;slotExists[slot]=true;lastAutoAt=millis();
  Serial.printf("[SMS][STATE][SAVE] slot=%d bytes=%lu path=%s\n",slot,(unsigned long)(sizeof h+size),path);
  return true;
}
bool load(int slot){
  if(!haveGame||!SmsCore::loaded()||!validSlot(slot))return false;
  slotKnown[slot]=false;
  char path[320],backup[320];
  if(!makePath(path,sizeof path,slot,"")||!makePath(backup,sizeof backup,slot,".bak"))return false;
  if(apply(path,slot)){
    slotKnown[slot]=slotExists[slot]=true;lastAutoAt=millis();
    Serial.printf("[SMS][STATE][LOAD] slot=%d OK\n",slot);return true;
  }
  if(apply(backup,slot)){
    if(preserveInvalid(path))rename(backup,path);
    slotKnown[slot]=slotExists[slot]=true;lastAutoAt=millis();
    Serial.printf("[SMS][STATE][LOAD] slot=%d backup recuperado\n",slot);return true;
  }
  slotKnown[slot]=true;slotExists[slot]=false;
  Serial.printf("[SMS][STATE][LOAD][FAIL] slot=%d ausente/invalido/ROM diferente\n",slot);return false;
}
int saveAuto(){if(!autoDue())return 0;lastAutoAt=millis();return save(9)?1:-1;}
} // namespace SmsSaveState
