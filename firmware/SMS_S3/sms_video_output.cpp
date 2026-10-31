#include "sms_video_output.h"
#include "sms_frame_mailbox.h"
#include "sms_video_scale.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <stdlib.h>

namespace SmsVideoOutput {
static TFT_eSPI *panel=nullptr;
struct Job{uint8_t *pixels=nullptr;uint16_t palette[256]={};bool enlarged=false;unsigned height=192;uint32_t sequence=0;};
static Job jobs[2];
static uint16_t *staging=nullptr,lastPalette[256]={};
static uint8_t *lastDisplayed=nullptr;
static TaskHandle_t worker=nullptr;
static SmsFrameMailbox mailbox;
static bool referenceValid=false,lastEnlarged=false;
static unsigned lastHeight=0;
static uint32_t publishedSequence=0,lastCompletedSequence=0;

static void displayTask(void*){
  const bool watched=esp_task_wdt_add(nullptr)==ESP_OK;
  if(!watched)Serial.println("[VIDEO][WARN] watchdog da tarefa nao foi registrado");
  for(;;){
    if(watched)esp_task_wdt_reset();
    const int slot=mailbox.beginRead();
    if(slot<0){ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1000));continue;}
    // Both slots can briefly be READY between jobs. If a newer snapshot in
    // the preferred slot was already displayed, never display an older pending
    // slot afterwards. Ownership protects sequence just like its pixel buffer.
    if((int32_t)(jobs[slot].sequence-lastCompletedSequence)<=0){
      mailbox.finishRead((unsigned)slot);continue;
    }
    const uint8_t *snapshot=jobs[slot].pixels;
    const uint16_t *palette=jobs[slot].palette;
    const bool jobEnlarged=jobs[slot].enlarged;
    const unsigned jobHeight=jobs[slot].height;
    bool changed=false;
    const bool compare=lastDisplayed&&referenceValid&&lastEnlarged==jobEnlarged&&lastHeight==jobHeight&&memcmp(palette,lastPalette,sizeof lastPalette)==0;
    // An in-game VDP height transition can expose pixels outside the new native
    // rectangle. The worker owns SPI here; erase those old border pixels.
    if(referenceValid&&!jobEnlarged&&(lastEnlarged||lastHeight!=jobHeight))panel->fillScreen(0);
    if(jobEnlarged){
      for(unsigned y=0;y<240;y+=8){
        const unsigned firstSource=y*jobHeight/240U;
        const unsigned lastSource=(y+7U)*jobHeight/240U;
        if(compare&&memcmp(snapshot+firstSource*256U,lastDisplayed+firstSource*256U,
            (lastSource-firstSource+1U)*256U)==0)continue;

        sms_expand_indexed_stripe(snapshot,palette,staging,jobHeight,y,8,true);

        panel->pushImage(0,y,320,8,staging);
changed=true;
      }
    }else{
      for(unsigned y=0;y<jobHeight;y+=8){
        const unsigned rows=jobHeight-y<8?jobHeight-y:8;
        if(compare&&memcmp(snapshot+y*256U,lastDisplayed+y*256U,rows*256U)==0)continue;

        sms_expand_indexed_stripe(snapshot,palette,staging,jobHeight,y,rows,false);

        panel->pushImage(32,(240-jobHeight)/2+y,256,rows,staging);
changed=true;
      }
    }
    if(lastDisplayed){
      if(changed||!compare)memcpy(lastDisplayed,snapshot,256U*jobHeight);
      memcpy(lastPalette,palette,sizeof lastPalette);
    }
    lastEnlarged=jobEnlarged;lastHeight=jobHeight;referenceValid=true;
    lastCompletedSequence=jobs[slot].sequence;
    mailbox.finishRead((unsigned)slot);vTaskDelay(1);
  }
}
bool begin(TFT_eSPI &display,uint16_t *stripe){
  if(worker)return true;
  if(!stripe)return false;
  for(unsigned i=0;i<2;++i){
    jobs[i].pixels=(uint8_t*)heap_caps_malloc(256U*240U,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!jobs[i].pixels){for(unsigned j=0;j<i;++j){heap_caps_free(jobs[j].pixels);jobs[j].pixels=nullptr;}return false;}
  }
  lastDisplayed=(uint8_t*)heap_caps_malloc(256U*240U,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  panel=&display;staging=stripe;
  const BaseType_t loopCore=xPortGetCoreID(),displayCore=loopCore==0?1:0;
  if(xTaskCreatePinnedToCore(displayTask,"SMS_LCD",6144,nullptr,1,&worker,displayCore)!=pdPASS){
    for(unsigned i=0;i<2;++i){heap_caps_free(jobs[i].pixels);jobs[i].pixels=nullptr;}
    heap_caps_free(lastDisplayed);lastDisplayed=nullptr;
    panel=nullptr;staging=nullptr;return false;
  }
  Serial.printf("[VIDEO][SMS] ASYNC loopCore=%d lcdCore=%d indexed=ON snapshots=2x61440B+palettes stripe=5120B DMA=OFF dirty=%s\n",
    (int)loopCore,(int)displayCore,lastDisplayed?"ON":"OFF");
  return true;
}
bool submitIndexed(const uint8_t *frame,const uint16_t palette[256],bool enlarged,unsigned activeHeight){
  if(!worker||!frame||!palette||!sms_video_height_valid(activeHeight))return false;
  bool replaced=false;const int slot=mailbox.beginWrite(replaced);if(slot<0)return false;
  memcpy(jobs[slot].pixels,frame,256U*activeHeight);
  memcpy(jobs[slot].palette,palette,sizeof jobs[slot].palette);
  jobs[slot].height=activeHeight;jobs[slot].enlarged=enlarged;jobs[slot].sequence=++publishedSequence;

  mailbox.publish((unsigned)slot);xTaskNotifyGive(worker);return true;
}
void drain(){
  const int64_t drainStarted=esp_timer_get_time();
  while(!mailbox.idle()){
    if(esp_timer_get_time()-drainStarted>10000000LL){Serial.println("[VIDEO][FATAL] SPI nao terminou em 10s");abort();}
    vTaskDelay(1);
  }
}
void invalidate(){drain();referenceValid=false;}
}
