#pragma once
/* SMS-S3 port, 2026-10-31: fixed-width types, bounded platform allocation. */
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void *sms_port_malloc(size_t size);
void *sms_port_table_alloc(size_t size);
void *sms_port_calloc(size_t count,size_t size);
void sms_port_free(void *ptr);
uint32_t sms_port_crc32(uint32_t crc,const void *data,size_t size);
int sms_port_prepare_tables(void);
void sms_port_release_tables(void);
#ifdef __cplusplus
}
#endif
