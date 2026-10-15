param()
$ErrorActionPreference = 'Stop'
$audioRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Content/Audio/Plaguefront'))
foreach ($folder in @('Music','Sfx','Ambient')) { New-Item -ItemType Directory -Path (Join-Path $audioRoot $folder) -Force | Out-Null }
if (-not ('PlaguefrontSynth' -as [type])) {
Add-Type -TypeDefinition @'
using System;
using System.IO;
public sealed class PlaguefrontSynth {
  public const int Rate=22050;
  double[] data; bool loop; Random rng=new Random(14837);
  public PlaguefrontSynth(double seconds,bool looping=false){data=new double[(int)Math.Round(seconds*Rate)];loop=looping;}
  static double F(double midi){return 440*Math.Pow(2,(midi-69)/12);}
  static double Env(double t,double duration,double attack,double release){return Math.Min(1,t/attack)*Math.Min(1,(duration-t)/release);}
  void Put(int i,double value){if(loop){i%=data.Length;if(i<0)i+=data.Length;}if(i>=0&&i<data.Length)data[i]+=value;}
  public void Note(double midi,double start,double duration,double gain,string instrument){
    double hz=F(midi);int first=(int)Math.Round(start*Rate),length=(int)(duration*Rate);
    for(int i=0;i<length;i++){
      double t=(double)i/Rate,p=2*Math.PI*hz*t,v=0,e=1;
      switch(instrument){
        case "harp":v=Math.Sin(p)+.34*Math.Sin(2*p)+.17*Math.Sin(3*p)+.08*Math.Sin(5*p);e=Math.Exp(-5*t/duration)*Env(t,duration,.004,.10);break;
        case "reed":v=Math.Sin(p+Math.Sin(2*Math.PI*4.8*t)*.045)+.21*Math.Sin(2*p)+.09*Math.Sin(3*p);e=Env(t,duration,.06,.16);break;
        case "strings":v=Math.Sin(p)+.3*Math.Sin(2*p+.018*Math.Sin(t*25))+.17*Math.Sin(3*p)+.10*Math.Sin(4*p);e=Env(t,duration,.15,.25);break;
        case "brass":v=Math.Sin(p)+.44*Math.Sin(2*p)+.21*Math.Sin(3*p);e=Env(t,duration,.035,.20);break;
        case "bell":v=Math.Sin(p)*Math.Exp(-3*t/duration)+.45*Math.Sin(p*2.756)*Math.Exp(-8*t/duration)+.18*Math.Sin(p*5.404)*Math.Exp(-14*t/duration);e=Env(t,duration,.002,.12);break;
        case "bass":v=Math.Sin(p)+.2*Math.Sin(2*p);e=Env(t,duration,.025,.13);break;
        default:v=Math.Sin(p);e=Env(t,duration,.02,.1);break;
      }
      Put(first+i,v*e*gain);
    }
  }
  public void Noise(double start,double duration,double gain,double smooth,double decay){
    int first=(int)(start*Rate),n=(int)(duration*Rate);double z=0;
    for(int i=0;i<n;i++){double t=(double)i/Rate;z=z*smooth+(rng.NextDouble()*2-1)*(1-smooth);Put(first+i,z*gain*Math.Exp(-decay*t/duration)*Env(t,duration,.003,.06));}
  }
  public void Sweep(double start,double duration,double from,double to,double gain,double grit=0){
    int first=(int)(start*Rate),n=(int)(duration*Rate);double phase=0,noise=0;
    for(int i=0;i<n;i++){double t=(double)i/Rate,frac=t/duration;phase+=2*Math.PI*(from*Math.Pow(to/from,frac))/Rate;noise=noise*.6+(rng.NextDouble()*2-1)*.4;
      Put(first+i,(Math.Sin(phase)+.24*Math.Sin(phase*2)+grit*noise)*gain*Math.Pow(1-frac,1.6)*Env(t,duration,.003,.04));}
  }
  public void Bed(double midi,double gain){
    double length=(double)data.Length/Rate,hz=Math.Round(F(midi)*length)/length;
    for(int i=0;i<data.Length;i++){double t=(double)i/Rate;data[i]+=gain*(Math.Sin(2*Math.PI*hz*t)+.16*Math.Sin(2*Math.PI*hz*2*t))*(.88+.12*Math.Cos(2*Math.PI*t/length));}
  }
  public void Wind(double gain){
    double length=(double)data.Length/Rate;
    for(int harmonic=0;harmonic<72;harmonic++){
      double hz=Math.Round((32+harmonic*13.7)*length)/length,phase=rng.NextDouble()*Math.PI*2,a=gain/(2+harmonic*.18);
      for(int i=0;i<data.Length;i++){double t=(double)i/Rate;data[i]+=a*Math.Sin(2*Math.PI*hz*t+phase)*(.7+.3*Math.Cos(2*Math.PI*t/length*3));}
    }
  }
  public void Write(string path){
    double peak=0;foreach(double x in data)peak=Math.Max(peak,Math.Abs(x));double scale=peak>.84?.84/peak:1;
    using(var writer=new BinaryWriter(File.Create(path))){writer.Write(System.Text.Encoding.ASCII.GetBytes("RIFF"));writer.Write(36+data.Length*2);writer.Write(System.Text.Encoding.ASCII.GetBytes("WAVEfmt "));writer.Write(16);writer.Write((short)1);writer.Write((short)1);writer.Write(Rate);writer.Write(Rate*2);writer.Write((short)2);writer.Write((short)16);writer.Write(System.Text.Encoding.ASCII.GetBytes("data"));writer.Write(data.Length*2);
      for(int i=0;i<data.Length;i++){double edge=loop?1:Math.Min(1,Math.Min(i,data.Length-1-i)/110.0);writer.Write((short)Math.Round(Math.Max(-1,Math.Min(1,data[i]*scale*edge))*32767));}}
  }
  static void Hit(PlaguefrontSynth s,double at,double gain){s.Sweep(at,.16,130,42,gain);s.Noise(at,.13,gain*.7,.55,5);}
  public static void Sfx(string name,string file){
    double seconds=.9;if(name=="notice"||name=="rot_birth"||name=="purify"||name=="spell_lightning")seconds=1.5;
    if(name=="rot_pulse")seconds=1.9;
    var s=new PlaguefrontSynth(seconds);
    switch(name){
      case "hover":s.Note(76,0,.12,.07,"harp");s.Note(83,.035,.12,.025,"bell");break;
      case "select":s.Note(62,0,.16,.12,"harp");s.Note(69,.045,.20,.07,"harp");break;
      case "confirm":s.Note(62,0,.28,.11,"bell");s.Note(69,.06,.30,.10,"harp");s.Note(74,.13,.34,.08,"bell");break;
      case "cancel":s.Note(65,0,.17,.10,"reed");s.Note(60,.08,.23,.08,"harp");break;
      case "menu":for(int i=0;i<4;i++)s.Note(50+i*5,i*.045,.3,.065,"harp");break;
      case "open":s.Noise(0,.22,.16,.83,4);s.Note(57,.02,.2,.065,"harp");break;
      case "close":s.Noise(0,.16,.13,.90,6);s.Note(45,.02,.19,.075,"harp");break;
      case "notice":s.Note(50,0,1.2,.09,"bell");s.Note(62,.16,1,.065,"bell");s.Note(69,.34,.9,.05,"reed");break;
      case "handoff":s.Note(57,0,.23,.08,"brass");s.Note(62,.20,.43,.10,"brass");break;
      case "move":Hit(s,0,.095);Hit(s,.17,.075);s.Noise(.03,.10,.04,.8,4);break;
      case "sword":s.Noise(0,.15,.38,.38,3);s.Note(82,.10,.32,.095,"bell");Hit(s,.09,.10);break;
      case "dagger":s.Noise(0,.08,.24,.20,5);s.Note(88,.065,.20,.065,"bell");break;
      case "axe":s.Noise(0,.24,.30,.65,2);Hit(s,.18,.24);s.Note(42,.19,.37,.08,"strings");break;
      case "arrow":s.Sweep(0,.23,1700,430,.13,.7);s.Noise(.17,.11,.2,.62,5);break;
      case "spear":s.Sweep(0,.21,620,155,.11,.55);s.Note(68,.15,.22,.09,"harp");Hit(s,.14,.09);break;
      case "charge":for(int i=0;i<4;i++)Hit(s,i*.12,.08+i*.025);s.Noise(.32,.24,.14,.7,3);break;
      case "impact":Hit(s,0,.21);s.Noise(0,.21,.14,.72,6);break;
      case "death":s.Sweep(0,.64,180,44,.12,.45);s.Noise(.06,.43,.17,.89,2);break;
      case "ability":s.Note(50,0,.36,.07,"strings");s.Note(62,.08,.33,.08,"harp");s.Note(69,.15,.34,.06,"bell");break;
      case "spell_fire":s.Noise(0,.68,.38,.64,1.6);s.Sweep(0,.45,110,490,.08,.5);break;
      case "spell_ice":for(int i=0;i<7;i++)s.Note(78+i*3,i*.045,.4,.06,"bell");s.Noise(.05,.48,.075,.24,2);break;
      case "spell_lightning":s.Noise(0,.075,.68,.12,2);s.Noise(.08,.35,.3,.78,5);s.Sweep(.015,.7,270,38,.12,.8);break;
      case "spell_heal":s.Note(62,0,.6,.07,"reed");s.Note(65,.1,.6,.08,"bell");s.Note(69,.2,.6,.08,"bell");s.Note(74,.3,.55,.07,"harp");break;
      case "spell_support":s.Note(57,0,.5,.07,"strings");s.Note(64,.1,.5,.065,"reed");s.Note(69,.18,.5,.055,"bell");break;
      case "incinerate":s.Noise(0,.7,.31,.78,2);s.Sweep(0,.5,340,65,.07,.4);break;
      case "purify":for(int i=0;i<5;i++)s.Note(62+i*5,i*.11,.65,.055,"bell");s.Noise(.05,.8,.045,.92,3);break;
      case "rot_birth":s.Sweep(0,1.25,68,127,.08,.7);s.Noise(.1,1.1,.23,.94,1.2);s.Note(39,.12,1.1,.06,"strings");s.Note(40,.25,1.05,.035,"strings");break;
      case "rot_pulse":s.Sweep(0,.75,49,32,.085,.2);s.Sweep(.75,.85,52,31,.065,.4);s.Noise(.12,1.45,.1,.96,1);break;
      case "capture_house":s.Note(50,0,.35,.08,"brass");s.Note(57,.17,.4,.08,"reed");s.Note(62,.35,.4,.075,"bell");break;
      case "capture_fortress":s.Note(38,0,.7,.09,"brass");s.Note(50,.12,.6,.085,"brass");s.Note(57,.28,.5,.075,"bell");break;
      case "recruit":Hit(s,0,.07);s.Note(50,.05,.23,.075,"brass");s.Note(62,.22,.42,.07,"reed");break;
      case "buy":s.Note(83,0,.28,.07,"bell");s.Note(88,.075,.3,.05,"bell");s.Noise(.03,.1,.025,.4,5);break;
      case "income":for(int i=0;i<3;i++)s.Note(76+i*5,i*.075,.35,.04,"bell");break;
      case "build":Hit(s,0,.13);Hit(s,.24,.10);s.Note(50,.43,.35,.065,"harp");break;
      case "item":s.Note(72,0,.17,.075,"bell");s.Noise(.09,.23,.09,.90,3);s.Note(65,.17,.35,.065,"reed");break;
      case "turn":s.Note(50,0,.16,.07,"harp");s.Note(57,.10,.22,.055,"reed");break;
      case "round":s.Note(38,0,.35,.075,"bass");s.Note(57,.14,.45,.065,"bell");s.Note(62,.27,.4,.055,"reed");break;
      default:throw new Exception("Unknown cue: "+name);
    }s.Write(file);
  }
  public static void Music(string name,string file){
    bool battle=name=="battle";double beat=battle?.6:.75;var s=new PlaguefrontSynth(beat*32,true);
    int[][] chords={new[]{50,53,57},new[]{46,50,53},new[]{43,46,50},new[]{45,49,52},new[]{50,53,57},new[]{48,52,55},new[]{46,50,53},new[]{45,49,52}};
    int[] melody={74,77,76,69,72,74,70,69,67,70,74,72,73,76,69,73,74,77,81,79,76,72,74,76,77,74,70,65,69,73,76,73};
    s.Bed(38,.014);s.Bed(45,.009);
    for(int bar=0;bar<8;bar++){
      double at=bar*4*beat;int[] c=chords[bar];
      foreach(int note in c)s.Note(note,at,beat*4.5,.021,"strings");
      s.Note(c[0]-12,at,beat*1.8,.065,"bass");s.Note(c[0]-12,at+beat*2,beat*1.8,.047,"bass");
      int[] pattern={0,1,2,1,0,2,1,2};
      for(int k=0;k<8;k++)s.Note(c[pattern[k]]+12,at+k*beat*.5,beat*.9,battle?.045:.053,"harp");
      for(int k=0;k<4;k++)s.Note(melody[bar*4+k],at+k*beat+(k==1?beat*.12:0),beat*(k==3?1.35:.85),battle?.046:.052,battle?"brass":"reed");
      if(battle){for(int k=0;k<4;k++){Hit(s,at+k*beat,k%2==0?.09:.055);s.Noise(at+k*beat+beat*.5,.09,.07,.45,7);}}
      else {s.Note(c[1]+24,at+beat*2.5,beat*1.3,.019,"bell");s.Noise(at+beat*3,.14,.025,.8,7);}
    }s.Write(file);
  }
  public static void Stinger(bool win,string file){var s=new PlaguefrontSynth(6.4);int[] notes=win?new[]{50,57,62,65,69,74}:new[]{50,48,46,45,41,38};
    for(int i=0;i<notes.Length;i++)s.Note(notes[i],i*.45,1.8,win?.10:.075,win?"brass":"strings");
    s.Note(win?50:38,0,5.8,.055,"bass");s.Note(win?62:45,2.9,3.2,.055,"strings");s.Note(win?65:50,2.9,3.2,.04,"strings");
    if(win){s.Note(81,3.2,2.7,.045,"bell");s.Note(86,3.45,2.5,.035,"bell");}else s.Noise(2.5,2.9,.05,.96,2);s.Write(file);}
  public static void Ambient(string file){var s=new PlaguefrontSynth(18,true);s.Wind(.015);s.Bed(26,.008);s.Bed(33,.005);
    for(int i=0;i<6;i++){s.Note(43+(i%3)*7,i*3+.5,1.9,.007,"strings");}s.Write(file);}
}
'@
}
[PlaguefrontSynth]::Music('menu',(Join-Path $audioRoot 'Music/menu.wav'))
[PlaguefrontSynth]::Music('battle',(Join-Path $audioRoot 'Music/battle.wav'))
[PlaguefrontSynth]::Stinger($true,(Join-Path $audioRoot 'Music/victory.wav'))
[PlaguefrontSynth]::Stinger($false,(Join-Path $audioRoot 'Music/defeat.wav'))
[PlaguefrontSynth]::Ambient((Join-Path $audioRoot 'Ambient/battle.wav'))
$soundNames=@('hover','select','confirm','cancel','menu','open','close','notice','handoff','move','sword','dagger','axe','arrow','spear','charge','impact','death','ability','spell_fire','spell_ice','spell_lightning','spell_heal','spell_support','incinerate','purify','rot_birth','rot_pulse','capture_house','capture_fortress','recruit','buy','income','build','item','turn','round')
$cues=@()
foreach($name in @('menu','battle','victory','defeat')) {$cues+=[ordered]@{id="music_$name";file="Content/Audio/Plaguefront/Music/$name.wav";bus='music';gain=if($name -eq 'battle'){51}else{58};loop=($name -in @('menu','battle'));cooldown=0}}
$cues+=[ordered]@{id='ambient_battle';file='Content/Audio/Plaguefront/Ambient/battle.wav';bus='ambient';gain=30;loop=$true;cooldown=0}
foreach($name in $soundNames) {
  [PlaguefrontSynth]::Sfx($name,(Join-Path $audioRoot "Sfx/$name.wav"))
  $gain=if($name -eq 'hover'){30}elseif($name -in @('income','move','turn','rot_pulse')){48}elseif($name -in @('notice','rot_birth','capture_fortress','spell_lightning')){78}else{65}
  $cooldown=if($name -eq 'hover'){0.12}elseif($name -in @('move','impact','income')){0.15}elseif($name -eq 'rot_pulse'){5.0}else{0.08}
  $cues+=[ordered]@{id=$name;file="Content/Audio/Plaguefront/Sfx/$name.wav";bus='effects';gain=$gain;loop=$false;cooldown=$cooldown}
}
$manifest=[ordered]@{version=1;origin='Original local procedural compositions and sound design, PlaguefrontSynth';format='PCM signed 16-bit mono WAV';sample_rate=22050;cues=$cues}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $PSScriptRoot '../Content/Plaguefront/audio.json') -Encoding utf8
Write-Output "Generated 4 music tracks, 1 ambient loop, $($soundNames.Count) distinct effects; 42 cues, entirely local."
