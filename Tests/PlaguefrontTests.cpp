#include "Plaguefront/Rules.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <stdexcept>

using namespace Plaguefront;
using Json=nlohmann::json;
namespace {
int checks=0;
void Check(bool condition,const char *message) {++checks;if(!condition)throw std::runtime_error(message);}
void Ok(const Result &result) {++checks;if(!result)throw std::runtime_error(result.message);}
void Valid(const Match &m) {std::string error;const bool valid=m.Validate(error);Check(valid,error.c_str());}
Match Sandbox() {
  Config config;config.weatherPeriod=1000;
  Match m(config);m.state.mode=Mode::Local;
  for(int q=-6;q<=6;++q)for(int r=-5;r<=5;++r)m.state.cells.push_back({{q,r}});
  m.At({-5,0})->building=Building::Fortress;m.At({-5,0})->owner=0;
  m.At({5,0})->building=Building::Fortress;m.At({5,0})->owner=1;
  return m;
}
int Add(Match &m,int owner,UnitClass type,Hex pos) {
  Unit u;u.id=m.state.nextId++;u.owner=owner;u.type=type;u.pos=pos;
  u.hp=m.config.classes[static_cast<int>(type)].hp;u.mp=type==UnitClass::Caster?m.config.maxMana:0;
  m.state.units.push_back(u);return u.id;
}
void Geometry() {
  Check(Distance({0,0},{3,-2})==3,"Axial distance");Check(Distance({-2,1},{4,-3})==6,"Negative distance");
  std::set<Hex> distinct;for(auto h:Neighbors({-2,4})){distinct.insert(h);Check(Distance(h,{-2,4})==1,"Neighbor distance");}
  Check(distinct.size()==6,"Six distinct neighbors");
  for(int q=-8;q<=8;++q)for(int r=-8;r<=8;++r){const auto p=HexToPixel({q,r},24);Check(PixelToHex(p[0],p[1],24)==Hex{q,r},"Pixel round trip");}
  const auto line=HexLine({0,0},{4,-2});Check(line.size()==5 && line.front()==Hex{0,0} && line.back()==Hex{4,-2},"Hex line endpoints");
  for(std::size_t i=1;i<line.size();++i)Check(Distance(line[i-1],line[i])==1,"Continuous hex line");
}
void Movement() {
  auto m=Sandbox();const int rider=Add(m,0,UnitClass::Rider,{0,0});
  m.At({1,0})->terrain=Terrain::Forest;m.At({1,0})->height=1;
  Check(m.MoveCost(*m.Find(rider),{0,0},{1,0})==3,"Rider forest and uphill cost");
  m.At({1,0})->terrain=Terrain::Swamp;m.At({1,0})->height=0;
  Check(m.MoveCost(*m.Find(rider),{0,0},{1,0})==2,"Swamp cost");m.state.weather=Weather::Rain;
  Check(m.MoveCost(*m.Find(rider),{0,0},{1,0})==3,"Rain swamp cost");
  m.At({1,0})->terrain=Terrain::Plain;m.At({1,0})->height=-1;
  Check(m.MoveCost(*m.Find(rider),{0,0},{1,0})==2,"Rain lowland cost");
  m.At({1,0})->terrain=Terrain::DeepWater;Check(!m.FindPath(rider,{1,0}),"Deep water blocks path");
  Add(m,1,UnitClass::Saber,{0,1});Check(!m.FindPath(rider,{0,1}),"Occupied destination blocks path");
  const auto path=m.FindPath(rider,{2,0});Check(path && path->cells.front()==Hex{0,0} && path->cells.back()==Hex{2,0},"Path around obstacle");
  Ok(m.BeginActivation(rider));Ok(m.Move(rider,{1,-1}));Check(!m.Move(rider,{2,-1}),"Only one ordinary movement");
}
void Combat() {
  auto m=Sandbox();const int saber=Add(m,0,UnitClass::Saber,{0,0});const int assassin=Add(m,1,UnitClass::Assassin,{1,0});
  Check(m.EstimateDamage(saber,assassin)==9,"Advantage applied exactly once");
  m.Find(assassin)->guard=true;Check(m.EstimateDamage(saber,assassin)==5,"Guard halves rounded up");m.Find(assassin)->guard=false;
  m.Find(saber)->precise=true;Check(m.EstimateDamage(saber,assassin)==12,"Precise bonus");
  m.Find(assassin)->marked=true;Check(m.EstimateDamage(saber,assassin)==14,"Mark bonus");
  Ok(m.BeginActivation(saber));Ok(m.UseAbility(saber,Ability::Attack,{1,0}));
  Check(!m.Find(saber)->precise && !m.Find(assassin)->marked,"Attack consumes preparation and mark");
  Check(!m.UseAbility(saber,Ability::Attack,{1,0}),"No second action");
}
void RangedAndLOS() {
  auto m=Sandbox();const int archer=Add(m,0,UnitClass::Archer,{0,0});const int enemy=Add(m,1,UnitClass::Lancer,{2,0});
  Check(m.CanTarget(archer,enemy),"Archer range2");m.At({1,0})->terrain=Terrain::Ruins;
  Check(!m.CanTarget(archer,enemy),"Ruins block LOS");m.At({1,0})->terrain=Terrain::Plain;
  m.Find(enemy)->pos={1,0};Check(!m.CanTarget(archer,enemy),"Archer minimum range");m.Find(enemy)->pos={4,0};
  Check(m.CanTarget(archer,enemy),"Archer maximum range");m.state.weather=Weather::Fog;Check(!m.CanTarget(archer,enemy),"Fog reduces maximum range");
  m.state.weather=Weather::Clear;m.At({0,0})->terrain=Terrain::Forest;Check(m.EffectiveStats(*m.Find(archer)).attack==9,"Archer forest attack");
}
void Turns() {
  auto m=Sandbox();const int a=Add(m,0,UnitClass::Saber,{0,0}),b=Add(m,1,UnitClass::Saber,{2,0});
  const int c=Add(m,0,UnitClass::Lancer,{-2,0});
  Ok(m.BeginActivation(a));Check(!m.BeginActivation(b),"Only one activation");Ok(m.EndActivation());Check(m.state.currentPlayer==0,"Ending a unit keeps its army turn");
  Check(!m.BeginActivation(a) && !m.Find(c)->activated,"Used unit cannot reactivate and unused ally remains available");
  Ok(m.BeginActivation(c));Ok(m.EndActivation());Check(m.state.round==1 && m.state.currentPlayer==0,"Army can use multiple units before ending turn");
  Ok(m.EndTurn());Check(m.state.currentPlayer==1 && m.state.round==1,"Explicit end changes army without completing round");
  Ok(m.BeginActivation(b));Ok(m.EndActivation());Ok(m.EndTurn());Check(m.state.round==2,"Round after both army turns");
  Check(!m.Find(a)->activated && !m.Find(c)->activated,"Owner turn resets its activations");
  auto n=Sandbox();const int caster=Add(n,0,UnitClass::Caster,{0,0});n.Find(caster)->mp=3;
  Ok(n.BeginActivation(caster));Check(n.Find(caster)->mp==4,"Mana recovers on activation");
}
void Abilities() {
  auto m=Sandbox();const int caster=Add(m,0,UnitClass::Caster,{0,0}),enemy=Add(m,1,UnitClass::Rider,{2,0});
  const int hp=m.Find(enemy)->hp;Ok(m.BeginActivation(caster));Ok(m.UseAbility(caster,Ability::Ice,{2,0}));
  Check(m.Find(enemy)->hp<hp && m.Find(enemy)->slowThroughRound==m.state.round+1,"Ice damages and slows next round");
  Check(m.Find(caster)->mp==6,"Ice mana cost");
  auto n=Sandbox();const int saber=Add(n,0,UnitClass::Saber,{0,0});Ok(n.BeginActivation(saber));Ok(n.UseAbility(saber,Ability::Precise,{0,0}));
  Check(n.Find(saber)->precise && n.Find(saber)->cooldowns[static_cast<int>(Ability::Precise)]==2,"Precise buff and cooldown");
  n.EndRound();Check(n.Find(saber)->cooldowns[static_cast<int>(Ability::Precise)]==1,"Cooldown decreases each round");
  auto h=Sandbox();const int healer=Add(h,0,UnitClass::Caster,{0,0});h.Find(healer)->hp=8;Ok(h.BeginActivation(healer));Ok(h.UseAbility(healer,Ability::Heal,{0,0}));Check(h.Find(healer)->hp==14,"Self heal");
  auto r=Sandbox();const int rider=Add(r,0,UnitClass::Rider,{0,0});Ok(r.BeginActivation(rider));Check(!r.UseAbility(rider,Ability::Retreat,{0,0}),"Retreat requires damaging attack");
}
void ClassTacticsAndActionOrder() {
  auto m=Sandbox();const int saber=Add(m,0,UnitClass::Saber,{0,0});Add(m,1,UnitClass::Saber,{1,0});
  Ok(m.BeginActivation(saber));Ok(m.UseAbility(saber,Ability::Attack,{1,0}));Ok(m.Move(saber,{0,1}));
  Check(!m.Move(saber,{0,2}),"Action then move still allows only one ordinary move");
  auto f=Sandbox();const int archer=Add(f,0,UnitClass::Archer,{0,0});Add(f,1,UnitClass::Saber,{2,0});
  Ok(f.BeginActivation(archer));Ok(f.UseAbility(archer,Ability::Focus,{2,0}));Check(!f.Move(archer,{0,1}),"Focus explicitly locks later movement");
  auto r=Sandbox();const int rider=Add(r,0,UnitClass::Rider,{0,0}),target=Add(r,1,UnitClass::Saber,{4,0});
  Ok(r.BeginActivation(rider));Ok(r.Move(rider,{3,0}));Check(r.EstimateDamage(rider,target)==7,"Three-cell rider charge bonus");
  Ok(r.UseAbility(rider,Ability::Attack,{4,0}));Ok(r.UseAbility(rider,Ability::Retreat,{3,0}));Ok(r.Move(rider,{2,0}));Check(!r.Move(rider,{1,0}),"Retreat grants only one additional move");
  auto a=Sandbox();const int assassin=Add(a,0,UnitClass::Assassin,{-1,0}),defender=Add(a,1,UnitClass::Saber,{0,0});
  Check(a.EstimateDamage(assassin,defender)==5,"No flank bonus without opposite ally");Add(a,0,UnitClass::Saber,{1,0});Check(a.EstimateDamage(assassin,defender)==8,"Opposite ally grants geometric flank");
  auto l=Sandbox();const int lancer=Add(l,0,UnitClass::Lancer,{0,0}),enemy=Add(l,1,UnitClass::Rider,{2,0});
  Check(l.CanTarget(lancer,enemy,Ability::Thrust),"Thrust reaches two cells on axial line");l.Find(enemy)->pos={1,1};Check(!l.CanTarget(lancer,enemy,Ability::Thrust),"Thrust rejects bent direction");
  const int berserker=Add(l,0,UnitClass::Berserker,{-2,0});l.Find(berserker)->hp=17;
  Check(l.EffectiveStats(*l.Find(berserker)).attack==13 && l.EffectiveStats(*l.Find(berserker)).defense==3,"Berserker frenzy at fifty percent");
}
void DeathAndRewards() {
  auto m=Sandbox();const int unit=Add(m,1,UnitClass::Saber,{0,0});const int money=m.state.players[0].coins;
  m.Damage(unit,100,0);Check(!m.Find(unit)->Alive() && m.state.corpses.size()==1,"Death creates corpse");
  Check(m.Occupant({0,0})==nullptr,"Corpse does not block occupancy");Check(m.state.players[0].coins==money+10,"Kill reward");
  m.Damage(unit,100,0);Check(m.state.corpses.size()==1 && m.state.players[0].coins==money+10,"Kill reward only once");
}
void RotTimers() {
  auto m=Sandbox();const int dead=Add(m,1,UnitClass::Saber,{0,0}),living=Add(m,0,UnitClass::Saber,{1,0});
  m.Damage(dead,100);const int hp=m.Find(living)->hp;
  m.EndRound();Check(m.state.round==2 && m.state.sources.empty(),"Death round excluded");
  m.EndRound();m.EndRound();Check(m.state.sources.empty(),"Two complete waiting rounds");
  m.EndRound();Check(m.state.sources.size()==1 && m.state.corpses.empty(),"Source after third full wait");
  Check(m.Find(living)->hp==hp,"No damage during waiting phase");
  for(int tick=1;tick<=5;++tick){m.EndRound();Check(m.Find(living)->hp==hp-tick*2,"Exactly one local damage per tick");}
  Check(m.state.sources.empty(),"Source expires after five damage ticks");m.EndRound();Check(m.Find(living)->hp==hp-10,"No damage after expiry");
}
void RotConnectionsAndSimultaneous() {
  auto m=Sandbox();const int local=Add(m,0,UnitClass::Saber,{0,0}),outside=Add(m,1,UnitClass::Saber,{-5,-5});
  for(Hex p:std::vector<Hex>{{0,0},{3,0},{6,0}})m.state.sources.push_back({m.state.nextId++,p,5});
  Check(m.LargestSourceGroup()==3,"Transitive component connects distant endpoints");m.EndRound();
  Check(m.state.globalRemaining==5,"Global contamination starts");Check(m.Find(local)->hp==26,"Local and global do not stack");Check(m.Find(outside)->hp==27,"Global damage outside local radius");
  m.state.sources.clear();for(int i=0;i<5;++i)m.EndRound();Check(m.state.globalRemaining==0,"Residual contamination expires");
  auto n=Sandbox();const int a=Add(n,0,UnitClass::Saber,{0,0}),b=Add(n,1,UnitClass::Saber,{1,0});
  n.Find(a)->hp=n.Find(b)->hp=1;n.state.sources.push_back({n.state.nextId++,{0,0},1});n.EndRound();
  Check(!n.Find(a)->Alive() && !n.Find(b)->Alive() && n.state.corpses.size()==2,"Environmental deaths simultaneous");
  Check(n.state.sources.empty(),"New corpses cannot cause same-tick contamination");
}
void Purification() {
  auto m=Sandbox();const int caster=Add(m,0,UnitClass::Caster,{0,0});m.state.sources.push_back({m.state.nextId++,{1,0},5});
  Ok(m.BeginActivation(caster));Ok(m.UseAbility(caster,Ability::Purify,{1,0}));Check(m.state.sources.empty() && m.Find(caster)->mp==5,"Magic purifier removes real source");
  auto n=Sandbox();const int saber=Add(n,0,UnitClass::Saber,{0,0});n.state.corpses.push_back({n.state.nextId++,{1,0},1});
  Ok(n.BeginActivation(saber));Ok(n.UseAbility(saber,Ability::Incinerate,{1,0}));Check(n.state.corpses.empty(),"Incineration removes corpse");
  auto k=Sandbox();const int carrier=Add(k,0,UnitClass::Saber,{0,0});k.Find(carrier)->inventory={Item::Purifier};k.state.sources.push_back({k.state.nextId++,{1,0},5});
  Ok(k.BeginActivation(carrier));Ok(k.UseItem(carrier,0,{1,0}));Check(k.state.sources.empty() && k.Find(carrier)->inventory.empty() && k.Find(carrier)->acted,"Purifier kit removes source, consumes slot and action");
}
void EconomyAndItems() {
  auto m=Sandbox();const int saber=Add(m,0,UnitClass::Saber,{0,0});auto *house=m.At({0,0});house->building=Building::House;
  m.config.houseCapture=1;
  Ok(m.BeginActivation(saber));Ok(m.Capture(saber));Check(m.state.players[0].coins==140,"First neutral house reward");
  Ok(m.EndActivation());Check(m.state.players[0].coins==140,"A unit activation pays no income");
  Ok(m.EndTurn());Ok(m.EndTurn());Check(m.state.players[0].coins==155,"Fortress and house income at owner turn start");
  house=m.At({0,0});house->owner=1;m.Find(saber)->activated=false;Ok(m.BeginActivation(saber));Ok(m.Capture(saber));
  Check(m.state.players[0].coins==155,"No repeated neutral reward");
  auto n=Sandbox();const int buyer=Add(n,0,UnitClass::Saber,{-4,0});Ok(n.BuyItem(buyer,Item::Potion));Ok(n.BuyItem(buyer,Item::Purifier));Check(!n.BuyItem(buyer,Item::Potion),"Inventory has only two slots");
  n.Find(buyer)->hp=10;Ok(n.BeginActivation(buyer));Ok(n.UseItem(buyer,0,{-4,0}));Check(n.Find(buyer)->hp==18 && n.Find(buyer)->inventory.size()==1,"Potion heals and is consumed");
  auto o=Sandbox();const int builder=Add(o,0,UnitClass::Saber,{-4,0});Ok(o.BeginActivation(builder));Ok(o.BuildOutpost(builder));Check(o.At({-4,0})->building==Building::Outpost && o.state.players[0].coins==50,"Outpost cost and position");
}
void Recruitment() {
  auto m=Sandbox();m.state.players[0].coins=1000;
  Ok(m.RecruitBattalion(0,{UnitClass::Saber,UnitClass::Lancer,UnitClass::Berserker},{-5,0}));
  Check(m.state.units.size()==3 && m.state.players[0].coins==855,"Battalion sum without discount");
  for(const auto &u:m.state.units)Check(u.readyRound==2,"Recruit waits until next round");
  const auto count=m.state.units.size();const auto money=m.state.players[0].coins;
  Check(!m.RecruitBattalion(0,{UnitClass::Rider,static_cast<UnitClass>(99)},{-5,0}),"Invalid battalion rejected");
  Check(m.state.units.size()==count && m.state.players[0].coins==money,"Failed battalion is transactional");
}
void FortressCapture() {
  auto m=Sandbox();const int attacker=Add(m,0,UnitClass::Saber,{5,0}),defender=Add(m,1,UnitClass::Saber,{0,0});
  Ok(m.BeginActivation(attacker));Ok(m.Capture(attacker));Check(m.state.winner==-1 && m.At({5,0})->captureUnit==attacker,"Fortress capture pending");
  Ok(m.EndActivation());Ok(m.EndTurn());Ok(m.BeginActivation(defender));Check(m.state.winner==-1,"Defender receives full army turn");Ok(m.EndActivation());Ok(m.EndTurn());
  Ok(m.BeginActivation(attacker));Check(m.state.winner==-1,"Selection alone does not finish capture");Ok(m.Capture(attacker));Check(m.state.winner==-1,"Two Saber actions remain below fortress total");
  Ok(m.EndActivation());Ok(m.EndTurn());Ok(m.EndTurn());Ok(m.BeginActivation(attacker));Ok(m.Capture(attacker));Check(m.state.winner==0,"Capture wins only at configured points");Valid(m);
  auto n=Sandbox();const int captor=Add(n,0,UnitClass::Saber,{5,0});Add(n,1,UnitClass::Saber,{0,0});Ok(n.BeginActivation(captor));Ok(n.Capture(captor));n.Damage(captor,100,1);
  Check(n.At({5,0})->captureUnit==-1 && n.At({5,0})->captureProgress==28 && n.state.winner==-1,"Captor death pauses but preserves progress");
  auto p=Sandbox();const int leaver=Add(p,0,UnitClass::Saber,{5,0});Add(p,1,UnitClass::Saber,{0,0});
  Ok(p.BeginActivation(leaver));Ok(p.Capture(leaver));Ok(p.Move(leaver,{5,1}));Check(p.At({5,0})->captureUnit==-1 && p.At({5,0})->captureProgress==28,"Leaving pauses attempt without deleting progress");
}
void WeatherRules() {
  auto m=Sandbox();const int rider=Add(m,0,UnitClass::Rider,{0,0}),archer=Add(m,1,UnitClass::Archer,{3,0});m.state.weather=Weather::Storm;
  Check(m.EffectiveStats(*m.Find(rider)).movement==4,"Storm reduces rider movement");Check(m.EffectiveStats(*m.Find(archer)).maxRange==3,"Storm reduces archer range");
  auto n=Sandbox();const int caster=Add(n,0,UnitClass::Caster,{0,0}),enemy=Add(n,1,UnitClass::Saber,{2,0});
  const auto fire=n.EstimateDamage(caster,enemy,Ability::Fire),lightning=n.EstimateDamage(caster,enemy,Ability::Lightning);n.state.weather=Weather::Rain;
  Check(n.EstimateDamage(caster,enemy,Ability::Fire)==fire-1,"Rain weakens fire");Check(n.EstimateDamage(caster,enemy,Ability::Lightning)==lightning+1,"Rain strengthens lightning");
  n.config.weatherPeriod=3;n.EndRound();n.EndRound();n.EndRound();Check(n.state.weather!=Weather::Rain,"Weather changes at configured period");
}
void SaveLoadAndValidation() {
  const auto folder=std::filesystem::current_path()/"build"/"test-output";std::filesystem::create_directories(folder);
  const auto filename=(folder/"roundtrip.json").string();std::string error;
  auto m=Match::Create(Config::Load("Content/Plaguefront/balance.json"),"Content/Plaguefront/scenarios/frontier.json",Mode::Local,Difficulty::Hard);
  const int id=m.state.units.front().id;Ok(m.BeginActivation(id));m.Find(id)->precise=true;m.Find(id)->cooldowns[2]=2;m.Find(id)->inventory={Item::Potion,Item::Purifier};
  m.state.sources.push_back({m.state.nextId++,{2,2},4});m.state.corpses.push_back({m.state.nextId++,{3,2},1});m.config.forestDefense=7;
  if(!m.Save(filename,error))throw std::runtime_error("Save: "+error);
  ++checks;
  const auto loaded=Match::Load(filename,Config{},error);Check(loaded.has_value(),error.c_str());
  Check(loaded->state.activeUnit==id && loaded->Find(id)->precise && loaded->Find(id)->cooldowns[2]==2 && loaded->Find(id)->inventory.size()==2,"States and inventory restored");
  Check(loaded->state.corpses.size()==1 && loaded->state.sources.size()==1 && loaded->config.forestDefense==7,"Environmental state and balance restored");
  Json original;{std::ifstream in(filename);in>>original;}
  auto reject=[&](Json broken){std::ofstream out(filename);out<<broken.dump();out.close();Check(!Match::Load(filename,Config{},error),"Corrupted save must be rejected");};
  auto broken=original;broken["version"]=999;reject(broken);
  broken=original;broken["state"]["units"][0]["hp"]=999999;reject(broken);
  broken=original;broken["state"]["units"][0]["class"]=99;reject(broken);
  broken=original;broken["state"]["units"][1]["pos"]=broken["state"]["units"][0]["pos"];reject(broken);
  broken=original;broken["state"]["cells"].push_back(broken["state"]["cells"][0]);reject(broken);
  broken=original;broken["state"]["activeUnit"]=999;reject(broken);
  broken=original;broken["state"]["sources"][0]["remaining"]=-2;reject(broken);
  broken=original;broken["state"]["players"][0]["coins"]=1.5;reject(broken);
  broken=original;broken.erase("config");reject(broken);
  broken=original;broken["state"]["units"][0]["flags"][0]=1;reject(broken);
  {std::ofstream out(filename);out<<"{malformed";}Check(!Match::Load(filename,Config{},error),"Malformed JSON rejected");
  {std::ofstream out(filename);out<<"{\"nested\":"<<std::string(60,'[')<<"0"<<std::string(60,']')<<"}";}Check(!Match::Load(filename,Config{},error),"Overly nested JSON rejected");
  {std::ofstream out(filename);out<<std::string(4*1024*1024+1,' ');}Check(!Match::Load(filename,Config{},error),"Oversized JSON rejected before parse");
  std::filesystem::remove(filename);
}
void ScenariosAndOverrides() {
  Config config=Config::Load("Content/Plaguefront/balance.json");
  for(const auto *file:{"frontier","marsh"}) {
    const auto m=Match::Create(config,std::string("Content/Plaguefront/scenarios/")+file+".json",Mode::Local,Difficulty::Easy);
    Check(m.state.cells.size()==99 && m.state.units.size()==10,"Complete finite map and five units per side");
  }
  std::array<std::vector<UnitClass>,2> armies;armies[0]=std::vector<UnitClass>(5,UnitClass::Berserker);
  const auto m=Match::Create(config,"Content/Plaguefront/scenarios/frontier.json",Mode::VersusAI,Difficulty::Normal,armies);
  Check(m.state.units[0].type==UnitClass::Berserker && m.state.units[0].hp==34,"Army customization applies matching stats");
}
void AI() {
  for(auto difficulty:{Difficulty::Easy,Difficulty::Normal,Difficulty::Hard}) {
    auto m=Sandbox();const int ai=Add(m,1,UnitClass::Saber,{0,0});Add(m,0,UnitClass::Saber,{-4,4});
    m.At({0,0})->building=Building::House;m.state.currentPlayer=1;m.state.mode=Mode::VersusAI;m.state.difficulty=difficulty;m.state.players[1].coins=0;
    Check(m.StepAI(),"AI runs activation");Check(m.At({0,0})->captureProgress>0,"AI advances economic capture rather than chasing nearest enemy");
    Check(m.Find(ai)->hp==28,"Difficulty does not boost stats");Check(m.state.activeUnit==-1 && m.state.currentPlayer==0,"AI finishes activation");
  }
  auto m=Match::Create(Config{},"Content/Plaguefront/scenarios/frontier.json",Mode::VersusAI,Difficulty::Hard);
  for(int tick=0;tick<50 && m.state.winner==-1;++tick) {if(m.state.currentPlayer==1)m.StepAI();else m.Pass();Valid(m);}
  Check(m.state.round>=4,"AI and pass commands advance multiple rounds without stalling");
}
}

int main() {
  int groups=0;
  const std::vector<std::pair<const char*,std::function<void()>>> tests={
    {"hex geometry",Geometry},{"paths and movement",Movement},{"combat",Combat},{"range and LOS",RangedAndLOS},
    {"turns",Turns},{"abilities",Abilities},{"class tactics and action order",ClassTacticsAndActionOrder},{"deaths and unique rewards",DeathAndRewards},{"rot chronology",RotTimers},
    {"rot groups and simultaneous deaths",RotConnectionsAndSimultaneous},{"purification",Purification},
    {"economy and items",EconomyAndItems},{"recruitment",Recruitment},{"fortress capture",FortressCapture},
    {"weather",WeatherRules},{"save and invalid data",SaveLoadAndValidation},{"scenarios and armies",ScenariosAndOverrides},{"AI",AI}};
  for(const auto &[name,test]:tests)try{test();++groups;std::cout<<"[PASS] "<<name<<'\n';}
    catch(const std::exception &e){std::cerr<<"[FAIL] "<<name<<": "<<e.what()<<'\n';return 1;}
  std::cout<<groups<<" groups, "<<checks<<" checks passed.\n";return 0;
}
