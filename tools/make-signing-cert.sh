#!/usr/bin/env bash
# Generate the local signing material for the shim.
#
# Warhammer 3's libled.dll calls WinVerifyTrust on the SDK DLL and then
# CertGetNameStringW to check the publisher is "Logitech Inc". An unsigned shim
# is rejected outright (TRUST_E_NOSIGNATURE), so we sign it with a locally
# generated cert whose CN matches.
#
# The resulting key is trusted only inside your own Proton prefix. It is
# gitignored and must stay that way.
set -euo pipefail

OUT="${1:-certs}"
mkdir -p "$OUT"
cd "$OUT"

if [[ -f code.key ]]; then
    echo "signing material already exists in $OUT/ — remove it first to regenerate"
    exit 0
fi

# Local CA
openssl req -x509 -newkey rsa:2048 -keyout ca.key -out ca.crt -days 3650 -nodes \
    -subj "/CN=G915 Bridge Local CA/O=g915-bridge" \
    -addext "basicConstraints=critical,CA:TRUE,pathlen:0" \
    -addext "keyUsage=critical,keyCertSign,cRLSign"

# Code-signing leaf. The CN is what libled.dll compares against.
openssl req -newkey rsa:2048 -keyout code.key -out code.csr -nodes \
    -subj "/CN=Logitech Inc/O=Logitech Inc"

cat > ext.cnf <<'EOF'
basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature
extendedKeyUsage=critical,codeSigning
EOF

openssl x509 -req -in code.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
    -out code.crt -days 3650 -extfile ext.cnf

cat code.crt ca.crt > chain.pem
openssl x509 -in ca.crt -outform DER -out ca.der

echo
openssl x509 -in code.crt -noout -subject -issuer
echo
echo "wrote $PWD/{ca.crt,ca.der,ca.key,code.crt,code.key,chain.pem}"
