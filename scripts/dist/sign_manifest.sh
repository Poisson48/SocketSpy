#!/usr/bin/env bash
# Creates and signs a release manifest for SocketSpy update verification.
# Usage: bash scripts/dist/sign_manifest.sh <SocketSpy-VERSION-x86_64.AppImage> <privkey.pem>
set -euo pipefail

APPIMAGE="$1"
PRIVKEY="$2"

[[ -f "$APPIMAGE" ]] || { echo "AppImage not found: $APPIMAGE"; exit 1; }
[[ -f "$PRIVKEY"  ]] || { echo "Private key not found: $PRIVKEY"; exit 1; }

SHA256=$(sha256sum "$APPIMAGE" | awk '{print $1}')
FILENAME=$(basename "$APPIMAGE")
VERSION=$(echo "$FILENAME" | sed 's/SocketSpy-\(.*\)-x86_64\.AppImage/\1/')
OUTDIR=$(dirname "$APPIMAGE")
MANIFEST="$OUTDIR/release-manifest.json"
SIG="$MANIFEST.sig"

printf '{\n  "version": "%s",\n  "filename": "%s",\n  "sha256": "%s"\n}\n' \
    "$VERSION" "$FILENAME" "$SHA256" > "$MANIFEST"

openssl pkeyutl -sign -inkey "$PRIVKEY" -rawin -in "$MANIFEST" -out "$SIG"

echo "Manifest : $MANIFEST"
echo "Signature: $SIG"
echo "Version  : $VERSION"
echo "SHA-256  : $SHA256"
