#pragma once
#include <stdint.h>
#include <stddef.h>

static constexpr bool sms_video_due(int64_t now,int64_t last,int64_t interval){return last==0||now-last>=interval;}
static constexpr bool sms_should_render(uint32_t frame,bool native,int64_t behind,uint32_t frameUs){
  return (native||(frame&1U)==0)&&(behind<(int64_t)frameUs*2||(frame&3U)==0);
}
static constexpr bool sms_video_height_valid(unsigned height){return height==192||height==224||height==240;}
// Indexed source, expanded only in the worker's small internal-RAM stripe.
// Source stride256; inactive rows are never read. Original RGB565 channels stay
// unchanged; native mode performs palette expansion without scaling.
static inline void sms_expand_indexed_stripe(const uint8_t *src,const uint16_t palette[256],uint16_t *dst,
    unsigned activeHeight,unsigned firstY,unsigned rows,bool enlarged){
  const unsigned width=enlarged?320U:256U;
  for(unsigned r=0;r<rows;++r){
    const unsigned sy=enlarged?(firstY+r)*activeHeight/240U:firstY+r;
    const uint8_t *in=src+(size_t)sy*256U;
    uint16_t *out=dst+(size_t)r*width;
    for(unsigned x=0;x<width;++x)out[x]=palette[in[enlarged?x*256U/320U:x]];
  }
}
