# PLE

Minimal JUCE iOS audio player and AUv3 host.

## Project layout

- `source/app` - JUCE application entrypoint and `MainComponent` composition.
- `source/audio` - playback state, playback controller, and audio file helpers.
- `source/browser` - Documents folder audio browser.
- `source/plugins` - AUv3 discovery, loading, and editor hosting.
- `source/ui` - reusable app views, popup views, and look-and-feel.
- `resources` - bundled fonts and app/document icon.
- `temp` - iOS CMake project, build scripts, shared FetchContent dependencies, and build outputs.

## Builds

Run scripts from `temp`:

```sh
cd temp
./build-ios-device.command
./build-ios-simulator.command
./post-build-ios-device.command
./post-build-ios-simulator.command
```

The build output goes to `temp/build/ios-device` and `temp/build/ios-simulator`.
