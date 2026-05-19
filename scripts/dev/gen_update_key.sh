#!/usr/bin/env bash
# Generates an Ed25519 key pair for SocketSpy release signing.
# Usage: bash scripts/dev/gen_update_key.sh [output_dir]
set -euo pipefail

OUT="${1:-keys}"
mkdir -p "$OUT"

if [[ -f "$OUT/update_privkey.pem" ]]; then
    echo "Key already exists at $OUT/update_privkey.pem — remove it first."
    exit 1
fi

openssl genpkey -algorithm Ed25519 -out "$OUT/update_privkey.pem"
chmod 600 "$OUT/update_privkey.pem"
openssl pkey -pubout -in "$OUT/update_privkey.pem" -out "$OUT/update_pubkey.pem"

echo ""
echo "Generated:"
echo "  Private: $OUT/update_privkey.pem  ← KEEP SECRET"
echo "  Public:  $OUT/update_pubkey.pem   ← embed in gui/src/updater.cpp"
echo ""
echo "Add the private key to GitHub Actions secret: UPDATE_SIGNING_KEY"
echo "  cat $OUT/update_privkey.pem | pbcopy  (macOS)"
echo "  cat $OUT/update_privkey.pem | xclip   (Linux)"
echo ""
cat "$OUT/update_pubkey.pem"
echo ""
echo "WARNING: Never commit update_privkey.pem to git."
