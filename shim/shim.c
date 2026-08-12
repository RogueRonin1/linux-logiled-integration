/* Logging-only LogiLED SDK shim (Phase 1 instrument).
 *
 * Impersonates LogitechLedEnginesWrapper.dll. Every export records its call
 * with arguments to the file named by LOGILED_SHIM_LOG and returns success.
 * No hardware is touched here - interpretation lives on the Linux side.
 *
 * Unit trap: the per-key and global calls take 0-100 percentages; the bitmap
 * takes 0-255 bytes. They are logged verbatim, un-normalised.
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define EXPORT __declspec(dllexport)

#define LOGI_BITMAP_WIDTH  21
#define LOGI_BITMAP_HEIGHT 6
#define LOGI_BITMAP_BYTES_PER_KEY 4
#define LOGI_BITMAP_SIZE \
    (LOGI_BITMAP_WIDTH * LOGI_BITMAP_HEIGHT * LOGI_BITMAP_BYTES_PER_KEY)

static CRITICAL_SECTION g_lock;
static FILE *g_log;
static LARGE_INTEGER g_freq, g_start;
static int g_ready;

static double now_ms(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (double)(t.QuadPart - g_start.QuadPart) * 1000.0 / (double)g_freq.QuadPart;
}

static void logf_(const char *fmt, ...)
{
    va_list ap;
    if (!g_log) return;
    EnterCriticalSection(&g_lock);
    fprintf(g_log, "{\"t\":%.3f,", now_ms());
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fprintf(g_log, "}\n");
    fflush(g_log);
    LeaveCriticalSection(&g_lock);
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved)
{
    (void)h; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        char path[MAX_PATH];
        DWORD n;
        InitializeCriticalSection(&g_lock);
        QueryPerformanceFrequency(&g_freq);
        QueryPerformanceCounter(&g_start);
        n = GetEnvironmentVariableA("LOGILED_SHIM_LOG", path, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) {
            /* Steam does not forward our environment to the game, so fall back
             * to a file beside this DLL. */
            n = GetModuleFileNameA(h, path, MAX_PATH);
            if (n > 0 && n < MAX_PATH) {
                char *slash = strrchr(path, '\\');
                if (slash) slash[1] = '\0';
                strncat(path, "logiled-shim-trace.jsonl", MAX_PATH - strlen(path) - 1);
            } else {
                path[0] = '\0';
            }
        }
        if (path[0])
            g_log = fopen(path, "a");
        g_ready = 1;
        logf_("\"op\":\"DllMain\",\"reason\":\"attach\"");
    } else if (reason == DLL_PROCESS_DETACH) {
        logf_("\"op\":\"DllMain\",\"reason\":\"detach\"");
        if (g_log) { fclose(g_log); g_log = NULL; }
    }
    return TRUE;
}

/* --- lifecycle --- */
EXPORT bool LogiLedInit(void)
{
    logf_("\"op\":\"LogiLedInit\"");
    return true;
}

EXPORT bool LogiLedInitWithName(const char *name)
{
    logf_("\"op\":\"LogiLedInitWithName\",\"name\":\"%s\"", name ? name : "");
    return true;
}

EXPORT bool LogiLedGetSdkVersion(int *major, int *minor, int *build)
{
    if (major) *major = 9;
    if (minor) *minor = 0;
    if (build) *build = 0;
    logf_("\"op\":\"LogiLedGetSdkVersion\"");
    return true;
}

EXPORT bool LogiLedSetTargetDevice(int targetDevice)
{
    logf_("\"op\":\"LogiLedSetTargetDevice\",\"target\":%d", targetDevice);
    return true;
}

EXPORT void LogiLedShutdown(void)
{
    logf_("\"op\":\"LogiLedShutdown\"");
}

/* --- save / restore --- */
EXPORT bool LogiLedSaveCurrentLighting(void)
{
    logf_("\"op\":\"LogiLedSaveCurrentLighting\"");
    return true;
}

EXPORT bool LogiLedRestoreLighting(void)
{
    logf_("\"op\":\"LogiLedRestoreLighting\"");
    return true;
}

EXPORT bool LogiLedSaveLightingForKey(int keyName)
{
    logf_("\"op\":\"LogiLedSaveLightingForKey\",\"key\":%d", keyName);
    return true;
}

EXPORT bool LogiLedRestoreLightingForKey(int keyName)
{
    logf_("\"op\":\"LogiLedRestoreLightingForKey\",\"key\":%d", keyName);
    return true;
}

/* --- colour setters (percentages 0-100) --- */
EXPORT bool LogiLedSetLighting(int r, int g, int b)
{
    logf_("\"op\":\"LogiLedSetLighting\",\"pct\":[%d,%d,%d]", r, g, b);
    return true;
}

EXPORT bool LogiLedSetLightingForKeyWithScanCode(int code, int r, int g, int b)
{
    logf_("\"op\":\"SetLightingForKeyWithScanCode\",\"code\":%d,\"pct\":[%d,%d,%d]",
          code, r, g, b);
    return true;
}

EXPORT bool LogiLedSetLightingForKeyWithHidCode(int code, int r, int g, int b)
{
    logf_("\"op\":\"SetLightingForKeyWithHidCode\",\"code\":%d,\"pct\":[%d,%d,%d]",
          code, r, g, b);
    return true;
}

EXPORT bool LogiLedSetLightingForKeyWithQuartzCode(int code, int r, int g, int b)
{
    logf_("\"op\":\"SetLightingForKeyWithQuartzCode\",\"code\":%d,\"pct\":[%d,%d,%d]",
          code, r, g, b);
    return true;
}

EXPORT bool LogiLedSetLightingForKeyWithKeyName(int keyName, int r, int g, int b)
{
    logf_("\"op\":\"SetLightingForKeyWithKeyName\",\"key\":%d,\"pct\":[%d,%d,%d]",
          keyName, r, g, b);
    return true;
}

EXPORT bool LogiLedSetLightingForTargetZone(int deviceType, int zone,
                                            int r, int g, int b)
{
    logf_("\"op\":\"SetLightingForTargetZone\",\"dev\":%d,\"zone\":%d,\"pct\":[%d,%d,%d]",
          deviceType, zone, r, g, b);
    return true;
}

/* --- bitmap (bytes 0-255, 21x6 RGBA raster from ESC) --- */
EXPORT bool LogiLedSetLightingFromBitmap(unsigned char bitmap[])
{
    if (!bitmap) {
        logf_("\"op\":\"SetLightingFromBitmap\",\"null\":true");
        return true;
    }
    if (g_log) {
        int i;
        EnterCriticalSection(&g_lock);
        fprintf(g_log, "{\"t\":%.3f,\"op\":\"SetLightingFromBitmap\",\"rgba\":\"",
                now_ms());
        for (i = 0; i < LOGI_BITMAP_SIZE; i++)
            fprintf(g_log, "%02x", bitmap[i]);
        fprintf(g_log, "\"}\n");
        fflush(g_log);
        LeaveCriticalSection(&g_lock);
    }
    return true;
}

EXPORT bool LogiLedExcludeKeysFromBitmap(int *keyList, int listCount)
{
    (void)keyList;
    logf_("\"op\":\"ExcludeKeysFromBitmap\",\"count\":%d", listCount);
    return true;
}

/* --- time-based effects (daemon reimplements these locally) --- */
EXPORT bool LogiLedFlashLighting(int r, int g, int b, int durMs, int intervalMs)
{
    logf_("\"op\":\"LogiLedFlashLighting\",\"pct\":[%d,%d,%d],\"dur\":%d,\"iv\":%d",
          r, g, b, durMs, intervalMs);
    return true;
}

EXPORT bool LogiLedPulseLighting(int r, int g, int b, int durMs, int intervalMs)
{
    logf_("\"op\":\"LogiLedPulseLighting\",\"pct\":[%d,%d,%d],\"dur\":%d,\"iv\":%d",
          r, g, b, durMs, intervalMs);
    return true;
}

EXPORT bool LogiLedFlashSingleKey(int keyName, int r, int g, int b,
                                  int durMs, int intervalMs)
{
    logf_("\"op\":\"LogiLedFlashSingleKey\",\"key\":%d,\"pct\":[%d,%d,%d],\"dur\":%d,\"iv\":%d",
          keyName, r, g, b, durMs, intervalMs);
    return true;
}

EXPORT bool LogiLedPulseSingleKey(int keyName, int rS, int gS, int bS,
                                  int rE, int gE, int bE,
                                  int durMs, bool isInfinite)
{
    logf_("\"op\":\"LogiLedPulseSingleKey\",\"key\":%d,\"from\":[%d,%d,%d],"
          "\"to\":[%d,%d,%d],\"dur\":%d,\"inf\":%s",
          keyName, rS, gS, bS, rE, gE, bE, durMs, isInfinite ? "true" : "false");
    return true;
}

EXPORT bool LogiLedStopEffects(void)
{
    logf_("\"op\":\"LogiLedStopEffects\"");
    return true;
}

EXPORT bool LogiLedStopEffectsOnKey(int keyName)
{
    logf_("\"op\":\"LogiLedStopEffectsOnKey\",\"key\":%d", keyName);
    return true;
}
