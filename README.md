now with 100% more source code ™
# picohaxx
```
Open source Kernel exploit for Pico 4 devices based on cve-2023-33107
Optimized for a rock-solid 100% success rate and speed [~7s] on all vulnerable Pico 4 devices. 

Supported Pico 4 OS Versions: 5.2.0 up to 5.11.0 Chinese or Global 5.9.9. 
(As of this writing any newer version can be downgraded via edl)

Usage: picohaxx [options] [-- <final command>]

GENERAL OPTIONS:
  -help             Show this help menu
  -v                Print build timestamp and exit
  -unroot           unroot adbd
  -force            run the exploit again, even though you're already root
  -cfg              show default / runtime config
  -(no)adbd         patch adbd
  -(no)ftpd         spawn internal root ftpd daemon

DEBUG/TEST:
  -dbg              verbose debug output, use twice for even more
  -dbgchild         verbose debug output and statisktics for child processes
  -(no)sound        plays a sound to indicate various events
  -dump             dump 64MB of memory after PTE spray stage
  -ttest            test terminal input
  -sim              simulated dry-run of the final exploit stage
  -marathon         do extra laps on the task walk for stability testing,
                    can be used multiple times

CUSTOM POST-EXPLOIT EXEC:
    -- <cmd> ...    Everything after '--' is the final command to execve into.
                    picohaxx -- /system/bin/sh -i

The default post exploit behavior (on the first run) is to:
patch adb root, enable persistent tcp 5555 and spawn a root ftp on port 21.

Note: adbd needs to restart after the root patch. so if you're running the exploit inside
adb shell, your connection will drop after the first run. adb shell will default to root
once you reconnect. you can use adb unroot/adb root to toggle the default shell mode.
```
Big thx to Keto! Check out his awesome 2023-33107 writeup at:
https://keto0422.github.io/2026/02/11/cve-writeup/ 
