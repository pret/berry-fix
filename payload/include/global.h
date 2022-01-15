#ifndef GUARD_GLOBAL_H
#define GUARD_GLOBAL_H

#include "gba/gba.h"
#include "constants/vars.h"

// IDE support
#if defined(__APPLE__) || defined(__CYGWIN__) || defined(__INTELLISENSE__)
// We define these when using certain IDEs to fool preproc
#define _(x)        (x)
#define __(x)       (x)
#define INCBIN(...) {0}
#define INCBIN_U8   INCBIN
#define INCBIN_U16  INCBIN
#define INCBIN_U32  INCBIN
#define INCBIN_S8   INCBIN
#define INCBIN_S16  INCBIN
#define INCBIN_S32  INCBIN
#endif // IDE support

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

#define RomHeaderGameTitle       ((const char *)0x080000A0)
#define RomHeaderGameCode        ((const char *)0x080000AC)
#define RomHeaderMakerCode       ((const char *)0x080000B0)
#define RomHeaderMagic           ((const u8 *)  0x080000B2)
#define RomHeaderSoftwareVersion ((const u8 *)  0x080000BC)

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) >= (b) ? (a) : (b))

enum
{
    VERSION_SAPPHIRE = 1,
    VERSION_RUBY = 2,
    VERSION_EMERALD = 3,
};

enum LanguageId
{
    LANGUAGE_JAPANESE = 1,
    LANGUAGE_ENGLISH = 2,
    LANGUAGE_GERMAN = 5,
};

struct Time
{
    /*0x00*/ s16 days;
    /*0x02*/ s8 hours;
    /*0x03*/ s8 minutes;
    /*0x04*/ s8 seconds;
};

// Dummy Ruby/Sapphire save structs.
// Only vars, localTimeOffset, and lastBerryTreeUpdate are needed.

struct SaveBlock1
{
    u8 dummy_0[0x1340];
    u16 vars[VARS_COUNT];
    u8 dummy_1[0x2580];
};
extern struct SaveBlock1 gSaveBlock1;

struct SaveBlock2
{
    u8 dummy_0[0x98];
    struct Time localTimeOffset;
    struct Time lastBerryTreeUpdate;
    u8 dummy_1[0x7E8];
};
extern struct SaveBlock2 gSaveBlock2;

struct PokemonStorage
{
    u8 dummy[0x83D0];
};
extern struct PokemonStorage gPokemonStorage;

#endif //GUARD_GLOBAL_H
