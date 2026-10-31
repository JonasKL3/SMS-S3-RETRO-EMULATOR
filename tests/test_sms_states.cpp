// Wrapper unit test: fake only the core payload API, real file format/CRC/
// temporary/backup/recovery implementation from sms_savestates.cpp.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <vector>
#include <Arduino.h>
#include <SD.h>
#include "../firmware/SMS_S3/sms_core.h"
#include "../firmware/SMS_S3/sms_paths.h"
#include "../firmware/SMS_S3/sms_savestates.h"

SerialClass Serial;SDClass SD;
static unsigned applied=0;
static uint8_t machine[8192];
namespace SmsCore {
bool loaded(){return true;}
size_t stateSize(){return sizeof machine;}
bool saveState(void *out,size_t size){if(!out||size!=sizeof machine)return false;memcpy(out,machine,size);return true;}
bool loadState(const void *in,size_t size){
  if(!in||size!=sizeof machine||((const uint8_t*)in)[0]!=0x53)return false;
  memcpy(machine,in,size);++applied;return true;
}
}
static uint32_t crc32(const uint8_t *p,size_t size){
  uint32_t crc=0xffffffffU;
  for(size_t i=0;i<size;++i){crc^=p[i];for(unsigned b=0;b<8;++b)crc=(crc>>1)^(0xedb88320U&(uint32_t)-(int32_t)(crc&1));}
  return crc^0xffffffffU;
}
static void put32(std::vector<uint8_t> &data,unsigned offset,uint32_t value){
  for(unsigned i=0;i<4;++i)data[offset+i]=(uint8_t)(value>>(i*8));
}
static std::vector<uint8_t> readFile(const char *path){
  FILE *f=fopen(path,"rb");assert(f);assert(fseek(f,0,SEEK_END)==0);long n=ftell(f);assert(n>0);rewind(f);
  std::vector<uint8_t> bytes((size_t)n);assert(fread(bytes.data(),1,bytes.size(),f)==bytes.size());assert(fclose(f)==0);return bytes;
}
static void writeFile(const char *path,const std::vector<uint8_t> &bytes){
  FILE *f=fopen(path,"wb");assert(f);assert(fwrite(bytes.data(),1,bytes.size(),f)==bytes.size());assert(fclose(f)==0);
}
static uint32_t hashPath(const char *p){uint32_t h=2166136261U;while(*p)h=(h^(uint8_t)*p++)*16777619U;return h;}
static void pathFor(char *out,size_t cap,int slot,const char *suffix=""){
  char sd[160];assert(SmsPaths::makeStatePath(sd,sizeof sd,hashPath("/JOGOS/SMS/jogos/test.sms"),0x12345678U,slot,suffix));
  assert(SmsPaths::makeVfsPath(out,cap,sd));
}
static void select(uint32_t hash=0x12345678U){SmsSaveState::selectGame("/JOGOS/SMS/jogos/test.sms",hash,32768);}
int main(){
#ifdef _WIN32
  _set_error_mode(1);_set_abort_behavior(0,3);
#endif
  for(unsigned i=0;i<sizeof machine;++i)machine[i]=(uint8_t)i;machine[0]=0x53;
  const std::vector<uint8_t> original(machine,machine+sizeof machine);
  select();assert(!SmsSaveState::enabled());assert(!SmsSaveState::autoDue());assert(SmsSaveState::saveAuto()==0);
  assert(!SmsSaveState::save(-1)&&!SmsSaveState::save(4)&&!SmsSaveState::load(10));
  assert(!SmsSaveState::present(0));assert(SmsSaveState::save(0));assert(SmsSaveState::present(0));
  assert(SmsSaveState::save(0));
  char path[320],backup[320],temp[320];pathFor(path,sizeof path,0);pathFor(backup,sizeof backup,0,".bak");
  auto bytes=readFile(path);assert(bytes.size()==44+sizeof machine);
  bytes[44+100]^=0xff;writeFile(path,bytes);machine[10]^=1;select();
  assert(SmsSaveState::load(0));assert(applied==1);assert(memcmp(machine,original.data(),sizeof machine)==0);
  // The good backup is promoted and the corrupt main preserved as bad0.
  char bad[360];snprintf(bad,sizeof bad,"%s.bad0",path);assert(readFile(bad).size()==44+sizeof machine);
  assert(readFile(path)[44+100]==original[100]);
  select(0xaabbccddU);const unsigned beforeWrong=applied;assert(!SmsSaveState::load(0));assert(applied==beforeWrong);select();

  // Correct external CRC is insufficient if the core rejects semantic content.
  assert(SmsSaveState::save(1));pathFor(path,sizeof path,1);bytes=readFile(path);bytes[44]=0xff;
  put32(bytes,36,crc32(bytes.data()+44,sizeof machine));put32(bytes,40,crc32(bytes.data(),40));writeFile(path,bytes);
  const unsigned beforeInvalid=applied;assert(!SmsSaveState::load(1));assert(applied==beforeInvalid);
  assert(memcmp(machine,original.data(),sizeof machine)==0);

  // A trailing byte is rejected even when header and payload CRC match.
  assert(SmsSaveState::save(2));pathFor(path,sizeof path,2);bytes=readFile(path);bytes.push_back(0);writeFile(path,bytes);
  assert(!SmsSaveState::load(2));assert(applied==beforeInvalid);

  // Reject temporary write failures without touching the committed principal.
  assert(SmsSaveState::save(3));pathFor(path,sizeof path,3);const auto principal=readFile(path);
  pathFor(temp,sizeof temp,3,".tmp");char sdTemp[160];assert(SmsPaths::makeStatePath(sdTemp,sizeof sdTemp,hashPath("/JOGOS/SMS/jogos/test.sms"),0x12345678U,3,".tmp"));
  assert(SD.mkdir(sdTemp));char blocked[360];snprintf(blocked,sizeof blocked,"%s/blocked",temp);
  FILE *f=fopen(blocked,"wb");assert(f);assert(fputc(1,f)!=EOF);assert(fclose(f)==0);
  assert(!SmsSaveState::save(3));assert(readFile(path)==principal);
  puts("SMS states PASS: versioned header, ROM isolation, exact length, CRC before apply, semantic rejection, backup recovery, manual saves with autosave OFF, failed commit preserves principal");
}
