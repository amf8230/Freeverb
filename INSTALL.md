# Echo Vault: Build & Install Guide

Echo Vault is a vintage-style reverb plugin (VST3 + Audio Unit) with four rooms:
**Chamber, Plate, Room and Hall**. This guide covers getting it built on GitHub and
installed on a Mac running Studio One.

---

## Part 1: Build it on GitHub (one-time setup, about 15 minutes)

GitHub compiles the plugin for Mac automatically in the cloud, so you don't need a Mac or
any developer tools to build it.

1. **Create a free account** at <https://github.com/signup> (skip this if you already have one).
2. **Create a repository.** Click the **+** in the top-right corner, choose **New repository**,
   name it `echo-vault`, choose **Public** or **Private**, and click **Create repository**.
   * Public: build minutes are unlimited and free.
   * Private: the free plan still gives roughly 20+ Mac builds per month, which is plenty.
3. **Upload the files.** On the new repository page, click **uploading an existing file**.
   Unzip `EchoVault-source.zip` on your computer, open the `EchoVault` folder, select
   **everything inside it** (including the `.github` folder), and drag it into the browser.
   Click **Commit changes**.
   > On Windows, if you don't see the `.github` folder, turn on *View > Show > Hidden items*
   > in File Explorer. That folder is what tells GitHub how to build the plugin.
4. **Wait for the build.** Click the **Actions** tab. A run called *Build Echo Vault*
   starts automatically and takes about 8 to 12 minutes. A green check means it finished.
   (If Actions asks you to enable workflows, click the green button to allow them.)
5. **Download the plugin.** Click the finished run, scroll to **Artifacts**, and click
   **EchoVault-macOS** to download it. Copy the downloaded file to your dad's Mac
   (AirDrop, USB stick, email, or Google Drive).

To rebuild later (for example after a change), open **Actions > Build Echo Vault > Run workflow**.

---

## Part 2: Install on the Mac

1. Double-click the downloaded file to unzip it. You may need to unzip twice, since GitHub
   wraps it in an extra zip. You should see a folder named **EchoVault**.
2. **Easiest way:** right-click `install-echo-vault.command`, choose **Open**, then click
   **Open** again in the warning box. A Terminal window runs the installer.

   **Or do it manually in Terminal** (Applications > Utilities > Terminal). Type `cd `
   (with a space after it), drag the EchoVault folder into the Terminal window, press Return,
   then paste:

   ```bash
   mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
   cp -R "Echo Vault.vst3" ~/Library/Audio/Plug-Ins/VST3/
   cp -R "Echo Vault.component" ~/Library/Audio/Plug-Ins/Components/
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Echo Vault.vst3"
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"Echo Vault.component"
   ```

3. **Restart Studio One.** It scans for new plugins at startup. Echo Vault will appear in
   the Browser under **Effects > Vault Audio > Echo Vault**. (Both a VST3 and an AU version
   are installed. Either works; the VST3 one is recommended.)

**If Studio One doesn't list it:** go to *Studio One > Settings (Options) > Locations >
VST Plug-Ins*, make sure `~/Library/Audio/Plug-Ins/VST3` is listed, and click
**Reset Blocklist**. Then restart Studio One.

---

## Part 3: Using Echo Vault

| Control | What it does |
|---|---|
| **Room** | Chamber (warm, dense), Plate (bright, smooth), Room (short, natural), Hall (big, lush) |
| **Pre-Delay** | Gap before the reverb starts. 20 to 80 ms keeps vocals up front and clear. |
| **Decay** | Reverb time (RT60) in seconds |
| **Size** | Room dimensions, i.e. how spaced out the reflections are |
| **Diffusion** | Low = grainy and echoey, high = smooth and dense |
| **Early** | Level of the first reflections (the "room" feel). Plates have none. |
| **Modulation** | Gentle movement in the tail. Adds lushness and prevents metallic ringing. |
| **Damping** | How much faster the highs die away than the lows. Higher = darker tail. |
| **Low Cut / High Cut** | EQ on the reverb only. Cutting lows keeps the mix from getting muddy. |
| **Warmth** | Tube-style saturation on the signal feeding the reverb |
| **Width** | Stereo width of the reverb: 0% = mono, 100% = normal, 200% = extra wide |
| **Mix** | Dry/wet balance. The dry signal stays at full level up to 50%. |
| **Output** | Final output level |

Double-click any knob to reset it. Drag the bottom-right corner to resize the window.

**Tips**
* **On an FX (send) channel:** set **Mix to 100%** and control the amount with the send levels.
  This is the classic studio approach.
* **On the master bus (mastering):** start with the *Master Glue*, *Master Air* or
  *Master Depth* presets. They use a 5 to 7% mix and a high low-cut so the low end stays
  tight. On a full mix, a little reverb goes a long way.
* The 15 factory presets are in the dropdown at the top. Studio One's own preset menu
  also saves your custom settings.
