#include "Plaguefront/Rules.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>

using namespace Plaguefront;
using Json=nlohmann::json;
namespace {
int checks=0;
void Check(bool condition,const char *message) {++checks;if(!condition)throw std::runtime_error(message);}
void Ok(const Result &result) {++checks;if(!result)throw std::runtime_error(result.message);}
void Valid(const Match &match) {std::string error;Check(match.Validate(error),error.c_str());}
Match Board() {
  Config config;config.weatherPeriod=1000;Match match(config);match.state.mode=Mode::Local;
  for(int q=-7;q<=7;++q)for(int r=-4;r<=4;++r)match.state.cells.push_back({{q,r}});
  match.At({-7,0})->building=Building::Fortress;match.At({-7,0})->owner=0;
  match.At({7,0})->building=Building::Fortress;match.At({7,0})->owner=1;
  return match;
}
int Add(Match &match,int owner,UnitClass type,Hex pos,int hp=-1) {
  Unit unit;unit.id=match.state.nextId++;unit.owner=owner;unit.type=type;unit.pos=pos;
  unit.hp=hp<0?match.config.classes[static_cast<int>(type)].hp:hp;
  unit.mp=type==UnitClass::Caster?match.config.maxMana:0;
  match.state.units.push_back(unit);return unit.id;
}
void Cycle(Match &match) {Ok(match.EndTurn());Ok(match.EndTurn());}
std::filesystem::path Output(const char *name) {
  const auto folder=std::filesystem::current_path()/"build"/"test-output";
  std::filesystem::create_directories(folder);return folder/name;
}
Match RoundTrip(const Match &match,const char *name) {
  const auto path=Output(name);std::string error;
  Check(match.Save(path.string(),error),error.c_str());
  auto loaded=Match::Load(path.string(),Config{},error);Check(loaded.has_value(),error.c_str());
  std::filesystem::remove(path);Valid(*loaded);return *loaded;
}
void ArmyTurnsAndInterleaving() {
  auto match=Board();const int caster=Add(match,0,UnitClass::Caster,{0,0}),saber=Add(match,0,UnitClass::Saber,{-2,0});
  const int red=Add(match,1,UnitClass::Lancer,{4,0});match.Find(caster)->mp=2;
  Check(match.AvailableCount()==2 && match.Available(*match.Find(caster)),"Availability counts logical current army");
  const int coins=match.state.players[0].coins;
  Ok(match.BeginActivation(caster));Check(match.Find(caster)->mp==3,"Caster recovers on first eligible activation");
  Ok(match.Move(caster,{1,0}));Ok(match.BeginActivation(saber));Ok(match.Move(saber,{-1,0}));
  Ok(match.BeginActivation(caster));Check(match.Find(caster)->mp==3,"Resuming selection never recovers mana twice");
  Check(!match.Move(caster,{1,1}),"Resuming a unit preserves its used movement");
  Ok(match.UseAbility(caster,Ability::Attack,{4,0}));
  Ok(match.EndActivation());
  Check(match.state.currentPlayer==0 && match.Find(caster)->activated && !match.Available(*match.Find(caster)),"Finishing a unit marks it used and keeps army control");
  Check(!match.BeginActivation(caster),"Selecting an acted unit cannot reactivate it");
  Check(match.Available(*match.Find(saber)) && match.AvailableCount()==1,"Interrupted unfinished ally keeps its activation");
  Ok(match.BeginActivation(saber));Check(!match.Move(saber,{0,-1}),"Switching between troops preserves independent movement flags");
  Ok(match.EndActivation());Check(match.AvailableCount()==0 && match.state.currentPlayer==0,"Using entire army still requires explicit end turn");
  Check(match.state.players[0].coins==coins && match.state.round==1,"Unit activations grant no money or rounds");
  const int redCoins=match.state.players[1].coins;Ok(match.EndTurn());
  Check(match.state.currentPlayer==1 && match.state.round==1,"Blue end begins red army in same round");
  Check(match.state.players[1].coins==redCoins+match.Income(1),"Only incoming army receives forecast income");
  Check(match.Available(*match.Find(red)) && !match.Available(*match.Find(saber)),"Eligibility belongs only to current army");
  Ok(match.EndTurn());
  Check(match.state.round==2 && match.state.currentPlayer==0,"Both army ends complete one round");
  Check(match.AvailableCount()==2 && !match.Find(caster)->activationStarted,"New owner turn resets used and started flags");
  Ok(match.BeginActivation(caster));Check(match.Find(caster)->mp==4,"Next owner turn grants exactly one new mana recovery");
  Valid(match);
}
void OwnerIncomeAndRoundClocks() {
  auto match=Board();const int first=Add(match,0,UnitClass::Saber,{}),second=Add(match,0,UnitClass::Saber,{-2,0});
  Add(match,1,UnitClass::Saber,{4,0});match.At({-6,1})->building=Building::House;match.At({-6,1})->owner=0;
  match.state.sources.push_back({match.state.nextId++,{7,4},5});
  match.Find(first)->cooldowns[static_cast<int>(Ability::Precise)]=3;
  Check(match.Income(0)==15 && match.Income(1)==10,"Forecast derives from actual current ownership");
  const int blue=match.state.players[0].coins,red=match.state.players[1].coins;
  for(int id:{first,second}) {Ok(match.BeginActivation(id));Ok(match.EndActivation());}
  Check(match.Find(first)->cooldowns[2]==3 && match.state.sources.front().remaining==5,"Each unit activation leaves all round clocks untouched");
  Ok(match.EndTurn());Check(match.state.players[0].coins==blue && match.state.players[1].coins==red+10,"Red receives income once on entering its turn");
  Check(match.Find(first)->cooldowns[2]==3 && match.state.sources.front().remaining==5,"Half round advances neither recarga nor rot");
  Ok(match.EndTurn());
  Check(match.state.players[0].coins==blue+15 && match.state.players[1].coins==red+10,"Complete round pays only newly entering owner");
  Check(match.Find(first)->cooldowns[2]==2 && match.state.sources.front().remaining==4,"Complete round advances each cooldown and source once");
  auto created=Match::Create(Config{},"Content/Plaguefront/scenarios/frontier.json",Mode::Local,Difficulty::Normal);
  Check(created.state.players[0].coins==created.config.initialCoins+created.Income(0),"First owner starts with exactly its first income");
  Check(created.state.players[1].coins==created.config.initialCoins,"Second owner receives no early income");
  Valid(match);Valid(created);
}
void RecruitmentAndEarlyEnd() {
  auto match=Board();const int first=Add(match,0,UnitClass::Saber,{});Add(match,0,UnitClass::Lancer,{-2,0});
  match.state.players[0].coins=1000;Ok(match.Recruit(0,UnitClass::Caster,{-7,0}));const int recruit=match.state.units.back().id;
  Check(!match.Available(*match.Find(recruit)) && !match.BeginActivation(recruit),"Fresh recruit cannot act in recruitment turn");
  Ok(match.BeginActivation(first));Ok(match.EndTurn());
  Check(match.state.activeUnit==-1 && match.Find(first)->activated,"Early army end safely closes an unfinished activation");
  Ok(match.EndTurn());Check(match.Available(*match.Find(recruit)),"Recruit becomes eligible at next owner turn");
  Valid(match);
}
void CounterattacksAndHealth() {
  auto attack=[](int defenderHp) {
    auto match=Board();match.config.classes[0].attack=12;
    const int attacker=Add(match,0,UnitClass::Saber,{}),defender=Add(match,1,UnitClass::Saber,{1,0},defenderHp);
    const int initial=match.EstimateDamage(attacker,defender);Ok(match.BeginActivation(attacker));
    Ok(match.UseAbility(attacker,Ability::Attack,{1,0}));
    const int remaining=std::max(0,defenderHp-initial);
    Check(match.Find(defender)->hp==remaining,"Initial impact is resolved exactly once");
    Check(match.state.combatEvents.size()==2,"Surviving legal defender produces exactly attack and one counter");
    const int expected=std::max(1,static_cast<int>(std::ceil(match.EstimateDamage(defender,attacker)*remaining/28.0)));
    Check(match.state.combatEvents.back().counter && match.state.combatEvents.back().amount==expected,"Counter uses central normal damage scaled by post-hit HP");
    Check(match.Find(attacker)->hp==28-expected,"Only one scaled response hits original attacker");
    Check(!match.Find(defender)->activated && !match.Find(defender)->acted,"Counter does not consume defender next normal action");
    Check(!match.UseAbility(attacker,Ability::Attack,{1,0}),"Response never grants attacker a second action");Valid(match);
    return expected;
  };
  const int healthy=attack(28),wounded=attack(12);
  Check(wounded<healthy,"Wounded post-impact defender deals less counter damage");
  auto minimum=Board();minimum.config.classes[0].attack=12;
  const int a=Add(minimum,0,UnitClass::Saber,{}),d=Add(minimum,1,UnitClass::Saber,{1,0},8);
  Ok(minimum.BeginActivation(a));Ok(minimum.UseAbility(a,Ability::Attack,{1,0}));
  Check(minimum.Find(d)->hp==1 && minimum.state.combatEvents.back().amount==1,"A legal near-dead counter retains minimum one damage");
  auto lethal=Board();const int killer=Add(lethal,0,UnitClass::Saber,{}),victim=Add(lethal,1,UnitClass::Saber,{1,0},1);
  Ok(lethal.BeginActivation(killer));Ok(lethal.UseAbility(killer,Ability::Attack,{1,0}));
  Check(!lethal.Find(victim)->Alive() && lethal.Find(killer)->hp==28 && lethal.state.combatEvents.size()==1 && lethal.state.combatEvents.front().lethal,"Lethal hit records death at impact and prevents a response or counter chain");
}
void CounterRangeVisionAndModifiers() {
  auto melee=Board();const int saber=Add(melee,0,UnitClass::Saber,{}),archer=Add(melee,1,UnitClass::Archer,{1,0});
  Ok(melee.BeginActivation(saber));Ok(melee.UseAbility(saber,Ability::Attack,{1,0}));
  Check(melee.state.combatEvents.size()==1 && melee.CounterDamage(archer,saber)==0,"Archer minimum range forbids adjacent counter");
  auto ranged=Board();const int a=Add(ranged,0,UnitClass::Archer,{}),d=Add(ranged,1,UnitClass::Archer,{2,0});
  ranged.At({2,0})->height=1;ranged.At({2,0})->terrain=Terrain::Forest;
  const int modified=ranged.EstimateDamage(d,a);Check(modified>0,"Legal ranged counter includes forest and elevation modifiers");
  Ok(ranged.BeginActivation(a));Ok(ranged.UseAbility(a,Ability::Attack,{2,0}));
  Check(ranged.state.combatEvents.size()==2 && ranged.state.combatEvents.back().counter,"In-range ranged survivor responds once");
  const int maximum=ranged.config.classes[static_cast<int>(UnitClass::Archer)].hp;
  const int expected=std::max(1,(modified*ranged.Find(d)->hp+maximum-1)/maximum);
  Check(ranged.state.combatEvents.back().amount==expected,"Ranged counter scales same modified normal damage");
  auto blocked=Board();const int ca=Add(blocked,0,UnitClass::Caster,{}),cd=Add(blocked,1,UnitClass::Caster,{2,0});
  blocked.At({1,0})->terrain=Terrain::Ruins;Check(blocked.CounterDamage(cd,ca)==0,"Blocked LOS forbids ranged response");
  Ok(blocked.BeginActivation(ca));Check(!blocked.UseAbility(ca,Ability::Attack,{2,0}) && blocked.state.combatEvents.empty(),"Blocked initial attack has no impact or invented counter");
  auto caster=Board();const int attacker=Add(caster,0,UnitClass::Archer,{}),mage=Add(caster,1,UnitClass::Caster,{2,0});
  Ok(caster.BeginActivation(attacker));Ok(caster.UseAbility(attacker,Ability::Attack,{2,0}));
  Check(caster.state.combatEvents.size()==2 && caster.state.combatEvents.back().attackerId==mage && caster.state.combatEvents.back().ability==Ability::Attack,"Caster response uses its legal normal magic attack");
  auto prepared=Board();const int pa=Add(prepared,0,UnitClass::Saber,{}),pd=Add(prepared,1,UnitClass::Saber,{1,0});
  Ok(prepared.BeginActivation(pa));prepared.Find(pa)->guard=true;prepared.Find(pd)->precise=true;
  auto prediction=prepared;prediction.Damage(pd,prediction.EstimateDamage(pa,pd));
  const int guarded=prediction.CounterDamage(pd,pa);prediction.Find(pa)->guard=false;
  Check(guarded<prediction.CounterDamage(pd,pa),"Central counter damage respects the incoming target guard and responder precise bonus");
  Ok(prepared.UseAbility(pa,Ability::Attack,{1,0}));
  Check(prepared.state.combatEvents.back().amount==guarded && !prepared.Find(pa)->guard && !prepared.Find(pd)->precise,"Counter consumes only applicable combat preparations once");
  auto fatal=Board();const int doomed=Add(fatal,0,UnitClass::Saber,{},1),available=Add(fatal,0,UnitClass::Lancer,{-2,0});Add(fatal,1,UnitClass::Saber,{1,0});
  Ok(fatal.BeginActivation(doomed));Ok(fatal.UseAbility(doomed,Ability::Attack,{1,0}));
  Check(!fatal.Find(doomed)->Alive() && fatal.state.activeUnit==-1 && fatal.state.currentPlayer==0,"Fatal counter closes dead active unit without stealing army turn");
  Check(fatal.Available(*fatal.Find(available)),"Another own unit remains available after attacker dies from counter");Valid(fatal);
}
void LightningChains() {
  auto match=Board();const int caster=Add(match,0,UnitClass::Caster,{});
  const int first=Add(match,1,UnitClass::Saber,{2,0}),second=Add(match,1,UnitClass::Caster,{3,0}),third=Add(match,1,UnitClass::Rider,{4,0});
  const int far=Add(match,1,UnitClass::Saber,{6,0}),ally=Add(match,0,UnitClass::Saber,{2,1});
  const std::array hp{match.Find(first)->hp,match.Find(second)->hp,match.Find(third)->hp};
  Ok(match.BeginActivation(caster));Ok(match.UseAbility(caster,Ability::Lightning,{2,0}));
  Check(match.state.combatEvents.size()==3,"Lightning limited to initial plus two configured jumps");
  const std::array ids{first,second,third};std::set<int> hit;
  for(int i=0;i<3;++i) {
    const auto &event=match.state.combatEvents[static_cast<std::size_t>(i)];hit.insert(event.targetId);
    Check(event.targetId==ids[static_cast<std::size_t>(i)] && event.chainIndex==i && !event.counter,"Chain chooses nearest next enemy with one event per hit");
    Check(match.Find(ids[static_cast<std::size_t>(i)])->hp<hp[static_cast<std::size_t>(i)],"Every chain event applies actual HP damage");
    const int resistance=match.EffectiveStats(*match.Find(event.targetId)).resistance;
    Check(event.amount==match.config.classes[4].magic+match.config.lightningPower-resistance,"Every chain target uses its own magical resistance");
    if(i)Check(Distance(event.from,event.to)<=match.config.lightningRadius,"Each visual link obeys real configured hex radius");
  }
  Check(hit.size()==3 && match.Find(far)->hp==28 && match.Find(ally)->hp==28,"No repeated targets, remote jumps or allied hits");
  Check(match.Find(caster)->hp==20 && match.CanTarget(second,caster),"Legal secondary Caster cannot create an extra recursive counter");
  Check(match.Find(caster)->mp==5,"Entire chain charges mana once");Valid(match);
  auto blocked=Board();blocked.config.lightningRadius=2;
  const int mage=Add(blocked,0,UnitClass::Caster,{}),start=Add(blocked,1,UnitClass::Saber,{2,0}),other=Add(blocked,1,UnitClass::Saber,{4,0});
  blocked.At({3,0})->terrain=Terrain::Ruins;Ok(blocked.BeginActivation(mage));Ok(blocked.UseAbility(mage,Ability::Lightning,{2,0}));
  Check(blocked.Find(start)->hp<28 && blocked.Find(other)->hp==28,"Chain jumps respect their own blocked line of sight");
  auto limited=Board();limited.config.lightningMaxTargets=1;const int lm=Add(limited,0,UnitClass::Caster,{});Add(limited,1,UnitClass::Saber,{2,0});const int untouched=Add(limited,1,UnitClass::Saber,{3,0});
  Ok(limited.BeginActivation(lm));Ok(limited.UseAbility(lm,Ability::Lightning,{2,0}));
  Check(limited.state.combatEvents.size()==1 && limited.Find(untouched)->hp==28,"Configured total target maximum includes initial enemy");
}
void PersistentFire() {
  auto match=Board();const int mage=Add(match,0,UnitClass::Caster,{}),target=Add(match,1,UnitClass::Saber,{2,0});
  const int adjacent=Add(match,1,UnitClass::Saber,{3,-1});
  match.At({1,0})->terrain=Terrain::River;match.At({1,1})->terrain=Terrain::Swamp;match.At({2,-1})->terrain=Terrain::DeepWater;
  match.At({2,1})->terrain=Terrain::Bridge;
  Ok(match.BeginActivation(mage));Ok(match.UseAbility(mage,Ability::Fire,{2,0}));
  Check(match.Find(target)->hp<28 && match.Find(adjacent)->hp==28,"Fire initial impact and persistent area are separate phases");
  Check(match.FireDamageAt({2,0})==2 && match.FireDamageAt({2,1})==2,"Fire covers center and dry bridge in configured radius");
  Check(match.FireDamageAt({1,0})==0 && match.FireDamageAt({1,1})==0 && match.FireDamageAt({2,-1})==0,"River swamp and deep water never retain fire zones");
  Check(match.FireDamageAt({4,0})==0,"Fire does not extend outside configured hex radius");
  for(const auto &zone:match.state.fires)Check(zone.remaining==2 && zone.owner==0,"Every fire cell has persisted real duration and responsible owner");
  const int hp=match.Find(adjacent)->hp;Ok(match.EndActivation());Ok(match.EndTurn());
  Check(match.Find(adjacent)->hp==hp,"End of first army turn does not tick fire");Ok(match.EndTurn());
  Check(match.Find(adjacent)->hp==hp-2 && !match.state.fires.empty(),"First complete round applies exactly one burn tick");
  match=RoundTrip(match,"phase4-fire.json");Cycle(match);
  Check(match.Find(adjacent)->hp==hp-4 && match.state.fires.empty(),"Second complete round applies final saved burn then expires");
  const int finalHp=match.Find(adjacent)->hp;Cycle(match);Check(match.Find(adjacent)->hp==finalHp,"Expired fire applies no extra damage");
  auto overlap=Board();const int unit=Add(overlap,0,UnitClass::Saber,{},10);
  overlap.state.fires.push_back({overlap.state.nextId++,{},2,1});overlap.state.fires.push_back({overlap.state.nextId++,{},2,1});
  overlap.state.sources.push_back({overlap.state.nextId++,{},5});overlap.state.weather=Weather::Rain;
  Check(overlap.FireDamageAt({})==1,"Rain weakens persistent fire with a floor of one");
  overlap.EndRound();Check(overlap.Find(unit)->hp==7,"Overlapping fire does not stack; fire and rot retain distinct environmental damage");Valid(overlap);
}
void BurnDeathsAndRotChronology() {
  auto match=Board();const int dead=Add(match,1,UnitClass::Archer,{},1);
  match.state.fires.push_back({match.state.nextId++,{},1,0});Cycle(match);
  Check(!match.Find(dead)->Alive() && match.state.fires.empty() && match.state.corpses.size()==1,"Final fire tick creates ordinary persistent corpse before expiration");
  const auto corpse=match.state.corpses.front();Check(corpse.owner==1 && corpse.type==UnitClass::Archer && corpse.unitId==dead,"Corpse retains original unit faction and class for recognizable fallen sprite");
  Check(match.Occupant({})==nullptr && match.state.sources.empty(),"Dead body is nonblocking and initially inert");
  for(int wait=0;wait<match.config.rotWait-1;++wait) {Cycle(match);Check(match.state.sources.empty(),"Burn corpse keeps original full-round waiting chronology");}
  Cycle(match);Check(match.state.sources.size()==1 && match.state.corpses.empty(),"Burn corpse converts only after configured complete waits");
  Check(match.state.sources.front().remaining==match.config.rotDuration,"New source begins with full damage duration");Valid(match);
}
void CaptureAcrossTurnsAndSave() {
  auto match=Board();const int captor=Add(match,0,UnitClass::Saber,{7,0},20);Add(match,1,UnitClass::Saber,{});
  Ok(match.BeginActivation(captor));Ok(match.Capture(captor));
  Check(match.At({7,0})->captureProgress==20 && match.At({7,0})->capturingPlayer==0 && match.At({7,0})->owner==1 && match.state.winner==-1,"First capture adds current HP while fortress remains defended");
  Ok(match.EndActivation());Ok(match.EndTurn());
  Check(match.At({7,0})->captureProgress==20,"Partial capture survives end of captor army turn");Ok(match.EndTurn());
  match=RoundTrip(match,"phase4-partial-fortress.json");
  Check(match.At({7,0})->captureProgress==20 && match.At({7,0})->capturingPlayer==0 && match.Available(*match.Find(captor)),"Save restores partial owner and next-turn availability");
  Ok(match.BeginActivation(captor));Check(match.state.winner==-1,"Selecting former captor never triggers premature victory");
  Ok(match.Capture(captor));Check(match.At({7,0})->captureProgress==40 && match.state.winner==-1,"Continued capture is still legal and increases actual saved progress");
  Ok(match.EndActivation());Cycle(match);Ok(match.BeginActivation(captor));Ok(match.Capture(captor));
  Check(match.At({7,0})->owner==0 && match.state.winner==0,"Final exact point completes ownership and winner atomically");
  Check(!match.UseAbility(captor,Ability::Attack,{}) && !match.EndTurn() && !match.Recruit(0,UnitClass::Saber,{-7,0}),"Victory prevents new combat turn and economic actions");
  const int hp=match.Find(captor)->hp;match.Damage(captor,1000);Check(match.Find(captor)->hp==hp,"Terminal victory also refuses direct residual damage");
  match=RoundTrip(match,"phase4-victory.json");Check(match.state.winner==0,"Victory remains terminal after loading");
}
void PausedAlliedAndContestedCapture() {
  auto match=Board();match.At({0,0})->building=Building::House;
  const int first=Add(match,0,UnitClass::Saber,{},10),ally=Add(match,0,UnitClass::Lancer,{-1,0},15),red=Add(match,1,UnitClass::Saber,{1,0},12);
  Ok(match.BeginActivation(first));Ok(match.Capture(first));Ok(match.Move(first,{0,1}));Ok(match.EndActivation());
  Check(match.At({0,0})->captureProgress==10 && match.At({0,0})->captureUnit==-1,"Leaving pauses but retains captured points");
  Ok(match.BeginActivation(ally));Ok(match.Move(ally,{}));Ok(match.Capture(ally));Ok(match.EndActivation());
  Check(match.At({0,0})->captureProgress==25,"Another eligible ally continues same faction progress using its current HP");
  match.Damage(ally,100,1);Check(match.At({0,0})->captureProgress==25 && match.At({0,0})->captureUnit==-1,"Captor death retains progress for later allies");
  Ok(match.EndTurn());Ok(match.BeginActivation(red));Ok(match.Move(red,{}));Ok(match.Capture(red));
  Check(match.At({0,0})->captureProgress==13 && match.At({0,0})->capturingPlayer==0,"Opposing capture first reduces existing faction progress");
  Ok(match.EndActivation());Cycle(match);Ok(match.BeginActivation(red));Ok(match.Capture(red));
  Check(match.At({0,0})->captureProgress==1 && match.At({0,0})->capturingPlayer==0,"Contest consistently removes remaining enemy progress");
  Ok(match.EndActivation());Cycle(match);Ok(match.BeginActivation(red));Ok(match.Capture(red));
  Check(match.At({0,0})->captureProgress==11 && match.At({0,0})->capturingPlayer==1 && match.At({0,0})->owner==-1,"Overflow starts new faction progress without stale colors or instant property transfer");
  Valid(match);
}
void NeutralEconomyAndRestoration() {
  auto match=Board();match.config.houseCapture=20;match.At({0,0})->building=Building::House;
  const int captor=Add(match,0,UnitClass::Saber,{},20);const int money=match.state.players[0].coins;
  Ok(match.BeginActivation(captor));Ok(match.Capture(captor));
  Check(match.At({0,0})->owner==0 && match.At({0,0})->neutralRewardClaimed && match.state.players[0].coins==money+20,"Neutral reward occurs only at completed first ownership transfer");
  Check(match.Income(0)==15,"Completed house immediately changes predicted income");
  match.At({0,0})->capturingPlayer=1;match.At({0,0})->captureProgress=10;Ok(match.EndActivation());Cycle(match);
  const int coins=match.state.players[0].coins;Ok(match.BeginActivation(captor));Ok(match.Capture(captor));
  Check(match.At({0,0})->owner==0 && match.At({0,0})->captureProgress==0 && match.At({0,0})->capturingPlayer==-1,"Owner may clear disputed property without accumulating capture of its own building");
  Check(match.state.players[0].coins==coins,"Restoration never repeats the neutral bounty");Valid(match);
}
void ConnectedSourcesAndRoundRestore() {
  auto match=Board();const int nearby=Add(match,0,UnitClass::Saber,{}),far=Add(match,1,UnitClass::Saber,{-7,-4});
  for(Hex pos:{Hex{0,0},Hex{3,0},Hex{6,0}})match.state.sources.push_back({match.state.nextId++,pos,5});
  Check(match.LargestSourceGroup()==3,"Sources form transitive real connected component");
  for(const auto &cell:match.state.cells) {
    bool local=false;for(const auto &source:match.state.sources)local|=Distance(cell.pos,source.pos)<=match.config.rotRadius;
    Check(match.RotDamageAt(cell.pos)==(local?2:0),"Logical local highlight mask exactly equals configured hex radius");
  }
  Cycle(match);Check(match.Find(nearby)->hp==26 && match.Find(far)->hp==27 && match.state.globalRemaining==5,"Global round damage does not add to local damage");
  match=RoundTrip(match,"phase4-plague.json");match.state.sources.clear();
  for(int remaining=4;remaining>=0;--remaining) {Cycle(match);Check(match.state.globalRemaining==remaining,"Saved residual global duration ticks once per complete round");}
  auto disconnected=Board();for(Hex pos:{Hex{-7,-4},Hex{0,0},Hex{7,4}})disconnected.state.sources.push_back({disconnected.state.nextId++,pos,5});
  disconnected.EndRound();Check(disconnected.LargestSourceGroup()==1 && disconnected.state.globalRemaining==0,"Three distant sources do not form fictitious global group");
  auto inert=Board();for(Hex pos:{Hex{0,0},Hex{1,0},Hex{2,0}})inert.state.corpses.push_back({inert.state.nextId++,pos,1});
  Cycle(inert);Check(inert.state.globalRemaining==0 && inert.state.sources.empty(),"Inert corpses cannot activate global contamination");Valid(match);
}
void FullAITurn() {
  auto match=Board();match.state.mode=Mode::VersusAI;match.state.currentPlayer=1;match.state.players[1].coins=0;
  const int first=Add(match,1,UnitClass::Saber,{-2,0}),second=Add(match,1,UnitClass::Lancer,{-3,1});Add(match,0,UnitClass::Saber,{4,0});
  Check(match.StepAI(),"First AI unit accepts public command turn");
  Check(match.state.currentPlayer==1 && match.state.round==1 && match.Find(first)->activated!=match.Find(second)->activated,"AI retains complete army turn after one of two units");
  Check(match.StepAI(),"Second AI unit accepts same army turn");
  Check(match.state.currentPlayer==0 && match.state.round==2 && match.Find(first)->activated && match.Find(second)->activated,"AI explicitly ends only after completing its chosen army");
  Check(!match.StepAI(),"AI cannot act during human army turn");Valid(match);
}
void UsedUnitSaveAndLegacyMigration() {
  auto match=Board();const int first=Add(match,0,UnitClass::Caster,{}),second=Add(match,0,UnitClass::Saber,{-2,0});Add(match,1,UnitClass::Saber,{4,0});
  match.Find(first)->mp=2;Ok(match.BeginActivation(first));Ok(match.Move(first,{1,0}));Ok(match.BeginActivation(second));Ok(match.EndActivation());
  match=RoundTrip(match,"phase4-availability.json");
  Check(match.Find(second)->activated && match.Find(first)->activationStarted && !match.Find(first)->activated,"Save distinguishes finished unit from interrupted unfinished activation");
  Ok(match.BeginActivation(first));Check(match.Find(first)->mp==3 && !match.Move(first,{2,0}),"Loaded interrupted unit resumes without extra mana or movement");
  const auto path=Output("phase4-legacy-v1.json");std::string error;Check(match.Save(path.string(),error),error.c_str());
  Json legacy;{std::ifstream in(path);in>>legacy;}legacy["version"]=1;
  legacy["state"].erase("fires");legacy["state"].erase("nextEventId");legacy["state"].erase("combatEvents");
  for(auto &unit:legacy["state"]["units"]) {unit.erase("activationStarted");unit["flags"][0]=unit["id"]==first || unit["id"]==second;}
  for(auto &cell:legacy["state"]["cells"]) {cell.erase("captureProgress");cell.erase("capturingPlayer");}
  for(const auto *field:{"houseCapture","outpostCapture","fortressCapture","lightningMaxTargets","lightningRadius","fireRadius","fireDuration","fireDamage"})legacy["config"].erase(field);
  {std::ofstream out(path);out<<legacy.dump(2);}const auto bytes=std::filesystem::file_size(path);
  auto loaded=Match::Load(path.string(),Config{},error);Check(loaded.has_value(),error.c_str());
  Check(std::filesystem::file_size(path)==bytes,"Legacy migration reads without overwriting original save");
  Check(loaded->state.activeUnit==first && loaded->Find(first)->activationStarted && !loaded->Find(first)->activated && loaded->Find(second)->activated,"Version one active unit migrates to resumable while previous used remains acted");
  Check(loaded->config.houseCapture==40 && loaded->state.fires.empty(),"Legacy missing systems receive explicit safe defaults");
  Valid(*loaded);std::filesystem::remove(path);
}
void PendingLegacyCaptureAndCorruption() {
  auto match=Board();const int captor=Add(match,0,UnitClass::Saber,{7,0},20),dead=Add(match,1,UnitClass::Archer,{});
  match.Damage(dead,100);Ok(match.BeginActivation(captor));Ok(match.Capture(captor));Ok(match.EndActivation());
  match.state.fires.push_back({match.state.nextId++,{1,0},2,0});
  const auto path=Output("phase4-pending-migration.json");std::string error;Check(match.Save(path.string(),error),error.c_str());
  Json original;{std::ifstream in(path);in>>original;}
  auto reject=[&](Json broken) {std::ofstream out(path);out<<broken.dump();out.close();Check(!Match::Load(path.string(),Config{},error),"Inconsistent phase 4 state must reject instead of silently repairing");};
  std::size_t fortress=0;for(std::size_t i=0;i<original["state"]["cells"].size();++i)if(original["state"]["cells"][i]["pos"]==Json::array({7,0}))fortress=i;
  auto broken=original;broken["state"]["cells"][fortress]["captureProgress"]=60;reject(broken);
  broken=original;broken["state"]["cells"][fortress]["capturingPlayer"]=2;reject(broken);
  broken=original;broken["state"]["fires"][0]["remaining"]=0;reject(broken);
  broken=original;broken["state"]["fires"][0]["owner"]=2;reject(broken);
  broken=original;broken["state"]["activeUnit"]=captor;reject(broken);
  broken=original;broken["config"].erase("fireDuration");reject(broken);
  Json legacy=original;legacy["version"]=1;legacy["state"].erase("fires");legacy["state"].erase("nextEventId");legacy["state"].erase("combatEvents");
  for(auto &unit:legacy["state"]["units"])unit.erase("activationStarted");
  for(auto &cell:legacy["state"]["cells"]) {cell.erase("captureProgress");cell.erase("capturingPlayer");}
  for(auto &corpse:legacy["state"]["corpses"]) {corpse.erase("owner");corpse.erase("class");corpse.erase("unitId");}
  for(const auto *field:{"houseCapture","outpostCapture","fortressCapture","lightningMaxTargets","lightningRadius","fireRadius","fireDuration","fireDamage"})legacy["config"].erase(field);
  {std::ofstream out(path);out<<legacy.dump(2);}auto loaded=Match::Load(path.string(),Config{},error);Check(loaded.has_value(),error.c_str());
  Check(loaded->At({7,0})->captureProgress==20 && loaded->At({7,0})->capturingPlayer==0 && loaded->state.winner==-1,"Pending v1 fortress migrates to one partial HP action without automatic victory");
  Check(loaded->state.players[0].coins==match.state.players[0].coins && loaded->state.players[1].coins==match.state.players[1].coins,"Migration never grants either army another income");
  Check(loaded->state.corpses.front().owner==1 && loaded->state.corpses.front().type==UnitClass::Archer && loaded->state.corpses.front().unitId==dead,"Legacy corpse reconstructs material identity from original dead unit");
  Valid(*loaded);broken=legacy;broken["state"]["cells"][fortress]["captureRound"]=999;reject(broken);std::filesystem::remove(path);
}
void EventBoundaryIsTransactional() {
  auto match=Board();const int attacker=Add(match,0,UnitClass::Saber,{}),defender=Add(match,1,UnitClass::Saber,{1,0});
  Ok(match.BeginActivation(attacker));match.state.nextEventId=99999999;
  const auto hp=match.Find(defender)->hp,coins=match.state.players[0].coins;
  Check(!match.UseAbility(attacker,Ability::Attack,{1,0}),"Near event-ID limit rejects entire damaging sequence before overflow");
  Check(match.Find(defender)->hp==hp && match.state.players[0].coins==coins && !match.Find(attacker)->acted && match.state.combatEvents.empty(),"Rejected event reservation mutates neither HP money action nor events");
  Ok(match.UseAbility(attacker,Ability::Guard,{}));Check(match.Find(attacker)->guard,"Non-damaging legal actions remain available at event-ID boundary");
}
void CompleteLongChainHistory() {
  auto match=Board();match.config.lightningMaxTargets=140;
  for(int q=8;q<=141;++q)match.state.cells.push_back({{q,0}});
  const int caster=Add(match,0,UnitClass::Caster,{});
  for(int q=2;q<=141;++q)Add(match,1,UnitClass::Saber,{q,0});
  Ok(match.BeginActivation(caster));Ok(match.UseAbility(caster,Ability::Lightning,{2,0}));
  Check(match.state.combatEvents.size()==140 && match.state.combatEvents.front().id==1,"Long configurable chain retains complete initial and secondary presentation events");
  std::set<int> targets;
  for(std::size_t i=0;i<match.state.combatEvents.size();++i) {
    const auto &event=match.state.combatEvents[i];targets.insert(event.targetId);
    Check(event.chainIndex==static_cast<int>(i) && event.id==static_cast<int>(i)+1,"Long chain retains every ordered unique impact identifier");
    Check(match.Find(event.targetId)->hp==18,"Each retained long-chain hit corresponds to real magical damage");
  }
  Check(targets.size()==140,"Long chain never repeats an enemy");
  const int next=match.state.nextEventId;
  Check(!match.UseAbility(caster,Ability::Attack,{7,-4}) && match.state.combatEvents.size()==140 && match.state.nextEventId==next,"Rejected command preserves preceding complete combat evidence");
  Ok(match.EndActivation());Cycle(match);Ok(match.BeginActivation(caster));Ok(match.UseAbility(caster,Ability::Attack,{2,0}));
  Check(match.state.combatEvents.size()==1 && match.state.combatEvents.front().id==next,"Next valid damaging command replaces history while IDs remain monotonic");Valid(match);
}
}
int main() {
  const std::vector<std::pair<const char *,std::function<void()>>> tests{
    {"army turn interleaving and once-only mana",ArmyTurnsAndInterleaving},
    {"owner income and complete-round clocks",OwnerIncomeAndRoundClocks},
    {"recruit eligibility and early army end",RecruitmentAndEarlyEnd},
    {"single health-scaled automatic counter",CounterattacksAndHealth},
    {"counter range LOS modifiers and caster",CounterRangeVisionAndModifiers},
    {"bounded legal lightning propagation",LightningChains},
    {"persistent fire water climate and save",PersistentFire},
    {"burn corpse identity and rot chronology",BurnDeathsAndRotChronology},
    {"fortress continuation save and terminal victory",CaptureAcrossTurnsAndSave},
    {"paused allied and contested capture",PausedAlliedAndContestedCapture},
    {"neutral bounty income and restoration",NeutralEconomyAndRestoration},
    {"connected sources local mask and global save",ConnectedSourcesAndRoundRestore},
    {"AI completes its army before yielding",FullAITurn},
    {"used/interrupted saves and explicit v1 migration",UsedUnitSaveAndLegacyMigration},
    {"legacy pending capture and phase 4 corruption rejection",PendingLegacyCaptureAndCorruption},
    {"transactional combat event ID boundary",EventBoundaryIsTransactional},
    {"complete long chain combat evidence",CompleteLongChainHistory}
  };
  int groups=0;
  for(const auto &[name,test]:tests)try{test();++groups;std::cout<<"[PASS] "<<name<<'\n';}
    catch(const std::exception &error){std::cerr<<"[FAIL] "<<name<<": "<<error.what()<<'\n';return 1;}
  std::cout<<groups<<" phase 4 groups, "<<checks<<" checks passed.\n";
}
