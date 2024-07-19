---
title: DCS Tabs
parent: Troubleshooting
---

# The DCS Tabs

## I still see DCS's built-in kneeboard

OpenKneeboard does not modify or replace DCS's kneeboard; it provides an additional one. If you do not see OpenKneeboard, see the answer below. If you do not want to see DCS's built-in kneeboard, use a different key combination to show/hide it.

## I don't see OpenKneeboard

See troubleshooting pages for:

- [OpenXR](steamvr-or-openxr.md)
- [Non-VR](oculus-or-non-vr.md)
- [Legacy Oculus API (LibOVR)](oculus-or-non-vr.md)
- [Legacy SteamVR API (OpenVR)](steamvr-or-openxr.md)

## None of the tabs ever show anything

1. Check if the Theater tab loads for Caucasus or NTTR; if so, check a different section.
2. Check that DCS is in OpenKneeboard's Games list inside settings, and that the Saved Games path is correct

## The theater tabs only loads for NTTR and Caucasus

DCS World does not include kneeboard images for other maps; OpenKneeboard does not currently generate charts.

## Some built-in aircraft kneeboard pages are not loaded

Dynamic (Lua) kneeboard pages are not supported at all, and are unlikely to be supported in the future. This
includes most of the kneeboard pages for the F14, F16, and F18.

## The path is incorrect for some aircraft

1. [Check if this has already been reported](https://github.com/OpenKneeboard/OpenKneeboard/issues?q=is%3Aissue)
2. If not, please [report the issue](https://github.com/OpenKneeboard/OpenKneeboard/issues/new) so it can be fixed in a future version of OpenKneeboard; include the incorrect path, corrected path, and the module name
3. If you are comfortable editing JSON:
   1. Open `%PROGRAMFILES%\OpenKneeboard\share` in Windows explorer
   2. Open the `Saved Games\OpenKneeboard` folder too
   3. Copy `DCS-Aircraft-Mapping.json` from the `share` folder to OpenKneeboard's Saved Games folder
   4. Edit `Saved Games\OpenKneeboard\DCS-Aircraft-Mapping.json` to include the correction
   5. If it works, attach your modified `DCS-Aircraft-Mapping.json` to your GitHub issue