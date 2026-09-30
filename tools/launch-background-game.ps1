param(
    [string]$GameDir,
    [string]$RuntimeManifest,
    [string]$OperatorDir,
    [string]$GraphicsConfig,
    [string]$ExpectedDllSha256,
    [switch]$Launch,
    [switch]$Status,
    [switch]$Close
)

$ErrorActionPreference = 'Stop'
$actionCount = 0
if ($Launch) { $actionCount++ }
if ($Status) { $actionCount++ }
if ($Close) { $actionCount++ }
if ($actionCount -gt 1) { throw 'Choose only one of -Launch, -Status, or -Close.' }
$action = if ($Status) { 'status' } elseif ($Close) { 'close' } elseif ($Launch) { 'launch' } else { 'preflight' }
$sessionRoot = Join-Path $env:LOCALAPPDATA 'MGS5VR\background-sessions'
New-Item -ItemType Directory -Path $sessionRoot -Force | Out-Null
$currentPath = Join-Path $sessionRoot 'current.json'

$nativeSource = @'
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;

namespace Mgs5vr {
    public sealed class LaunchInfo {
        public uint ProcessId { get; set; }
        public uint MainThreadId { get; set; }
        public ulong CreationFileTimeUtc { get; set; }
        public IntPtr ProcessHandle { get; set; }
        public string DesktopName { get; set; }
        public string ActiveDesktopBefore { get; set; }
        public string ActiveDesktopAfter { get; set; }
        public bool ActiveDesktopUnchanged { get; set; }
    }
    public sealed class ProcessInfo {
        public bool ProcessFound { get; set; }
        public bool IdentityMatches { get; set; }
        public bool Alive { get; set; }
        public bool ExitCodeAvailable { get; set; }
        public uint ExitCode { get; set; }
        public bool DesktopAccessible { get; set; }
        public uint WindowCount { get; set; }
        public uint CloseMessagesPosted { get; set; }
        public bool CloseWaitCompleted { get; set; }
        public string ActiveDesktop { get; set; }
    }
    public static class BackgroundNative {
        const uint DESKTOP_READOBJECTS = 0x0001;
        const uint DESKTOP_CREATEWINDOW = 0x0002;
        const uint DESKTOP_CREATEMENU = 0x0004;
        const uint DESKTOP_ENUMERATE = 0x0040;
        const uint DESKTOP_WRITEOBJECTS = 0x0080;
        const uint PROCESS_QUERY_LIMITED_INFORMATION = 0x1000;
        const uint SYNCHRONIZE = 0x00100000;
        const uint STILL_ACTIVE = 259;
        const uint WAIT_OBJECT_0 = 0;
        const uint WAIT_TIMEOUT = 258;
        const uint CREATE_UNICODE_ENVIRONMENT = 0x00000400;
        const uint CREATE_NO_WINDOW = 0x08000000;
        const uint CREATE_SUSPENDED = 0x00000004;
        const uint STARTF_USESHOWWINDOW = 0x00000001;
        const ushort SW_SHOWNOACTIVATE = 4;
        const uint WM_CLOSE = 0x0010;
        const uint UOI_NAME = 2;

        [StructLayout(LayoutKind.Sequential)] struct FILETIME { public uint Low; public uint High; }
        [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] struct STARTUPINFO {
            public uint cb; public string lpReserved; public string lpDesktop; public string lpTitle;
            public uint dwX; public uint dwY; public uint dwXSize; public uint dwYSize;
            public uint dwXCountChars; public uint dwYCountChars; public uint dwFillAttribute;
            public uint dwFlags; public ushort wShowWindow; public ushort cbReserved2;
            public IntPtr lpReserved2; public IntPtr hStdInput; public IntPtr hStdOutput; public IntPtr hStdError;
        }
        [StructLayout(LayoutKind.Sequential)] struct PROCESS_INFORMATION {
            public IntPtr hProcess; public IntPtr hThread; public uint dwProcessId; public uint dwThreadId;
        }
        delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);

        [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
        static extern IntPtr CreateDesktopW(string name, IntPtr device, IntPtr devMode, uint flags, uint access, IntPtr security);
        [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern IntPtr OpenDesktopW(string name, uint flags, bool inherit, uint access);
        [DllImport("user32.dll", SetLastError=true)] static extern bool CloseDesktop(IntPtr desktop);
        [DllImport("user32.dll", SetLastError=true)] static extern IntPtr OpenInputDesktop(uint flags, bool inherit, uint access);
        [DllImport("user32.dll", SetLastError=true)] static extern bool EnumDesktopWindows(IntPtr desktop, EnumWindowsProc callback, IntPtr lParam);
        [DllImport("user32.dll", SetLastError=true)] static extern bool PostMessageW(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
        [DllImport("user32.dll", SetLastError=true)] static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
        [DllImport("user32.dll", CharSet=CharSet.Auto, SetLastError=true)] static extern bool GetUserObjectInformationW(IntPtr handle, int index, StringBuilder info, uint length, out uint needed);
        [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
        static extern bool CreateProcessW(string app, StringBuilder commandLine, IntPtr processAttributes, IntPtr threadAttributes,
            bool inheritHandles, uint flags, IntPtr environment, string currentDirectory, ref STARTUPINFO startup, out PROCESS_INFORMATION info);
        [DllImport("kernel32.dll", SetLastError=true)] static extern bool GetProcessTimes(IntPtr process, out FILETIME creation, out FILETIME exit, out FILETIME kernel, out FILETIME user);
        [DllImport("kernel32.dll", SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr process, out uint exitCode);
        [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);
        [DllImport("kernel32.dll", SetLastError=true)] static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);
        [DllImport("kernel32.dll", SetLastError=true)] static extern uint ResumeThread(IntPtr thread);
        [DllImport("kernel32.dll", SetLastError=true)] static extern bool TerminateProcess(IntPtr process, uint exitCode);
        [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);

        static string UserObjectName(IntPtr handle) {
            uint needed;
            GetUserObjectInformationW(handle, (int)UOI_NAME, null, 0, out needed);
            if (needed < 2) throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not size desktop-name buffer.");
            var value = new StringBuilder((int)(needed / 2) + 1);
            if (!GetUserObjectInformationW(handle, (int)UOI_NAME, value, needed, out needed))
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not read desktop name.");
            return value.ToString();
        }
        public static string GetInputDesktopName() {
            IntPtr desktop = OpenInputDesktop(0, false, DESKTOP_READOBJECTS);
            if (desktop == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not inspect the active input desktop.");
            try { return UserObjectName(desktop); } finally { CloseDesktop(desktop); }
        }
        static IntPtr CreatePrivateDesktop(string name) {
            IntPtr desktop = CreateDesktopW(name, IntPtr.Zero, IntPtr.Zero, 0,
                DESKTOP_READOBJECTS | DESKTOP_CREATEWINDOW | DESKTOP_CREATEMENU | DESKTOP_ENUMERATE | DESKTOP_WRITEOBJECTS, IntPtr.Zero);
            if (desktop == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not create the private Win32 desktop.");
            if (Marshal.GetLastWin32Error() == 183) { CloseDesktop(desktop); throw new InvalidOperationException("Private desktop name already exists."); }
            return desktop;
        }
        public static bool TestPrivateDesktop(string name) {
            string before = GetInputDesktopName();
            IntPtr desktop = CreatePrivateDesktop(name);
            try { return String.Equals(before, GetInputDesktopName(), StringComparison.Ordinal); }
            finally { CloseDesktop(desktop); }
        }
        static ulong FileTimeValue(FILETIME value) { return ((ulong)value.High << 32) | value.Low; }
        static string Quote(string value) {
            var result = new StringBuilder("\"");
            int slashes = 0;
            foreach (char c in value) {
                if (c == '\\') { slashes++; continue; }
                if (c == '"') { result.Append('\\', slashes * 2 + 1).Append('"'); slashes = 0; continue; }
                result.Append('\\', slashes).Append(c); slashes = 0;
            }
            result.Append('\\', slashes * 2).Append('"');
            return result.ToString();
        }
        static string JsonString(string value) {
            var b = new StringBuilder("\"");
            foreach (char c in value ?? "") {
                switch (c) {
                    case '\\': b.Append("\\\\"); break;
                    case '"': b.Append("\\\""); break;
                    case '\r': b.Append("\\r"); break;
                    case '\n': b.Append("\\n"); break;
                    case '\t': b.Append("\\t"); break;
                    default: if (c < 0x20) b.Append("\\u").Append(((int)c).ToString("x4")); else b.Append(c); break;
                }
            }
            return b.Append('"').ToString();
        }
        public static LaunchInfo Launch(string exe, string args, string workingDirectory, string desktopName, string runId, string recordPath, IDictionary<string,string> environment) {
            string before = GetInputDesktopName();
            IntPtr desktop = CreatePrivateDesktop(desktopName);
            IntPtr envBlock = IntPtr.Zero;
            PROCESS_INFORMATION info = new PROCESS_INFORMATION();
            bool processCreated = false;
            bool processResumed = false;
            try {
                var block = new StringBuilder();
                foreach (var item in environment.OrderBy(x => x.Key, StringComparer.OrdinalIgnoreCase))
                    block.Append(item.Key).Append('=').Append(item.Value).Append('\0');
                block.Append('\0');
                envBlock = Marshal.StringToHGlobalUni(block.ToString());
                var startup = new STARTUPINFO();
                startup.cb = (uint)Marshal.SizeOf(typeof(STARTUPINFO));
                startup.lpDesktop = "WinSta0\\" + desktopName;
                startup.dwFlags = STARTF_USESHOWWINDOW;
                startup.wShowWindow = SW_SHOWNOACTIVATE;
                var commandLine = new StringBuilder(Quote(exe) + (String.IsNullOrWhiteSpace(args) ? "" : " " + args));
                uint flags = CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW | CREATE_SUSPENDED;
                if (!CreateProcessW(exe, commandLine, IntPtr.Zero, IntPtr.Zero, false, flags, envBlock, workingDirectory, ref startup, out info))
                    throw new Win32Exception(Marshal.GetLastWin32Error(), "Direct CreateProcessW on the private desktop failed.");
                processCreated = true;
                FILETIME creation, exit, kernel, user;
                if (!GetProcessTimes(info.hProcess, out creation, out exit, out kernel, out user))
                    throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not read the launched process creation identity.");
                string after = GetInputDesktopName();
                var identity = new LaunchInfo { ProcessId=info.dwProcessId, MainThreadId=info.dwThreadId,
                    CreationFileTimeUtc=FileTimeValue(creation), DesktopName=desktopName,
                    ActiveDesktopBefore=before, ActiveDesktopAfter=after,
                    ActiveDesktopUnchanged=String.Equals(before, after, StringComparison.Ordinal) };
                string identityJson = "{\"schema\":1,\"runId\":" + JsonString(runId) + ",\"status\":\"started\",\"lastStatusReport\":null,\"processId\":" + identity.ProcessId +
                    ",\"mainThreadId\":" + identity.MainThreadId + ",\"creationFileTimeUtc\":" + identity.CreationFileTimeUtc +
                    ",\"desktopName\":" + JsonString(identity.DesktopName) + ",\"activeDesktopBefore\":" + JsonString(before) +
                    ",\"activeDesktopAfter\":" + JsonString(after) + ",\"activeDesktopUnchanged\":" + (identity.ActiveDesktopUnchanged ? "true" : "false") + "}\n";
                System.IO.File.WriteAllText(recordPath, identityJson, new UTF8Encoding(false));
                uint previousSuspendCount = ResumeThread(info.hThread);
                if (previousSuspendCount == UInt32.MaxValue)
                    throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not resume the recorded game process.");
                processResumed = true;
                identity.ProcessHandle = info.hProcess;
                info.hProcess = IntPtr.Zero;
                return identity;
            } catch {
                // Only dispose of a child that is still suspended and has not
                // executed its entry point; resumed game processes are never killed.
                if (processCreated && !processResumed && info.hProcess != IntPtr.Zero) {
                    if (TerminateProcess(info.hProcess, 0xE0000001)) WaitForSingleObject(info.hProcess, 5000);
                    try {
                        string failed = "{\"schema\":1,\"runId\":" + JsonString(runId) + ",\"status\":\"discarded-before-resume\",\"processId\":" + info.dwProcessId +
                            ",\"mainThreadId\":" + info.dwThreadId + ",\"desktopName\":" + JsonString(desktopName) + "}\n";
                        System.IO.File.WriteAllText(recordPath, failed, new UTF8Encoding(false));
                    } catch { }
                }
                throw;
            } finally {
                if (envBlock != IntPtr.Zero) Marshal.FreeHGlobal(envBlock);
                if (info.hThread != IntPtr.Zero) CloseHandle(info.hThread);
                if (info.hProcess != IntPtr.Zero) CloseHandle(info.hProcess);
                CloseDesktop(desktop);
            }
        }
        static ProcessInfo Inspect(uint pid, ulong expectedCreation, string desktopName, bool postClose, uint waitMs) {
            var result = new ProcessInfo { ActiveDesktop=GetInputDesktopName() };
            IntPtr process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, false, pid);
            if (process == IntPtr.Zero) {
                int error = Marshal.GetLastWin32Error();
                if (error == 87) { result.ProcessFound=false; result.IdentityMatches=false; result.Alive=false; return result; }
                throw new Win32Exception(error, "Could not inspect the recorded process.");
            }
            try {
                result.ProcessFound = true;
                FILETIME creation, exit, kernel, user;
                if (!GetProcessTimes(process, out creation, out exit, out kernel, out user))
                    throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not verify process creation identity.");
                result.IdentityMatches = FileTimeValue(creation) == expectedCreation;
                if (!result.IdentityMatches) return result;
                uint exitCode;
                if (!GetExitCodeProcess(process, out exitCode)) throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not inspect launched process state.");
                uint waitState = WaitForSingleObject(process, 0);
                if (waitState != WAIT_OBJECT_0 && waitState != WAIT_TIMEOUT)
                    throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not inspect launched process wait state.");
                result.Alive = waitState == WAIT_TIMEOUT;
                result.ExitCodeAvailable = !result.Alive;
                result.ExitCode = exitCode;
                if (!result.Alive) return result;
                IntPtr desktop = OpenDesktopW(desktopName, 0, false, DESKTOP_READOBJECTS | DESKTOP_ENUMERATE | DESKTOP_WRITEOBJECTS);
                if (desktop == IntPtr.Zero) return result;
                result.DesktopAccessible = true;
                try {
                    int callbackError = 0;
                    EnumWindowsProc callback = delegate(IntPtr hwnd, IntPtr unused) {
                        uint windowPid;
                        GetWindowThreadProcessId(hwnd, out windowPid);
                        if (windowPid == pid) {
                            result.WindowCount++;
                            if (postClose && !PostMessageW(hwnd, WM_CLOSE, IntPtr.Zero, IntPtr.Zero)) {
                                callbackError = Marshal.GetLastWin32Error();
                                return false;
                            }
                            if (postClose) result.CloseMessagesPosted++;
                        }
                        return true;
                    };
                    if (!EnumDesktopWindows(desktop, callback, IntPtr.Zero))
                        throw new Win32Exception(callbackError != 0 ? callbackError : Marshal.GetLastWin32Error(), callbackError != 0 ? "Could not send WM_CLOSE to the launched game window." : "Could not enumerate windows on the owned desktop.");
                } finally { CloseDesktop(desktop); }
                if (postClose) result.CloseWaitCompleted = WaitForSingleObject(process, waitMs) == 0;
                return result;
            } finally { CloseHandle(process); }
        }
        public static ProcessInfo Status(uint pid, ulong expectedCreation, string desktopName) {
            return Inspect(pid, expectedCreation, desktopName, false, 0);
        }
        public static ProcessInfo GracefulClose(uint pid, ulong expectedCreation, string desktopName, uint waitMs) {
            return Inspect(pid, expectedCreation, desktopName, true, waitMs);
        }
        public static uint WaitForProcess(IntPtr process, uint waitMs) {
            uint state = WaitForSingleObject(process, waitMs);
            if (state != WAIT_OBJECT_0 && state != WAIT_TIMEOUT)
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not wait for the launched process.");
            return state;
        }
        public static uint ReadExitCode(IntPtr process) {
            uint exitCode;
            if (!GetExitCodeProcess(process, out exitCode))
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not read the launched process exit code.");
            return exitCode;
        }
        public static void CloseProcessHandle(IntPtr process) {
            if (process != IntPtr.Zero) CloseHandle(process);
        }
    }
}
'@
if (-not ('Mgs5vr.BackgroundNative' -as [type])) { Add-Type -TypeDefinition $nativeSource -Language CSharp }

$sessionMutex = $null
if ($action -in @('launch','status','close')) {
    $sessionMutex = [Threading.Mutex]::new($false,'Local\MGS5VR.BackgroundSession')
    try { $mutexAcquired = $sessionMutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $mutexAcquired = $true }
    if (-not $mutexAcquired) { $sessionMutex.Dispose(); throw 'Another background-session action is in progress; retry after it writes its status.' }
}
try {

function Write-SessionJson([string]$Path, $Value) {
    [IO.File]::WriteAllText($Path,($Value | ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
}
function New-UniquePath([string]$Prefix,[string]$Extension) {
    Join-Path $sessionRoot ($Prefix+'.'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')+'.'+[Guid]::NewGuid().ToString('N')+$Extension)
}
function Get-StartupProgress([uint32]$ProcessId,[string]$GameDirectory,[long]$InitialLogLength,[long]$ElapsedMs) {
    $modules = @()
    $moduleError = $null
    try {
        $process = [Diagnostics.Process]::GetProcessById([int]$ProcessId)
        try {
            $modules = @($process.Modules | ForEach-Object { $_.ModuleName } | Where-Object {
                $_ -match '(?i)^(dinput8|openxr_loader|simulator|xrapilayer.*operator|xr_api.*operator|metax.*operator).*\.dll$'
            } | Sort-Object -Unique)
        } finally { $process.Dispose() }
    } catch { $moduleError = $_.Exception.Message }

    $simulators = @()
    foreach ($simulator in @(Get-Process -Name MetaXRSimulator -ErrorAction SilentlyContinue)) {
        $path = $null
        try { $path = $simulator.Path } catch { }
        $simulators += [ordered]@{ processId=$simulator.Id; path=$path }
    }

    $logPath = Join-Path $GameDirectory 'mgs5vr.log'
    $logLength = $null
    $logWriteUtc = $null
    $logError = $null
    $relevantLines = @()
    if (Test-Path -LiteralPath $logPath -PathType Leaf) {
        $stream = $null
        try {
            $file = Get-Item -LiteralPath $logPath
            $logLength = [long]$file.Length
            $logWriteUtc = $file.LastWriteTimeUtc.ToString('o')
            $stream = [IO.File]::Open($logPath,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
            $startOffset = if ($logLength -lt $InitialLogLength) { [long]0 } else { $InitialLogLength }
            if (($logLength - $startOffset) -gt 262144) { $startOffset = $logLength - 262144 }
            $stream.Position = $startOffset
            $readLength = [int]($logLength - $startOffset)
            if ($readLength -gt 0) {
                $buffer = [byte[]]::new($readLength)
                $actual = $stream.Read($buffer,0,$readLength)
                $tail = [Text.UTF8Encoding]::new($false,$false).GetString($buffer,0,$actual)
                $relevantLines = @($tail -split "`r?`n" | Where-Object {
                    $_ -match '(?i)(openxr|xr session|xr frames=|runtime|simulator|operator|dinput8|MGS5VR|module|hook|error|failed|exception|exitprocess|cleanup)'
                } | Select-Object -Last 40)
            }
        } catch { $logError = $_.Exception.Message }
        finally { if ($stream) { $stream.Dispose() } }
    } else { $logError = 'mgs5vr.log is not present yet.' }

    return [ordered]@{
        elapsedMs=$ElapsedMs; recordedUtc=[DateTime]::UtcNow.ToString('o')
        loadedModuleNames=$modules; moduleQueryError=$moduleError
        simulatorProcesses=$simulators
        logPath=$logPath; logLengthBytes=$logLength; logBytesAtLaunch=$InitialLogLength
        logBytesSinceLaunch=if ($null -ne $logLength -and $logLength -ge $InitialLogLength) { $logLength - $InitialLogLength } elseif ($null -ne $logLength) { $logLength } else { $null }
        logLastWriteUtc=$logWriteUtc; logError=$logError; recentRelevantLogLines=$relevantLines
    }
}
function Read-CurrentSession {
    if (-not (Test-Path -LiteralPath $currentPath -PathType Leaf)) { throw 'No background game session record exists.' }
    $pointer = Get-Content -Raw -LiteralPath $currentPath | ConvertFrom-Json
    $recordPath = [IO.Path]::GetFullPath([string]$pointer.sessionFile)
    $rootPrefix = [IO.Path]::GetFullPath($sessionRoot).TrimEnd('\') + '\'
    if (-not $recordPath.StartsWith($rootPrefix,[StringComparison]::OrdinalIgnoreCase) -or -not (Test-Path -LiteralPath $recordPath -PathType Leaf)) {
        throw 'The current session pointer does not resolve to an owned report under the session directory.'
    }
    $record = Get-Content -Raw -LiteralPath $recordPath | ConvertFrom-Json
    if ($null -eq $record.PSObject.Properties['lastStatusReport']) {
        $record | Add-Member -NotePropertyName lastStatusReport -NotePropertyValue $null
    }
    if ($record.schema -ne 1 -or $record.desktopName -notmatch '^M5B_[0-9a-f]{28}$' -or -not $record.processId -or -not $record.creationFileTimeUtc) {
        throw 'The current report lacks a complete owned-process identity; no window action was taken.'
    }
    return [pscustomobject]@{ RecordPath=$recordPath; Record=$record }
}

if ($action -in @('status','close')) {
    $read = Read-CurrentSession
    $recordPath = $read.RecordPath
    $record = $read.Record
    $pidValue = [uint32]$record.processId
    $creation = [uint64]$record.creationFileTimeUtc
    $desktopName = [string]$record.desktopName
    if ($action -eq 'close') {
        $state = [Mgs5vr.BackgroundNative]::GracefulClose($pidValue,$creation,$desktopName,10000)
    } else {
        $state = [Mgs5vr.BackgroundNative]::Status($pidValue,$creation,$desktopName)
    }
    if ($state.ProcessFound -and -not $state.IdentityMatches) { throw 'PID creation identity does not match this launch report; no window action was taken.' }
    $report = [ordered]@{
        schema=1; action=$action; recordedUtc=[DateTime]::UtcNow.ToString('o')
        runId=$record.runId; processId=$pidValue; creationFileTimeUtc=$creation
        desktopName=$desktopName; activeInputDesktop=$state.ActiveDesktop
        processFound=$state.ProcessFound; identityMatches=$state.IdentityMatches
        alive=$state.Alive; exitCodeAvailable=$state.ExitCodeAvailable; exitCode=if ($state.ExitCodeAvailable) { $state.ExitCode } else { $null }
        desktopAccessible=$state.DesktopAccessible; gameWindowCount=$state.WindowCount
        closeMessagesPosted=$state.CloseMessagesPosted; closeWaitCompleted=$state.CloseWaitCompleted
        status=if (-not $state.Alive) { 'exited' } elseif ($action -eq 'close' -and $state.CloseWaitCompleted) { 'gracefully-closed' } elseif ($action -eq 'close') { 'close-pending' } else { 'running' }
        startupRecord=$recordPath
    }
    $statusPath = New-UniquePath $action '.json'
    Write-SessionJson $statusPath $report
    if (-not $state.Alive -or $state.CloseWaitCompleted) {
        $record.status = if ($state.Alive) { 'gracefully-closed' } else { 'exited' }
        $record.lastStatusReport = $statusPath
        Write-SessionJson $recordPath $record
    }
    Write-SessionJson $currentPath ([ordered]@{ schema=1; runId=$record.runId; sessionFile=$recordPath })
    Write-Output ($report | ConvertTo-Json -Depth 8)
    Write-Output "Report: $statusPath"
    return
}

# Resolve inputs from the saved launcher and Steam account when available. This
# path is read-only; preflight and launch never rewrite game or Steam settings.
if (-not $GameDir) {
    $saved = Get-ItemProperty -LiteralPath 'HKCU:\Software\Nikami\MGS5VR\Launcher' -ErrorAction SilentlyContinue
    if ($saved.TppExe) { $GameDir = Split-Path -Parent $saved.TppExe }
}
if (-not $GameDir) { throw 'Pass -GameDir with the folder containing mgsvtpp.exe, or select TPP in the existing launcher first.' }
$gameDirectory = (Resolve-Path -LiteralPath $GameDir).Path
$gameExe = Join-Path $gameDirectory 'mgsvtpp.exe'
if (-not (Test-Path -LiteralPath $gameExe -PathType Leaf)) { throw 'The game directory must contain mgsvtpp.exe.' }
$expectedGameHash = '085c2f82d1c963c40b3d2d55786661dfee2b18cbbf388a710c00fa76c5e9bb45'
$gameHash = (Get-FileHash -LiteralPath $gameExe -Algorithm SHA256).Hash.ToLowerInvariant()
if ($gameHash -ne $expectedGameHash) { throw 'This background adapter only accepts the baselined TPP 1.0.15.4 executable.' }

if (-not $RuntimeManifest) {
    $runtimeSettings = Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Khronos\OpenXR\1' -ErrorAction SilentlyContinue
    if ($runtimeSettings.ActiveRuntime) { $RuntimeManifest = $runtimeSettings.ActiveRuntime }
}
if (-not $RuntimeManifest -or -not (Test-Path -LiteralPath $RuntimeManifest -PathType Leaf)) { throw 'Pass -RuntimeManifest with the installed Meta XR Simulator manifest.' }
$runtimePath = (Resolve-Path -LiteralPath $RuntimeManifest).Path
$runtimeJson = Get-Content -Raw -LiteralPath $runtimePath | ConvertFrom-Json
if (-not $runtimeJson.runtime.name -or -not $runtimeJson.runtime.library_path) { throw 'The selected runtime manifest is incomplete.' }
if (($runtimePath + ' ' + $runtimeJson.runtime.name + ' ' + $runtimeJson.runtime.library_path) -notmatch 'Simulator') {
    throw 'The selected OpenXR runtime is not identified as Meta XR Simulator.'
}
$runtimeDll = [string]$runtimeJson.runtime.library_path
if (-not [IO.Path]::IsPathRooted($runtimeDll)) { $runtimeDll = Join-Path (Split-Path -Parent $runtimePath) $runtimeDll }
$runtimeDll = [IO.Path]::GetFullPath($runtimeDll)
if (-not (Test-Path -LiteralPath $runtimeDll -PathType Leaf)) { throw "OpenXR runtime DLL is missing: $runtimeDll" }

if (-not $OperatorDir) {
    $knownOperator = 'D:\code\gta-iv\out-openxr\external\meta-xr-operator-standalone-205.1\extracted\meta-xr-operator-standalone-public\windows'
    if (Test-Path -LiteralPath (Join-Path $knownOperator 'XrApiLayer_METAX_operator.json') -PathType Leaf) { $OperatorDir = $knownOperator }
}
if (-not $OperatorDir) { throw 'Pass -OperatorDir with the installed Meta XR Operator layer files.' }
$operatorDirectory = (Resolve-Path -LiteralPath $OperatorDir).Path
$operatorManifestPath = Join-Path $operatorDirectory 'XrApiLayer_METAX_operator.json'
if (-not (Test-Path -LiteralPath $operatorManifestPath -PathType Leaf)) { throw 'Meta XR Operator layer manifest is missing.' }
$operatorJson = Get-Content -Raw -LiteralPath $operatorManifestPath | ConvertFrom-Json
if ($operatorJson.api_layer.name -ne 'XR_APILAYER_METAX_operator' -or -not $operatorJson.api_layer.library_path) { throw 'Meta XR Operator manifest has an unexpected layer identity.' }
$operatorDll = [string]$operatorJson.api_layer.library_path
if (-not [IO.Path]::IsPathRooted($operatorDll)) { $operatorDll = Join-Path $operatorDirectory $operatorDll }
$operatorDll = [IO.Path]::GetFullPath($operatorDll)
if (-not (Test-Path -LiteralPath $operatorDll -PathType Leaf)) { throw "Meta XR Operator layer DLL is missing: $operatorDll" }
$operatorProxy = Join-Path $operatorDirectory 'meta-xr-operator-mcp-proxy.exe'
if (-not (Test-Path -LiteralPath $operatorProxy -PathType Leaf)) { throw 'The installed operator proxy executable is missing beside its layer manifest.' }

$installRecordPath = Join-Path $gameDirectory 'mgs5vr-install.json'
$vrIni = Join-Path $gameDirectory 'mgs5vr.ini'
$vrDll = Join-Path $gameDirectory 'dinput8.dll'
if (-not (Test-Path -LiteralPath $installRecordPath -PathType Leaf) -or -not (Test-Path -LiteralPath $vrIni -PathType Leaf) -or -not (Test-Path -LiteralPath $vrDll -PathType Leaf)) {
    throw 'A complete installed MGS5VR package (install record, dinput8.dll, and mgs5vr.ini) is required.'
}
$installRecord = Get-Content -Raw -LiteralPath $installRecordPath | ConvertFrom-Json
if ($installRecord.game_sha256 -ne $gameHash -or [IO.Path]::GetFullPath([string]$installRecord.game_dir) -ne $gameDirectory) {
    throw 'MGS5VR install metadata does not match this game directory and executable.'
}
$recordedDllProperty = $installRecord.files.PSObject.Properties['dinput8.dll']
$recordedDllHash = if ($recordedDllProperty) { [string]$recordedDllProperty.Value } else { '' }
$installedDllHash = (Get-FileHash -LiteralPath $vrDll -Algorithm SHA256).Hash.ToUpperInvariant()
if ($ExpectedDllSha256) {
    if ($ExpectedDllSha256 -notmatch '^[0-9a-fA-F]{64}$') { throw '-ExpectedDllSha256 must be a 64-digit SHA256 hex string.' }
    $expectedDllHash = $ExpectedDllSha256.ToUpperInvariant()
    if ($installedDllHash -ne $expectedDllHash) { throw 'Installed dinput8.dll does not match -ExpectedDllSha256.' }
    $dllHashSource = 'explicit-local-candidate'
} else {
    if (-not $recordedDllHash) { throw 'MGS5VR install metadata does not record dinput8.dll; pass -ExpectedDllSha256 for an approved local candidate.' }
    $expectedDllHash = $recordedDllHash.ToUpperInvariant()
    if ($installedDllHash -ne $expectedDllHash) { throw 'Installed dinput8.dll differs from the package install record; pass an explicitly approved -ExpectedDllSha256 for a local candidate.' }
    $dllHashSource = 'mgs5vr-install.json'
}
$vrIniText = Get-Content -Raw -LiteralPath $vrIni
$vrIniSections = [regex]::Matches($vrIniText,'(?m)^\s*\[[^\]\r\n]+\]\s*(?:;.*)?$').Count
$vrIniKeys = [regex]::Matches($vrIniText,'(?m)^\s*[A-Za-z0-9_.-]+\s*=.*$').Count
$vrIniMalformed = @($vrIniText -split "`r?`n" | Where-Object {
    $line = $_.Trim()
    $line -and -not $line.StartsWith(';') -and -not $line.StartsWith('#') -and
    $line -notmatch '^\[[^\]\r\n]+\]\s*(?:;.*)?$' -and $line -notmatch '^[A-Za-z0-9_.-]+\s*=.*$'
})
if ($vrIniSections -eq 0 -or $vrIniKeys -eq 0 -or $vrIniMalformed.Count -gt 0) { throw 'Installed mgs5vr.ini is not structurally parseable; no settings were changed.' }
$vrIniHash = (Get-FileHash -LiteralPath $vrIni -Algorithm SHA256).Hash.ToUpperInvariant()
$controlsPath = Join-Path $gameDirectory 'mgs5vr-controls.ini'
$controlsHash = if (Test-Path -LiteralPath $controlsPath -PathType Leaf) { (Get-FileHash -LiteralPath $controlsPath -Algorithm SHA256).Hash.ToUpperInvariant() } else { $null }

if (-not $GraphicsConfig) {
    $steamSettings = Get-ItemProperty -LiteralPath 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue
    if (-not $steamSettings.SteamPath) { throw 'Pass -GraphicsConfig; Steam installation path is unavailable.' }
    $candidates = @(Get-ChildItem -LiteralPath (Join-Path $steamSettings.SteamPath 'userdata') -Directory | ForEach-Object {
        $candidate = Join-Path $_.FullName '287700\local\TPP_GRAPHICS_CONFIG'
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { $candidate }
    })
    if ($candidates.Count -ne 1) { throw 'Pass the TPP -GraphicsConfig for the Steam account used by this installation.' }
    $GraphicsConfig = $candidates[0]
}
$graphicsPath = (Resolve-Path -LiteralPath $GraphicsConfig).Path
$graphicsHash = (Get-FileHash -LiteralPath $graphicsPath -Algorithm SHA256).Hash.ToUpperInvariant()
$graphics = Get-Content -Raw -LiteralPath $graphicsPath | ConvertFrom-Json
if ($graphics.project -ne 'tpp' -or -not $graphics.graphics.videoout_setting) { throw 'Graphics config is not a TPP configuration.' }
$video = $graphics.graphics.videoout_setting
if ($video.window_mode -ne 'Windowed') { throw 'Background launch requires the existing Windowed game configuration; this tool never edits display settings.' }
if ([int]$video.width -lt 640 -or [int]$video.height -lt 360) { throw 'Current native render dimensions are below the supported windowed minimum.' }
$displayPath = Join-Path $gameDirectory 'mgs5vr-display.ini'
if (-not (Test-Path -LiteralPath $displayPath -PathType Leaf)) { throw 'The existing independent mirror config is missing; this tool does not create or edit display settings.' }
$displayHash = (Get-FileHash -LiteralPath $displayPath -Algorithm SHA256).Hash.ToUpperInvariant()
$displayText = Get-Content -Raw -LiteralPath $displayPath
$displaySectionMatch = [regex]::Match($displayText,'(?ims)^\s*\[display\]\s*(.*?)(?=^\s*\[|\z)')
if (-not $displaySectionMatch.Success) { throw 'The existing MGS5VR display config has no [display] section.' }
$displaySection = $displaySectionMatch.Groups[1].Value
function Get-DisplayValue([string]$Name) {
    $match = [regex]::Match($displaySection,'(?im)^\s*'+[regex]::Escape($Name)+'\s*=\s*(\S+)')
    if (-not $match.Success) { return $null }
    return $match.Groups[1].Value
}
$displayEnabled = Get-DisplayValue 'enabled'
$displayRenderWidth = [int](Get-DisplayValue 'render_width')
$displayRenderHeight = [int](Get-DisplayValue 'render_height')
$displayMirrorWidth = [int](Get-DisplayValue 'mirror_width')
$displayMirrorHeight = [int](Get-DisplayValue 'mirror_height')
if ($displayEnabled -ne '1' -or $displayRenderWidth -lt 640 -or $displayRenderWidth -gt 8192 -or $displayRenderWidth % 2 -or
    $displayRenderHeight -lt 360 -or $displayRenderHeight -gt 8192 -or $displayRenderHeight % 2 -or
    $displayMirrorWidth -lt 320 -or $displayMirrorWidth -gt 1920 -or $displayMirrorHeight -lt 240 -or $displayMirrorHeight -gt 1080 -or
    $displayMirrorWidth -ge $displayRenderWidth -or $displayMirrorHeight -ge $displayRenderHeight) {
    throw 'Existing MGS5VR independent render/mirror dimensions are missing or outside the validated windowed range.'
}

$currentSession = [Diagnostics.Process]::GetCurrentProcess().SessionId
$steamProcesses = @(Get-Process -Name steam -ErrorAction SilentlyContinue | Where-Object { $_.SessionId -eq $currentSession -and -not $_.HasExited })
if ($steamProcesses.Count -eq 0) { throw 'An already-running Steam client in this Windows session is required; the adapter never starts or restarts Steam.' }
$runningGames = @(Get-Process -Name mgsvtpp -ErrorAction SilentlyContinue | Where-Object { -not $_.HasExited })
if ($runningGames.Count -gt 0) { throw 'MGSV is already running; the adapter requires a stopped singleton before launch.' }
$simulatorProcesses = @(Get-Process -Name MetaXRSimulator -ErrorAction SilentlyContinue | Where-Object { -not $_.HasExited })
if ($action -eq 'launch' -and $simulatorProcesses.Count -gt 0) { throw 'MetaXRSimulator is already running outside this owned session; close it manually before an isolated launch.' }

$activeDesktop = [Mgs5vr.BackgroundNative]::GetInputDesktopName()
$probeDesktop = 'M5B_' + [Guid]::NewGuid().ToString('N').Substring(0,28)
if (-not [Mgs5vr.BackgroundNative]::TestPrivateDesktop($probeDesktop)) { throw 'Private desktop preflight failed to preserve the active input desktop.' }
$envMap = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($entry in [Environment]::GetEnvironmentVariables().GetEnumerator()) { $envMap[[string]$entry.Key] = [string]$entry.Value }
$existingLayers = @(([string]$envMap['XR_ENABLE_API_LAYERS']) -split ';' | Where-Object { $_ -and $_ -ne 'XR_APILAYER_METAX_operator' })
$envMap['XR_ENABLE_API_LAYERS'] = (@($existingLayers + 'XR_APILAYER_METAX_operator') -join ';')
$existingPaths = @(([string]$envMap['XR_API_LAYER_PATH']) -split ';' | Where-Object { $_ -and -not [string]::Equals($_,$operatorDirectory,[StringComparison]::OrdinalIgnoreCase) })
$envMap['XR_API_LAYER_PATH'] = (@($operatorDirectory + $existingPaths) -join ';')
$envMap['XR_RUNTIME_JSON'] = $runtimePath
$envMap['SteamAppId'] = '287700'
$envMap['SteamGameId'] = '287700'
$envMap.Remove('OPENXR_SIMULATOR_HEADLESS') | Out-Null
$envMap.Remove('METAX_operator_disabled') | Out-Null

$plan = [ordered]@{
    schema=1; action=$action; generatedUtc=[DateTime]::UtcNow.ToString('o')
    gameExe=$gameExe; gameSha256=$gameHash; gameDirectory=$gameDirectory
    graphicsConfig=$graphicsPath; windowMode=$video.window_mode; gameWindowWidth=[int]$video.width; gameWindowHeight=[int]$video.height
    graphicsConfigSha256=$graphicsHash; gameVrIni=$vrIni; gameVrIniSha256=$vrIniHash; gameVrIniParseValid=$true
    displayConfig=$displayPath; displayConfigSha256=$displayHash; displayConfigParseValid=$true
    controlsConfig=$controlsPath; controlsConfigSha256=$controlsHash
    installedDllSha256=$installedDllHash; expectedDllSha256=$expectedDllHash; installRecordDllSha256=$recordedDllHash
    installRecordMatchesInstalledDll=($installedDllHash -eq $recordedDllHash.ToUpperInvariant()); dllHashSource=$dllHashSource
    independentMirrorEnabled=$true; nativeRenderWidth=$displayRenderWidth; nativeRenderHeight=$displayRenderHeight
    mirrorWidth=$displayMirrorWidth; mirrorHeight=$displayMirrorHeight
    steamClientPids=@($steamProcesses | ForEach-Object { $_.Id }); steamRequiredAlreadyRunning=$true
    runtimeManifest=$runtimePath; runtimeDll=$runtimeDll; operatorManifest=$operatorManifestPath; operatorDll=$operatorDll; operatorProxy=$operatorProxy
    enabledOpenXrLayer='XR_APILAYER_METAX_operator'; steamAppId='287700'; simulatorHeadlessEnvironmentRemoved=$true
    activeInputDesktop=$activeDesktop; privateDesktopPreflight=$probeDesktop; privateDesktopPassed=$true
    simulatorFrontendAlreadyRunning=($simulatorProcesses.Count -gt 0); simulatorFrontendManaged=$false
    audioEndpointChanged=$false; systemMuteChanged=$false; perGameQuietingVerified=$false; captureAudioVerified=$false
    launchRequested=($action -eq 'launch'); launchOccursByDefault=$false
    result='preflight-passed-only'
}
if ($action -eq 'launch') {
    if (Test-Path -LiteralPath $currentPath -PathType Leaf) {
        $oldPointer = Get-Content -Raw -LiteralPath $currentPath | ConvertFrom-Json
        if ($oldPointer.sessionFile -and (Test-Path -LiteralPath $oldPointer.sessionFile -PathType Leaf)) {
            $oldRecord = Get-Content -Raw -LiteralPath $oldPointer.sessionFile | ConvertFrom-Json
            if ($oldRecord.processId -and $oldRecord.creationFileTimeUtc) {
                $oldState = [Mgs5vr.BackgroundNative]::Status([uint32]$oldRecord.processId,[uint64]$oldRecord.creationFileTimeUtc,[string]$oldRecord.desktopName)
                if ($oldState.IdentityMatches -and $oldState.Alive) { throw 'An existing owned background game session is still running; use -Status or -Close first.' }
            }
        }
    }
    $runId = [Guid]::NewGuid().ToString('N')
    $desktopName = 'M5B_' + $runId.Substring(0,28)
    $sessionPath = New-UniquePath ('session.'+$runId) '.json'
    Write-SessionJson $sessionPath ([ordered]@{ schema=1; runId=$runId; status='launching'; desktopName=$desktopName; gameExe=$gameExe; gameSha256=$gameHash; startedUtc=[DateTime]::UtcNow.ToString('o') })
    Write-SessionJson $currentPath ([ordered]@{ schema=1; runId=$runId; sessionFile=$sessionPath })
    $gameLogPath = Join-Path $gameDirectory 'mgs5vr.log'
    $initialLogLength = if (Test-Path -LiteralPath $gameLogPath -PathType Leaf) { [long](Get-Item -LiteralPath $gameLogPath).Length } else { [long]0 }
    $launchInfo = [Mgs5vr.BackgroundNative]::Launch($gameExe,'',$gameDirectory,$desktopName,$runId,$sessionPath,$envMap)
    $startupWatch = [Diagnostics.Stopwatch]::StartNew()
    $startupSamples = [Collections.Generic.List[object]]::new()
    $startupExitObserved = $false
    $startupExitCode = $null
    $nextSampleMs = [long]0
    try {
        while ($startupWatch.ElapsedMilliseconds -lt 45000) {
            $elapsed = [long]$startupWatch.ElapsedMilliseconds
            if ($elapsed -ge $nextSampleMs) {
                $startupSamples.Add((Get-StartupProgress $launchInfo.ProcessId $gameDirectory $initialLogLength $elapsed))
                $nextSampleMs = $elapsed + 5000
            }
            $remaining = [Math]::Max(1,45000 - [int]$startupWatch.ElapsedMilliseconds)
            $waitMs = [uint32][Math]::Min(1000,$remaining)
            if ([Mgs5vr.BackgroundNative]::WaitForProcess($launchInfo.ProcessHandle,$waitMs) -eq 0) {
                $startupExitObserved = $true
                $startupExitCode = [Mgs5vr.BackgroundNative]::ReadExitCode($launchInfo.ProcessHandle)
                break
            }
        }
        $startupWatch.Stop()
        $activeDesktopAfterObservation = $null
        $activeDesktopCheckError = $null
        try { $activeDesktopAfterObservation = [Mgs5vr.BackgroundNative]::GetInputDesktopName() }
        catch { $activeDesktopCheckError = $_.Exception.Message }
        $startupSamples.Add((Get-StartupProgress $launchInfo.ProcessId $gameDirectory $initialLogLength ([long]$startupWatch.ElapsedMilliseconds)))
    } finally {
        [Mgs5vr.BackgroundNative]::CloseProcessHandle($launchInfo.ProcessHandle)
    }
    $startupObservation = [ordered]@{
        timeoutMs=45000; elapsedMs=[long]$startupWatch.ElapsedMilliseconds
        processExitObserved=$startupExitObserved; exitCodeAvailable=$startupExitObserved; exitCode=$startupExitCode
        activeInputDesktopCheckError=$activeDesktopCheckError
        activeInputDesktopAfterObservation=$activeDesktopAfterObservation
        activeInputDesktopUnchanged=($null -ne $activeDesktopAfterObservation -and $launchInfo.ActiveDesktopBefore -eq $activeDesktopAfterObservation)
        samples=@($startupSamples.ToArray())
    }
    $record = [ordered]@{
        schema=1; runId=$runId; status=if ($startupExitObserved) { 'exited-during-startup-observation' } else { 'running-after-startup-observation' }; lastStatusReport=$null; startedUtc=[DateTime]::UtcNow.ToString('o')
        processId=$launchInfo.ProcessId; mainThreadId=$launchInfo.MainThreadId; creationFileTimeUtc=$launchInfo.CreationFileTimeUtc
        desktopName=$launchInfo.DesktopName; activeDesktopBefore=$launchInfo.ActiveDesktopBefore; activeDesktopAfter=$launchInfo.ActiveDesktopAfter
        activeDesktopUnchanged=$launchInfo.ActiveDesktopUnchanged; gameExe=$gameExe; gameSha256=$gameHash; gameDirectory=$gameDirectory
        installedDllSha256=$installedDllHash; expectedDllSha256=$expectedDllHash; installRecordDllSha256=$recordedDllHash
        installRecordMatchesInstalledDll=($installedDllHash -eq $recordedDllHash.ToUpperInvariant()); dllHashSource=$dllHashSource
        runtimeManifest=$runtimePath; runtimeDll=$runtimeDll; operatorManifest=$operatorManifestPath; operatorDll=$operatorDll; operatorProxy=$operatorProxy
        graphicsConfig=$graphicsPath; windowMode=$video.window_mode; gameWindowWidth=[int]$video.width; gameWindowHeight=[int]$video.height
        graphicsConfigSha256=$graphicsHash; gameVrIni=$vrIni; gameVrIniSha256=$vrIniHash; gameVrIniParseValid=$true
        displayConfig=$displayPath; displayConfigSha256=$displayHash; displayConfigParseValid=$true
        controlsConfig=$controlsPath; controlsConfigSha256=$controlsHash
        independentMirrorEnabled=$true; nativeRenderWidth=$displayRenderWidth; nativeRenderHeight=$displayRenderHeight
        mirrorWidth=$displayMirrorWidth; mirrorHeight=$displayMirrorHeight
        steamClientPids=@($steamProcesses | ForEach-Object { $_.Id }); steamAppId='287700'; steamGameId='287700'
        audioEndpointChanged=$false; systemMuteChanged=$false; perGameQuietingVerified=$false; captureAudioVerified=$false
        simulatorFrontendManaged=$false; notes='The game was started directly. Runtime-created simulator windows and actual XR/capture progress require live verification.'
        startupObservation=$startupObservation
    }
    Write-SessionJson $sessionPath $record
    Write-SessionJson $currentPath ([ordered]@{ schema=1; runId=$runId; sessionFile=$sessionPath })
    $plan.result = 'game-process-created-private-desktop'
    $plan.startupRecord = $sessionPath
    $plan.processId = $launchInfo.ProcessId
    $plan.creationFileTimeUtc = $launchInfo.CreationFileTimeUtc
    $plan.desktopName = $desktopName
    $plan.activeDesktopUnchangedAfterCreate = $launchInfo.ActiveDesktopUnchanged
    $plan.result = if ($startupExitObserved) { 'direct-launch-exited-during-bounded-observation' } else { 'game-alive-after-bounded-observation; XR, simulator placement, capture, and audio still unverified' }
    Write-SessionJson $sessionPath $record
    $startupSummary = [ordered]@{
        timeoutMs=45000; elapsedMs=$startupObservation.elapsedMs
        processExitObserved=$startupObservation.processExitObserved; exitCodeAvailable=$startupObservation.exitCodeAvailable; exitCode=$startupObservation.exitCode
        activeInputDesktopAfterObservation=$startupObservation.activeInputDesktopAfterObservation
        activeInputDesktopUnchanged=$startupObservation.activeInputDesktopUnchanged
        samples=@($startupSamples | ForEach-Object {
            [ordered]@{
                elapsedMs=$_.elapsedMs; loadedModuleNames=$_.loadedModuleNames
                simulatorProcesses=$_.simulatorProcesses; logBytesSinceLaunch=$_.logBytesSinceLaunch
                recentRelevantLogLines=@($_.recentRelevantLogLines | Select-Object -Last 6)
            }
        })
    }
    $launchSummary = [ordered]@{
        status=$record.status; runId=$runId; processId=$launchInfo.ProcessId
        creationFileTimeUtc=$launchInfo.CreationFileTimeUtc; desktopName=$desktopName
        activeDesktopBefore=$launchInfo.ActiveDesktopBefore; activeDesktopAfterCreate=$launchInfo.ActiveDesktopAfter
        activeDesktopUnchangedAfterCreate=$launchInfo.ActiveDesktopUnchanged
        startupObservation=$startupSummary; startupRecord=$sessionPath
    }
    Write-Output ($launchSummary | ConvertTo-Json -Depth 8)
    Write-Output "Startup record: $sessionPath"
} else {
    $planPath = New-UniquePath 'preflight' '.json'
    Write-SessionJson $planPath $plan
    Write-Output ($plan | ConvertTo-Json -Depth 8)
    Write-Output "Preflight report: $planPath"
}
} finally {
    if ($sessionMutex) {
        try { $sessionMutex.ReleaseMutex() } catch { }
        $sessionMutex.Dispose()
    }
}
