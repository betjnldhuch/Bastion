# Bastion Android client integration

## Target experience

The release target is one Android application named **Bastion Online**:
launcher, installer, server selection, and the native multiplayer client run inside
the same APK/process. Players should not have to install or open a second launcher.

This is an integration/rebrand of the client code, not an attempt to conceal its
origin or remove required notices. The SampDroid upstream project is MIT-licensed;
when its code is incorporated, retain its copyright and MIT license text in
`mobile/licenses/SampDroid-MIT.txt` and show third-party notices in
Bastion's About / Open-source licenses screen. The app name, icon, screens, and
navigation can be Bastion-branded where the license permits.

## Current state

The current Bastion installer only downloads Bastion-owned files into its own
private app directory. The separately installed SampDroid app is only a temporary
handoff. Its private files cannot be edited by Bastion, and Bastion assets are not
mounted by that client. Do not describe this handoff as a completed game integration.

The inspected SampDroid Android module currently has:
- application ID and namespace: `com.xykal.sampdroid`;
- launcher activity: `com.xykal.sampdroid.ui.MainActivity`;
- minSdk 24;
- native ABIs `armeabi-v7a` and `arm64-v8a`;
- no documented external connect Intent contract in its manifest.

## Required integration work

1. Bring the compatible client source into a Bastion-controlled fork/module and
   build it as part of the Bastion application. Keep upstream license notices.
2. Change the installed application ID to a Bastion-owned ID only as part of that
   integrated build; do not impersonate or overwrite an independently installed
   upstream app.
3. Add a typed in-process navigation interface from launcher to game client:
   nickname, server hostname, server port. Validate nickname and port before
   connecting; never accept arbitrary executable paths or downloaded code.
4. Replace the temporary external-app handoff with an in-process activity
   transition after the integrated client is built and tested.
5. Define how Bastion-owned resources are loaded by the client. The installer
   must write to a location the integrated client actually reads; a successful
   download alone does not mean a mod is active.
6. Test on Android 7+ and both supported ARM ABIs, then test connection against
   the actual Bastion server before calling the client compatible.

## Future launcher-to-client data contract

The preferred implementation is a typed Kotlin interface in the same app module,
not cross-app SharedPreferences or undocumented private file access:

- `nickname: String`
- `serverHost: String`
- `serverPort: Int`

If a separate-process boundary is ever required, define and document a versioned
Android Intent/API on the client side first. Do not rely on guessed extras or
write into another application's private storage.
