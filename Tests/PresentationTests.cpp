#include "Plaguefront/Presentation.h"
#include "Plaguefront/Weather.h"
#include "Plaguefront/SpellPresentation.h"
#include "Plaguefront/UnitPresentation.h"

#include <algorithm>
#include <bit>
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
void RotatedLargeMapCamera() {
  const auto match=Match::Create(Config{},"Content/Plaguefront/scenarios/highlands.json",Mode::Local,Difficulty::Normal);
  Check(match.state.cells.size()==188,"Perspective keeps the complete third scenario");
  Camera camera;camera.SetRotation(1);camera.Fit(match.state.cells,834,700);
  const auto origin=camera.Project({0,0});
  const auto column=camera.Project({1,0}),row=camera.Project({0,1});
  Near(column.x-origin.x,30.F,"Rotated column X follows the hexagonal lattice");
  Near(column.y-origin.y,-14.F,"Third-map columns recede diagonally");
  Near(row.x-origin.x,30.F,"Third-map rows advance diagonally instead of vertically");
  Near(row.y-origin.y,14.F,"Rotated row Y preserves tile proportions");
  Near(camera.DepthContinuous(.5F,.25F),-3.5F,"Animated depth uses the same rotated axial basis");
  for(const Point viewport : {Point{834,700},Point{1140,720},Point{1560,900}}) {
    camera.FrameAll(match.state.cells,viewport.x,viewport.y);
    const auto bounds=camera.Bounds(match.state.cells);
    Check(bounds.left>=0 && bounds.top>=0 && bounds.left+bounds.width<=viewport.x && bounds.top+bounds.height<=viewport.y,
          "Rotated large-map overview keeps all cells and prop margins visible");
    for(int zoom=1;zoom<=3;++zoom) {
      camera.SetZoom(zoom,{viewport.x/2,viewport.y/2});camera.PanBy({-17,23});
      for(const auto &cell:match.state.cells) {
        const auto point=camera.Project(cell.pos,static_cast<float>(cell.height));
        const auto inverse=camera.Inverse(point,static_cast<float>(cell.height));
        Near(inverse.x,static_cast<float>(cell.pos.q),"Rotated elevated projection round-trips logical Q");
        Near(inverse.y,static_cast<float>(cell.pos.r),"Rotated elevated projection round-trips logical R");
        Check(camera.Pick({cell},point)==cell.pos,"Rotated top picking keeps the original logical cell at every zoom");
      }
    }
  }
  std::vector<Cell> overlap{{{0,0},Terrain::Plain,0},{{1,0},Terrain::Plain,-2}};
  camera.Fit(overlap);
  const auto center=camera.Project({0,0});
  const Point shared{center.x+15.F,center.y};
  Check(InsideTop(shared,camera.Surface({0,0})) && InsideTop(shared,camera.Surface({1,0},-2.F)),
        "Rotated picking fixture must overlap both tile surfaces");
  Check(camera.Depth({1,0})<camera.Depth({0,0}),"Rotated depth reverses the old column ordering");
  Check(camera.Pick(overlap,shared)==Hex{0,0},"Rotated picking selects the same foreground tile as the renderer");
  std::ranges::reverse(overlap);
  Check(camera.Pick(overlap,shared)==Hex{0,0},"Rotated occlusion is independent of cell storage order");
  camera.SetRotation(-5);Check(camera.Rotation()==1,"Negative hexagonal turns normalize without reflecting the map");
  camera.SetRotation(6);Check(camera.Rotation()==0,"A full turn restores the original camera orientation");
  camera.Fit(match.state.cells);
  const auto standard=camera.Project({0,1}),zero=camera.Project({0,0});
  Near(standard.x-zero.x,0.F,"The first two maps keep their original projection");
  Near(standard.y-zero.y,28.F,"The original row projection remains unchanged");
}
void RotatedUnitFacing() {
  auto walking=Board();const int walker=Add(walking,0,UnitClass::Rider,{});
  Ok(walking.BeginActivation(walker));const Match beforeWalk=walking;
  Ok(walking.Move(walker,{1,0}));
  AnimationDirector standard,rotated;
  standard.Reset(beforeWalk);rotated.SetRotation(1);rotated.Reset(beforeWalk);
  standard.Observe(beforeWalk,walking);rotated.Observe(beforeWalk,walking);
  Check(standard.VisualFor(walker,*walking.Find(walker)).facing==0,"Original map walking faces its original direction");
  Check(rotated.VisualFor(walker,*walking.Find(walker)).facing==2,"Rotated map walking faces the projected destination");
  rotated.Update(1.F);
  Check(rotated.VisualFor(walker,*walking.Find(walker)).facing==2,"Idle preserves the completed walk's rotated facing");
  auto combat=Board();const int attacker=Add(combat,0,UnitClass::Saber,{}),victim=Add(combat,1,UnitClass::Lancer,{1,0});
  Ok(combat.BeginActivation(attacker));const Match beforeAttack=combat;
  Ok(combat.UseAbility(attacker,Ability::Attack,combat.Find(victim)->pos));
  rotated.Reset(beforeAttack);rotated.Observe(beforeAttack,combat);
  Check(rotated.VisualFor(attacker,*combat.Find(attacker)).facing==2,"Rotated attack faces the projected enemy");
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
  const float twoFrames=AnimationTiming.idleFrameSeconds*2.F+.001F;
  director.Update(twoFrames);const auto first=director.VisualFor(id,*match.Find(id));Check(first.frame==2,"Idle advances to a distinct frame");
  director.Observe(match,match);Near(director.VisualFor(id,*match.Find(id)).elapsed,first.elapsed,"No-change Observe preserves idle phase");
  director.Update(twoFrames);Check(director.VisualFor(id,*match.Find(id)).frame==4,"Idle continues instead of resetting on each snapshot");
  Check(!director.Busy(),"Idle loop does not block tactical input");
  director.Update(std::numeric_limits<float>::quiet_NaN());
  Check(LogicalFingerprint(match)==original,"Presentation cannot alter logical state");
}
void IdleTemporalCycleAllClassesAndFacings() {
  const std::array<Hex,4> destinations{{{1,0},{-1,1},{1,-1},{-1,0}}};
  for(int owner=0;owner<2;++owner)for(int cls=0;cls<7;++cls)for(int facing=0;facing<4;++facing) {
    auto match=Board();match.state.currentPlayer=owner;
    const int id=Add(match,owner,static_cast<UnitClass>(cls),{});Ok(match.BeginActivation(id));
    AnimationDirector director;director.Reset(match);const Match before=match;
    Ok(match.Move(id,destinations[static_cast<std::size_t>(facing)]));director.Observe(before,match);
    director.Update(AnimationTiming.walkCellSeconds+.0001f);
    auto visual=director.VisualFor(id,*match.Find(id));
    Check(visual.state==AnimState::Idle&&visual.facing==facing,"Caminar deja el idle en la orientación real del trayecto");
    const auto logical=LogicalFingerprint(match);const auto position=visual;
    director.Update(AnimationTiming.idleFrameSeconds*.5f);
    for(int sample=0;sample<AnimationTiming.frames*2;++sample) {
      if(sample)director.Update(AnimationTiming.idleFrameSeconds);
      visual=director.VisualFor(id,*match.Find(id));
      Check(visual.frame==sample%AnimationTiming.frames,"Idle reproduce dos ciclos ordenados con el intervalo configurado");
      Check(visual.state==AnimState::Idle&&visual.facing==facing&&visual.visible,"Idle conserva estado, orientación y visibilidad en cada muestra");
      Near(visual.q,position.q,"Idle no desplaza la coordenada Q");Near(visual.r,position.r,"Idle no desplaza la coordenada R");
      Near(visual.elevation,position.elevation,"Idle no altera la elevación del anclaje");
      director.Observe(match,match);
      Near(director.VisualFor(id,*match.Find(id)).elapsed,visual.elapsed,"Observar la misma partida no reinicia un ciclo idle");
      Check(!director.Busy()&&LogicalFingerprint(match)==logical,"Idle no bloquea entradas ni avanza el estado táctico");
    }
  }
  auto match=Board();const int id=Add(match,0,UnitClass::Saber,{});AnimationDirector director;director.Reset(match);
  director.Update(AnimationTiming.idleFrameSeconds-.001f);Check(director.VisualFor(id,*match.Find(id)).frame==0,"Idle mantiene el marco hasta completar su intervalo");
  director.Update(.002f);Check(director.VisualFor(id,*match.Find(id)).frame==1,"Idle cambia el marco al cruzar su intervalo");
}
void RainViewportMotionAndBounds() {
  for(const auto size:{Point{286.f,260.f},Point{486.f,320.f},Point{760.f,480.f},Point{1220.f,720.f}})for(bool storm:{false,true}) {
    const auto first=RainDrops(size.x,size.y,.25f,storm),same=RainDrops(size.x,size.y,.25f,storm),next=RainDrops(size.x,size.y,.33f,storm);
    Check(first.size()>=64&&first.size()<=360&&first.size()==same.size()&&first.size()==next.size(),"La cantidad de lluvia está acotada por vista y no crece con el tiempo");
    std::array<int,16> coverage{};std::set<int> layers,opacities;int moving=0;
    for(std::size_t i=0;i<first.size();++i) {
      const auto &drop=first[i],&repeat=same[i],&after=next[i];
      Check(std::isfinite(drop.head.x)&&std::isfinite(drop.head.y)&&std::isfinite(drop.tail.x)&&std::isfinite(drop.tail.y),"La lluvia contiene únicamente coordenadas finitas");
      Check(drop.head.x>=0&&drop.head.x<size.x&&drop.head.y>=0&&drop.head.y<size.y,"Las gotas cubren el viewport nativo sin depender de la cámara");
      Check(drop.head.x==repeat.head.x&&drop.head.y==repeat.head.y&&drop.tail.x==repeat.tail.x&&drop.tail.y==repeat.tail.y,"La misma fase produce lluvia determinista");
      const float dx=drop.tail.x-drop.head.x,dy=drop.tail.y-drop.head.y;
      Check(dy<0&&dx>0&&std::hypot(dx,dy)>=3.f&&std::hypot(dx,dy)<=16.f,"La cola del trazo sigue la trayectoria descendente con viento");
      Check(drop.opacity>0&&drop.layer<3,"Las capas de lluvia tienen opacidad visible y un índice válido");
      layers.insert(drop.layer);opacities.insert(drop.opacity);
      const int x=std::min(3,static_cast<int>(drop.head.x*4/size.x)),y=std::min(3,static_cast<int>(drop.head.y*4/size.y));
      ++coverage[static_cast<std::size_t>(y*4+x)];
      moving+=drop.head.x!=after.head.x||drop.head.y!=after.head.y;
    }
    Check(std::ranges::all_of(coverage,[](int count){return count>0;}),"La lluvia ocupa las dieciséis regiones de la vista");
    Check(layers.size()==3&&opacities.size()>=3,"Tres profundidades distinguen los trazos sin aumentar partículas");
    Check(moving==static_cast<int>(first.size()),"Todas las gotas avanzan entre dos instantes reales");
    for(float elapsed:{-8.f,0.f,10000.f}) {
      const auto drops=RainDrops(size.x,size.y,elapsed,storm);
      Check(drops.size()==first.size(),"El reloj de lluvia no altera el presupuesto de partículas");
      for(const auto &drop:drops)Check(drop.head.x>=0&&drop.head.x<size.x&&drop.head.y>=0&&drop.head.y<size.y,"El módulo positivo conserva cobertura con tiempos negativos o largos");
    }
  }
  for(const auto size:{Point{},Point{-1.f,320.f},Point{480.f,0.f},Point{std::numeric_limits<float>::infinity(),320.f}})
    Check(RainDrops(size.x,size.y,0).empty(),"Una vista inválida no genera partículas");
  Check(RainDrops(480,320,std::numeric_limits<float>::quiet_NaN()).empty(),"Un reloj inválido no genera coordenadas de lluvia inválidas");
}
void SpellStagesPreserveContactAndFiniteLifetime() {
  for(EffectType type:{EffectType::Fire,EffectType::Ice,EffectType::Lightning,EffectType::Heal,EffectType::Purify}) {
    VisualEffect effect;effect.type=type;effect.start={};effect.end={2,0};effect.stagedSpell=true;
    effect.contactSeconds=AnimationTiming.spellContactSeconds;effect.lifetime=effect.contactSeconds+.42f;
    const std::array<std::pair<float,SpellStage>,5> samples{{
      {effect.contactSeconds*.2f,SpellStage::Preparation},{effect.contactSeconds*.525f,SpellStage::Launch},
      {effect.contactSeconds*.8f,SpellStage::Travel},{effect.contactSeconds+.42f*.2f,SpellStage::Impact},
      {effect.contactSeconds+.42f*.85f,SpellStage::Fade}}};
    for(const auto &[time,stage]:samples) {
      effect.elapsed=time;const auto sample=SampleSpell(effect);
      Check(sample.stage==stage,"El muestreador recorre preparación, lanzamiento, viaje, impacto y desvanecimiento en orden");
      Check(sample.phase>=0&&sample.phase<=1&&sample.travel>=0&&sample.travel<=1&&sample.impact>=0&&sample.impact<=1,"Las fases mágicas permanecen normalizadas");
      Check(sample.projectile==(type==EffectType::Fire||type==EffectType::Ice),"Solo Fire e Ice comunican un proyectil en trayectoria");
    }
    effect.elapsed=effect.contactSeconds-.0001f;Check(SampleSpell(effect).stage==SpellStage::Travel,"La magia no impacta antes del contacto existente");
    effect.elapsed=effect.contactSeconds;Check(SampleSpell(effect).stage==SpellStage::Impact,"El impacto empieza exactamente en el contacto existente");
    effect.elapsed=effect.lifetime;Check(!effect.Active(),"Una magia termina en su límite sin partículas huérfanas");
  }
  for(Ability ability:{Ability::Attack,Ability::Fire,Ability::Ice,Ability::Lightning}) {
    auto match=Board();const int caster=Add(match,0,UnitClass::Caster,{});Add(match,1,UnitClass::Saber,{2,0});
    Ok(match.BeginActivation(caster));AnimationDirector director;director.Reset(match);const Match before=match;
    Ok(match.UseAbility(caster,ability,{2,0}));const auto committed=LogicalFingerprint(match);director.Observe(before,match,ability);
    const auto spell=std::ranges::find_if(director.Effects(),[](const VisualEffect &effect){return effect.stagedSpell;});
    const auto damage=std::ranges::find_if(director.Effects(),[](const VisualEffect &effect){return effect.type==EffectType::Damage;});
    Check(spell!=director.Effects().end()&&damage!=director.Effects().end(),"Cada golpe mágico real conserva un único efecto y su aviso de daño");
    Near(spell->delay+spell->contactSeconds,damage->delay,"La preparación visual conserva el instante del contacto del aviso de daño");
    Near(spell->delay+spell->lifetime,damage->delay+.42f,"La magia conserva el final anterior y no prolonga el bloqueo de entradas");
    const auto count=director.Effects().size();director.Observe(match,match);
    Check(director.Effects().size()==count,"Observar otra vez el mismo golpe no duplica magia ni impactos");
    director.Update(3.f);Check(director.Effects().empty()&&!director.Busy(),"La cola de magia se vacía y devuelve el control táctico");
    Check(LogicalFingerprint(match)==committed,"Las etapas visuales no vuelven a aplicar el daño ya resuelto");
  }
}
void NewFireWaitsForContactWithoutHidingExistingFire() {
  for(bool previousFire:{false,true}) {
    auto match=Board();const int caster=Add(match,0,UnitClass::Caster,{});const Hex target{2,0};Add(match,1,UnitClass::Saber,target);
    int existingId=-1;
    if(previousFire) {
      existingId=match.state.nextId++;
      match.state.fires.push_back({existingId,target,1,0});
    }
    Ok(match.BeginActivation(caster));AnimationDirector director;director.Reset(match);const Match before=match;
    Ok(match.UseAbility(caster,Ability::Fire,target));const auto committed=LogicalFingerprint(match);director.Observe(before,match,Ability::Fire);
    const auto spell=std::ranges::find_if(director.Effects(),[](const VisualEffect &effect){return effect.stagedSpell&&effect.type==EffectType::Fire;});
    const auto damage=std::ranges::find_if(director.Effects(),[](const VisualEffect &effect){return effect.type==EffectType::Damage;});
    Check(spell!=director.Effects().end()&&damage!=director.Effects().end(),"Fire conserva su único proyectil primario y el aviso de daño");
    const float contact=spell->delay+spell->contactSeconds;
    Near(contact,damage->delay,"El incendio nuevo conserva el contacto existente del impacto Fire");
    Near(spell->delay+spell->lifetime,damage->delay+.42f,"Ocultar el suelo incendiado no prolonga el efecto Fire");
    std::size_t created=0;
    for(const auto &fire:match.state.fires) {
      const bool existed=std::ranges::any_of(before.state.fires,[&](const FireZone &old){return old.id==fire.id;});
      Check(FireAwaitsContact(fire,director.Effects())==!existed,"Solo los incendios recién creados esperan al contacto, también en el área del hechizo");
      created+=!existed;
      if(fire.id==existingId) Check(fire.remaining==match.config.fireDuration,"Refrescar un incendio conserva su identidad y su representación visible");
    }
    Check(created==spell->createdFireIds.size()&&created>0,"El efecto primario registra cada zona nueva real una sola vez");
    director.Update(contact-.001f);
    for(const auto &fire:match.state.fires) Check(FireAwaitsContact(fire,director.Effects())==(fire.id!=existingId),"El suelo nuevo no arde visualmente antes del contacto");
    director.Update(.002f);
    for(const auto &fire:match.state.fires) Check(!FireAwaitsContact(fire,director.Effects()),"Al llegar el impacto se muestran todas las zonas incendiadas reales");
    Check(LogicalFingerprint(match)==committed,"Esperar el contacto del incendio no vuelve a resolver daño ni cambia la partida");
    director.Update(3.f);Check(!director.Busy()&&director.Effects().empty(),"La señal de incendio nuevo se descarta con su efecto finito");
  }
  auto match=Board();const int caster=Add(match,0,UnitClass::Caster,{});Add(match,1,UnitClass::Saber,{3,0});
  Ok(match.BeginActivation(caster));AnimationDirector director;director.Reset(match);const Match beforeMove=match;
  Ok(match.Move(caster,{1,0}));director.Observe(beforeMove,match);const Match beforeFire=match;
  Ok(match.UseAbility(caster,Ability::Fire,{3,0}));director.Observe(beforeFire,match,Ability::Fire);
  const auto spell=std::ranges::find_if(director.Effects(),[](const VisualEffect &effect){return effect.stagedSpell&&effect.type==EffectType::Fire;});
  Check(spell!=director.Effects().end()&&spell->delay>0.f,"El Fire posterior a un movimiento conserva su espera real en la cola");
  for(const auto &fire:match.state.fires) Check(FireAwaitsContact(fire,director.Effects()),"La zona nueva permanece oculta también durante la espera anterior a la preparación");
}
void NonCasterSupportRetainsOriginalEffects() {
  for(UnitClass type:{UnitClass::Saber,UnitClass::Caster}) for(bool purifier:{false,true}) {
    if(type==UnitClass::Caster&&!purifier) continue;
    auto match=Board();const int saber=Add(match,0,type,{});Add(match,1,UnitClass::Saber,{4,0});const Hex target{1,0};
    if(purifier) {
      match.state.sources.push_back({match.state.nextId++,target,4});
      match.Find(saber)->inventory.push_back(Item::Purifier);
    } else match.state.corpses.push_back({match.state.nextId++,target,match.state.round,1,UnitClass::Saber,-1});
    Ok(match.BeginActivation(saber));AnimationDirector director;director.Reset(match);const Match before=match;
    if(purifier) Ok(match.UseItem(saber,0,target));
    else Ok(match.UseAbility(saber,Ability::Incinerate,target));
    const auto committed=LogicalFingerprint(match);director.Observe(before,match);
    const auto type=purifier?EffectType::Purify:EffectType::Fire;
    const auto effect=std::ranges::find_if(director.Effects(),[&](const VisualEffect &value){return value.type==type;});
    Check(effect!=director.Effects().end()&&!effect->stagedSpell,"El kit de cualquier clase y la incineración física conservan su efecto original sin preparación de Caster");
    Near(effect->contactSeconds,0.f,"El soporte no mágico no añade un contacto nuevo");Near(effect->lifetime,.65f,"El soporte no mágico conserva su duración anterior");
    Check(effect->createdFireIds.empty(),"Incinerar un cadáver no inventa zonas de incendio persistente");
    director.Update(3.f);Check(!director.Busy()&&LogicalFingerprint(match)==committed,"El soporte visual termina sin repetir el uso de objeto o habilidad");
  }
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
  director.Update(AnimationTiming.walkCellSeconds*.5F);visual=director.VisualFor(id,*match.Find(id));
  Near(visual.q,static_cast<float>(path->cells[1].q)*0.5F,"Half-step Q interpolation");
  Near(visual.r,static_cast<float>(path->cells[1].r)*0.5F,"Half-step R interpolation");
  Check(visual.frame==AnimationTiming.frames/2,"Walk cycles a distinct animation frame");
  director.Update(static_cast<float>(path->cells.size()-1)*AnimationTiming.walkCellSeconds);visual=director.VisualFor(id,*match.Find(id));
  Check(visual.state==AnimState::Idle,"Walk finishes in idle");Near(visual.q,2,"Walk reaches committed Q");Near(visual.elevation,2,"Walk interpolates and reaches destination elevation");
  Check(LogicalFingerprint(match)==afterFingerprint,"Animation elapsed time does not advance rules or turns");
}
void CompleteWalkCyclePerCell() {
  Near(AnimationTiming.walkCellSeconds,AnimationTiming.frames*AnimationTiming.walkFrameSeconds,"Una casilla dura exactamente un ciclo de marcha");
  const std::array<Hex,4> destinations{{{1,0},{-1,1},{1,-1},{-1,0}}};
  for(int owner=0;owner<2;++owner)for(int cls=0;cls<7;++cls)for(int facing=0;facing<4;++facing) {
    auto match=Board();match.state.currentPlayer=owner;
    const int id=Add(match,owner,static_cast<UnitClass>(cls),{});const auto destination=destinations[static_cast<std::size_t>(facing)];
    match.At(destination)->height=1;Ok(match.BeginActivation(id));const Match before=match;
    AnimationDirector director;director.Reset(before);Ok(match.Move(id,destination));director.Observe(before,match);
    const auto committed=LogicalFingerprint(match);std::set<int> frames;float elapsed=0.F;
    for(int frame=0;frame<AnimationTiming.frames;++frame) {
      const float target=(static_cast<float>(frame)+.5F)*AnimationTiming.walkFrameSeconds;
      director.Update(target-elapsed);elapsed=target;const auto visual=director.VisualFor(id,*match.Find(id));
      const float fraction=target/AnimationTiming.walkCellSeconds;frames.insert(visual.frame);
      Check(visual.state==AnimState::Walk&&visual.frame==frame&&visual.facing==facing,"Cada marcha de una casilla muestra sus ocho marcos en orden y orientación");
      Near(visual.q,destination.q*fraction,"La marcha avanza Q durante todos sus marcos");
      Near(visual.r,destination.r*fraction,"La marcha avanza R durante todos sus marcos");
      Near(visual.elevation,fraction,"La marcha sube de forma continua mientras reproduce el ciclo");
      Check(director.Busy()&&LogicalFingerprint(match)==committed,"La marcha bloquea solo su presentación y conserva la partida ya resuelta");
    }
    Check(frames.size()==static_cast<std::size_t>(AnimationTiming.frames),"No se omite ningún marco al recorrer una sola casilla");
    director.Update(AnimationTiming.walkCellSeconds-.001F-elapsed);const auto arriving=director.VisualFor(id,*match.Find(id));
    Check(arriving.state==AnimState::Walk&&arriving.frame==AnimationTiming.frames-1,"El último marco permanece visible hasta llegar");
    director.Update(.002F);const auto idle=director.VisualFor(id,*match.Find(id));
    Check(idle.state==AnimState::Idle&&idle.frame==0&&idle.facing==facing&&!director.Busy(),"La llegada inicia idle sin una marcha residual");
    Near(idle.q,static_cast<float>(destination.q),"La llegada conserva Q exacta");Near(idle.r,static_cast<float>(destination.r),"La llegada conserva R exacta");
    Near(idle.elevation,1.F,"La llegada conserva la altura final");Near(idle.elapsed,.001F,"Idle recibe únicamente el tiempo sobrante de la llegada");
    Camera camera;const auto a=camera.ProjectContinuous(arriving.q,arriving.r,arriving.elevation),b=camera.ProjectContinuous(idle.q,idle.r,idle.elevation);
    Check(std::hypot(b.x-a.x,b.y-a.y)<.2F,"El último intervalo llega a idle sin salto del anclaje proyectado");
    director.Update(AnimationTiming.idleFrameSeconds);
    Check(director.VisualFor(id,*match.Find(id)).frame==1&&LogicalFingerprint(match)==committed,"Idle continúa su reloj en el destino sin repetir la orden");
  }
}
void FasterWalkPreservesCadenceAndOtherStates() {
  Near(AnimationTiming.walkFrameSeconds,.05F*.70F,"Walk reduce un treinta por ciento el intervalo anterior de cada marco",.000001F);
  Near(AnimationTiming.walkCellSeconds,.40F*.70F,"Walk reduce un treinta por ciento la duración completa de una casilla",.000001F);
  const std::array<std::pair<float,float>,8> unchanged{{
    {AnimationTiming.idleFrameSeconds,.18F},{AnimationTiming.attackSeconds,.45F},{AnimationTiming.hurtSeconds,.30F},{AnimationTiming.deathSeconds,.65F},
    {AnimationTiming.castSeconds,.60F},{AnimationTiming.weaponContactSeconds,.18F},{AnimationTiming.spellContactSeconds,.225F},{AnimationTiming.chainIntervalSeconds,.18F}
  }};
  for(const auto [actual,expected]:unchanged)Near(actual,expected,"Acelerar Walk conserva los intervalos de idle, combate, contactos y cadena",.000001F);
  auto match=Board();const int id=Add(match,0,UnitClass::Saber,{});Ok(match.BeginActivation(id));const Match before=match;
  AnimationDirector director;director.Reset(before);Ok(match.Move(id,{1,0}));director.Observe(before,match);
  const auto committed=LogicalFingerprint(match);const std::array<float,5> deltas{{.013F,.030F,.021F,.029F,.017F}};
  std::set<int> frames{0};float elapsed=0.F,previousQ=0.F,lastDelta=0.F;int previousFrame=0,samples=0;
  while(director.Busy()&&samples<64) {
    lastDelta=deltas[static_cast<std::size_t>(samples)%deltas.size()];director.Update(lastDelta);elapsed+=lastDelta;++samples;
    const auto visual=director.VisualFor(id,*match.Find(id));
    Near(visual.q,std::min(elapsed/AnimationTiming.walkCellSeconds,1.F),"La posición sigue el mismo reloj acelerado con deltas variables del motor");
    Near(visual.r,0.F,"Acelerar la marcha no desvía su ruta");Near(visual.elevation,0.F,"Acelerar la marcha no altera la altura");
    Check(visual.q>=previousQ,"La marcha acelerada no retrocede ni salta antes de llegar");previousQ=visual.q;
    if(visual.state==AnimState::Walk) {
      frames.insert(visual.frame);Check(visual.frame>=previousFrame&&visual.frame<=previousFrame+1,"Los deltas de hasta treinta milisegundos no omiten marcos de Walk");previousFrame=visual.frame;
      Check(elapsed<AnimationTiming.walkCellSeconds&&director.Busy(),"Walk mantiene el bloqueo visual hasta completar la llegada acelerada");
    } else Check(visual.state==AnimState::Idle&&!director.Busy(),"La llegada acelerada libera entradas únicamente al entrar en idle");
    Check(LogicalFingerprint(match)==committed,"El nuevo ritmo visual no vuelve a ejecutar la acción táctica");
  }
  Check(samples<64&&frames.size()==static_cast<std::size_t>(AnimationTiming.frames),"La marcha acelerada reproduce sus ocho marcos bajo la cadencia real del motor");
  Check(elapsed>=AnimationTiming.walkCellSeconds&&elapsed-lastDelta<AnimationTiming.walkCellSeconds,"La transición a idle sucede en el primer delta que cruza la llegada");
  const auto idle=director.VisualFor(id,*match.Find(id));Near(idle.q,1.F,"La marcha acelerada conserva el destino exacto");
  Near(idle.elapsed,elapsed-AnimationTiming.walkCellSeconds,"Idle recibe solo el sobrante del último delta acelerado",.00001F);
}
void MultiSegmentWalkContinuity() {
  auto match=Board();const std::vector<Hex> route{{0,0},{0,1},{1,1},{2,0}};
  for(auto &cell:match.state.cells)cell.terrain=Terrain::Blocked;
  for(const auto pos:route)match.At(pos)->terrain=Terrain::Plain;
  match.At({1,1})->height=1;match.At({2,0})->height=1;
  const int id=Add(match,0,UnitClass::Rider,{});Ok(match.BeginActivation(id));const Match before=match;
  const auto path=before.FindPath(id,route.back());Check(path&&path->cells==route,"La ruta legal de prueba obliga a cambiar de dirección y altura");
  AnimationDirector director;director.Reset(before);Ok(match.Move(id,route.back()));director.Observe(before,match);
  const auto committed=LogicalFingerprint(match);const int cost=path->cost,steps=match.Find(id)->stepsMoved;
  const std::array<int,3> expectedFacings{{0,0,2}};float elapsed=0.F;
  for(std::size_t segment=0;segment+1<route.size();++segment) {
    std::set<int> frames;const auto from=route[segment],to=route[segment+1];
    const float a=static_cast<float>(before.At(from)->height),b=static_cast<float>(before.At(to)->height);
    for(int frame=0;frame<AnimationTiming.frames;++frame) {
      const float fraction=(static_cast<float>(frame)+.5F)/static_cast<float>(AnimationTiming.frames);
      const float target=(static_cast<float>(segment)+fraction)*AnimationTiming.walkCellSeconds;
      director.Update(target-elapsed);elapsed=target;const auto visual=director.VisualFor(id,*match.Find(id));frames.insert(visual.frame);
      Check(visual.state==AnimState::Walk&&visual.frame==frame&&visual.facing==expectedFacings[segment],"Cada tramo completa el ciclo y toma la orientación del siguiente vecino");
      Near(visual.q,from.q+(to.q-from.q)*fraction,"Q sigue el tramo legal sin atajar el giro");
      Near(visual.r,from.r+(to.r-from.r)*fraction,"R sigue el tramo legal sin atajar el giro");
      Near(visual.elevation,a+(b-a)*fraction,"La altura se interpola entre los dos apoyos reales del tramo");
    }
    Check(frames.size()==static_cast<std::size_t>(AnimationTiming.frames),"Cada casilla de una ruta larga conserva los ocho marcos");
    const float boundary=static_cast<float>(segment+1)*AnimationTiming.walkCellSeconds;
    director.Update(boundary-.001F-elapsed);const auto left=director.VisualFor(id,*match.Find(id));
    director.Update(.002F);elapsed=boundary+.001F;const auto right=director.VisualFor(id,*match.Find(id));
    Check(std::abs(right.q-left.q)<.01F&&std::abs(right.r-left.r)<.01F&&std::abs(right.elevation-left.elevation)<.01F,"Cruzar el vértice de la ruta no salta posición ni altura");
  }
  const auto idle=director.VisualFor(id,*match.Find(id));Check(idle.state==AnimState::Idle&&idle.facing==2&&!director.Busy(),"La ruta termina en idle con la última orientación");
  Near(idle.q,2.F,"La ruta completa llega a Q");Near(idle.r,0.F,"La ruta completa llega a R");Near(idle.elevation,1.F,"La ruta completa llega a su altura");
  Check(match.Find(id)->stepsMoved==steps&&before.FindPath(id,route.back())->cost==cost&&LogicalFingerprint(match)==committed,"Reproducir la ruta no modifica pasos, coste ni estado táctico");
}
void MoveThenAttackContactUsesSharedTiming() {
  for(int cells:{1,2}) {
    auto match=Board();const int actor=Add(match,0,UnitClass::Saber,{}),target=Add(match,1,UnitClass::Archer,{cells+1,0});
    Ok(match.BeginActivation(actor));const Match before=match;const auto path=before.FindPath(actor,{cells,0});
    Check(path&&path->cells.size()==static_cast<std::size_t>(cells+1),"El movimiento previo al ataque tiene una ruta legal conocida");
    Ok(match.Move(actor,{cells,0}));Ok(match.UseAbility(actor,Ability::Attack,{cells+1,0}));
    AnimationDirector director;director.Reset(before);director.Observe(before,match,Ability::Attack);
    const auto committed=LogicalFingerprint(match);const float walk=static_cast<float>(path->cells.size()-1)*AnimationTiming.walkCellSeconds;
    const auto damage=std::ranges::find_if(director.Effects(),[&](const VisualEffect &effect){return effect.type==EffectType::Damage&&effect.end==match.Find(target)->pos;});
    Check(damage!=director.Effects().end(),"El ataque real crea una reacción visual de daño");
    Near(damage->delay,walk+AnimationTiming.weaponContactSeconds,"El contacto comparte el tiempo de marcha usado por la secuencia de audio");
    director.Update(walk-.001F);Check(director.VisualFor(actor,*match.Find(actor)).state==AnimState::Walk&&director.VisualFor(target,*match.Find(target)).state==AnimState::Idle,"Ni arma ni víctima anticipan la llegada de la marcha");
    director.Update(.002F);auto visual=director.VisualFor(actor,*match.Find(actor));
    Check(visual.state==AnimState::Attack&&visual.frame==0,"El ataque empieza después del último marco de marcha");
    Near(visual.q,static_cast<float>(cells),"El ataque se origina en la casilla final");Near(visual.elapsed,.001F,"El ataque recibe solo el sobrante de marcha");
    director.Update(AnimationTiming.weaponContactSeconds-.002F);Check(director.VisualFor(target,*match.Find(target)).state==AnimState::Idle,"La víctima espera al contacto configurado");
    director.Update(.002F);Check(director.VisualFor(target,*match.Find(target)).state==AnimState::Hurt,"La víctima reacciona al mismo contacto posterior a la llegada");
    director.Update(3.F);Check(!director.Busy()&&LogicalFingerprint(match)==committed,"La secuencia completa no repite movimiento, ataque ni resolución lógica");
  }
}
void UnitContactSupportAndSharedAnchor() {
  for(int cls=0;cls<7;++cls)for(const auto state:{AnimState::Idle,AnimState::Walk,AnimState::Attack,AnimState::Hurt,AnimState::Death,AnimState::Cast})for(int frame=0;frame<AnimationTiming.frames;++frame) {
    const auto type=static_cast<UnitClass>(cls);const auto window=UnitContactWindow(type,state,0,frame);
    Check(window.left>=0&&window.top>=0&&window.right<=48&&window.bottom<=64&&window.left<window.right&&window.top<window.bottom,"La ventana de apoyo pertenece a la celda nativa de cada estado");
    std::array<std::uint8_t,48*64> alpha{};Check(!UnitContactGeometry(type,state,0,frame,alpha).valid,"Un marco sin apoyo visible no inventa una sombra");
    const int left=window.left+2,right=window.right-5,bottom=window.bottom-2;
    for(int y=bottom-2;y<bottom;++y)for(int x=0;x<3;++x) {
      alpha[static_cast<std::size_t>(y*48+left+x)]=255;
      alpha[static_cast<std::size_t>(y*48+right+x)]=255;
    }
    const auto contact=UnitContactGeometry(type,state,0,frame,alpha);
    Check(contact.valid&&contact.left==left&&contact.right==right+3&&contact.bottom==bottom,"El contacto mide los dos apoyos opacos del marco");
    Check(contact.shadowTop==contact.bottom-1&&contact.shadowHeight>=2&&contact.shadowHeight<=3,"La sombra se solapa con la última fila de apoyo sin dejar un píxel transparente");
    Check(contact.shadowWidth<contact.right-contact.left&&contact.shadowLeft>=1&&contact.shadowLeft+contact.shadowWidth<=47,"La sombra compacta permanece dentro de la celda y debajo del cuerpo");
    std::array<std::uint8_t,48*64> mirrored{};
    for(int y=0;y<64;++y)for(int x=0;x<48;++x)mirrored[static_cast<std::size_t>(y*48+47-x)]=alpha[static_cast<std::size_t>(y*48+x)];
    const auto opposite=UnitContactGeometry(type,state,1,frame,mirrored);
    Check(opposite.valid&&opposite.left==48-contact.right&&opposite.right==48-contact.left&&opposite.bottom==contact.bottom,"El apoyo reflejado conserva los pies y su altura");
    Check(opposite.shadowLeft==48-contact.shadowLeft-contact.shadowWidth&&opposite.shadowWidth==contact.shadowWidth&&opposite.shadowTop==contact.shadowTop,"Reflejar orientación refleja exactamente el suelo de la sombra");
    alpha[63*48]=alpha[63*48+47]=255;
    alpha[static_cast<std::size_t>((window.top-1)*48+24)]=255;
    const auto armed=UnitContactGeometry(type,state,0,frame,alpha);
    Check(armed.left==contact.left&&armed.right==contact.right&&armed.bottom==contact.bottom&&armed.shadowLeft==contact.shadowLeft&&armed.shadowTop==contact.shadowTop&&armed.shadowWidth==contact.shadowWidth,"Un arma fuera de la ventana no arrastra la sombra hacia su extremo");
    Camera camera;camera.SetRotation(frame%6);camera.CenterOn({-1,2},1.F,{120.F,90.F});camera.PanBy({-13.F,17.F});
    for(int zoom=1;zoom<=3;++zoom) {
      camera.SetZoom(zoom,{121.F,89.F});const auto projected=camera.ProjectContinuous(-1.375F,2.625F,1.25F),point=SnapUnitPoint(projected);
      Near(point.x,std::round(projected.x),"El anclaje compartido redondea X una sola vez");Near(point.y,std::round(projected.y),"El anclaje compartido redondea Y una sola vez");
      const float spriteBottom=point.y+(contact.bottom-56)*zoom,shadowTop=point.y+(contact.shadowTop-56)*zoom;
      Near(spriteBottom-shadowTop,static_cast<float>(zoom),"La última fila del pie y el inicio de la sombra se solapan a todos los zoom");
      Near((point.x-14.F*zoom)-point.x,-14.F*zoom,"La insignia de suelo conserva el mismo anclaje redondeado durante cámara y movimiento");
      Check(std::abs(point.x-projected.x)<=.5F&&std::abs(point.y-projected.y)<=.5F,"El redondeo compartido no añade deriva subpíxel al sprite");
    }
  }
  std::array<std::uint8_t,48*64> alpha{};alpha[55*48+24]=255;
  Check(!UnitContactGeometry(static_cast<UnitClass>(-1),AnimState::Idle,0,0,alpha).valid,"Una clase inválida no accede a máscaras ajenas");
  Near(SnapUnitPoint({-1.5F,-1.49F}).x,-2.F,"El anclaje mantiene el redondeo nativo con coordenadas negativas");
  Near(SnapUnitPoint({-1.5F,-1.49F}).y,-1.F,"El anclaje negativo no introduce un desplazamiento entero extra");
}
void LowerBandSurfaceIntersection() {
  const auto tinySurface=[](Point center) {
    std::array<Point,6> surface{};
    for(std::size_t index=0;index<surface.size();++index)surface[index]={center.x+TopOffsets[index].x*.01F,center.y+TopOffsets[index].y*.01F};
    return surface;
  };
  const std::array<std::pair<Rect,bool>,9> cases{{
    {{-1,-1,2,2},true},{{-30,-1,60,2},true},{{-1,-30,2,60},true},{{-30,-30,60,60},true},
    {{19,-13,1,1},false},{{9,-15,1,1},false},{{-21,-1,1,2},false},{{20,-1,1,2},false},{{10,-14,1,1},true}
  }};
  for(int zoom=1;zoom<=3;++zoom)for(const auto origin:{Point{},Point{-53,29},Point{137,-81}}) {
    std::array<Point,6> surface{};
    for(std::size_t index=0;index<surface.size();++index)surface[index]={origin.x+TopOffsets[index].x*zoom,origin.y+TopOffsets[index].y*zoom};
    for(const auto &[rectangle,expected]:cases) {
      const Rect transformed{origin.x+rectangle.left*zoom,origin.y+rectangle.top*zoom,rectangle.width*zoom,rectangle.height*zoom};
      Check(UnitSurfaceTouchesRect(transformed,surface)==expected,"SAT distingue solapamiento real, borde sin área y esquina vacía del hexágono");
      auto reversed=surface;std::ranges::reverse(reversed);
      Check(UnitSurfaceTouchesRect(transformed,reversed)==expected,"La intersección convexa no depende del sentido de los vértices");
    }
  }
  Check(!UnitSurfaceTouchesRect({0,0,0,1},TopOffsets)&&!UnitSurfaceTouchesRect({0,0,1,-1},TopOffsets),"Un rectángulo sin superficie no altera la prioridad de dibujo");
  Check(!UnitSurfaceTouchesRect({std::numeric_limits<float>::quiet_NaN(),0,1,1},TopOffsets),"Una posición de rectángulo no finita no produce una intersección");
  auto invalidSurface=TopOffsets;invalidSurface[2].x=std::numeric_limits<float>::infinity();
  Check(!UnitSurfaceTouchesRect({0,0,1,1},invalidSurface),"Una superficie no finita no adelanta una unidad");

  std::array<std::uint8_t,48*64> alpha{};
  alpha[43*48+24]=255;alpha[44*48]=1;alpha[53*48+38]=255;alpha[63*48+47]=255;
  const auto band=UnitLowerBandGeometry(alpha);
  Check(band[0]==1&&band[9]==(std::uint64_t{1}<<38)&&band[19]==(std::uint64_t{1}<<47),"La banda incluye alfa tenue, punta del arma y última fila sin desplazar bits");
  int count=0;for(const auto row:band)count+=std::popcount(row);
  Check(count==3,"La máscara conserva solo las veinte filas inferiores y excluye la fila 43");
  const UnitLowerBand emptyBand{};const UnitContact noContact{};
  for(int zoom=1;zoom<=3;++zoom)for(const auto anchor:{Point{},Point{-17,29},Point{73,-41}}) {
    const auto at=[&](float x,float y) {return tinySurface({anchor.x+x*zoom,anchor.y+y*zoom});};
    Check(UnitLowerBandTouchesSurface(band,noContact,anchor,zoom,-1,at(14.5F,-2.5F)),"La banda detecta el píxel inferior de una espada fuera de la ventana de pies");
    Check(UnitLowerBandTouchesSurface(band,noContact,anchor,zoom,-1,at(-23.5F,-11.5F))&&UnitLowerBandTouchesSurface(band,noContact,anchor,zoom,-1,at(23.5F,7.5F)),"La banda incluye los extremos izquierdo y derecho a cualquier escala");
    Check(!UnitLowerBandTouchesSurface(band,noContact,anchor,zoom,-1,at(15.5F,-2.5F)),"Un hueco transparente junto al arma no cuenta como apoyo inferior");
    Check(!UnitLowerBandTouchesSurface(emptyBand,noContact,anchor,zoom,-1,at(.5F,.5F)),"Una banda vacía sin sombra ni marca no intersecta el suelo");
    Check(UnitLowerBandTouchesSurface(emptyBand,noContact,anchor,zoom,0,at(-12.5F,3.5F))&&!UnitLowerBandTouchesSurface(emptyBand,noContact,anchor,zoom,1,at(-12.5F,3.5F)),"La cruz azul conserva su barra vertical independiente del cuerpo");
    Check(!UnitLowerBandTouchesSurface(emptyBand,noContact,anchor,zoom,0,at(-13.5F,3.5F))&&UnitLowerBandTouchesSurface(emptyBand,noContact,anchor,zoom,1,at(-13.5F,3.5F)),"Las dos barras rojas usan su propia ocupación de suelo");
    UnitContact shadow;shadow.valid=true;shadow.shadowLeft=18;shadow.shadowTop=55;shadow.shadowWidth=12;shadow.shadowHeight=2;
    Check(UnitLowerBandTouchesSurface(emptyBand,shadow,anchor,zoom,-1,at(-5.5F,-.5F)),"La primera fila real de sombra participa en la intersección");
    Check(!UnitLowerBandTouchesSurface(emptyBand,shadow,anchor,zoom,-1,at(-5.5F,.5F))&&UnitLowerBandTouchesSurface(emptyBand,shadow,anchor,zoom,-1,at(-3.5F,.5F)),"El borde suave de sombra conserva sus dos píxeles de recorte");
    shadow.shadowHeight=3;
    Check(!UnitLowerBandTouchesSurface(emptyBand,shadow,anchor,zoom,-1,at(-5.5F,-.5F))&&UnitLowerBandTouchesSurface(emptyBand,shadow,anchor,zoom,-1,at(-5.5F,.5F)),"La sombra de Rider distingue la fila inicial estrecha y la fila central completa");
  }
  Check(!UnitLowerBandTouchesSurface(band,noContact,{},0,-1,TopOffsets)&&!UnitLowerBandTouchesSurface(band,noContact,{},-1,-1,TopOffsets),"Una escala inválida no activa la banda inferior");
  Check(!UnitLowerBandTouchesSurface(band,noContact,{std::numeric_limits<float>::quiet_NaN(),0},1,-1,TopOffsets),"Un anclaje no finito no altera el orden de dibujo");
}
void WalkLowerBandProtectsDestinationBeforeProps() {
  std::array<std::uint8_t,48*64> alpha{};alpha[55*48+24]=255;
  const auto contact=UnitContactGeometry(UnitClass::Saber,AnimState::Walk,0,1,alpha);
  const auto feetOnly=UnitLowerBandGeometry(alpha);alpha[53*48+38]=255;const auto withSword=UnitLowerBandGeometry(alpha);
  for(int zoom=1;zoom<=3;++zoom) {
    auto match=Board();const int id=Add(match,0,UnitClass::Saber,{});Ok(match.BeginActivation(id));const Match before=match;
    AnimationDirector director;director.Reset(before);Ok(match.Move(id,{1,0}));director.Observe(before,match);
    Camera camera;camera.SetZoom(zoom,{});camera.CenterOn({},0.F,{});
    director.Update(AnimationTiming.walkFrameSeconds*.5F);auto visual=director.VisualFor(id,*match.Find(id));
    auto point=SnapUnitPoint(camera.ProjectContinuous(visual.q,visual.r,visual.elevation));
    Check(!UnitLowerBandTouchesSurface(withSword,contact,point,zoom,0,camera.Surface({1,0})),"El destino no adelanta el dibujo antes de tocar una pieza real del marco");
    director.Update(AnimationTiming.walkFrameSeconds);visual=director.VisualFor(id,*match.Find(id));point=SnapUnitPoint(camera.ProjectContinuous(visual.q,visual.r,visual.elevation));
    Check(visual.state==AnimState::Walk&&visual.frame==1&&visual.path.size()==2,"El caso de regresión reproduce el segundo marco de una marcha real");
    Check(!UnitLowerBandTouchesSurface(feetOnly,contact,point,zoom,0,camera.Surface({1,0})),"Los pies todavía no alcanzan la superficie que cortaba la punta de la espada");
    Check(UnitLowerBandTouchesSurface(withSword,contact,point,zoom,0,camera.Surface({1,0})),"La espada inferior detecta la oclusión antes que los pies y la sombra");
    Check(!UnitLowerBandTouchesSurface(withSword,contact,point,zoom,0,camera.Surface({2,0})),"Una casilla posterior al tramo no adelanta la unidad a través del mapa");
    const float base=camera.DepthContinuous(visual.q,visual.r)+1.F,ground=camera.Depth({1,0});
    Check(base<ground&&ground+.25F<ground+.5F,"El ajuste del tramo queda encima del suelo y debajo de sus props");
    director.Update(AnimationTiming.walkCellSeconds);const auto idle=director.VisualFor(id,*match.Find(id));
    Check(idle.state==AnimState::Idle&&idle.path.empty()&&idle.heights.empty(),"Al llegar idle descarta los extremos que podían aplicar la corrección exclusiva de Walk");
    Near(camera.DepthContinuous(idle.q,idle.r)+1.F,ground+1.F,"Idle conserva la profundidad ordinaria de su casilla final");
  }
}
void WalkBadgeTouchesNeighborWithoutCrossingHigherFloor() {
  auto match=Board();const int id=Add(match,0,UnitClass::Saber,{});Ok(match.BeginActivation(id));const Match before=match;
  AnimationDirector director;director.Reset(before);Ok(match.Move(id,{1,0}));director.Observe(before,match);
  director.Update(AnimationTiming.walkFrameSeconds*5.5F);const auto walking=director.VisualFor(id,*match.Find(id));
  Check(walking.state==AnimState::Walk&&walking.frame==5,"La regresión reproduce el marco cinco del movimiento que falló en el renderer");
  Near(walking.q,.6875F,"La marca del caso real avanza hasta Q .6875");
  const UnitLowerBand emptyBand{};const UnitContact noContact{};
  for(int zoom=1;zoom<=3;++zoom) {
    Camera camera;camera.SetZoom(zoom,{});camera.CenterOn({},0.F,{});
    const auto point=SnapUnitPoint(camera.ProjectContinuous(walking.q,walking.r,walking.elevation));
    const auto neighbor=camera.Surface({0,1});
    Check(UnitLowerBandTouchesSurface(emptyBand,noContact,point,zoom,0,neighbor),"La cruz de suelo toca el vecino ajeno a los extremos del tramo");
    Check(UnitLowerBandTouchesSurface(emptyBand,noContact,point,zoom,1,neighbor),"Las barras del otro bando también participan en el cruce del vecino");
    Check(!UnitLowerBandTouchesSurface(emptyBand,noContact,point,zoom,-1,neighbor),"El fallo de este caso procede de la marca de suelo y no de alfa corporal inventada");
    const Rect envelope{point.x-24.F*zoom,point.y-12.F*zoom,48.F*zoom,20.F*zoom};
    Check(UnitSurfaceTouchesRect(envelope,neighbor),"La preselección de la banda completa conserva el vecino tocado por la marca");
    const auto raised=camera.Surface({0,1},1.F);
    Check(UnitSurfaceTouchesRect(envelope,raised)&&UnitLowerBandTouchesSurface(emptyBand,noContact,point,zoom,0,raised),"Un piso vecino alto puede intersectar la misma marca y requiere el límite de altura");
    Check(camera.Depth({0,1})>camera.Depth({1,0})+.25F,"Los extremos del tramo no bastaban para proteger la marca ante este vecino");
    if(zoom==1) {
      Near(point.x,21.F,"El anclaje nativo del fallo real se redondea a X21");Near(point.y,10.F,"El anclaje nativo del fallo real se redondea a Y10");
      Check(UnitSurfaceTouchesRect({7,14,1,1},neighbor),"El píxel real 10,60 del atlas ocupa el suelo vecino en pantalla7,14");
    }
  }
  Check(UnitWalkSurfaceEligible(walking,{0,1},0.F,AnimationTiming.walkCellSeconds),"Un vecino de igual altura puede proteger el marcador durante Walk");
  Check(!UnitWalkSurfaceEligible(walking,{0,1},1.F,AnimationTiming.walkCellSeconds),"Un piso alto fuera de la ruta conserva la oclusión y no adelanta la unidad");
  Check(UnitWalkSurfaceEligible(walking,{0,0},3.F,AnimationTiming.walkCellSeconds)&&UnitWalkSurfaceEligible(walking,{1,0},3.F,AnimationTiming.walkCellSeconds),"Los extremos reales permiten interpolar subidas sin excluir el piso de llegada");
  for(const auto state:{AnimState::Idle,AnimState::Attack,AnimState::Hurt,AnimState::Death,AnimState::Cast}) {
    auto other=walking;other.state=state;
    Check(!UnitWalkSurfaceEligible(other,{0,1},0.F,AnimationTiming.walkCellSeconds)&&!UnitWalkSurfaceEligible(other,{1,0},3.F,AnimationTiming.walkCellSeconds),"La prioridad adicional de banda inferior se limita a Walk incluso si conserva una ruta antigua");
  }
  auto routed=walking;routed.path={{0,0},{0,1},{1,1},{2,0}};routed.elapsed=AnimationTiming.walkCellSeconds*1.5F;
  Check(UnitWalkSurfaceEligible(routed,{0,1},3.F,AnimationTiming.walkCellSeconds)&&UnitWalkSurfaceEligible(routed,{1,1},3.F,AnimationTiming.walkCellSeconds),"La excepción de altura sigue los dos extremos del tramo activo de una ruta larga");
  Check(!UnitWalkSurfaceEligible(routed,{0,0},3.F,AnimationTiming.walkCellSeconds)&&!UnitWalkSurfaceEligible(routed,{2,0},3.F,AnimationTiming.walkCellSeconds),"Un extremo anterior o futuro no permite atravesar un piso alto");
  routed.elapsed=AnimationTiming.walkCellSeconds;
  Check(!UnitWalkSurfaceEligible(routed,{0,0},3.F,AnimationTiming.walkCellSeconds)&&UnitWalkSurfaceEligible(routed,{1,1},3.F,AnimationTiming.walkCellSeconds),"La frontera exacta entre casillas cambia los extremos de la excepción");
  auto invalid=walking;invalid.path.resize(1);Check(!UnitWalkSurfaceEligible(invalid,{0,0},0.F,AnimationTiming.walkCellSeconds),"Una marcha sin segmento no eleva su prioridad");
  Check(!UnitWalkSurfaceEligible(walking,{0,1},0.F,0.F)&&!UnitWalkSurfaceEligible(walking,{0,1},0.F,-1.F),"Una duración de casilla inválida no divide el tramo visual");
  Check(!UnitWalkSurfaceEligible(walking,{0,1},std::numeric_limits<float>::quiet_NaN(),AnimationTiming.walkCellSeconds),"Un piso con altura no finita no afecta la profundidad");
  director.Update(AnimationTiming.walkCellSeconds);const auto idle=director.VisualFor(id,*match.Find(id));
  Check(idle.state==AnimState::Idle&&!UnitWalkSurfaceEligible(idle,{0,1},0.F,AnimationTiming.walkCellSeconds),"La llegada a idle desactiva la protección de vecinos de Walk");
}
void MoveAttackQueue() {
  auto match=Board();const int actor=Add(match,0,UnitClass::Saber,{}),target=Add(match,1,UnitClass::Saber,{3,0});
  Ok(match.BeginActivation(actor));AnimationDirector director;director.Reset(match);
  Match before=match;Ok(match.Move(actor,{2,0}));director.Observe(before,match);
  before=match;Ok(match.UseAbility(actor,Ability::Attack,{3,0}));director.Observe(before,match,Ability::Attack);
  Check(director.VisualFor(actor,*match.Find(actor)).state==AnimState::Walk,"Attack queues behind unfinished walk");
  const float movementSeconds=2.F*AnimationTiming.walkCellSeconds;
  director.Update(movementSeconds+.01F);auto visual=director.VisualFor(actor,*match.Find(actor));
  Check(visual.state==AnimState::Attack,"Attack starts after movement completes");Near(visual.q,2,"Queued attack starts at destination");
  const float elapsed=visual.elapsed;director.Observe(match,match);Near(director.VisualFor(actor,*match.Find(actor)).elapsed,elapsed,"No-change Observe does not restart attack");
  director.Update(AnimationTiming.weaponContactSeconds);Check(director.VisualFor(target,*match.Find(target)).state==AnimState::Hurt,"Damage reaction follows queued attack contact pose");
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
    {"affine projection and inverse",AffineProjectionAndInverse},{"rotated large-map perspective and picking",RotatedLargeMapCamera},
    {"rotated movement and attack facing",RotatedUnitFacing},
    {"anchor zoom and fitted bounds",AnchorAndFit},
    {"elevated top picking and painter order",TopPicking},{"idle phase and immutable rules",IdleClockAndImmutability},
    {"ciclo idle temporal de todas las clases y orientaciones",IdleTemporalCycleAllClassesAndFacings},
    {"lluvia continua y acotada en distintos viewports",RainViewportMotionAndBounds},
    {"etapas mágicas con contacto y duración conservados",SpellStagesPreserveContactAndFiniteLifetime},
    {"incendio nuevo espera al contacto y conserva incendios previos",NewFireWaitsForContactWithoutHidingExistingFire},
    {"soporte no mágico conserva los efectos originales",NonCasterSupportRetainsOriginalEffects},
    {"weighted movement interpolation",LegalWalkInterpolation},{"ciclo completo de marcha por casilla y llegada continua",CompleteWalkCyclePerCell},
    {"marcha más ágil conserva cadencia y otros estados",FasterWalkPreservesCadenceAndOtherStates},
    {"marcha continua con giros y desnivel",MultiSegmentWalkContinuity},{"marcha y contacto comparten el reloj de audio",MoveThenAttackContactUsesSharedTiming},
    {"apoyos de pies y anclaje compartido de sombra e insignia",UnitContactSupportAndSharedAnchor},
    {"banda inferior e intersección convexa con el suelo",LowerBandSurfaceIntersection},
    {"marcha protege arma y pies sin adelantar props ni idle",WalkLowerBandProtectsDestinationBeforeProps},
    {"marca de marcha sobre vecino y límite de pisos altos",WalkBadgeTouchesNeighborWithoutCrossingHigherFloor},
    {"move attack damage queue",MoveAttackQueue},
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
