1. Use a third-party tool like **[syspin](https://www.technosys.net/products/utils/pintotaskbar)**

2. Create a MultiScript entry in 'User Defined Command' with the following template to enable easy access via context menu:

Here's the MultiScript using `cmd.exe`:

```multiscript
@var $filePath = GetSourceFocusPath();
@var $syspin = "C:\\Path\\To\\syspin.exe"; // Replace with your actual full path, using double backslashes
@var $cmdArgs = "/C \"\"" + $syspin + "\" \"" + $filePath + "\" 5386\""; // 5386 pins to taskbar
MC.Run CMD="cmd.exe" ARG="{$cmdArgs}";
```
3. The `/K` keeps the window open instead of `/C` which closes it. This way you can see if syspin is throwing any errors. Once confirmed working, switch `/K` back to `/C`.

Note:
- `$filePath` captures the currently focused file path. 
- Replace `"C:\\Path\\To\\syspin.exe"` with the actual full path to your `syspin.exe`. 
- Use the following codes for different actions:

+--------------------------------------------------------------+
| Action                     | Code   | Description                 |
+----------------------------|--------|------------------------------+
| Pin to taskbar             | 5386   | Pin the file to taskbar     |
| Unpin from taskbar         | 5387   | Remove from taskbar        |
| Pin to Start               | 51201  | Pin to Start menu          |
| Unpin from Start           | 51394  | Remove from Start menu     |
+--------------------------------------------------------------+

+--------------------------------------------------------------+
| Usage Examples                                               |
+--------------------------------------------------------------+
| syspin "PROGRAMFILES\Internet Explorer\iexplore.exe" 5386     |
| syspin "C:\Windows\notepad.exe" "Pin to taskbar"              |
| syspin "WINDIR\System32\calc.exe" "Pin to start"              |
| syspin "WINDIR\System32\calc.exe" "Unpin from taskbar"        |
| syspin "C:\Windows\System32\calc.exe" 51201                   |
+--------------------------------------------------------------+
Additional Notes:
You cannot pin Metro apps or batch files using SysPin.
