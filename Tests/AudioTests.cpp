#include "Managers/AudioManager.h"
#include "Plaguefront/Audio.h"
#include "Config/PlaguefrontPresentationConfig.h"
#include <SFML/Audio/Listener.hpp>
#include <SFML/Audio/PlaybackDevice.hpp>
#include <SFML/Audio/SoundBuffer.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {
int checks=0;
void Check(bool condition,const char *message){++checks;if(!condition)throw std::runtime_error(message);}
Plaguefront::Match LastActivation(bool caster) {
  using namespace Plaguefront;
  Match match(Config{});match.state.mode=Mode::VersusAI;match.state.difficulty=Difficulty::Hard;match.state.currentPlayer=1;
  match.state.players[0].coins=match.state.players[1].coins=0;
  for(int q=-4;q<=4;++q)match.state.cells.push_back({{q,0}});
  match.At({-4,0})->building=Building::Fortress;match.At({-4,0})->owner=0;
  match.At({4,0})->building=Building::Fortress;match.At({4,0})->owner=1;
  Unit human;human.id=match.state.nextId++;human.owner=0;human.pos={0,0};human.hp=28;human.activated=true;
  Unit ai;ai.id=match.state.nextId++;ai.owner=1;ai.type=caster?UnitClass::Caster:UnitClass::Saber;ai.pos={caster?2:1,0};ai.hp=caster?20:28;ai.mp=caster?8:0;
  match.state.units={human,ai};
  if(!caster)match.At({2,0})->terrain=Terrain::Blocked;
  return match;
}
}
int main(){try{
  const auto devices=sf::PlaybackDevice::getAvailableDevices();
  std::cout<<"Playback devices available: "<<devices.size()<<" (test output remains muted).\n";
  std::ifstream manifest("Content/Plaguefront/audio.json");Check(static_cast<bool>(manifest),"Audio manifest exists");
  const auto data=nlohmann::json::parse(manifest);
  Check(data.at("cues").size()>=35,"Complete UI, battle, music and environment cues");
  std::set<std::string> ids,files;std::set<std::uint64_t> fingerprints;
  int music=0,ambient=0,effects=0;
  for(const auto &cue:data.at("cues")){
    const auto id=cue.at("id").get<std::string>(),file=cue.at("file").get<std::string>();
    Check(ids.insert(id).second,"Unique cue identifier");Check(files.insert(file).second,"Distinct original audio file per cue");
    sf::SoundBuffer buffer;Check(buffer.loadFromFile(file),"SFML decodes generated WAV");
    Check(buffer.getSampleRate()==22050&&buffer.getChannelCount()==1,"Declared PCM sample rate and channels");
    const auto count=buffer.getSampleCount();Check(count>1000,"Audio contains real samples");
    double energy=0;int peak=0;std::uint64_t fingerprint=1469598103934665603ULL;
    for(std::uint64_t i=0;i<count;++i){const auto sample=buffer.getSamples()[i];const double x=static_cast<double>(sample)/32768.0;energy+=x*x;peak=std::max(peak,std::abs(static_cast<int>(sample)));if(i%97==0){fingerprint^=static_cast<std::uint16_t>(sample);fingerprint*=1099511628211ULL;}}
    const double rms=std::sqrt(energy/static_cast<double>(count));
    Check(rms>0.0003&&peak>100,"Cue has nonzero intentional waveform");Check(peak<32767,"No clipped PCM samples");Check(fingerprints.insert(fingerprint).second,"Cues have distinct waveforms");
    const auto bus=cue.at("bus").get<std::string>();const double seconds=static_cast<double>(count)/22050;
    if(bus=="music"){
      ++music;if(cue.value("loop",false)){Check(seconds>=16&&seconds<=30,"Music loop length");Check(std::abs(static_cast<int>(buffer.getSamples()[0])-buffer.getSamples()[count-1])<2500,"Loop boundary avoids an abrupt discontinuity");}
      else Check(seconds>=5&&seconds<=8,"Victory and defeat stinger lengths");
    }else if(bus=="ambient"){++ambient;Check(cue.value("loop",false),"Ambient loops");}
    else{++effects;Check(seconds<2.5,"Sound effects remain short");}
  }
  Check(music==4&&ambient==1&&effects>=30,"Music, ambience and varied effects present");
  AudioManager manager;Plaguefront::AudioDirector director(manager);
  Check(director.Start(),"Director preloads manifest exactly once");
  const auto loaded=manager.LoadedCueCount();Check(director.Start()&&manager.LoadedCueCount()==loaded,"Repeated Start reuses cache");
  director.SetVolumes(71,33,48,false);Check(manager.GetGlobalVolume()==71&&manager.GetMusicVolume()==33&&manager.GetEffectsVolume()==48,"Independent channels apply immediately");
  director.SetVolumes(71,33,48,true);Check(manager.IsMuted()&&sf::Listener::getGlobalVolume()==0,"Mute applies to SFML listener immediately");
  director.SetVolumes(71,33,48,false);Check(sf::Listener::getGlobalVolume()==71,"Unmute restores selected master");
  // Silencia la reproducción durante la verificación para evitar sonidos de prueba no solicitados.
  director.SetVolumes(71,33,48,true);
  director.SetScene("menu");const auto starts=manager.MusicStarts();
  for(int i=0;i<20;++i)director.SetScene("menu");
  Check(manager.MusicStarts()==starts,"Same scene does not restart music");
  director.SetScene("battle");Check(manager.CurrentMusic()=="music_battle"&&manager.CurrentAmbient()=="ambient_battle","Battle switches music and ambience");Check(manager.ActiveMusicCount()<=1,"Single active music stream");
  if(!devices.empty())Check(manager.ActiveMusicCount()==1,"SFML reports music playing on available output device");
  Check(director.Play("hover"),"Effect starts");Check(!director.Play("hover"),"Per-cue debounce rejects identical immediate effect");
  director.Update(.2f);Check(director.Play("hover"),"Effect can play after cooldown");
  for(const auto &cue:data.at("cues"))if(cue.at("bus")=="effects")director.Play(cue.at("id").get<std::string>());
  Check(manager.ActiveVoiceCount()<=AudioManager::VoiceLimit&&manager.VoicePoolSize()<=12,"Voice pool is strictly bounded");
  Check(manager.EffectStarts()>15,"Director issued distinct effect playback commands");
  director.SetScene("victory");Check(manager.CurrentMusic()=="music_victory"&&manager.CurrentAmbient().empty(),"Victory stops battlefield ambience");
  director.SetScene("defeat");Check(manager.CurrentMusic()=="music_defeat"&&manager.ActiveMusicCount()<=1,"Defeat switches without music overlap");
  {
    using namespace Plaguefront;
    auto before=Match::Create(Config{},"Content/Plaguefront/scenarios/frontier.json",Mode::Local,Difficulty::Normal);
    const int actor=before.state.units.front().id;
    Check(before.BeginActivation(actor).ok,"Prepare legal activation for audio observation");
    auto after=before;const auto reachable=after.Reachable(actor);
    auto destination=std::find_if(reachable.begin(),reachable.end(),[&](const auto &entry){return entry.first!=before.Find(actor)->pos;});
    Check(destination!=reachable.end()&&after.Move(actor,destination->first).ok,"Execute real movement");
    director.Update(1);auto count=manager.CueStarts("move");director.Observe(before,after);Check(manager.CueStarts("move")==count+1,"Real position transition produces move audio");
    count=manager.EffectStarts();director.Observe(after,after);Check(manager.EffectStarts()==count,"Unchanged state produces no repeated events");
    before=Match::Create(Config{},"Content/Plaguefront/scenarios/frontier.json",Mode::Local,Difficulty::Normal);
    const int caster=before.state.units[4].id,enemy=before.state.units[5].id;
    before.Find(enemy)->pos={-1,3};before.Find(enemy)->slowThroughRound=before.state.round+1;
    Check(before.BeginActivation(caster).ok,"Prepare caster activation");after=before;
    Check(after.UseAbility(caster,Ability::Ice,after.Find(enemy)->pos).ok,"Execute real Ice with existing slow status");
    director.Update(1);count=manager.CueStarts("spell_ice");director.Observe(before,after,Ability::Ice);
    Check(manager.CueStarts("spell_ice")==count+1,"Ability hint preserves exact spell sound when status was already present");
  }
  {
    using namespace Plaguefront;
    for(const bool caster:{false,true}) {
      auto before=LastActivation(caster);auto after=before;
      Check(after.StepAI(),"Execute whole final AI activation");
      Check(after.EndTurn().ok,"Next human army ends before AI flags reset at its own turn start");
      Check(after.state.round==before.state.round+1&&!after.Find(2)->attacked,"AI final activation clears attacked at EndRound");
      Check(after.Find(1)->hp<before.Find(1)->hp,"AI performed a real damaging action");
      director.Update(1);const std::string cue=caster?"spell_lightning":"sword";
      const auto count=manager.CueStarts(cue);director.Observe(before,after);
      float movementSeconds=0.F;
      for(const auto &event:after.state.combatEvents)if(!event.counter&&event.chainIndex==0)
        if(const auto *actor=before.Find(event.attackerId);actor&&actor->pos!=event.from)
          if(const auto path=before.FindPath(actor->id,event.from))movementSeconds=static_cast<float>(path->cells.size()-1)*AnimationTiming.walkCellSeconds;
      director.Update(movementSeconds+.01F);
      Check(manager.CueStarts(cue)==count+1,"Final AI activation retains exact weapon or spell cue after round reset");
      director.Update(1);const auto total=manager.EffectStarts();director.Observe(after,after);
      Check(manager.EffectStarts()==total,"Identical post-round snapshot never replays attack or cast");
    }
    auto before=LastActivation(false);before.Find(1)->pos={-4,0};before.At({0,0})->terrain=Terrain::Blocked;
    before.Find(2)->cooldowns[static_cast<std::size_t>(Ability::Precise)]=2;
    before.state.sources.push_back({before.state.nextId++,{1,0},5});before.state.globalRemaining=5;
    for(int i=0;i<159;++i)before.state.log.push_back("Old notification "+std::to_string(i));
    before.state.log.push_back(Name(Ability::Lightning)+": 10 de dano.");
    auto after=before;Check(after.StepAI()&&after.Find(2)->guard,"Isolated AI uses real Guard and finishes army turn and round");
    Check(after.Find(1)->hp<before.Find(1)->hp&&after.state.log.size()<before.state.log.size(),"Environment causes HP loss while bounded log trims an old prefix");
    director.Update(1);const auto blade=manager.CueStarts("sword"),spell=manager.CueStarts("spell_lightning");
    director.Observe(before,after);
    Check(manager.CueStarts("sword")==blade&&manager.CueStarts("spell_lightning")==spell,"Stale attack log plus environmental damage cannot turn Guard into an attack sound");
    director.Update(1);const auto total=manager.EffectStarts();director.Observe(after,after,Ability::Lightning);
    Check(manager.EffectStarts()==total,"Stale explicit hint without a state transition produces no attack audio");
  }
  {
    using namespace Plaguefront;
    for(int cells:{1,2}) {
      director.Update(3.F);auto before=LastActivation(false);before.At({2,0})->terrain=Terrain::Plain;before.Find(2)->pos={cells+1,0};
      Check(before.BeginActivation(2).ok,"La secuencia de audio usa una activación real con movimiento previo");
      const auto path=before.FindPath(2,{1,0});Check(path&&path->cells.size()==static_cast<std::size_t>(cells+1),"La ruta de audio contiene las casillas esperadas");
      auto after=before;Check(after.Move(2,{1,0}).ok&&after.UseAbility(2,Ability::Attack,{0,0}).ok,"La misma instantánea resuelve movimiento y ataque legales");
      const float walk=static_cast<float>(path->cells.size()-1)*AnimationTiming.walkCellSeconds;
      const auto swords=manager.CueStarts("sword"),impacts=manager.CueStarts("impact"),moves=manager.CueStarts("move");
      director.Observe(before,after,Ability::Attack);
      Check(manager.CueStarts("move")==moves+1&&manager.CueStarts("sword")==swords&&manager.CueStarts("impact")==impacts,"Solo la marcha suena mientras comienza su ciclo completo");
      director.Update(walk-.001F);Check(manager.CueStarts("sword")==swords&&manager.CueStarts("impact")==impacts,"El arma espera al último marco de la marcha");
      director.Update(.002F);Check(manager.CueStarts("sword")==swords+1&&manager.CueStarts("impact")==impacts,"El arma comienza en la llegada configurada también por el director visual");
      director.Update(AnimationTiming.weaponContactSeconds-.002F);Check(manager.CueStarts("impact")==impacts,"El impacto de audio no precede al contacto visual compartido");
      director.Update(.002F);Check(manager.CueStarts("sword")==swords+1&&manager.CueStarts("impact")==impacts+1,"El impacto suena una sola vez al cruzar el contacto posterior a la llegada");
      director.Update(3.F);const auto starts=manager.EffectStarts();director.Observe(after,after);director.Update(3.F);
      Check(manager.EffectStarts()==starts,"Volver a observar la instantánea no repite marcha, arma ni impacto");
    }
  }
  {
    using namespace Plaguefront;
    director.Update(3.f);director.SetScene("battle");
    auto before=LastActivation(false);Check(before.BeginActivation(2).ok,"Prepare direct counter audio fixture");auto after=before;
    Check(after.UseAbility(2,Ability::Attack,{0,0}).ok && after.state.combatEvents.size()==2,"Counter audio is driven by two real combat events");
    director.Update(1.f);const auto swords=manager.CueStarts("sword"),impacts=manager.CueStarts("impact");
    director.Observe(before,after);
    Check(manager.CueStarts("sword")==swords+1 && manager.CueStarts("impact")==impacts,"Primary weapon begins before its contact sound");
    director.Update(.19f);
    Check(manager.CueStarts("impact")==impacts+1 && manager.CueStarts("sword")==swords+1,"First impact sounds without premature counter");
    director.Update(.30f);Check(manager.CueStarts("sword")==swords+2,"Single counter weapon cue begins after defender recovery");
    director.Update(.18f);Check(manager.CueStarts("impact")==impacts+2,"Single counter impact follows its weapon cue");
    director.Update(3.f);const auto starts=manager.EffectStarts();director.Observe(after,after);director.Update(3.f);
    Check(manager.EffectStarts()==starts,"Unchanged combat snapshot cannot schedule duplicate counter sounds");
  }
  {
    using namespace Plaguefront;
    director.Update(3.f);auto before=LastActivation(false);before.Find(1)->hp=6;
    before.state.fires.push_back({before.state.nextId++,{},1,1});auto after=before;
    Check(after.StepAI()&&!after.Find(1)->Alive()&&after.state.combatEvents.size()==2,"Real last AI activation includes nonlethal attack counter and later fatal fire");
    Check(!after.state.combatEvents[0].lethal&&!after.state.combatEvents[1].lethal,"Audio can distinguish hit-time survival from later environmental death");
    director.Update(1.f);const auto swords=manager.CueStarts("sword"),impacts=manager.CueStarts("impact"),deaths=manager.CueStarts("death");
    director.Observe(before,after);director.Update(.19f);
    Check(manager.CueStarts("sword")==swords+1&&manager.CueStarts("impact")==impacts+1&&manager.CueStarts("death")==deaths,"First nonlethal impact cannot emit premature death audio");
    director.Update(.30f);director.Update(.18f);
    Check(manager.CueStarts("sword")==swords+2&&manager.CueStarts("impact")==impacts+2&&manager.CueStarts("death")==deaths,"Counter weapon and contact precede separate environmental death");
    director.Update(2.f);
    Check(manager.CueStarts("impact")==impacts+3&&manager.CueStarts("death")==deaths+1,"Residual burn emits exactly one additional impact and one death after combat");
    const auto starts=manager.EffectStarts();director.Observe(after,after);director.Update(3.f);
    Check(manager.EffectStarts()==starts,"Repeated combined snapshot cannot replay residual burn or death sounds");
  }
  manager.StopAll();Check(manager.CurrentMusic().empty()&&manager.CurrentAmbient().empty()&&manager.ActiveVoiceCount()==0,"StopAll releases playing channels");
  std::cout<<"Audio: "<<files.size()<<" decoded original WAV files, "<<checks<<" checks passed. Playback invoked muted; no subjective listening claim.\n";
  return 0;
}catch(const std::exception &e){std::cerr<<"[FAIL] audio: "<<e.what()<<'\n';return 1;}}
