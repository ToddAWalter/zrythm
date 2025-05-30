#! /bin/bash
# SPDX-FileCopyrightText: © 2025 Alexandros Theodotou <alex@zrythm.org>
# SPDX-License-Identifier: LicenseRef-ZrythmLicense

# This also notarizes the app inside the dmg, so no need to notarize the .app
# separately

set -e

dmg_file="$1"
codesign_identity="$2"
notarization_keychain_profile="$3"

# FIXME: hardcoded
gtimeout_cmd="/opt/homebrew/opt/coreutils/bin/gtimeout"

codesign \
  --force --verbose --timestamp \
  --sign "$codesign_identity" \
  "$dmg_file"
codesign --verify --verbose "$dmg_file"

if [ -n "$notarization_keychain_profile" ]; then
  # Use keychain profile if provided
  $gtimeout_cmd 20m xcrun notarytool submit \
    "$dmg_file" \
    --keychain-profile "$notarization_keychain_profile" \
    --wait || { echo "Warning: Notarization timed out"; exit 0; }
else
  # Fall back to environment variables (used in CI)
  $gtimeout_cmd 20m xcrun notarytool submit \
    "$dmg_file" \
    --apple-id "$APPLE_NOTARIZATION_APPLE_ID" \
    --team-id "$APPLE_NOTARIZATION_TEAM_ID" \
    --password "$APPLE_NOTARIZATION_ZRYTHM_APP_SPECIFIC_PASSWORD" \
    --wait || { echo "Warning: Notarization timed out"; exit 0; }
fi

xcrun stapler staple "$dmg_file"

spctl -a -t open --context context:primary-signature -v "$dmg_file"
