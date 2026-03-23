#pragma once
#include <stdint.h>
 
#ifdef __cplusplus
extern "C" {
#endif
 
//#define MSG_DATA_LEN 8
// MSG_DATA_LEN doit rester 2 dans com.h
// Format 16 bits: [15..3]=speed13, [2..1]=dir2, [0]=type1
 
enum {
    PROTO_TYPE_CMD = 0,
    PROTO_TYPE_TLM = 1
};
 
enum {
    PROTO_DIR_NONE = 0,
    PROTO_DIR_FWD  = 1,
    PROTO_DIR_REV  = 2,
    PROTO_DIR_RSV  = 3
};
 
static inline uint16_t proto_pack(uint16_t speed13, uint8_t dir2, uint8_t type1)
{
    return (uint16_t)(((speed13 & 0x1FFF) << 3) | ((dir2 & 0x03) << 1) | (type1 & 0x01));
}
 
static inline void proto_unpack(uint16_t w, uint16_t *speed13, uint8_t *dir2, uint8_t *type1)
{
    if (speed13) *speed13 = (w >> 3) & 0x1FFF;
    if (dir2)    *dir2    = (w >> 1) & 0x03;
    if (type1)   *type1   =  w       & 0x01;
}
 
#ifdef __cplusplus
}
#endif