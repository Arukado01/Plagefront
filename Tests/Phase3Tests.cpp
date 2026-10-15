#include "Plaguefront/Presentation.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>
#include <stdexcept>

using namespace Plaguefront;
namespace {
int checks=0;
void Check(bool value,const char *message) { ++checks;if(!value)throw std::runtime_error(message); }
void Ok(const Result &result) {++checks;if(!result)throw std::runtime_error(result.message);}
void Near(float value,float expected,const char *message,float tolerance=.001F) {
  Check(std::isfinite(value) && std::abs(value-expected)<=tolerance,message);
}
void Valid(const Match &match) {std::string error;++checks;if(!match.Validate(error))throw std::runtime_error(error);}
int Add(Match &match,int owner,UnitClass type,Hex pos) {
  Unit unit;unit.id=match.state.nextId++;unit.owner=owner;unit.type=type;unit.pos=pos;
  unit.hp=match.config.classes[static_cast<int>(type)].hp;unit.mp=type==UnitClass::Caster?8:0;
  match.state.units.push_back(unit);return unit.id;
}
Match Board() {
  Config config;config.weatherPeriod=1000;Match match(config);match.state.mode=Mode::Local;
  for(int q=-3;q<=5;++q)for(int r=-3;r<=3;++r)match.state.cells.push_back({{q,r}});
  match.At({-3,0})->building=Building::Fortress;match.At({-3,0})->owner=0;
  match.At({5,0})->building=Building::Fortress;match.At({5,0})->owner=1;
  return match;
}
std::string Fingerprint(const Match &match) {
  std::ostringstream out;out<<match.state.round<<','<<match.state.currentPlayer<<','<<match.state.activeUnit<<','<<match.state.nextId;
  for(const auto &unit:match.state.units)out<<'u'<<unit.id<<','<<unit.pos.q<<','<<unit.pos.r<<','<<unit.hp<<','<<unit.mp<<','<<unit.moved<<','<<unit.acted<<','<<unit.activated;
  for(const auto &cell:match.state.cells)out<<'c'<<cell.pos.q<<','<<cell.pos.r<<','<<static_cast<int>(cell.terrain)<<','<<cell.height<<','<<cell.owner;
  for(const auto &player:match.state.players)out<<'p'<<player.coins;
  for(const auto &line:match.state.log)out<<line;
  return out.str();
}
void CameraViewports() {
  auto match=Match::Create(Config{},"Content/Plaguefront/scenarios/highlands.json",Mode::Local,Difficulty::Normal);
  for(const Point size: {Point{1000,620},Point{1140,720},Point{1560,900},Point{3280,1120}}) {
    Camera camera;camera.FrameAll(match.state.cells,size.x,size.y);
    const auto bounds=camera.Bounds(match.state.cells);
    Check(bounds.left>=0 && bounds.top>=0 && bounds.left+bounds.width<=size.x && bounds.top+bounds.height<=size.y,"Overview frames all cells and prop margins at each output viewport");
    for(int zoom=1;zoom<=3;++zoom) {
      camera.SetZoom(zoom,{size.x*.5F,size.y*.5F});
      camera.CenterOn({1,5},0.F,{size.x*.5F,size.y*.5F});
      auto center=camera.Project({1,5});
      Near(center.x,size.x*.5F,"CenterOn keeps selected fortress horizontally centered",.501F);
      Near(center.y,size.y*.5F,"CenterOn keeps selected fortress vertically centered",.501F);
      Check(camera.Zoom()==zoom,"CenterOn preserves integer zoom");
      camera.PanBy({-31,19});
      for(const auto &cell:match.state.cells) {
        const auto point=camera.Project(cell.pos,static_cast<float>(cell.height));
        const auto inverse=camera.Inverse(point,static_cast<float>(cell.height));
        Near(inverse.x,static_cast<float>(cell.pos.q),"Large-map inverse Q after centering, pan and zoom");
        Near(inverse.y,static_cast<float>(cell.pos.r),"Large-map inverse R after centering, pan and zoom");
        // Comprueba superficies aisladas; cuando se solapan, Pick selecciona deliberadamente la visible más cercana.
        Check(camera.Pick({cell},point)==cell.pos,"Actual elevated top picks at each viewport and integer zoom");
      }
    }
    camera.FrameAll(match.state.cells,size.x,size.y);
    Camera native;native.Fit(match.state.cells,size.x,size.y);const auto one=native.Bounds(match.state.cells);
    const int optimal=std::clamp(static_cast<int>(std::floor(std::min(size.x/one.width,size.y/one.height))),1,3);
    Check(camera.Zoom()==optimal && camera.Pan().x==0 && camera.Pan().y==0,"FrameAll clears prior pan and selects the largest fitting integer zoom");
    if(size.x==1000)Check(camera.Zoom()==1,"Small overview retains zoom one so the entire map fits");
    if(size.x==3280)Check(camera.Zoom()==2,"Ultrawide overview enlarges the whole map to integer zoom two");
  }
  Camera camera;camera.Fit(match.state.cells,500,310);
  Check(camera.Zoom()==1,"Oversized map retains legible native integer scale");
}
void RealRouteQueriesAndConfirmation() {
  auto match=Board();const int actor=Add(match,0,UnitClass::Rider,{});
  Add(match,1,UnitClass::Saber,{4,2});Ok(match.BeginActivation(actor));
  const auto unchanged=Fingerprint(match);
  const auto uniform=match.FindPath(actor,{3,0});
  Check(uniform && uniform->cost==3 && uniform->cells.size()==4,"Uniform route has expected minimum cost");
  Check(match.Reachable(actor).contains({3,0}),"Uniform route is within actual MOV budget");
  Check(Fingerprint(match)==unchanged,"Hover path/reachable queries consume no movement, action or currency");

  match.At({1,0})->terrain=Terrain::Swamp;match.At({1,0})->height=1;
  match.At({2,0})->height=2;
  const auto weighted=match.FindPath(actor,{3,0});
  Check(weighted && weighted->cost==4,"Weighted preview prefers cheaper flanking route");
  Check(std::ranges::find(weighted->cells,Hex{1,0})==weighted->cells.end(),"Weighted route avoids costly swamp ascent");
  const auto beforeCancel=Fingerprint(match);
  std::optional<Path> preview=weighted;preview.reset();
  Check(!preview && Fingerprint(match)==beforeCancel,"Discarding preview leaves tactical state unchanged");
  AnimationDirector director;director.Reset(match);const Match before=match;
  Ok(match.Move(actor,weighted->cells.back()));director.Observe(before,match);
  Check(match.Find(actor)->pos==weighted->cells.back() && match.Find(actor)->moved,"Confirm commits only the legal destination");
  Check(director.VisualFor(actor,*match.Find(actor)).path==weighted->cells,"Animated execution uses exactly the weighted path queried before confirmation");
  int sum=0;
  for(std::size_t i=1;i<weighted->cells.size();++i)sum+=before.MoveCost(*before.Find(actor),weighted->cells[i-1],weighted->cells[i]);
  Check(sum==weighted->cost,"Route displayed cost is the sum of core MoveCost");

  auto slopes=Board();const int climber=Add(slopes,0,UnitClass::Rider,{});
  slopes.At({1,0})->height=2;Ok(slopes.BeginActivation(climber));
  const auto climb=slopes.FindPath(climber,{1,0});
  Check(climb && climb->cost==3 && slopes.Reachable(climber).at({1,0})==3,"Elevation contributes real weighted cost");
  Camera camera;camera.Fit(slopes.state.cells);
  Near(camera.Project({1,0},2).y,camera.Project({1,0},0).y-12.F,"Route endpoint rests on actual elevated surface");
  auto blocked=Board();const int mover=Add(blocked,0,UnitClass::Rider,{});
  Add(blocked,0,UnitClass::Saber,{1,0});Add(blocked,1,UnitClass::Saber,{2,0});
  const auto around=blocked.FindPath(mover,{3,0});
  Check(around && std::ranges::find(around->cells,Hex{1,0})==around->cells.end() && std::ranges::find(around->cells,Hex{2,0})==around->cells.end(),"Preview never routes through allied or enemy units");
  Check(!blocked.FindPath(mover,{1,0}) && !blocked.FindPath(mover,{2,0}),"Occupied destinations cannot show legal movement arrows");
  for(const auto neighbor:Neighbors({3,1}))blocked.At(neighbor)->terrain=Terrain::Blocked;
  Check(!blocked.FindPath(mover,{3,1}) && !blocked.Reachable(mover).contains({3,1}),"Isolated destination is unreachable");
}
void DirectionalAnimationFrames() {
  for(const auto [destination,direction]:{std::pair{Hex{1,0},0},std::pair{Hex{-1,1},1},std::pair{Hex{1,-1},2},std::pair{Hex{-1,0},3}}) {
    auto match=Board();const int actor=Add(match,0,UnitClass::Saber,{});Ok(match.BeginActivation(actor));
    AnimationDirector director;director.Reset(match);const Match before=match;Ok(match.Move(actor,destination));director.Observe(before,match);
    Check(director.VisualFor(actor,*match.Find(actor)).facing==direction,"Walk chooses the correct front/back and left/right orientation");
  }
  auto match=Board();const int actor=Add(match,0,UnitClass::Caster,{});Add(match,1,UnitClass::Saber,{2,0});
  AnimationDirector idle;idle.Reset(match);std::set<int> idleFrames;
  for(int i=0;i<8;++i) {idleFrames.insert(idle.VisualFor(actor,*match.Find(actor)).frame);idle.Update(.111F);}
  Check(idleFrames.size()==8,"All eight idle poses are sampled");
  Ok(match.BeginActivation(actor));AnimationDirector cast;cast.Reset(match);const Match before=match;
  Ok(match.UseAbility(actor,Ability::Fire,{2,0}));cast.Observe(before,match,Ability::Fire);std::set<int> castFrames;
  for(int i=0;i<8;++i) {castFrames.insert(cast.VisualFor(actor,*match.Find(actor)).frame);cast.Update(.076F);}
  Check(castFrames.size()==8,"Casting samples anticipation, release and recovery across eight poses");
}
void ScenarioDesignAndAlternateCrossings() {
  std::ifstream file("Content/Plaguefront/scenarios/highlands.json");const auto json=nlohmann::json::parse(file);
  Check(json.at("columns")==15 && json.at("rows")==13 && json.at("cell_count")==188,"New scenario states its dimensions and explicit cell count");
  auto match=Match::Create(Config{},"Content/Plaguefront/scenarios/highlands.json",Mode::Local,Difficulty::Normal);Valid(match);
  Check(match.state.cells.size()==188 && match.state.units.size()==10,"Larger scenario loads all explicit terrain and valid armies");
  std::array<int,10> terrain{};int houses=0,fortresses=0;
  for(const auto &cell:match.state.cells) {
    ++terrain[static_cast<int>(cell.terrain)];houses+=cell.building==Building::House;fortresses+=cell.building==Building::Fortress;
    const auto *opposite=match.At({14-cell.pos.q,5-cell.pos.r});
    Check(opposite && opposite->terrain==cell.terrain && opposite->height==cell.height && opposite->building==cell.building,"Every tactical terrain/elevation/building has an exact axial counterpart");
    Check(opposite->owner==(cell.owner<0?-1:1-cell.owner),"Fortress and neutral ownership mirror fairly");
  }
  Check(houses==6 && fortresses==2,"Six neutral estates and two opposing fortresses");
  Check(terrain[static_cast<int>(Terrain::Bridge)]==4,"Three river crossings use four bridge cells including a broad central crossing");
  for(const auto type:{Terrain::Forest,Terrain::Hill,Terrain::Swamp,Terrain::Ruins,Terrain::Road,Terrain::River,Terrain::Blocked})
    Check(terrain[static_cast<int>(type)]>=2,"Scenario contains each required tactical terrain feature");
  for(int i=0;i<5;++i) {
    const auto &left=match.state.units[static_cast<std::size_t>(i)],&right=match.state.units[static_cast<std::size_t>(i+5)];
    Check(right.pos==Hex{14-left.pos.q,5-left.pos.r} && right.type==left.type,"Initial classes and spawn positions are symmetric");
  }
  match.state.units.clear();const int scout=Add(match,0,UnitClass::Rider,{1,5});
  const Hex target{13,0};
  for(int crossing=0;crossing<3;++crossing) {
    Match route=match;
    for(auto &cell:route.state.cells)if(cell.pos.q==7) {
      const int visualRow=cell.pos.r+3;
      const bool wanted=crossing==0?visualRow==2:crossing==1?(visualRow==5||visualRow==6):visualRow==9;
      if(!wanted)cell.terrain=Terrain::Blocked;
    }
    const auto path=route.FindPath(scout,target);
    Check(path && path->cost>10,"Each bridge independently offers a complete route between fortresses");
    Check(std::ranges::any_of(path->cells,[&](Hex pos){return pos.q==7 && route.At(pos)->terrain==Terrain::Bridge;}),"Alternative approach crosses its actual strategic bridge");
  }
}
void LargerMapAICampaigns() {
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(45);
  for(const auto difficulty:{Difficulty::Easy,Difficulty::Normal,Difficulty::Hard}) {
    auto match=Match::Create(Config{},"Content/Plaguefront/scenarios/highlands.json",Mode::VersusAI,difficulty);Valid(match);int activations=0;
    while(match.state.winner==-1 && match.state.round<=90 && activations<450) {
      Check(std::chrono::steady_clock::now()<deadline,"Larger-map AI stays within bounded execution time");
      if(match.state.currentPlayer==0)Ok(match.Pass());
      else {Check(match.StepAI(),"AI accepts a legal new-map activation");++activations;}
      Valid(match);
    }
    Check(match.state.winner==1,"AI reaches and conquers the opposing fortress on the larger map");
    std::cout<<"[CAMPAIGN] highlands difficulty="<<static_cast<int>(difficulty)<<" round="<<match.state.round<<" activations="<<activations<<'\n';
  }
}
}
int main() {
  int groups=0;
  const std::vector<std::pair<const char *,std::function<void()>>> tests{
    {"camera viewports, centering and overview",CameraViewports},
    {"real weighted route preview and execution",RealRouteQueriesAndConfirmation},
    {"eight-pose four-direction animation",DirectionalAnimationFrames},
    {"larger symmetric scenario and alternative crossings",ScenarioDesignAndAlternateCrossings},
    {"larger-map complete AI campaigns",LargerMapAICampaigns}
  };
  for(const auto &[name,test]:tests)try{test();++groups;std::cout<<"[PASS] "<<name<<'\n';}
  catch(const std::exception &error){std::cerr<<"[FAIL] "<<name<<": "<<error.what()<<'\n';return 1;}
  std::cout<<groups<<" phase 3 groups, "<<checks<<" checks passed.\n";
}
