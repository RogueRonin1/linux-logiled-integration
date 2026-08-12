/* Add a DER-encoded certificate to a system certificate store inside the
 * current Wine prefix. Used to trust the local CA that signs the shim, so
 * libled.dll's WinVerifyTrust check succeeds. Scope is the prefix only. */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>

int wmain(int argc, wchar_t **argv)
{
    HANDLE f;
    DWORD size, got;
    BYTE *buf;
    HCERTSTORE store;
    const wchar_t *storename = (argc > 2) ? argv[2] : L"Root";

    if (argc < 2) {
        wprintf(L"usage: installcert <cert.der> [storename]\n");
        return 1;
    }

    f = CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ, NULL,
                    OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        wprintf(L"open failed: %lu\n", (unsigned long)GetLastError());
        return 1;
    }
    size = GetFileSize(f, NULL);
    buf = (BYTE *)malloc(size);
    if (!ReadFile(f, buf, size, &got, NULL) || got != size) {
        wprintf(L"read failed\n");
        return 1;
    }
    CloseHandle(f);

    store = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0,
                          CERT_SYSTEM_STORE_LOCAL_MACHINE, storename);
    if (!store) {
        wprintf(L"CertOpenStore(%s) failed: %lu\n", storename,
                (unsigned long)GetLastError());
        return 1;
    }

    if (!CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING, buf, size,
                                          CERT_STORE_ADD_REPLACE_EXISTING,
                                          NULL)) {
        wprintf(L"CertAddEncodedCertificateToStore failed: %lu\n",
                (unsigned long)GetLastError());
        return 1;
    }
    CertCloseStore(store, 0);
    wprintf(L"installed %s into LocalMachine\\%s\n", argv[1], storename);
    return 0;
}
