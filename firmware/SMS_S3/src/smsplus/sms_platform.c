/* SMS-S3 platform, 2026-10-31. No SDL/BIOS/Retro-Go frontend dependency. */
#include "sms_platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef ARDUINO_ARCH_ESP32
#include <esp_heap_caps.h>
#endif
void *sms_port_malloc(size_t size){
#ifdef ARDUINO_ARCH_ESP32
  if(size>=65536)return heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  void *p=heap_caps_malloc(size,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  return p?p:heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
  return malloc(size);
#endif
}
void *sms_port_table_alloc(size_t size){
#ifdef ARDUINO_ARCH_ESP32
  void *p=heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
  void *p=malloc(size);
#endif
  if(p)memset(p,0,size);return p;
}
void *sms_port_calloc(size_t count,size_t size){
  if(size&&count>SIZE_MAX/size)return NULL;
  size*=count;void *p=sms_port_malloc(size);if(p)memset(p,0,size);return p;
}
void sms_port_free(void *p){free(p);}
uint32_t sms_port_crc32(uint32_t crc,const void *data,size_t size){
  const uint8_t *p=(const uint8_t*)data;crc=~crc;
  while(size--){crc^=*p++;for(unsigned i=0;i<8;++i)crc=(crc>>1)^(0xedb88320U& (uint32_t)-(int32_t)(crc&1));}
  return ~crc;
}

extern int sms_render_prepare(void),sms_tms_prepare(void),sms_z80_prepare(void);
extern void sms_render_release(void),sms_tms_release(void),z80_exit(void);
int sms_port_prepare_tables(void){return sms_render_prepare()&&sms_tms_prepare()&&sms_z80_prepare();}
void sms_port_release_tables(void){sms_render_release();sms_tms_release();z80_exit();}
