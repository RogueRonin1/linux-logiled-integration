/* Replicates the check libled.dll performs before loading the LogiLED SDK:
 * WinVerifyTrust on the candidate DLL, then CertGetNameStringW on the signer
 * to compare against "Logitech Inc". Tells us whether an unsigned shim can
 * ever be loaded by Warhammer 3 under Proton. */
#include <windows.h>
#include <wintrust.h>
#include <softpub.h>
#include <wincrypt.h>
#include <stdio.h>

static void probe(const wchar_t *path)
{
    WINTRUST_FILE_INFO fi;
    WINTRUST_DATA wd;
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG r;

    ZeroMemory(&fi, sizeof(fi));
    fi.cbStruct = sizeof(fi);
    fi.pcwszFilePath = path;

    ZeroMemory(&wd, sizeof(wd));
    wd.cbStruct = sizeof(wd);
    wd.dwUIChoice = WTD_UI_NONE;
    wd.fdwRevocationChecks = WTD_REVOKE_NONE;
    wd.dwUnionChoice = WTD_CHOICE_FILE;
    wd.pFile = &fi;
    wd.dwStateAction = WTD_STATEACTION_VERIFY;

    r = WinVerifyTrust(NULL, &action, &wd);
    wprintf(L"WinVerifyTrust: 0x%08lX", (unsigned long)r);
    if (r == 0)                    wprintf(L"  (TRUSTED)\n");
    else if (r == (LONG)0x800B0100) wprintf(L"  (TRUST_E_NOSIGNATURE)\n");
    else if (r == (LONG)0x800B0109) wprintf(L"  (CERT_E_UNTRUSTEDROOT)\n");
    else if (r == (LONG)0x800B0111) wprintf(L"  (CERT_E_UNTRUSTEDCA)\n");
    else                            wprintf(L"  (other)\n");

    /* Pull the signer name the way libled would */
    {
        DWORD enc = 0, ctype = 0, fmt = 0;
        HCERTSTORE store = NULL;
        HCRYPTMSG msg = NULL;
        if (CryptQueryObject(CERT_QUERY_OBJECT_FILE, path,
                             CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                             CERT_QUERY_FORMAT_FLAG_BINARY, 0,
                             &enc, &ctype, &fmt, &store, &msg, NULL)) {
            PCCERT_CONTEXT ctx = CertEnumCertificatesInStore(store, NULL);
            if (ctx) {
                wchar_t name[512] = L"";
                CertGetNameStringW(ctx, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL,
                                   name, 512);
                wprintf(L"signer: \"%s\"\n", name);
                CertFreeCertificateContext(ctx);
            } else {
                wprintf(L"signer: <no cert in store>\n");
            }
            if (store) CertCloseStore(store, 0);
            if (msg) CryptMsgClose(msg);
        } else {
            wprintf(L"signer: <CryptQueryObject failed, err=%lu>\n",
                    (unsigned long)GetLastError());
        }
    }

    wd.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(NULL, &action, &wd);
}

int wmain(int argc, wchar_t **argv)
{
    int i;
    if (argc < 2) {
        wprintf(L"usage: trustprobe <file> [file...]\n");
        return 1;
    }
    for (i = 1; i < argc; i++) {
        wprintf(L"=== %s ===\n", argv[i]);
        probe(argv[i]);
        wprintf(L"\n");
    }
    return 0;
}
