#pragma once
#include <stdint.h>
 
#ifdef __cplusplus
extern "C" {
#endif
 
// ─────────────────────────────────────────────────────────────
// Direction de déplacement
// ─────────────────────────────────────────────────────────────
enum {
    PROTO_DIR_NONE = 0,
    PROTO_DIR_FWD  = 1,
    PROTO_DIR_REV  = 2,
    PROTO_DIR_RSV  = 3
};
 
// ─────────────────────────────────────────────────────────────
// Layout 16 bits CMD (télécommande → robot)
// [15..8] speed8  : vitesse  0–255  (0–100%)
// [7..2]  angle6  : braquage 0–63   (-30° à +30°)
// [1..0]  dir2    : direction FWD/REV/NONE
// ─────────────────────────────────────────────────────────────
static inline uint16_t proto_pack_cmd(uint8_t speed8, uint8_t angle6, uint8_t dir2)
{
    return (uint16_t)(((speed8 & 0xFF) << 8) | ((angle6 & 0x3F) << 2) | (dir2 & 0x03));
}
 
static inline void proto_unpack_cmd(uint16_t w, uint8_t *speed8, uint8_t *angle6, uint8_t *dir2)
{
    if (speed8) *speed8 = (w >> 8) & 0xFF;
    if (angle6) *angle6 = (w >> 2) & 0x3F;
    if (dir2)   *dir2   =  w       & 0x03;
}
 
// ─────────────────────────────────────────────────────────────
// Unité de vitesse télémétrie
// ─────────────────────────────────────────────────────────────
enum {
    PROTO_UNIT_KMH = 0,   // km/h  (défaut)
    PROTO_UNIT_MPS = 1,   // m/s
    PROTO_UNIT_RPM = 2,   // RPM moteur
};
 
// ─────────────────────────────────────────────────────────────
// Télémétrie (robot → télécommande)
// speed_x100 : vitesse × 100 (ex: 150 = 1.50 km/h)
// unit       : PROTO_UNIT_KMH / MPS / RPM
// reserved   : libre pour plus tard
// ─────────────────────────────────────────────────────────────
typedef struct __attribute__((packed)) {
    int16_t  speed_x100;
    uint8_t  unit;
    uint8_t  reserved;
} proto_tlm_t;
 
#ifdef __cplusplus
}
#endif