// Compare the optimized helpers against the original table-generating rules,
// not another copy of the replacement operand/half-carry formula.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define SMS_Z80_FLAGS_ONLY
extern "C" {
#include "../firmware/SMS_S3/src/smsplus/cpu/z80.c"
}
#undef malloc
#undef calloc
#undef free

static unsigned upstreamAdd(unsigned oldval,unsigned newval,unsigned carry){
  const int val=(int)newval-(int)oldval-(int)carry;
  unsigned flags=newval?((newval&0x80)?0x80:0):0x40;
  flags|=newval&(0x20|0x08);
  if(carry?((newval&15)<=(oldval&15)):((newval&15)<(oldval&15)))flags|=0x10;
  if(carry?(newval<=oldval):(newval<oldval))flags|=1;
  if((val^(int)oldval^0x80)&(val^(int)newval)&0x80)flags|=4;
  return flags;
}
static unsigned upstreamSub(unsigned oldval,unsigned newval,unsigned carry){
  const int val=(int)oldval-(int)newval-(int)carry;
  unsigned flags=2|(newval?((newval&0x80)?0x80:0):0x40);
  flags|=newval&(0x20|0x08);
  if(carry?((newval&15)>=(oldval&15)):((newval&15)>(oldval&15)))flags|=0x10;
  if(carry?(newval>=oldval):(newval>oldval))flags|=1;
  if((val^(int)oldval)&((int)oldval^(int)newval)&0x80)flags|=4;
  return flags;
}
int main(){
#ifdef _WIN32
  _set_error_mode(1);_set_abort_behavior(0,3);
#endif
  unsigned cases=0;
  for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b)for(unsigned carry=0;carry<2;++carry){
    const unsigned add=(a+b+carry)&255,sub=(a-b-carry)&255;
    assert(sms_z80_flags_add(a,b,carry,add)==upstreamAdd(a,add,carry));
    assert(sms_z80_flags_sub(a,b,carry,sub)==upstreamSub(a,sub,carry));
    ++cases;
    if(!carry){
      const unsigned cp=(sms_z80_flags_sub(a,b,0,sub)&~0x28U)|(b&0x28U);
      assert(cp==((upstreamSub(a,sub,0)&~0x28U)|(b&0x28U)));
    }
  }
  printf("Z80 flags PASS: %u operand/carry cases, ADD/ADC/SUB/SBC and 65536 CP cases exactly match original tables\n",cases);
}
