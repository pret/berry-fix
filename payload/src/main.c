#include "global.h"
#include "main.h"
#include "rtc.h"
#include "berry_fix_save.h"
#include "event_data.h"
#include "message_box.h"

#define ROM_HEADER_MAGIC 0x96
#define ROM_GAME_TITLE_LEN 15

// States for the main BerryFix function
enum {
    STATE_INIT,
    STATE_CHECK_RTC,
    STATE_CHECK_FLASH,
    STATE_READ_SAVE,
    STATE_CHECK_TIME,
    STATE_FIX_DATE,
    STATE_NO_NEED_TO_FIX,
    STATE_ERROR_YEAR,
    STATE_FINISHED,
    STATE_CHECK_PACIFIDLOG_TM,
    STATE_FIX_PACIFIDLOG_TM,
    STATE_ERROR
};

// Return values for ValidateRomHeader
enum
{
    UPDATE_SAPPHIRE = 2,
    UPDATE_RUBY,
    NO_UPDATE_SAPPHIRE,
    NO_UPDATE_RUBY,
    INVALID
};

static s32 sInitialWaitTimer;
IntrFunc gIntrTable[16];
u16 gHeldKeys;
u16 gNewKeys;
u8 gIntrVector[0x100];
u32 gUpdateSuccessful;
u32 gUnusedVar;
u32 gUnusedBuffer[25];
u32 gMainCallbackState;
u32 gGameVersion;

static EWRAM_DATA u8 sSharedMem[0x8000] = {};

extern void IntrMain(void);

static void ReadKeys(void);
static void IntrDummy(void);
static void SerialIntr(void);
static void BerryFix(u32 *, void *, void *);

static const char sBerryFixGameCode[] = "AGBJ";

const IntrFunc gIntrFuncPointers[] = {
    IntrDummy,
    SerialIntr,
    IntrDummy,
    IntrDummy,
    IntrDummy,
    IntrDummy,
    IntrDummy,
    IntrDummy,
    IntrDummy,
    IntrDummy,
    NULL,
    NULL,
    NULL,
};

// Language character code, followed by version number
static const char sVersionData[][2] = {
    {'J', 1},
    {'E', 2},
    {'D', 1},
    {'F', 1},
    {'I', 1},
    {'S', 1}
};
static const char sRubyTitleAndCode[ROM_GAME_TITLE_LEN] = "POKEMON RUBYAXV";
static const char sSapphireTitleAndCode[ROM_GAME_TITLE_LEN] = "POKEMON SAPPAXP";
static const u16 sDebugPals[20] = {
    RGB_BLACK,
    RGB_RED,
    RGB_GREEN,
    RGB_BLUE
};
static const u16 sDebugDigitsGfx[] = INCBIN_U16("graphics/debug_digits.4bpp");

void AgbMain(void)
{
    RegisterRamReset(RESET_IWRAM | RESET_PALETTE | RESET_VRAM | RESET_OAM);
    DmaCopy32(3, gIntrFuncPointers, gIntrTable, sizeof(gIntrFuncPointers));
    DmaCopy32(3, IntrMain, gIntrVector, sizeof(gIntrVector));
    INTR_VECTOR = gIntrVector;
    REG_IE = INTR_FLAG_VBLANK;
    if (*RomHeaderMagic == ROM_HEADER_MAGIC && *(u32 *)RomHeaderGameCode == *(u32 *)sBerryFixGameCode)
        REG_IE |= INTR_FLAG_GAMEPAK;
    REG_DISPSTAT = DISPSTAT_VBLANK_INTR;
    REG_IME = INTR_FLAG_VBLANK;
    MessageBox_Load();
    gMainCallbackState = STATE_INIT;
    gUnusedVar = 0;
    for (;;)
    {
        VBlankIntrWait();
        ReadKeys();
        BerryFix(&gMainCallbackState, gUnusedBuffer, sSharedMem);
    }
}

static void SerialIntr(void)
{}

static void IntrDummy(void)
{}

static void ReadKeys(void)
{
    u16 keyInput = REG_KEYINPUT ^ KEYS_MASK;
    gNewKeys = keyInput & ~gHeldKeys;
    gHeldKeys = keyInput;
}

// Unused
static void fill_palette(const u8 * src, u16 * dest, u8 value)
{
    s32 i;
    for (i = 0; src[i] != 0; i++)
        dest[i] = src[i] | value << 12;
}

static bool32 CheckSameString(const char * a, const char * b, size_t size)
{
    s32 i;
    for (i = 0; i < size; i++)
    {
        if (a[i] != b[i])
            return FALSE;
    }
    return TRUE;
}

static s32 ValidateGameVersion(void)
{
    char languageCode = RomHeaderGameCode[3];
    s32 softwareVersion = *RomHeaderSoftwareVersion;
    s32 shouldUpdate = -1;
    s32 i;

    // Check rom header data to see if games of this
    // language and revision need the berry fix.
    for (i = 0; i < ARRAY_COUNT(sVersionData); i++)
    {
        if (languageCode == sVersionData[i][0])
        {
            if (softwareVersion >= sVersionData[i][1])
                shouldUpdate = FALSE;
            else
                shouldUpdate = TRUE;
            break;
        }
    }
    if (shouldUpdate != -1)
    {
        // A valid language/revision was found, check game title
        // and code to see if it's Ruby or Sapphire

        if (CheckSameString(RomHeaderGameTitle, sRubyTitleAndCode, ROM_GAME_TITLE_LEN) == TRUE)
        {
            if (shouldUpdate == FALSE)
            {
                return NO_UPDATE_RUBY;
            }
            else
            {
                gGameVersion = VERSION_RUBY;
                return UPDATE_RUBY;
            }
        }
        else if (CheckSameString(RomHeaderGameTitle, sSapphireTitleAndCode, ROM_GAME_TITLE_LEN) == TRUE)
        {
            if (shouldUpdate == FALSE)
            {
                return NO_UPDATE_SAPPHIRE;
            }
            else
            {
                gGameVersion = VERSION_SAPPHIRE;
                return UPDATE_SAPPHIRE;
            }
        }
    }
    return INVALID;
}

static s32 ValidateRomHeader(void)
{
    if (RomHeaderMakerCode[0] == '0' && RomHeaderMakerCode[1] == '1' && *RomHeaderMagic == ROM_HEADER_MAGIC)
        return ValidateGameVersion();
    else
        return INVALID;
}

static void BerryFix(u32 * state, void * unused1, void * unused2)
{
    u8 year;
    switch (*state)
    {
    case STATE_INIT:
        // "The Berry Program Update will now begin..."
        MessageBox_Display(MSG_WILL_NOW_UPDATE);
        if (++sInitialWaitTimer >= 180)
        {
            sInitialWaitTimer = 0;
            gUpdateSuccessful = 0;
            switch (ValidateRomHeader())
            {
            case UPDATE_SAPPHIRE:
            case UPDATE_RUBY:
                ++(*state); // STATE_CHECK_RTC
                break;
            case INVALID: // Invalid header
                *state = STATE_ERROR;
                break;
            case NO_UPDATE_SAPPHIRE:
            case NO_UPDATE_RUBY:
                *state = STATE_NO_NEED_TO_FIX;
                break;
            }
        }
        break;
    case STATE_CHECK_RTC:
        if (!BerryFix_TryInitRtc())
            *state = STATE_ERROR;
        else
            ++(*state); // STATE_CHECK_FLASH
        break;
    case STATE_CHECK_FLASH:
        if (BerryFix_IdentifyFlash() == TRUE)
            ++(*state); // STATE_READ_SAVE
        else
            *state = STATE_ERROR;
        break;
    case STATE_READ_SAVE:
        if (BerryFix_LoadSave(0) == SAVE_STATUS_OK)
            ++(*state); // STATE_CHECK_TIME
        else
            *state = STATE_ERROR;
        break;
    case STATE_CHECK_TIME:
        if (BerryFix_CalcTimeDifference(&year) == TRUE)
        {
            // Time difference is okay, only fix the date if
            // the Berry Glitch hasn't happened yet (if year is 2000)
            if (year == 0)
                ++(*state); // STATE_FIX_DATE
            else
                *state = STATE_CHECK_PACIFIDLOG_TM;
        }
        else
        {
            // Time difference is incorrect, if the year is 2001
            // then the Berry Glitch is occurring. If the year is
            // not 2001 then some error has occurred.
            if (year != 1)
                *state = STATE_ERROR_YEAR;
            else
                ++(*state); // STATE_FIX_DATE
        }
        break;
    case STATE_FIX_DATE:
        // Set the clock forward to fix the Berry Glitch
        // If the date is late enough that it is no
        // longer affected then this does nothing.
        BerryFix_TryFixDate();
        gUpdateSuccessful |= 1;
        *state = STATE_CHECK_PACIFIDLOG_TM;
        break;
    case STATE_CHECK_PACIFIDLOG_TM:
        if (BerryFix_IsPacifidlogTMCorrect() == TRUE)
            *state = STATE_FINISHED;
        else
            *state = STATE_FIX_PACIFIDLOG_TM;
        break;
    case STATE_FIX_PACIFIDLOG_TM:
        // "Updating. the Berry Program. Please wait..."
        MessageBox_Display(MSG_UPDATING);
        if (BerryFix_ResetPacifidlogTM() == TRUE)
        {
            gUpdateSuccessful |= 1;
            *state = STATE_FINISHED;
        }
        else
        {
            *state = STATE_ERROR;
        }
        break;
    // The below 4 cases are all the possible end states
    // The Berry Fix Program will remain in these states until
    // the player turns off the GBA.
    case STATE_FINISHED:
        if (gUpdateSuccessful == 0)
            *state = STATE_NO_NEED_TO_FIX;
        else // "Your Berry Program has been updated"
            MessageBox_Display(MSG_HAS_BEEN_UPDATED);
        break;
    case STATE_NO_NEED_TO_FIX:
        // "There is no need to update your Berry Program"
        MessageBox_Display(MSG_NO_NEED_TO_UPDATE);
        break;
    case STATE_ERROR_YEAR:
        // "Unable to update the Berry Program"
        MessageBox_Display(MSG_UNABLE_TO_UPDATE);
        break;
    case STATE_ERROR:
        // "Unable to update the Berry Program"
        MessageBox_Display(MSG_UNABLE_TO_UPDATE);
        break;
    }
}

static void Debug_LoadDigitsPal(void)
{
    s32 i;
    const u16 * src;
    vu16 * dest = (vu16 *)BG_PLTT + 1;
    DmaFill16(3, RGB_WHITE, (vu16 *)BG_PLTT, BG_PLTT_SIZE);
    src = sDebugPals;
    for (i = 0; i < 4; i++)
    {
        *dest = *src;
        dest += 16;
        src++;
    }
}

// Unused
static void Debug_LoadDigits(void)
{
    DmaFill16(3, 0x1111, (void *)VRAM + 0x8420, 0x1800);
    DmaCopy32(3, sDebugDigitsGfx, (void *)VRAM + 0x8600, 0x200);
    Debug_LoadDigitsPal();
}
