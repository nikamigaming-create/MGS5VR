using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using Microsoft.Win32;
using Microsoft.Web.WebView2.Core;
using Microsoft.Web.WebView2.WinForms;

// Only this local, packaged origin can use the small launcher RPC surface.
sealed class FieldTerminal : Form {
    const string Origin = "https://mgs5vr.local";
    readonly WebView2 web = new WebView2();
    readonly JavaScriptSerializer json = new JavaScriptSerializer { MaxJsonLength=16*1024*1024 };
    readonly string package, ui, smoke, startPage;
    string game;
    bool busy;
    public FieldTerminal(string[] args) {
        string bin=AppDomain.CurrentDomain.BaseDirectory;
        package=Option(args,"--package") ?? (File.Exists(Path.Combine(bin,"tools","launcher-bridge.ps1")) ? bin : Path.GetFullPath(Path.Combine(bin,"..","..")));
        ui=Option(args,"--ui") ?? Path.Combine(bin,"launcher-ui");
        game=Option(args,"--game-exe") ?? FindGame(); smoke=Option(args,"--smoke-output");
        string requestedPage=Option(args,"--page");
        startPage=new[]{"home","tapes","modes","controls","settings","notes","tour"}.Contains(requestedPage)?requestedPage:"home";
        Text="MGS5VR / Field Terminal"; ClientSize=new Size(1440,940); MinimumSize=new Size(1000,700);
        StartPosition=FormStartPosition.CenterScreen; BackColor=Color.FromArgb(239,235,224);
        if(smoke!=null){Opacity=0;ShowInTaskbar=false;}
        web.Dock=DockStyle.Fill; Controls.Add(web); Shown+=async(s,e)=>await Initialize();
        FormClosing+=(s,e)=>{ if(busy){e.Cancel=true;MessageBox.Show(this,"Wait for the current save or installation to finish.",Text);} };
    }
    static string Option(string[] args,string key) { int i=Array.IndexOf(args,key);return i>=0&&i+1<args.Length?args[i+1]:null; }
    string FindGame() {
        string saved=Registry.GetValue(@"HKEY_CURRENT_USER\Software\Nikami\MGS5VR\Launcher","TppExe","") as string;
        if(File.Exists(saved))return saved;
        var roots=new List<string>();
        string steam=Registry.GetValue(@"HKEY_CURRENT_USER\Software\Valve\Steam","SteamPath","") as string;
        if(!String.IsNullOrEmpty(steam)) {
            roots.Add(steam);string vdf=Path.Combine(steam,"steamapps","libraryfolders.vdf");
            if(File.Exists(vdf))foreach(Match m in Regex.Matches(File.ReadAllText(vdf),"\"path\"\\s+\"([^\"]+)\""))roots.Add(m.Groups[1].Value.Replace(@"\\",@"\"));
        }
        return roots.Select(r=>Path.Combine(r,"steamapps","common","MGS_TPP","mgsvtpp.exe")).FirstOrDefault(File.Exists) ?? "";
    }
    async Task Initialize() {
        try {
            var environment=await CoreWebView2Environment.CreateAsync(null,Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"MGS5VR","FieldTerminal"));
            await web.EnsureCoreWebView2Async(environment);
            web.CoreWebView2.SetVirtualHostNameToFolderMapping("mgs5vr.local",ui,CoreWebView2HostResourceAccessKind.DenyCors);
            web.CoreWebView2.Settings.AreDefaultContextMenusEnabled=false;
            web.CoreWebView2.Settings.AreDevToolsEnabled=smoke!=null;
            web.CoreWebView2.Settings.IsStatusBarEnabled=false;
            web.CoreWebView2.NavigationStarting+=(s,e)=>{if(!e.Uri.StartsWith(Origin+"/",StringComparison.Ordinal))e.Cancel=true;};
            web.CoreWebView2.NewWindowRequested+=(s,e)=>e.Handled=true;
            web.CoreWebView2.PermissionRequested+=(s,e)=>e.State=CoreWebView2PermissionState.Deny;
            web.CoreWebView2.WebMessageReceived+=Message;
            await web.CoreWebView2.AddScriptToExecuteOnDocumentCreatedAsync("window.__launcherStartPage="+json.Serialize(startPage)+";");
            web.CoreWebView2.Navigate(Origin+"/index.html");
            if(smoke!=null) await Smoke();
        } catch(Exception e) {if(smoke!=null){Directory.CreateDirectory(smoke);File.WriteAllText(Path.Combine(smoke,"error.txt"),e.ToString());}else MessageBox.Show(this,e.Message+"\nUse --classic for the original launcher. Install Microsoft Edge WebView2 Runtime if unavailable.",Text);Environment.ExitCode=1;Close(); }
    }
    async Task<string> Run(string exe, params string[] args) {
        using(var p=new Process()) {
            p.StartInfo=new ProcessStartInfo(exe,String.Join(" ",args.Select(Quote))) { UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true,WorkingDirectory=package };
            p.Start();var output=p.StandardOutput.ReadToEndAsync();var error=p.StandardError.ReadToEndAsync();
            await Task.Run(()=>p.WaitForExit());string text=await output,errors=await error;
            if(p.ExitCode!=0)throw new Exception(String.IsNullOrWhiteSpace(errors)?text:errors);
            return text;
        }
    }
    static string Quote(string value) {
        var b=new StringBuilder("\"");int n=0;
        foreach(char c in value){if(c=='\\'){n++;continue;}b.Append('\\',c=='"'?2*n+1:n);n=0;b.Append(c);}b.Append('\\',2*n);return b.Append('"').ToString();
    }
    Task<string> Script(string name,params string[] args) {
        return Run(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System),@"WindowsPowerShell\v1.0\powershell.exe"),new[]{"-NoLogo","-NoProfile","-NonInteractive","-ExecutionPolicy","Bypass","-File",Path.Combine(package,"tools",name)}.Concat(args).ToArray());
    }
    bool Running() {return Process.GetProcessesByName("mgsvtpp").Length>0||Process.GetProcessesByName("MgsGroundZeroes").Length>0;}
    async Task<object> Settings(string method,Dictionary<string,object> payload) {
        if(!File.Exists(game))return new {needsGame=true,gameExe="",gameRunning=Running()};
        payload["method"]=method;payload["gameExe"]=game;
        string temp=Path.Combine(Path.GetTempPath(),"mgs5vr-launcher-"+Guid.NewGuid().ToString("N")+".json");
        try {
            File.WriteAllText(temp,json.Serialize(payload),new UTF8Encoding(false));
            var state=json.Deserialize<Dictionary<string,object>>(await Script("launcher-bridge.ps1","-RequestPath",temp));
            state["gameRunning"]=Running(); return state;
        } finally {if(File.Exists(temp))File.Delete(temp);}
    }
    async void Message(object sender,CoreWebView2WebMessageReceivedEventArgs e) {
        if(e.Source!=Origin+"/index.html")return;
        object id=null;bool ownsOperation=false;
        try {
            var request=json.Deserialize<Dictionary<string,object>>(e.WebMessageAsJson);id=request["id"];
            string method=(string)request["method"];
            var payload=request.ContainsKey("payload") ? request["payload"] as Dictionary<string,object> : new Dictionary<string,object>();
            if(busy)throw new Exception("Another operation is still finishing.");
            busy=true;ownsOperation=true;object result;
            if(method=="load"||method=="saveControls"||method=="saveRuntime") result=await Settings(method,payload);
            else if(method=="chooseGame") {
                if(smoke!=null)throw new Exception("File selection disabled during test.");
                using(var dialog=new OpenFileDialog {Title="Select Metal Gear Solid V",Filter="MGSV executable|mgsvtpp.exe;MgsGroundZeroes.exe",CheckFileExists=true}) {
                    if(dialog.ShowDialog(this)==DialogResult.OK){game=dialog.FileName;Registry.SetValue(@"HKEY_CURRENT_USER\Software\Nikami\MGS5VR\Launcher",Path.GetFileName(game).Equals("mgsvtpp.exe",StringComparison.OrdinalIgnoreCase)?"TppExe":"GzExe",game);}
                }
                result=await Settings("load",new Dictionary<string,object>());
            } else if(method=="launch") {
                if(smoke!=null||Running())throw new Exception("A game is already running, or this is a test window.");
                if(!File.Exists(game))throw new Exception("Choose a game first.");
                string runtime=Convert.ToString(payload["runtime"]);
                if(runtime!="meta"&&runtime!="active")throw new Exception("Choose a supported headset runtime.");
                string preset=payload.ContainsKey("preset")?Convert.ToString(payload["preset"]):"Current";
                if(preset!="Current"&&preset!="Headset"&&preset!="Custom")throw new Exception("Unknown display preset.");
                if(preset!="Current") {
                    var displayArgs=new List<string>{"-Mode","Apply","-GameExe",game,"-Preset",preset,"-Scale",Convert.ToString(payload["scale"]),"-Width",Convert.ToString(payload["width"]),"-Height",Convert.ToString(payload["height"])};
                    if(runtime=="meta")displayArgs.AddRange(new[]{"-RuntimeManifest",Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),@"Oculus\Support\oculus-runtime\oculus_openxr_64.json")});
                    await Script("launcher-display.ps1",displayArgs.ToArray());
                }
                if(runtime=="meta") {
                    string manifest=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),@"Oculus\Support\oculus-runtime\oculus_openxr_64.json");
                    if(!File.Exists(manifest))throw new Exception("Quest Link runtime not found. Install or repair Meta Quest Link first.");
                    result=await Script("launch-headset.ps1","-GameDir",Path.GetDirectoryName(game),"-RuntimeManifest",manifest);
                } else if(runtime=="active")result=await Script("launch-headset.ps1","-GameDir",Path.GetDirectoryName(game));
                else throw new Exception("Choose a supported headset runtime.");
            } else if(method=="install") {
                if(smoke!=null||Running())throw new Exception("Close the game before installing or updating.");
                result=await Script("launcher-maintenance.ps1","-Mode",File.Exists(Path.Combine(Path.GetDirectoryName(game),"mgs5vr-install.json"))?"Update":"Install","-GameExe",game);
            } else if(method=="maintenance") {
                if(smoke!=null)throw new Exception("Maintenance disabled during test.");
                Process.Start(new ProcessStartInfo(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"MGS5VR-Launcher.exe"),"--classic"){UseShellExecute=false});
                result="Maintenance tools opened.";
            } else if(method=="display") {
                if(smoke!=null||Running())throw new Exception("Close the game before applying display settings.");
                string preset=Convert.ToString(payload["preset"]);
                if(preset!="Current"&&preset!="Headset"&&preset!="Custom")throw new Exception("Unknown display preset.");
                var displayArgs=new List<string>{"-Mode","Apply","-GameExe",game,"-Preset",preset,"-Scale",Convert.ToString(payload["scale"]),"-Width",Convert.ToString(payload["width"]),"-Height",Convert.ToString(payload["height"])};
                if(payload.ContainsKey("runtime")&&Convert.ToString(payload["runtime"])=="meta")displayArgs.AddRange(new[]{"-RuntimeManifest",Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),@"Oculus\Support\oculus-runtime\oculus_openxr_64.json")});
                result=await Script("launcher-display.ps1",displayArgs.ToArray());
            } else throw new Exception("Unknown launcher action.");
            web.CoreWebView2.PostWebMessageAsJson(json.Serialize(new {id=id,result=result}));
        } catch(Exception ex) {web.CoreWebView2.PostWebMessageAsJson(json.Serialize(new {id=id,error=ex.Message}));}
        finally {if(ownsOperation)busy=false;}
    }
    async Task Smoke() {
        for(int i=0;i<100;i++){await Task.Delay(200);if(await web.ExecuteScriptAsync("Boolean(window.launcherQA && window.launcherQA.ready)")=="true")break;}
        Directory.CreateDirectory(smoke);
        using(var file=File.Create(Path.Combine(smoke,"host.png")))await web.CoreWebView2.CapturePreviewAsync(CoreWebView2CapturePreviewImageFormat.Png,file);
        foreach(var stage in new[]{"tapes","modes","settings"}){
            await web.ExecuteScriptAsync("window.__hostStage=null;window.launcherQA.selectTab("+json.Serialize(stage)+").then(()=>window.__hostStage='ready').catch(e=>window.__hostStage=e.message)");
            for(int i=0;i<100;i++){await Task.Delay(100);if(await web.ExecuteScriptAsync("window.__hostStage!==null")=="true")break;}
            await Task.Delay(250);
            using(var file=File.Create(Path.Combine(smoke,"host-"+stage+".png")))await web.CoreWebView2.CapturePreviewAsync(CoreWebView2CapturePreviewImageFormat.Png,file);
        }
        string result=await web.ExecuteScriptAsync("window.launcherQA ? window.launcherQA.snapshot() : {error:'UI did not initialize'}");
        File.WriteAllText(Path.Combine(smoke,"host.json"),result);
        if(!result.Contains("\"ready\":true")||!result.Contains("\"controllers\":true")||!result.Contains("\"settings\":46"))Environment.ExitCode=1;
        Close();
    }
    [STAThread] static void Main(string[] args){Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);Application.Run(new FieldTerminal(args));}
}
