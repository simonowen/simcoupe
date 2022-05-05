#!/bin/sh
set -e

BUILD_DIR=build-macos
mkdir -p ${BUILD_DIR} && pushd ${BUILD_DIR}

cmake .. \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 \
  -DCMAKE_FIND_LIBRARY_SUFFIXES=".a"

rm -rf SimCoupe.app SimCoupe*.dmg

make -j$(getconf _NPROCESSORS_ONLN)

otool -l SimCoupe.app/Contents/MacOS/SimCoupe | grep minos
otool -L SimCoupe.app/Contents/MacOS/SimCoupe

install_name_tool -add_rpath @executable_path/../Frameworks \
  SimCoupe.app/Contents/MacOS/SimCoupe

mkdir SimCoupe.app/Contents/Frameworks
cp -r /Library/Frameworks/SDL2.framework SimCoupe.app/Contents/Frameworks
cp -r _deps/saasound-build/SAASound.framework SimCoupe.app/Contents/Frameworks

rm -rf dist
mkdir -p dist/.background
cp -r SimCoupe.app dist/
cp ../package/dmg_bkg.png dist/.background/

VERSION=$(plutil -extract CFBundleShortVersionString xml1 -o - ./Info-SimCoupe.plist | \
  sed -n "s/.*<string>\(.*\)<\/string>.*/\1/p")
VOLNAME="SimCoupe ${VERSION}"
VOLPATH="/Volumes/${VOLNAME}"
DMGFILE="SimCoupe-${VERSION}-macos.dmg"

hdiutil create /tmp/tmp.dmg -ov -volname "$VOLNAME" -fs HFS+ -format UDRW -srcfolder dist -noscrub
hdiutil attach -readwrite -noverify -noautoopen /tmp/tmp.dmg
sleep 3

osascript << EOT
  tell application "Finder"
    tell disk "${VOLNAME}"
      open

      tell container window
        set current view to icon view
        set toolbar visible to false
        set statusbar visible to false
        set sidebar width to 0
        set the bounds to {400, 100, 900, 465}
      end tell

      set opts to the icon view options of container window
      tell opts
        set arrangement to not arranged
        set icon size to 128
        set text size to 16
      end tell
      set background picture of opts to file ".background:dmg_bkg.png"

      make new alias file at container window to POSIX file "/Applications" with properties {name:"Applications"}
      set position of item "Applications" to {380, 190}
      set position of item "SimCoupe.app" to {120, 190}

      update without registering applications
      close
    end tell
  end tell
EOT

cp ../package/dmg_volume.icns "${VOLPATH}/.VolumeIcon.icns"
SetFile -a C "${VOLPATH}"

sync
hdiutil detach "${VOLPATH}"
hdiutil convert /tmp/tmp.dmg -format UDZO -o "${DMGFILE}"
echo DMG: ${BUILD_DIR}/${DMGFILE}
