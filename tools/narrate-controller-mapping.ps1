param([Parameter(Mandatory=$true)][string]$ScriptPath)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Speech
$mgsFilm=Get-Content -LiteralPath $ScriptPath -Raw | ConvertFrom-Json
$mgsSpeech=[System.Speech.Synthesis.SpeechSynthesizer]::new()
try {
    $mgsSpeech.SelectVoice('Microsoft Zira Desktop')
    $mgsSpeech.Rate=1
    foreach ($mgsScene in $mgsFilm.scenes) {
        $mgsSpeech.SetOutputToWaveFile([string]$mgsScene.audio)
        $mgsSpeech.Speak(([string]$mgsScene.narration).Replace('iDroid','eye Droid').Replace('MGS5VR','Metal Gear Solid Five V R'))
        $mgsSpeech.SetOutputToNull()
    }
} finally { $mgsSpeech.Dispose() }
