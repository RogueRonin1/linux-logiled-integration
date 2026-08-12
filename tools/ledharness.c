/* Drives Warhammer 3's own libled.dll directly, without launching the game.
 *
 * libled.dll resolves the LogiLED SDK from
 *   HKLM\SOFTWARE\Classes\CLSID\{A6519E67-...}\ServerBinary
 * then WinVerifyTrust's it, checks the signer is "Logitech Inc", LoadLibrary's
 * it and GetProcAddress's the LogiLed* exports. Constructing
 * EMPIRECOMMON::LOGITECH_HARDWARE_ACCESS triggers that whole path, and
 * IsLEDInitialised() reports whether it succeeded.
 *
 * The object's real size is unknown, so `this` is an over-allocated zeroed
 * block.
 */
#include <windows.h>
#include <stdio.h>

#define OBJ_SLACK (64 * 1024)

typedef void *(__cdecl *ctor_t)(void *self);
typedef bool  (__cdecl *isinit_t)(void *self);
typedef void  (__cdecl *voidfn_t)(void *self);

int wmain(int argc, wchar_t **argv)
{
    HMODULE m;
    ctor_t ctor;
    isinit_t is_led, is_gkey;
    voidfn_t setdefault, apply;
    void *obj;
    const wchar_t *dll = (argc > 1) ? argv[1] : L"libled.dll";

    m = LoadLibraryW(dll);
    if (!m) {
        wprintf(L"LoadLibrary(%s) failed: %lu\n", dll,
                (unsigned long)GetLastError());
        return 1;
    }
    wprintf(L"libled.dll loaded at %p\n", (void *)m);

    ctor = (ctor_t)(void *)GetProcAddress(
        m, "??0LOGITECH_HARDWARE_ACCESS@EMPIRECOMMON@@QEAA@XZ");
    is_led = (isinit_t)(void *)GetProcAddress(
        m, "?IsLEDInitialised@LOGITECH_HARDWARE_ACCESS@EMPIRECOMMON@@QEAA_NXZ");
    is_gkey = (isinit_t)(void *)GetProcAddress(
        m, "?IsGKeyInitialised@LOGITECH_HARDWARE_ACCESS@EMPIRECOMMON@@QEAA_NXZ");
    setdefault = (voidfn_t)(void *)GetProcAddress(
        m, "?SetDefaultColourLED@LOGITECH_HARDWARE_ACCESS@EMPIRECOMMON@@QEAAXXZ");
    apply = (voidfn_t)(void *)GetProcAddress(
        m, "?ApplyChanges@LOGITECH_HARDWARE_ACCESS@EMPIRECOMMON@@QEAAXXZ");

    wprintf(L"ctor=%p IsLEDInitialised=%p IsGKeyInitialised=%p\n",
            (void *)ctor, (void *)is_led, (void *)is_gkey);
    if (!ctor || !is_led) {
        wprintf(L"missing required exports\n");
        return 1;
    }

    obj = VirtualAlloc(NULL, OBJ_SLACK, MEM_COMMIT | MEM_RESERVE,
                       PAGE_READWRITE);
    ZeroMemory(obj, OBJ_SLACK);

    wprintf(L"calling constructor...\n");
    ctor(obj);
    wprintf(L"constructor returned\n");

    wprintf(L"IsLEDInitialised  = %s\n", is_led(obj) ? L"TRUE" : L"FALSE");
    if (is_gkey)
        wprintf(L"IsGKeyInitialised = %s\n", is_gkey(obj) ? L"TRUE" : L"FALSE");

    if (setdefault && apply) {
        wprintf(L"SetDefaultColourLED + ApplyChanges...\n");
        setdefault(obj);
        apply(obj);
        wprintf(L"done\n");
    }
    return 0;
}
