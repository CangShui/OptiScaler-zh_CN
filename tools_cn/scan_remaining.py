#!/usr/bin/env python3
"""Scan remaining English string literals in menu files to find untranslated UI strings."""
import json
import re
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")
items = json.loads(Path("tools_cn/strings_extracted.json").read_text(encoding="utf-8"))

# Strings that are intentionally kept in English (tech names, formats, comments, identity)
KEEP = re.compile(
    r"^(%|\d+(\.\d+)?$|16$|[2-6]X$|\| |\(|D3D1|Vulkan|FSR 2|FSR 3\.X|XeSS|DLSS$|DLSSD|DLSSG|MFG|RCAS|DLAA$|"
    r"HDR$|HUDFix|fakenvapi|Current Preset|Display only XeFG|Only Generated##2|Selecting setting below|FGId:|"
    r"PRESET|KPSS|SPLAT|MODEL_|AUTO$|COMMON$|VERTEX|INDEX_BUFFER|RENDER_TARGET|UNORDERED_ACCESS|DEPTH_|"
    r"NON_PIXEL|PIXEL_SHADER|STREAM_OUT|INDIRECT_ARGUMENT|COPY_|RESOLVE_|RAYTRACING|SHADING_RATE|GENERIC_READ|"
    r"ALL_SHADER|PRESENT$|PREDICATION|VIDEO_|Bicubic|Catmull-Rom|Lanczos|Kaiser|MAGIC|FSR1$|DEFAULT$|Latest|"
    r"Unused|Whatever|AntiLag|LatencyFlex|Reflex$|XeLL|Vulkan AntiLag|Fallback|Driver$|Present$|Simulation$|"
    r"RenderSubmit|RenderQueue|GpuRender|Input$|Opti$|Zero$|Conservative|Aggressive|Follow in-game|Force Disable|"
    r"Force Enable|Reflex ID|Trace$|Debug$|Information|Warning|Error|Top Left|Top Right|Bottom Left|Bottom Right|"
    r"Just FPS|Simple|Detailed|Full|Same as menu|Auto$|Auto \(%3|0\.5$|0\.6$|0\.7$|0\.8$|0\.9$|1\.0$|1\.1$|1\.2$|"
    r"1\.3$|1\.4$|1\.5$|1\.6$|1\.7$|1\.8$|1\.9$|2\.0$|On$|Off$|Just Ext|On \+ Ext|Exists|Exist|Doesn|Don't|"
    r"Active$|Passive$|ENABLED|DISABLED|Unbound|Unknown|Splash|Menu$|FPS Overlay Cycle|FrameTime|Upscaler$|"
    r"Open Wiki|Close$|Save Settings|Menu Scale|Color$|Depth$|Exposure$|Mask$|Output$|Set$|Reset$|Apply$|"
    r"Enable$|Extended$|Limit$|Scale$|Default$|Motion$|Stability$|Quality$|Performance$|Balanced$|Ultra|"
    r"Linear|Non-Linear|Preset \d|Input Colour|D3D11|D3D12|\| Input|\| Spoof|FFX FG|FFX |XeSS Settings|"
    r"Network Models|Dump|frames$|Framerate|Current method|FPS Limit|Apply Limit|Reset Limit|VRR|Refresh Rate|"
    r"Calculated Cap|Set as FPS|Enable Logging|Enable Trace|Force LatencyFlex|LatencyFlex mode|Force Reflex|"
    r"Sharpness$|Override$|Enable RCAS|Contrast Enabled|Depth Aware|Linear Depth|MAS Debug|MotionSharpness|"
    r"MotionThreshod|MotionRange|Depth Bias|Depth Scale|Clamp Output|DA Debug|Reset Depth|Upscale Ratio|"
    r"Override all|Override per|All Ratios|Output Scaling|Downscaler|Ratio$|Apply Change|Init Flags|"
    r"Auto Exposure|Disable Reactive|Depth Inverted|Display Res|Jitter Cancellation|React\. Mask|Use Binary|"
    r"Advanced |Active Quirks|DRS|Override Minimum|Override Maximum|Enable Extended|Use Precompiled|"
    r"Resource Barriers|Root Signatures|Restore Graphic|Restore Compute|Logging$|To File|To Console|Log Level|"
    r"Menu Theme|Light Theme|Accent Colour|Presets:|Blue$|Teal$|Gray$|Green$|Yellow$|Orange$|Red$|Purple$|"
    r"Blue##2|Teal##2|Gray##2|Green##2|Yellow##2|Orange##2|Red##2|Purple##2|Custom Accent|Reset Accent|"
    r"Background|Custom BG|Reset BG|FPS Overlay Enabled|Horizontal|Overlay Position|Overlay Type|"
    r"Upscaler Inputs|Use Fsr2|Use Fsr3|Use Ffx|V-Sync On|V-Sync Off|Sync Int|Controls the|the swap|0  =|1  =|"
    r"2\+ =|Higher values can|For most games|Force V-Sync|Can help|Negative values will|Positive values will|"
    r"Has a small|MB |Apply override|When using scale|override values|Apply same|Override all textures|"
    r"Normally OptiScaler|below zero|Calculate Mipmap|Current :|Will be|Will might|Use Value|Display Width|"
    r"Render Width|Upscaler Quality|Upscaler Ratio|Force Anisotropic|Modify Compare|Modify Min|Update |"
    r"Skip Point|Skip updating|Keybinds|Press any key|Escape to cancel|Key combinations|Menu Scale|"
    r"Click to open|Compatibility list|and other useful|OptiScaler -|OptiScaler Update|Press %s|"
    r"Update available|Open release|Update Available|Performance Overlay|Hudless Resources|Enable##%d|"
    r"Disable##%d|Clear##4|Close##4|Reflex timings,|Frame Time:|Upscaler Time:|nvngx|libxess|FSR Hooks|"
    r"FSR 3\.1|is active|Can't find|Please select %s|to enable Opti|nvngx\.ini| or |Cope|Coping|This is where|"
    r"Got any|Fake|I'm here|I find|I've got|It's over|This isn't|To infinity|I have a bad|It's Dangerous|"
    r"Trust the|Real fake|The illusion|This upscaler|Because native|The more you|It's never too|We don't|"
    r"Did you know|MFG totally|Some of those|Just don't|Even supports|It's too blurry|Thanks nitec|Tested and|"
    r"FSR4 DP4a|0\.8 was|OptiCopers|The Way|Your game may|Expanded and|It's only my|Latency with FG|"
    r"Console peasants|Hope you don't|Such an aggressive|Deep Learning|DLSS 5|Neural Slop|New app|One more|"
    r"2D AI filters|Guess we're|Guess who|Did you really|AI can't|Compiling shaders|How to remove|"
    r"<Your funny|Just when I think|Frame by frame|Resistance is|Upscaled beyond|I almost don't|And that's how|"
    r"Together We|For upscalers|Opti Sports|Render in your|All your pixels|Upscaling for|Generating discord|"
    r"Enabling DLSS|\[REDACTED\]|Free and always|Getting unshackled|Who's Nukem)$"
)

missed = []
for it in items:
    v = it["value"]
    if re.search(r"[\u4e00-\u9fff]", v):
        continue  # already Chinese
    if KEEP.match(v):
        continue
    loc = f"{it['files'][0]['file']}:{it['files'][0]['line']}"
    missed.append((it["count"], v, loc))

for c, v, loc in missed:
    print(f"{c:4d} | {v!r} | {loc}")
print("TOTAL remaining English:", len(missed))
