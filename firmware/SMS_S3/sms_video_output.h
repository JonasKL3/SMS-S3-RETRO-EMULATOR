#pragma once
#include <stdint.h>
class TFT_eSPI;
namespace SmsVideoOutput {
// Permanent 320*8-pixel internal-RAM staging stripe supplied by the frontend.
bool begin(TFT_eSPI &display,uint16_t *stripe);
// One reading snapshot plus one replaceable pending snapshot; never writes into
// a buffer the worker is reading. Newly submitted pending frames favor freshness.
// Palette and indices are copied under the same slot ownership; palette writes
// after submit cannot change the in-flight image. No RGB surface is copied.
bool submitIndexed(const uint8_t *indices,const uint16_t palette[256],bool enlarged,unsigned activeHeight);
void drain();
// Producer only. Waits for SPI, invalidates the reference after menu/clear.
void invalidate();
}
