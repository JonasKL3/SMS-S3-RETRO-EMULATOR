#pragma once
#include <atomic>
#include <stdint.h>

// Exactly one producer and one consumer. Reading frames remain immutable. A
// pending READY frame can be reclaimed exclusively before replacing its pixels.
class SmsFrameMailbox {
 public:
  int beginWrite(bool &replaced){
    replaced=false;
    for(unsigned pass=0;pass<2;++pass){
      for(unsigned i=0;i<2;++i){
        uint8_t expected=IDLE;
        if(state[i].compare_exchange_strong(expected,WRITING,std::memory_order_acq_rel))return (int)i;
      }
      for(unsigned i=0;i<2;++i){
        uint8_t expected=READY;
        if(state[i].compare_exchange_strong(expected,WRITING,std::memory_order_acq_rel)){replaced=true;return (int)i;}
      }
    }
    return -1;
  }
  void publish(unsigned slot){state[slot].store(READY,std::memory_order_release);}
  int beginRead(){
    for(unsigned i=0;i<2;++i){
      uint8_t expected=READY;
      if(state[i].compare_exchange_strong(expected,READING,std::memory_order_acq_rel))return (int)i;
    }
    return -1;
  }
  void finishRead(unsigned slot){state[slot].store(IDLE,std::memory_order_release);}
  bool idle() const{return state[0].load(std::memory_order_acquire)==IDLE&&state[1].load(std::memory_order_acquire)==IDLE;}
 private:
  enum : uint8_t {IDLE,WRITING,READY,READING};
  std::atomic<uint8_t> state[2]{{IDLE},{IDLE}};
};
