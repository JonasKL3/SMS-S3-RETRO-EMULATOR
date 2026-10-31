#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifndef SMS_VFS_MOUNT
#define SMS_VFS_MOUNT "/sd"
#endif
#include <string.h>

// Layout oficial do SMS-S3. Todos os caminhos sao absolutos a partir da raiz do SD.
// Mantemos os nomes da estrutura apresentada pelo usuario.
namespace SmsPaths {
static constexpr char ROOT[] = "/JOGOS/SMS";
static constexpr char ROMS[] = "/JOGOS/SMS/jogos";
static constexpr char ORIGINAL_COVERS[] = "/JOGOS/SMS/capas_originais";
static constexpr char COVER[] = "/JOGOS/SMS/cover";
static constexpr char PNG[] = "/JOGOS/SMS/png";
static constexpr char CACHE_INDICES[] = "/JOGOS/SMS/cache_indices";
static constexpr char REPORTS[] = "/JOGOS/SMS/relatorios";
static constexpr char SAVE[] = "/JOGOS/SMS/save";

inline bool makeVfsPath(char *out, size_t capacity, const char *sdPath) {
  if (!out || capacity == 0 || !sdPath || sdPath[0] != '/') return false;
  const int n = snprintf(out, capacity, "%s%s", SMS_VFS_MOUNT, sdPath);
  return n > 0 && (size_t)n < capacity;
}

inline bool makeSramPath(char *out, size_t capacity, uint32_t pathHash, uint32_t romHash) {
  if (!out || capacity == 0) return false;
  const int n = snprintf(out, capacity, "%s/%08lX_%08lX.sav", SAVE,
                         (unsigned long)pathHash, (unsigned long)romHash);
  return n > 0 && (size_t)n < capacity;
}

inline bool makeStatePath(char *out, size_t capacity, uint32_t pathHash,
                          uint32_t romHash, int slot, const char *suffix = "") {
  if (!out || capacity == 0 || ((slot < 0 || slot > 3) && slot != 9)) return false;
  const int n = snprintf(out, capacity, "%s/%08lX_%08lX.ss%d%s", SAVE,
                         (unsigned long)pathHash, (unsigned long)romHash,
                         slot, suffix ? suffix : "");
  return n > 0 && (size_t)n < capacity;
}

inline bool makeCoverPath(char *out, size_t capacity, const char *directory,
                          const char *romStem) {
  if (!out || capacity == 0 || !directory || !romStem || !romStem[0]) return false;
  // Prevent a ROM name from escaping its cover directory.
  if (strchr(romStem, '/') || strchr(romStem, '\\')) return false;
  const int n = snprintf(out, capacity, "%s/%s.cov", directory, romStem);
  return n > 0 && (size_t)n < capacity;
}

inline bool isRomPath(const char *path){
  if(!path)return false;const size_t n=strlen(path);
  return n>4 && path[n-4]=='.' && ((path[n-3]|32)=='s') && ((path[n-2]|32)=='m') && ((path[n-1]|32)=='s');
}

inline bool makeRomStem(char *out, size_t capacity, const char *path) {
  if (!out || capacity == 0 || !isRomPath(path)) return false;
  const char *base = strrchr(path, '/');
  base = base ? base + 1 : path;
  const size_t len = strlen(base);
  const size_t ext=4;
  if (len <= ext || len - ext + 1 > capacity) return false;
  memcpy(out, base, len - ext);
  out[len - ext] = '\0';
  return true;
}
}  // namespace SmsPaths
