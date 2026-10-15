#include "Plaguefront/Presentation.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

using namespace Plaguefront;

namespace {
int checks=0;
void Check(bool condition,const char *message) {++checks;if(!condition)throw std::runtime_error(message);}
void Near(float actual,float expected,const char *message,float tolerance=0.001F) {
  Check(std::isfinite(actual) && std::abs(actual-expected)<=tolerance,message);
}
void Ok(const Result &result) {++checks;if(!result)throw std::runtime_error(result.message);}
Match Board() {
  Config config;config.weatherPeriod=1000;
  Match match(config);match.state.mode=Mode::Local;
  for(int q=-5;q<=5;++q)for(int r=-4;r<=4;++r)match.state.cells.push_back({{q,r}});
  match.At({-5,0})->building=Building::Fortress;match.At({-5,0})->owner=0;
  match.At({5,0})->building=Building::Fortress;match.At({5,0})->owner=1;
  return match;
}
int Add(Match &match,int owner,UnitClass type,Hex pos) {
  Unit unit;unit.id=match.state.nextId++;unit.owner=owner;unit.type=type;unit.pos=pos;
  unit.hp=match.config.classes[static_cast<int>(type)].hp;unit.mp=type==UnitClass::Caster?8:0;
  match.state.units.push_back(unit);return unit.id;
}
std::string LogicalFingerprint(const Match &match) {
  std::ostringstream out;
  out<<match.state.round<<' '<<match.state.currentPlayer<<' '<<match.state.activeUnit<<' '<<match.state.nextId<<' '<<match.state.winner;
  for(const auto &u:match.state.units)out<<' '<<u.id<<','<<u.pos.q<<','<<u.pos.r<<','<<u.hp<<','<<u.mp<<','<<u.activated<<','<<u.moved<<','<<u.acted;
  for(const auto &c:match.state.corpses)out<<'c'<<c.id<<','<<c.deathRound;
  for(const auto &s:match.state.sources)out<<'s'<<s.id<<','<<s.remaining;
  for(const auto &p:match.state.players)out<<'p'<<p.coins;
  for(const auto &line:match.state.log)out<<line;
  return out.str();
}
bool Effect(const AnimationDirector &director,EffectType type) {
  return std::ranges::any_of(director.Effects(),[type](const VisualEffect &effect){return effect.type==type;});
}
void AffineProjectionAndInverse() {
  auto match=Board();
  for(auto &cell:match.state.cells)cell.height=(cell.pos.q-cell.pos.r)%4;
  Camera camera;camera.Fit(match.state.cells);
  for(int zoom=1;zoom<=3;++zoom) {
    Check(camera.SetZoom(zoom,{231.25F,153.5F}),"Integer zoom accepted");camera.PanBy({13.2F,-7.7F});
    Near(camera.Pan().x,std::round(camera.Pan().x),"Pan X snaps to native pixel");
    Near(camera.Pan().y,std::round(camera.Pan().y),"Pan Y snaps to native pixel");
    for(const auto &cell:match.state.cells) {
      const auto projected=camera.Project(cell.pos,static_cast<float>(cell.height));
      const auto inverse=camera.Inverse(projected,static_cast<float>(cell.height));
      Near(inverse.x,static_cast<float>(cell.pos.q),"Affine inverse Q at each height and zoom");
      Near(inverse.y,static_cast<float>(cell.pos.r),"Affine inverse R at each height and zoom");
      const auto polygon=camera.Surface(cell.pos,static_cast<float>(cell.height));
      for(std::size_t i=0;i<polygon.size();++i) {
        Near(polygon[i].x-projected.x,TopOffsets[i].x*static_cast<float>(zoom),"Native top polygon X");
        Near(polygon[i].y-projected.y,TopOffsets[i].y*static_cast<float>(zoom),"Native top polygon Y");
      }
    }
  }
  camera.ResetPan();Near(camera.Pan().x,0,"Reset pan X");Near(camera.Pan().y,0,"Reset pan Y");
  const auto origin=camera.Project({0,0}),u=camera.Project({1,0}),v=camera.Project({0,1});
  Near(u.x-origin.x,90,"U horizontal basis at 3x");Near(u.y-origin.y,42,"U vertical basis at 3x");
  Near(v.x-origin.x,0,"V horizontal basis");Near(v.y-origin.y,84,"V vertical basis at 3x");
  Near(camera.Project({0,0},1).y-origin.y,-18,"Height step is six native pixels times zoom");
}
void AnchorAndFit() {
  std::vector<Cell> cells;
  for(int q=-3;q<=3;++q)for(int r=-3;r<=3;++r)if(Distance({}, {q,r})<=3)cells.push_back({{q,r},Terrain::Plain,(q+r+6)%3});
  Camera camera;camera.Fit(cells);
  const auto bounds=camera.Bounds(cells);
  Check(bounds.left>=0 && bounds.top>=0 && bounds.left+bounds.width<=486 && bounds.top+bounds.height<=320,"Fit contains props and surface margins");
  Near(camera.Origin().x,std::round(camera.Origin().x),"Fit origin X is pixel aligned");
  Near(camera.Origin().y,std::round(camera.Origin().y),"Fit origin Y is pixel aligned");
  const Point axial{1.37F,-0.43F};const float elevation=1.25F;
  for(int zoom:{2,3,1}) {
    const auto anchor=camera.ProjectContinuous(axial.x,axial.y,elevation);
    Check(camera.SetZoom(zoom,anchor),"Zoom around anchor");
    const auto result=camera.ProjectContinuous(axial.x,axial.y,elevation);
    Near(result.x,anchor.x,"Zoom preserves anchor X within pixel snapping",0.501F);
    Near(result.y,anchor.y,"Zoom preserves anchor Y within pixel snapping",0.501F);
  }
  Check(!camera.SetZoom(0,{0,0}) && !camera.SetZoom(4,{0,0}),"Only integer zoom 1, 2, 3");
  const auto previous=camera.Pan();camera.PanBy({std::numeric_limits<float>::quiet_NaN(),1});
  Near(camera.Pan().x,previous.x,"Invalid pan ignored");
  Check(!camera.SetZoom(2,{std::numeric_limits<float>::infinity(),0}),"Invalid zoom anchor rejected");
}
void TopPicking() {
  std::vector<Cell> flat;
  for(int q=-3;q<=3;++q)for(int r=-3;r<=3;++r)flat.push_back({{q,r}});
  Camera camera;camera.Fit(flat);camera.PanBy({-14,9});
  for(int zoom=1;zoom<=3;++zoom) {
    camera.SetZoom(zoom,{223,137});
    for(const auto &cell:flat)Check(camera.Pick(flat,camera.Project(cell.pos))==cell.pos,"Pick every flat top center after pan/zoom");
    for(int height=-2;height<=4;++height) {
      const std::vector<Cell> isolated{{{2,-1},Terrain::Hill,height}};
      const auto center=camera.Project(isolated[0].pos,static_cast<float>(height));
      Check(camera.Pick(isolated,center)==isolated[0].pos,"Pick actual elevated top rather than flat inverse");
      const auto polygon=camera.Surface(isolated[0].pos,static_cast<float>(height));
      for(std::size_t i=0;i<polygon.size();++i) {
        Check(camera.Pick(isolated,polygon[i])==isolated[0].pos,"Pick all top vertices deterministically");
        const auto next=polygon[(i+1)%polygon.size()];
        Check(camera.Pick(isolated,{(polygon[i].x+next.x)*0.5F,(polygon[i].y+next.y)*0.5F})==isolated[0].pos,"Pick all top edge midpoints");
      }
      Check(!camera.Pick(isolated,{center.x+20.1F*zoom,center.y}),"Outside top silhouette is not pickable");
    }
  }
  std::vector<Cell> neighbors{{{0,0}},{{1,0}}};camera.Fit(neighbors);
  const auto center=camera.Project({0,0});const Point shared{center.x+15.F,center.y+7.F};
  Check(camera.Pick(neighbors,shared)==Hex{1,0},"Shared boundary chooses front cell");
  std::ranges::reverse(neighbors);Check(camera.Pick(neighbors,shared)==Hex{1,0},"Picking independent of input storage order");
  std::vector<Cell> overlap{{{0,0},Terrain::Plain,0},{{0,1},Terrain::Hill,3}};camera.Fit(overlap);
  Check(camera.Pick(overlap,camera.Project({0,0}))==Hex{0,1},"Overlapping elevated tops use same painter depth as rendering");
  const std::vector<Cell> tie{{{2,-1}},{{0,0}},{{-2,1}},{{0,1}}};
  const auto sorted=DrawOrder(tie);
  Check(sorted[0]->pos==Hex{-2,1} && sorted[1]->pos==Hex{0,0} && sorted[2]->pos==Hex{2,-1} && sorted[3]->pos==Hex{0,1},"Depth ties sort by Q consistently");
  Check(!camera.Pick({},{}),"Empty map has no pick");
}
void IdleClockAndImmutability() {
  auto match=Board();const int id=Add(match,0,UnitClass::Saber,{});AnimationDirector director;director.Reset(match);
  const auto original=LogicalFingerprint(match);
  director.Update(0.23F);const auto first=director.VisualFor(id,*match.Find(id));Check(first.frame==2,"Idle advances to a distinct frame");
  director.Observe(match,match);Near(director.VisualFor(id,*match.Find(id)).elapsed,first.elapsed,"No-change Observe preserves idle phase");
  director.Update(0.23F);Check(director.VisualFor(id,*match.Find(id)).frame==4,"Idle continues instead of resetting on each snapshot");
  Check(!director.Busy(),"Idle loop does not block tactical input");
  director.Update(std::numeric_limits<float>::quiet_NaN());
  Check(LogicalFingerprint(match)==original,"Presentation cannot alter logical state");
}
void LegalWalkInterpolation() {
  auto match=Board();const int id=Add(match,0,UnitClass::Rider,{});
  match.At({1,0})->terrain=Terrain::Blocked;match.At({2,0})->height=2;
  Ok(match.BeginActivation(id));const Match before=match;const auto path=before.FindPath(id,{2,0});
  Check(path && path->cells.size()>3,"Fixture path routes around blocked cell");
  AnimationDirector director;director.Reset(before);Ok(match.Move(id,{2,0}));
  const auto afterFingerprint=LogicalFingerprint(match);director.Observe(before,match);
  auto visual=director.VisualFor(id,*match.Find(id));
  Check(match.Find(id)->pos==Hex{2,0} && visual.state==AnimState::Walk,"Logic commits immediately while walk presentation starts");
  Near(visual.q,0,"Walk begins at previous logical Q");Near(visual.r,0,"Walk begins at previous logical R");
  Check(visual.path==path->cells,"Visual follows actual weighted legal route");
  director.Update(0.06F);visual=director.VisualFor(id,*match.Find(id));
  Near(visual.q,static_cast<float>(path->cells[1].q)*0.5F,"Half-step Q interpolation");
  Near(visual.r,static_cast<float>(path->cells[1].r)*0.5F,"Half-step R interpolation");
  director.Update(0.03F);Check(director.VisualFor(id,*match.Find(id)).frame==1,"Walk cycles a distinct animation frame");
  director.Update(2.F);visual=director.VisualFor(id,*match.Find(id));
  Check(visual.state==AnimState::Idle,"Walk finishes in idle");Near(visual.q,2,"Walk reaches committed Q");Near(visual.elevation,2,"Walk interpolates and reaches destination elevation");
  Check(LogicalFingerprint(match)==afterFingerprint,"Animation elapsed time does not advance rules or turns");
}
void MoveAttackQueue() {
  auto match=Board();const int actor=Add(match,0,UnitClass::Saber,{}),target=Add(match,1,UnitClass::Saber,{3,0});
  Ok(match.BeginActivation(actor));AnimationDirector director;director.Reset(match);
  Match before=match;Ok(match.Move(actor,{2,0}));director.Observe(before,match);
  before=match;Ok(match.UseAbility(actor,Ability::Attack,{3,0}));director.Observe(before,match,Ability::Attack);
  Check(director.VisualFor(actor,*match.Find(actor)).state==AnimState::Walk,"Attack queues behind unfinished walk");
  director.Update(0.25F);auto visual=director.VisualFor(actor,*match.Find(actor));
  Check(visual.state==AnimState::Attack,"Attack starts after movement completes");Near(visual.q,2,"Queued attack starts at destination");
  const float elapsed=visual.elapsed;director.Observe(match,match);Near(director.VisualFor(actor,*match.Find(actor)).elapsed,elapsed,"No-change Observe does not restart attack");
  director.Update(0.20F);Check(director.VisualFor(target,*match.Find(target)).state==AnimState::Hurt,"Damage reaction follows queued attack contact pose");
  Check(Effect(director,EffectType::Slash) && Effect(director,EffectType::Damage),"Melee and real HP feedback effects exist");
  director.Update(2);Check(director.VisualFor(actor,*match.Find(actor)).state==AnimState::Idle,"Attack returns to idle");Check(!director.Busy(),"All queued clips and effects eventually finish");
}
void DeathVisibility() {
  auto match=Board();const int actor=Add(match,0,UnitClass::Saber,{}),target=Add(match,1,UnitClass::Saber,{1,0});
  match.Find(target)->hp=2;Ok(match.BeginActivation(actor));AnimationDirector director;director.Reset(match);const Match before=match;
  Ok(match.UseAbility(actor,Ability::Attack,{1,0}));director.Observe(before,match,Ability::Attack);
  Check(!match.Find(target)->Alive() && director.VisualFor(target,*match.Find(target)).visible,"Dead logical unit remains visible for death animation");
  director.Update(0.19F);Check(director.VisualFor(target,*match.Find(target)).state==AnimState::Death,"Fatal damage selects death state at contact");
  director.Update(0.30F);Check(director.VisualFor(target,*match.Find(target)).visible,"Death persists while frames play");
  director.Update(0.36F);const auto dead=director.VisualFor(target,*match.Find(target));
  Check(!dead.visible && dead.state==AnimState::Death && dead.frame==7,"Death hides after final frame without returning to living idle");
}
void SpellEffectsAndAIInference() {
  for(const auto [ability,type]:{std::pair{Ability::Fire,EffectType::Fire},std::pair{Ability::Ice,EffectType::Ice},std::pair{Ability::Lightning,EffectType::Lightning}}) {
    auto match=Board();const int caster=Add(match,0,UnitClass::Caster,{});Add(match,1,UnitClass::Saber,{2,0});
    Ok(match.BeginActivation(caster));AnimationDirector director;director.Reset(match);const Match before=match;
    Ok(match.UseAbility(caster,ability,{2,0}));director.Observe(before,match,ability);
    Check(director.VisualFor(caster,*match.Find(caster)).state==AnimState::Cast,"Spell uses casting state");Check(Effect(director,type),"Ability-specific spell effect");
    director.Update(0.20F);Check(director.VisualFor(caster,*match.Find(caster)).frame==2,"Casting frame advances");
  }
  auto match=Board();const int human=Add(match,0,UnitClass::Saber,{2,0}),caster=Add(match,1,UnitClass::Caster,{});
  match.state.currentPlayer=1;match.Find(human)->activated=true;AnimationDirector director;director.Reset(match);const Match before=match;
  Ok(match.BeginActivation(caster));Ok(match.UseAbility(caster,Ability::Ice,{2,0}));Ok(match.EndActivation());
  Ok(match.EndTurn());Ok(match.EndTurn());
  Check(match.state.round==2 && !match.Find(caster)->attacked,"AI-like whole activation resets flags at round boundary");
  director.Observe(before,match);Check(director.VisualFor(caster,*match.Find(caster)).state==AnimState::Cast,"Infer AI caster even after round reset");
  Check(Effect(director,EffectType::Ice) && !Effect(director,EffectType::Fire),"Infer exact AI spell from new actual logs");
}
void EnvironmentalAndCaptureEffects() {
  auto match=Board();Add(match,0,UnitClass::Saber,{0,0});match.state.round=4;match.state.corpses.push_back({match.state.nextId++,{1,0},1});
  AnimationDirector director;director.Reset(match);Match before=match;match.EndRound();director.Observe(before,match);
  Check(Effect(director,EffectType::RotBurst),"Corpse-to-source transition creates environmental visual effect");
  Check(!Effect(director,EffectType::Slash),"Source activation does not invent a weapon attack");
  director.Reset(match);before=match;match.EndRound();director.Observe(before,match);
  Check(Effect(director,EffectType::Damage) && !Effect(director,EffectType::Slash),"Environmental damage has HP feedback without false melee");
  auto purified=Board();const int caster=Add(purified,0,UnitClass::Caster,{});purified.state.sources.push_back({purified.state.nextId++,{1,0},5});
  Ok(purified.BeginActivation(caster));director.Reset(purified);before=purified;Ok(purified.UseAbility(caster,Ability::Purify,{1,0}));director.Observe(before,purified,Ability::Purify);
  Check(Effect(director,EffectType::Purify),"Real source removal emits purification visual");
  auto captured=Board();const int saber=Add(captured,0,UnitClass::Saber,{});captured.At({0,0})->building=Building::House;
  captured.config.houseCapture=1;
  Ok(captured.BeginActivation(saber));director.Reset(captured);before=captured;Ok(captured.Capture(saber));director.Observe(before,captured);
  Check(Effect(director,EffectType::Capture),"Building owner transition emits capture visual");
}
void BoundedLogInference() {
  auto match=Board();const int saber=Add(match,0,UnitClass::Saber,{});
  match.At({0,0})->building=Building::House;
  match.config.houseCapture=1;
  for(int i=0;i<160;++i)match.state.log.push_back("Earlier event "+std::to_string(i));
  match.state.log[120]="Fire: 6 de dano.";
  Ok(match.BeginActivation(saber));AnimationDirector director;director.Reset(match);const Match before=match;
  Ok(match.Capture(saber));Check(match.state.log.size()<before.state.log.size(),"Capture fixture trims bounded log prefix");
  director.Observe(before,match);
  Check(director.VisualFor(saber,*match.Find(saber)).state==AnimState::Attack,"Old retained Fire must not turn a real physical capture pose into casting");
  Check(Effect(director,EffectType::Capture)&&!Effect(director,EffectType::Fire),"Capture visual remains accurate after log trimming");

  auto environmental=Board();const int human=Add(environmental,0,UnitClass::Saber,{2,0}),guard=Add(environmental,1,UnitClass::Saber,{});
  environmental.state.currentPlayer=1;environmental.Find(human)->activated=true;
  environmental.state.sources.push_back({environmental.state.nextId++,{1,0},5});
  for(int i=0;i<160;++i)environmental.state.log.push_back("Old environment event "+std::to_string(i));
  environmental.state.log[125]="Fire: 6 de dano.";
  director.Reset(environmental);const Match oldEnvironment=environmental;
  Ok(environmental.BeginActivation(guard));Ok(environmental.UseAbility(guard,Ability::Guard,{}));Ok(environmental.EndActivation());
  Ok(environmental.EndTurn());
  director.Observe(oldEnvironment,environmental);
  Check(Effect(director,EffectType::Damage),"Guard/end-round fixture still shows real environmental damage");
  Check(!Effect(director,EffectType::Fire)&&!Effect(director,EffectType::Ice)&&!Effect(director,EffectType::Lightning),"Guard and environmental damage do not replay an old spell");
  Check(director.VisualFor(guard,*environmental.Find(guard)).state!=AnimState::Cast,"Last AI guard plus rot does not invent a cast");
}
void SequencedCounterImpacts() {
  auto match=Board();const int attacker=Add(match,0,UnitClass::Saber,{}),defender=Add(match,1,UnitClass::Saber,{1,0});
  Ok(match.BeginActivation(attacker));AnimationDirector director;director.Reset(match);const Match before=match;
  Ok(match.UseAbility(attacker,Ability::Attack,{1,0}));const auto logical=LogicalFingerprint(match);director.Observe(before,match,Ability::Attack);
  std::vector<VisualEffect> damage;
  for(const auto &effect:director.Effects())if(effect.type==EffectType::Damage)damage.push_back(effect);
  Check(damage.size()==2,"One attack and one response create exactly two real HP labels");
  Check(damage[0].end==match.Find(defender)->pos && damage[1].end==match.Find(attacker)->pos,"HP labels remain associated with actual initial and counter victims");
  Check(damage[0].amount==28-match.Find(defender)->hp && damage[1].amount==28-match.Find(attacker)->hp,"Both animation amounts equal actual rules damage");
  Check(damage[0].delay<damage[1].delay && damage[1].text.starts_with("CONTRA"),"Counter is announced distinctly after the initial impact");
  director.Update(.19F);Check(director.VisualFor(defender,*match.Find(defender)).state==AnimState::Hurt,"Initial victim reacts at first contact");
  director.Update(.30F);Check(director.VisualFor(defender,*match.Find(defender)).state==AnimState::Attack,"Defender starts its response after hurt recovery");
  director.Update(.18F);Check(director.VisualFor(attacker,*match.Find(attacker)).state==AnimState::Hurt,"Original attacker reacts only at counter contact");
  const auto effects=director.Effects().size();director.Observe(match,match);
  Check(director.Effects().size()==effects,"Unchanged snapshots never replay sequenced counter events");
  director.Update(3.F);Check(!director.Busy() && LogicalFingerprint(match)==logical,"Entire sequence finishes without applying logical damage twice");
}
void OrderedChainLinksAndBurnFeedback() {
  auto match=Board();const int caster=Add(match,0,UnitClass::Caster,{});
  for(Hex pos:{Hex{2,0},Hex{3,0},Hex{4,0}})Add(match,1,UnitClass::Saber,pos);
  Ok(match.BeginActivation(caster));AnimationDirector director;director.Reset(match);const Match before=match;
  Ok(match.UseAbility(caster,Ability::Lightning,{2,0}));director.Observe(before,match,Ability::Lightning);
  std::vector<VisualEffect> links;for(const auto &effect:director.Effects())if(effect.type==EffectType::Lightning)links.push_back(effect);
  Check(links.size()==3,"Each actual lightning hit creates one visual link");
  for(std::size_t i=0;i<links.size();++i) {
    const auto &event=match.state.combatEvents[i];
    Check(links[i].start==event.from && links[i].end==event.to && links[i].amount==event.amount,"Visual chain endpoints and damage read actual combat events");
    if(i)Check(links[i].delay>links[i-1].delay,"Every chain jump appears after its preceding impact");
  }
  const auto count=director.Effects().size();director.Observe(match,match);
  Check(director.Effects().size()==count,"Repeated snapshots cannot duplicate chain hits");
  director.Update(3.F);Check(!director.Busy(),"Finite chain presentation eventually releases input");
  auto burning=Board();const int guard=Add(burning,1,UnitClass::Saber,{});Add(burning,0,UnitClass::Saber,{4,0});
  burning.state.currentPlayer=1;burning.state.fires.push_back({burning.state.nextId++,{},1,0});
  for(int i=0;i<160;++i)burning.state.log.push_back(i==125?"Lightning: 10 de dano.":"Earlier event "+std::to_string(i));
  director.Reset(burning);const Match beforeBurn=burning;
  Ok(burning.BeginActivation(guard));Ok(burning.UseAbility(guard,Ability::Guard,{}));Ok(burning.EndActivation());Ok(burning.EndTurn());
  director.Observe(beforeBurn,burning);
  Check(Effect(director,EffectType::Damage) && !Effect(director,EffectType::Lightning) && !Effect(director,EffectType::Slash),"Real burn damage cannot replay a stale spell or invent a weapon strike");
}
void CombatAndEnvironmentInOneSnapshot() {
  auto match=Board();match.state.currentPlayer=1;
  const int victim=Add(match,0,UnitClass::Saber,{}),attacker=Add(match,1,UnitClass::Saber,{1,0});
  match.Find(victim)->hp=6;match.state.fires.push_back({match.state.nextId++,{},1,1});
  AnimationDirector director;director.Reset(match);const Match before=match;
  Ok(match.BeginActivation(attacker));Ok(match.UseAbility(attacker,Ability::Attack,{}));
  Check(match.Find(victim)->hp==2&&match.Find(attacker)->hp==27,"Fixture initially survives initial hit and gives one real counter");
  Ok(match.EndActivation());Ok(match.EndTurn());
  Check(!match.Find(victim)->Alive()&&match.state.combatEvents.size()==2,"Same army-end snapshot includes later fatal fire after finite combat sequence");
  Check(!match.state.combatEvents[0].lethal&&!match.state.combatEvents[1].lethal,"Combat events retain nonlethal hit-time truth despite later environmental death");
  director.Observe(before,match);
  std::vector<VisualEffect> damage;
  for(const auto &effect:director.Effects())if(effect.type==EffectType::Damage)damage.push_back(effect);
  Check(damage.size()==3,"Combined snapshot must preserve initial counter and residual burn HP labels");
  const auto initial=std::ranges::find_if(damage,[&](const VisualEffect &effect){return effect.end==Hex{}&&effect.amount==4;});
  const auto counter=std::ranges::find_if(damage,[](const VisualEffect &effect){return effect.text.starts_with("CONTRA")&&effect.amount==1;});
  const auto burn=std::ranges::find_if(damage,[&](const VisualEffect &effect){return effect.end==Hex{}&&effect.amount==2;});
  Check(initial!=damage.end()&&counter!=damage.end()&&burn!=damage.end(),"Each exact logical damage component is represented once");
  Check(initial->delay<counter->delay&&counter->delay<burn->delay,"Environmental HP feedback must follow both combat impacts");
  director.Update(.19f);Check(director.VisualFor(victim,*match.Find(victim)).state==AnimState::Hurt&&director.VisualFor(victim,*match.Find(victim)).visible,"Initially surviving victim must hurt at first impact rather than die prematurely");
  director.Update(std::max(0.f,burn->delay-.19f)+.02f);
  Check(director.VisualFor(victim,*match.Find(victim)).state==AnimState::Death,"Victim dies visually when residual fire actually arrives");
  director.Update(3.f);Check(!director.Busy()&&!director.VisualFor(victim,*match.Find(victim)).visible,"Combined sequence finishes once and leaves the persistent logical corpse");
  Check(match.state.corpses.size()==1&&match.state.corpses.front().unitId==victim,"Presentation never changes death cause chronology or duplicates corpse");
}
void ContaminationMaskUsesRealRules() {
  auto match=Board();Add(match,0,UnitClass::Saber,{});Add(match,1,UnitClass::Saber,{4,0});
  const auto initial=LogicalFingerprint(match);
  match.state.corpses.push_back({200,{},match.state.round,0,UnitClass::Saber,-1});
  for(const auto &cell:match.state.cells)Check(!RotPresentationAt(match,cell.pos).Active(),"Inert corpse does not invent an active visual source");
  match.state.corpses.clear();
  for(int radius:{1,2,3}) {
    match.config.rotRadius=radius;
    match.state.sources={{100,{},5}};
    int expectedCount=0,visibleCount=0;
    for(const auto &cell:match.state.cells) {
      const auto exposure=RotPresentationAt(match,cell.pos);const bool inside=Distance({},cell.pos)<=radius;
      expectedCount+=inside;visibleCount+=exposure.local;
      Check(exposure.local==inside,"Every highlighted local hex matches the configured axial radius");
      Check(exposure.coveringSources==(inside?1:0),"A single source contributes exactly one coverage classification");
      Check(!exposure.global&&exposure.damage==match.RotDamageAt(cell.pos),"Visual damage is the authoritative Match query");
      Check(exposure.Active()==inside,"Clean terrain does not receive contaminated treatment");
    }
    Check(visibleCount==expectedCount,"Full map mask covers every and only affected cell");
    Check(RotPresentationAt(match,{radius,0}).local,"Configured boundary hex is included");
    Check(!RotPresentationAt(match,{radius+1,0}).local,"First hex outside radius is clean");
  }
  match.state.sources.clear();
  Check(LogicalFingerprint(match)==initial,"Repeated contamination queries never change gameplay or apply environmental damage");
  Check(!RotPresentationAt(match,{99,99}).Active(),"Off-board cursor has no visual exposure");
}
void OverlapAndGlobalPresentationLifecycle() {
  auto match=Board();match.config.rotRadius=1;match.config.connectionDistance=3;
  match.state.sources={{100,{},5},{101,{1,0},4},{102,{3,0},3},{103,{-5,-4},0}};
  Check(match.LargestSourceGroup()>=match.config.globalThreshold,"Connected fixture uses the real rules group query");
  const auto overlap=RotPresentationAt(match,{});
  Check(overlap.local&&overlap.coveringSources==2,"Overlap is represented once with an exact coverage count");
  Check(overlap.damage==match.config.rotDamage&&!overlap.global,"Connected sources do not invent global activation before the model tick");
  Check(!RotPresentationAt(match,{-5,-4}).local,"Expired source does not tint its neighborhood");
  match.state.globalRemaining=2;
  for(const auto &cell:match.state.cells) {
    const auto exposure=RotPresentationAt(match,cell.pos);
    Check(exposure.global,"Active global state reaches every existing map cell");
    Check(exposure.damage==match.RotDamageAt(cell.pos),"Global and local numbers remain exactly the model query");
    Check(exposure.damage==(exposure.local?match.config.rotDamage:match.config.globalDamage),"Local and global visual layers follow non-stacking precedence");
  }
  const auto combined=RotPresentationAt(match,{});
  Check(combined.local&&combined.global&&combined.damage==match.config.rotDamage,"Local warning can disclose active global state without summing it");
  for(auto &source:match.state.sources)source.remaining=0;
  for(const auto &cell:match.state.cells) {
    const auto exposure=RotPresentationAt(match,cell.pos);
    Check(!exposure.local&&exposure.coveringSources==0&&exposure.global&&exposure.damage==match.config.globalDamage,"Residual global effect persists after its last source expires");
  }
  match.state.globalRemaining=0;
  for(const auto &cell:match.state.cells) {
    const auto exposure=RotPresentationAt(match,cell.pos);
    Check(!exposure.local&&!exposure.global&&!exposure.Active(),"All visual contamination clears exactly with the logical state");
  }
}
}

int main() {
  int groups=0;
  const std::vector<std::pair<const char *,std::function<void()>>> tests{
    {"affine projection and inverse",AffineProjectionAndInverse},{"anchor zoom and fitted bounds",AnchorAndFit},
    {"elevated top picking and painter order",TopPicking},{"idle phase and immutable rules",IdleClockAndImmutability},
    {"weighted movement interpolation",LegalWalkInterpolation},{"move attack damage queue",MoveAttackQueue},
    {"persistent death animation",DeathVisibility},{"spell effects and AI inference",SpellEffectsAndAIInference},
    {"environmental purification and capture visuals",EnvironmentalAndCaptureEffects},
    {"bounded log inference",BoundedLogInference},
    {"attack and counter impact sequence",SequencedCounterImpacts},
    {"ordered chain links and environmental burn",OrderedChainLinksAndBurnFeedback},
    {"combat and later environmental death in one snapshot",CombatAndEnvironmentInOneSnapshot},
    {"contamination mask derives from actual rules",ContaminationMaskUsesRealRules},
    {"overlap and residual global presentation lifecycle",OverlapAndGlobalPresentationLifecycle}
  };
  for(const auto &[name,test]:tests)try{test();++groups;std::cout<<"[PASS] "<<name<<'\n';}
    catch(const std::exception &error){std::cerr<<"[FAIL] "<<name<<": "<<error.what()<<'\n';return 1;}
  std::cout<<groups<<" presentation groups, "<<checks<<" checks passed.\n";return 0;
}
