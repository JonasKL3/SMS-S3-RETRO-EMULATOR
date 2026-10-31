#pragma once
#include <stdint.h>
#include <stddef.h>
namespace SmsCore {
static constexpr uint16_t WIDTH=256,HEIGHT=240;
static constexpr size_t FRAME_PIXELS=(size_t)WIDTH*HEIGHT,FRAME_BYTES=FRAME_PIXELS*2;
bool beginPlatform(uint16_t *framebuffer,size_t bytes);
void endPlatform();
bool loadRomPath(const char *absoluteVfsPath);
void unloadRom();
bool loaded();
void reset();
int runFrame();
void setFrameRendering(bool enabled);
void setButtons(uint16_t buttons);
uint16_t *framebuffer(); // Compatibility RGB surface; refresh only when inspecting.
const uint8_t *indexedFramebuffer();
void copyPalette(uint16_t out[256]); // Native RGB565, stable snapshot for the worker.
void copyRgbFrame(); // Explicit slow compatibility/inspection conversion.

unsigned activeHeight();
uint32_t framePeriodUs();
size_t romSize();
uint32_t romHash();
int lastLoadIoError();
size_t stateRamSize();
uint8_t *cartRamData();
size_t cartRamSize();
bool hasBatteryRam();
bool soundMasterEnabled();
size_t stateSize();
bool saveState(void *buffer,size_t bytes);
bool loadState(const void *buffer,size_t bytes);
}
