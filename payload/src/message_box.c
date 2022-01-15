#include "global.h"
#include "message_box.h"

static const u16 sMessages_Pal[] = INCBIN_U16("graphics/messages.gbapal");
static const u8 sMessages_Tilemap[] = INCBIN_U8("graphics/messages.bin.lz");
static const u8 sMessages_Gfx[] = INCBIN_U8("graphics/messages.4bpp.lz");

void MessageBox_Load(void)
{
    REG_DISPCNT = 0;
    REG_BG0HOFS = 0;
    REG_BG0VOFS = 0;
    REG_BLDCNT = 0;
    LZ77UnCompVram(sMessages_Gfx, (void *)BG_VRAM);
    LZ77UnCompVram(sMessages_Tilemap, (void *)BG_SCREEN_ADDR(28));
    CpuCopy16(sMessages_Pal, (void *)BG_PLTT, BG_PLTT_SIZE);
    REG_BG0CNT = BGCNT_SCREENBASE(28) | BGCNT_TXT512x512;
    REG_DISPCNT = DISPCNT_BG0_ON;
}

void MessageBox_Display(enum Message msg)
{
    switch (msg)
    {
    case MSG_WILL_NOW_UPDATE:
        REG_BG0HOFS = 0;
        REG_BG0VOFS = 0;
        break;
    case MSG_HAS_BEEN_UPDATED:
        REG_BG0HOFS = 0x100;
        REG_BG0VOFS = 0;
        break;
    case MSG_UNABLE_TO_UPDATE:
        REG_BG0HOFS = 0x100;
        REG_BG0VOFS = 0xB0;
        break;
    case MSG_NO_NEED_TO_UPDATE:
        REG_BG0HOFS = 0;
        REG_BG0VOFS = 0xB0;
        break;
    case MSG_UPDATING:
        REG_BG0HOFS = 0;
        REG_BG0VOFS = 0x160;
        break;
    }
}
