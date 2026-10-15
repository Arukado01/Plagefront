#include "Plaguefront/Presentation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <ranges>
#include <tuple>

namespace Plaguefront {
  namespace {
    Point RotateAxial(Point point, int turns) {
      for (int turn = 0; turn < turns; ++turn)
        point = {point.x + point.y, -point.x};
      return point;
    }
    Point Raw(float q, float r, float elevation) {
      return {ColumnStep * q, ColumnRise * q + RowStep * r - ElevationStep * elevation};
    }
    float Height(const Match &match, Hex pos) {
      const auto *cell = match.At(pos);
      return cell ? static_cast<float>(cell->height) : 0.F;
    }
    float Duration(AnimState state) {
      switch (state) {
      case AnimState::Walk:
        return AnimationTiming.walkCellSeconds;
      case AnimState::Attack:
        return AnimationTiming.attackSeconds;
      case AnimState::Hurt:
        return AnimationTiming.hurtSeconds;
      case AnimState::Death:
        return AnimationTiming.deathSeconds;
      case AnimState::Cast:
        return AnimationTiming.castSeconds;
      case AnimState::Idle:
        return 0.F;
      }
      return 0.F;
    }
    bool Magic(Ability ability) {
      return ability == Ability::Fire || ability == Ability::Ice || ability == Ability::Lightning ||
             ability == Ability::Heal || ability == Ability::Purify;
    }
    EffectType AttackEffect(UnitClass type, std::optional<Ability> ability) {
      if (ability) {
        switch (*ability) {
        case Ability::Fire:
        case Ability::Incinerate:
          return EffectType::Fire;
        case Ability::Ice:
          return EffectType::Ice;
        case Ability::Lightning:
          return EffectType::Lightning;
        case Ability::Heal:
          return EffectType::Heal;
        case Ability::Purify:
          return EffectType::Purify;
        case Ability::Smoke:
          return EffectType::Smoke;
        default:
          break;
        }
      }
      if (type == UnitClass::Archer)
        return EffectType::Arrow;
      if (type == UnitClass::Caster)
        return EffectType::Fire;
      return EffectType::Slash;
    }
    int Facing(Hex from, Hex to, int fallback = 0, int rotation = 0) {
      if (from == to)
        return fallback;
      const auto direction = RotateAxial({static_cast<float>(to.q - from.q), static_cast<float>(to.r - from.r)}, rotation);
      const auto delta = Raw(direction.x, direction.y, 0.F);
      const int side = delta.x == 0.F ? fallback % 2 : (delta.x < 0.F ? 1 : 0);
      return side + (delta.y < 0.F ? 2 : 0);
    }
    std::optional<Ability> LoggedAbility(const Match &before, const Match &after) {
      // Busca el sufijo conservado al recortar Log() para no interpretar un hechizo antiguo como una pista nueva.
      const auto &old = before.state.log;
      const auto &next = after.state.log;
      std::size_t preserved = std::min(old.size(), next.size());
      for (; preserved > 0; --preserved)
        if (std::equal(old.end() - static_cast<std::ptrdiff_t>(preserved), old.end(), next.begin()))
          break;
      for (std::size_t i = next.size(); i > preserved; --i) {
        const auto &line = after.state.log[i - 1];
        for (int a = 0; a <= static_cast<int>(Ability::Incinerate); ++a) {
          const auto ability = static_cast<Ability>(a);
          if (line.starts_with(Name(ability) + ":"))
            return ability;
        }
        if (line.starts_with("Heal recupera"))
          return Ability::Heal;
        if (line == "Contaminacion purificada.")
          return Ability::Purify;
        if (line == "Cadaver incinerado.")
          return Ability::Incinerate;
      }
      return {};
    }
    int EnvironmentalLoss(const Match &before, const Match &after, const Unit &unit) {
      if (before.state.round == after.state.round)
        return 0;
      int damage = before.RotDamageAt(unit.pos);
      if (damage == 0 && before.LargestSourceGroup() >= before.config.globalThreshold)
        damage = before.config.globalDamage;
      return damage + before.FireDamageAt(unit.pos);
    }
  } // espacio de nombres anónimo

  RotExposure RotPresentationAt(const Match &match, Hex pos) {
    RotExposure exposure;
    if (!match.At(pos))
      return exposure;
    for (const Source &source : match.state.sources)
      if (source.remaining > 0 && Distance(source.pos, pos) <= match.config.rotRadius)
        ++exposure.coveringSources;
    exposure.local = exposure.coveringSources > 0;
    exposure.global = match.state.globalRemaining > 0;
    exposure.damage = match.RotDamageAt(pos);
    return exposure;
  }

  float GroundDepth(Hex pos) { return ColumnRise * static_cast<float>(pos.q) + RowStep * static_cast<float>(pos.r); }
  std::vector<const Cell *> DrawOrder(const std::vector<Cell> &cells) {
    std::vector<const Cell *> result;
    result.reserve(cells.size());
    for (const auto &cell : cells)
      result.push_back(&cell);
    std::ranges::sort(result, [](const Cell *left, const Cell *right) {
      return std::tuple{GroundDepth(left->pos), left->pos.q, left->pos.r} <
             std::tuple{GroundDepth(right->pos), right->pos.q, right->pos.r};
    });
    return result;
  }
  bool InsideTop(Point point, const std::array<Point, 6> &polygon) {
    bool positive = false, negative = false;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
      const auto a = polygon[i], b = polygon[(i + 1) % polygon.size()];
      const float cross = (b.x - a.x) * (point.y - a.y) - (b.y - a.y) * (point.x - a.x);
      positive |= cross > 0.001F;
      negative |= cross < -0.001F;
      if (positive && negative)
        return false;
    }
    return std::isfinite(point.x) && std::isfinite(point.y);
  }
  void Camera::Fit(const std::vector<Cell> &cells, float width, float height) {
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.F || height <= 0.F)
      return;
    zoom_ = 1;
    pan_ = {};
    origin_ = {};
    const Rect bounds = Bounds(cells);
    origin_ = {std::round((width - bounds.width) * 0.5F - bounds.left),
               std::round((height - bounds.height) * 0.5F - bounds.top)};
  }
  void Camera::FrameAll(const std::vector<Cell> &cells, float width, float height) {
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.F || height <= 0.F)
      return;
    Fit(cells, width, height);
    if (cells.empty())
      return;
    const auto native = Bounds(cells);
    // La vista de conjunto elige el mayor zoom entero que contiene el mapa y los márgenes de sprites de Bounds.
    for (int candidate = 3; candidate >= 1; --candidate)
      if (native.width * candidate <= width && native.height * candidate <= height) {
        zoom_ = candidate;
        break;
      }
    pan_ = {};
    origin_ = {};
    const auto bounds = Bounds(cells);
    origin_ = {std::round((width - bounds.width) * 0.5F - bounds.left),
               std::round((height - bounds.height) * 0.5F - bounds.top)};
  }
  void Camera::CenterOn(Hex pos, float elevation, Point nativeCenter) {
    if (!std::isfinite(elevation) || !std::isfinite(nativeCenter.x) || !std::isfinite(nativeCenter.y))
      return;
    const auto point = Project(pos, elevation);
    PanBy({nativeCenter.x - point.x, nativeCenter.y - point.y});
  }
  Point Camera::Project(Hex pos, float elevation) const {
    return ProjectContinuous(static_cast<float>(pos.q), static_cast<float>(pos.r), elevation);
  }
  void Camera::SetRotation(int hexTurns) { rotation_ = (hexTurns % 6 + 6) % 6; }
  Point Camera::ProjectContinuous(float q, float r, float elevation) const {
    const auto rotated = RotateAxial({q, r}, rotation_);
    const auto raw = Raw(rotated.x, rotated.y, elevation);
    return {origin_.x + pan_.x + raw.x * static_cast<float>(zoom_),
            origin_.y + pan_.y + raw.y * static_cast<float>(zoom_)};
  }
  float Camera::Depth(Hex pos) const { return DepthContinuous(static_cast<float>(pos.q), static_cast<float>(pos.r)); }
  float Camera::DepthContinuous(float q, float r) const {
    const auto rotated = RotateAxial({q, r}, rotation_);
    return ColumnRise * rotated.x + RowStep * rotated.y;
  }
  Point Camera::Inverse(Point nativePoint, float elevation) const {
    const float x = (nativePoint.x - origin_.x - pan_.x) / static_cast<float>(zoom_);
    const float y = (nativePoint.y - origin_.y - pan_.y) / static_cast<float>(zoom_) + ElevationStep * elevation;
    const float q = x / ColumnStep;
    return RotateAxial({q, (y - ColumnRise * q) / RowStep}, (6 - rotation_) % 6);
  }
  std::array<Point, 6> Camera::Surface(Hex pos, float elevation) const {
    const auto center = Project(pos, elevation);
    auto polygon = TopOffsets;
    for (auto &point : polygon) {
      point.x = center.x + point.x * static_cast<float>(zoom_);
      point.y = center.y + point.y * static_cast<float>(zoom_);
    }
    return polygon;
  }
  std::optional<Hex> Camera::Pick(const std::vector<Cell> &cells, Point nativePoint) const {
    auto sorted = DrawOrder(cells);
    if (rotation_ != 0)
      std::ranges::sort(sorted, [this](const Cell *left, const Cell *right) {
        return std::tuple{Depth(left->pos), left->pos.q, left->pos.r} <
               std::tuple{Depth(right->pos), right->pos.q, right->pos.r};
      });
    for (auto found = sorted.rbegin(); found != sorted.rend(); ++found) {
      const Cell &cell = **found;
      if (InsideTop(nativePoint, Surface(cell.pos, static_cast<float>(cell.height))))
        return cell.pos;
    }
    return std::nullopt;
  }
  Rect Camera::Bounds(const std::vector<Cell> &cells) const {
    if (cells.empty())
      return {};
    float left = std::numeric_limits<float>::max(), right = std::numeric_limits<float>::lowest();
    float top = left, bottom = right;
    const float zoom = static_cast<float>(zoom_);
    for (const auto &cell : cells) {
      const auto point = Project(cell.pos, static_cast<float>(cell.height));
      left = std::min(left, point.x - 24.F * zoom);
      right = std::max(right, point.x + 24.F * zoom);
      top = std::min(top, point.y - 60.F * zoom);
      bottom = std::max(bottom, point.y + 25.F * zoom);
    }
    return {left, top, right - left, bottom - top};
  }
  bool Camera::SetZoom(int zoom, Point nativeAnchor) {
    if (zoom < 1 || zoom > 3 || !std::isfinite(nativeAnchor.x) || !std::isfinite(nativeAnchor.y))
      return false;
    if (zoom == zoom_)
      return true;
    const Point world{(nativeAnchor.x - origin_.x - pan_.x) / static_cast<float>(zoom_),
                      (nativeAnchor.y - origin_.y - pan_.y) / static_cast<float>(zoom_)};
    pan_ = {std::round(nativeAnchor.x - origin_.x - world.x * static_cast<float>(zoom)),
            std::round(nativeAnchor.y - origin_.y - world.y * static_cast<float>(zoom))};
    zoom_ = zoom;
    return true;
  }
  void Camera::PanBy(Point nativeDelta) {
    if (!std::isfinite(nativeDelta.x) || !std::isfinite(nativeDelta.y))
      return;
    pan_.x = std::round(pan_.x + nativeDelta.x);
    pan_.y = std::round(pan_.y + nativeDelta.y);
  }
  void Camera::ResetPan() { pan_ = {}; }

  float VisualEffect::Progress() const {
    return lifetime > 0.F ? std::clamp(elapsed / lifetime, 0.F, 1.F) : 1.F;
  }
  UnitVisual AnimationDirector::Initial(const Match &match, const Unit &unit) {
    UnitVisual visual;
    visual.id = unit.id;
    visual.owner = unit.owner;
    visual.type = unit.type;
    visual.q = static_cast<float>(unit.pos.q);
    visual.r = static_cast<float>(unit.pos.r);
    visual.elevation = Height(match, unit.pos);
    visual.visible = unit.Alive();
    visual.facing = unit.owner == 0 ? 0 : 1;
    return visual;
  }
  void AnimationDirector::Reset(const Match &match) {
    tracks_.clear();
    effects_.clear();
    for (const auto &unit : match.state.units) {
      Track track;
      track.visual = Initial(match, unit);
      track.destination = unit.pos;
      track.destinationHeight = Height(match, unit.pos);
      track.dead = !unit.Alive();
      tracks_.emplace(unit.id, std::move(track));
    }
  }
  void AnimationDirector::Start(Track &track, Clip clip) {
    track.clip = std::move(clip);
    track.visual.state = track.clip.state;
    track.visual.elapsed = 0.F;
    track.visual.frame = 0;
    track.visual.path = track.clip.path;
    track.visual.heights = track.clip.heights;
    if (track.clip.facing >= 0)
      track.visual.facing = track.clip.facing;
    if (track.visual.state == AnimState::Death)
      track.visual.visible = true;
    Sample(track);
  }
  void AnimationDirector::Queue(Track &track, Clip clip) {
    if (track.clip.duration <= 0.F && track.queue.empty())
      Start(track, std::move(clip));
    else
      track.queue.push_back(std::move(clip));
  }
  void AnimationDirector::Sample(Track &track) {
    auto &visual = track.visual;
    if (visual.state == AnimState::Idle) {
      visual.frame = static_cast<int>(std::floor(visual.elapsed / AnimationTiming.idleFrameSeconds)) % AnimationTiming.frames;
      return;
    }
    if (visual.state == AnimState::Walk && track.clip.path.size() > 1) {
      const std::size_t segments = track.clip.path.size() - 1;
      const float progress = std::clamp(visual.elapsed / std::max(0.0001F, track.clip.duration), 0.F, 1.F) * static_cast<float>(segments);
      const auto segment = std::min(static_cast<std::size_t>(progress), segments - 1);
      const float fraction = std::clamp(progress - static_cast<float>(segment), 0.F, 1.F);
      const auto a = track.clip.path[segment], b = track.clip.path[segment + 1];
      visual.q = static_cast<float>(a.q) + static_cast<float>(b.q - a.q) * fraction;
      visual.r = static_cast<float>(a.r) + static_cast<float>(b.r - a.r) * fraction;
      if (track.clip.heights.size() == track.clip.path.size())
        visual.elevation = track.clip.heights[segment] + (track.clip.heights[segment + 1] - track.clip.heights[segment]) * fraction;
      visual.facing = Facing(a, b, visual.facing, rotation_);
      visual.frame = static_cast<int>(std::floor(visual.elapsed / AnimationTiming.walkFrameSeconds)) % AnimationTiming.frames;
    } else {
      const float progress = std::clamp(visual.elapsed / std::max(0.0001F, track.clip.duration), 0.F, 1.F);
      visual.frame = std::min(AnimationTiming.frames - 1, static_cast<int>(progress * AnimationTiming.frames));
    }
  }
  void AnimationDirector::Observe(const Match &before, const Match &after, std::optional<Ability> hint) {
    // Observar instantáneas no modifica la partida lógica ni vuelve a ejecutar sus órdenes.
    if (!hint)
      hint = LoggedAbility(before, after);
    for (const auto &unit : before.state.units)
      if (!tracks_.contains(unit.id)) {
        Track track;
        track.visual = Initial(before, unit);
        track.destination = unit.pos;
        track.destinationHeight = Height(before, unit.pos);
        track.dead = !unit.Alive();
        tracks_.emplace(unit.id, std::move(track));
      }
    std::vector<const Unit *> hurt, healed;
    const Unit *actor = nullptr;
    int actorPriority = -1;
    for (const auto &unit : after.state.units) {
      const auto *old = before.Find(unit.id);
      if (!old) {
        Track track;
        track.visual = Initial(after, unit);
        track.destination = unit.pos;
        track.destinationHeight = Height(after, unit.pos);
        track.dead = !unit.Alive();
        tracks_.insert_or_assign(unit.id, std::move(track));
        if (unit.Alive())
          effects_.push_back({EffectType::Spawn, unit.pos, unit.pos, Height(after, unit.pos), Height(after, unit.pos), 0.F, 0.6F, 0.F, 0, unit.owner, "+"});
        continue;
      }
      auto &track = tracks_.at(unit.id);
      track.visual.owner = unit.owner;
      track.visual.type = unit.type;
      track.destination = unit.pos;
      track.destinationHeight = Height(after, unit.pos);
      if (unit.hp < old->hp && old->Alive())
        hurt.push_back(&unit);
      if (unit.hp > old->hp)
        healed.push_back(&unit);
      int priority = -1;
      if (old->owner == before.state.currentPlayer && old->Alive()) {
        if (unit.pos != old->pos)
          priority = 2;
        if (unit.mp < old->mp || (!old->acted && unit.acted))
          priority = 3;
        if (!old->attacked && unit.attacked)
          priority = 5;
        if (before.state.activeUnit == unit.id && (!old->acted && unit.acted))
          priority = 6;
      }
      if (priority > actorPriority) {
        actor = &unit;
        actorPriority = priority;
      }
      if (unit.pos != old->pos && old->Alive()) {
        const auto route = before.FindPath(unit.id, unit.pos);
        if (route && route->cells.size() > 1) {
          Clip walk;
          walk.state = AnimState::Walk;
          walk.path = route->cells;
          walk.duration = static_cast<float>(walk.path.size() - 1) * Duration(AnimState::Walk);
          for (const auto pos : walk.path)
            walk.heights.push_back(Height(after, pos));
          Queue(track, std::move(walk));
        } else {
          // Cargas y teletransportes ajustan la posición visual sin inventar rutas ilegales.
          track.visual.q = static_cast<float>(unit.pos.q);
          track.visual.r = static_cast<float>(unit.pos.r);
          track.visual.elevation = track.destinationHeight;
        }
      }
    }
    std::vector<const CombatEvent *> events;
    std::map<int, int> combatLoss;
    float environmentDelay = 0.f;
    const auto pending = [](const Track &track) {float time=std::max(0.f,track.clip.duration-track.visual.elapsed);for(const auto &clip:track.queue)time+=clip.duration;return time; };
    for (const auto &event : after.state.combatEvents)
      if (event.id >= before.state.nextEventId)
        events.push_back(&event);
    if (!events.empty()) {
      actor = nullptr;
      for (const auto *event : events)
        combatLoss[event->targetId] += event->amount;
      std::erase_if(hurt, [&](const Unit *unit) { return combatLoss[unit->id] >= before.Find(unit->id)->hp - unit->hp; });
      const auto wait = [&](Track &track, float until) {const float delay=until-pending(track);if(delay>0.f)Queue(track,{AnimState::Idle,delay,{},{},-1}); };
      float base = 0.f;
      for (const auto *event : events)
        if (event->chainIndex == 0 && !event->counter)
          base = std::max(base, pending(tracks_.at(event->attackerId)));
      float lastImpact = base;
      for (const auto *event : events) {
        const auto *attacker = after.Find(event->attackerId), *victim = after.Find(event->targetId);
        if (!attacker || !victim)
          continue;
        auto &actorTrack = tracks_.at(attacker->id), &victimTrack = tracks_.at(victim->id);
        const bool cast = attacker->type == UnitClass::Caster || Magic(event->ability);
        const float start = event->counter ? lastImpact + AnimationTiming.hurtSeconds : base + (event->chainIndex > 0 ? AnimationTiming.spellContactSeconds : 0.f) + event->chainIndex * AnimationTiming.chainIntervalSeconds;
        const float impact = start + (event->chainIndex > 0 ? 0.f : cast ? AnimationTiming.spellContactSeconds
                                                                         : AnimationTiming.weaponContactSeconds);
        if (event->chainIndex == 0) {
          wait(actorTrack, start);
          Queue(actorTrack, {cast ? AnimState::Cast : AnimState::Attack, Duration(cast ? AnimState::Cast : AnimState::Attack), {}, {}, Facing(event->from, event->to, actorTrack.visual.facing, rotation_)});
        }
        wait(victimTrack, impact);
        victimTrack.dead = event->lethal;
        Queue(victimTrack, {event->lethal ? AnimState::Death : AnimState::Hurt, Duration(event->lethal ? AnimState::Death : AnimState::Hurt), {}, {}, -1});
        effects_.push_back({AttackEffect(attacker->type, event->ability), event->from, event->to, Height(after, event->from), Height(after, event->to), 0.f, 0.42f, impact, event->amount, attacker->owner, ""});
        effects_.push_back({EffectType::Damage, event->to, event->to, Height(after, event->to), Height(after, event->to), 0.f, 0.85f, impact, event->amount, victim->owner, (event->counter ? "CONTRA -" : "-") + std::to_string(event->amount)});
        lastImpact = std::max(lastImpact, impact);
      }
      environmentDelay = lastImpact + 0.35f;
      for (const auto &[id, track] : tracks_) {
        (void)id;
        environmentDelay = std::max(environmentDelay, pending(track));
      }
    }
    const bool attackTransition = actor && before.Find(actor->id) && !before.Find(actor->id)->attacked && actor->attacked;
    // Si la IA cierra la ronda y reinicia indicadores, infiere al actor por posición, maná o elegibilidad sin alterar las reglas.
    if (events.empty() && !actor && !hurt.empty()) {
      int nearest = std::numeric_limits<int>::max();
      for (const auto &candidate : after.state.units) {
        const auto *old = before.Find(candidate.id);
        if (!old || !old->Alive() || old->activated || old->owner != before.state.currentPlayer)
          continue;
        for (const auto *victim : hurt)
          if (victim->owner != candidate.owner &&
              before.Find(victim->id)->hp - victim->hp > EnvironmentalLoss(before, after, *victim)) {
            const int distance = Distance(candidate.pos, victim->pos);
            if (distance < nearest) {
              nearest = distance;
              actor = &candidate;
            }
          }
      }
    }
    float actorDelay = 0.F;
    float contactDelay = AnimationTiming.weaponContactSeconds;
    bool acted = false;
    if (actor) {
      const auto *old = before.Find(actor->id);
      auto &track = tracks_.at(actor->id);
      acted = old && ((!old->acted && actor->acted) || attackTransition || actor->mp < old->mp || actor->inventory.size() < old->inventory.size());
      const bool attack = attackTransition || std::ranges::any_of(hurt, [&](const Unit *unit) { return unit->owner != actor->owner &&
                                                                                                       before.Find(unit->id)->hp - unit->hp > EnvironmentalLoss(before, after, *unit); });
      const bool heal = std::ranges::any_of(healed, [&](const Unit *unit) { return unit->owner == actor->owner; });
      if (acted || attack || heal) {
        actorDelay = std::max(0.F, track.clip.duration - track.visual.elapsed);
        for (const auto &clip : track.queue)
          actorDelay += clip.duration;
        const bool cast = actor->type == UnitClass::Caster || (hint && Magic(*hint)) || heal;
        contactDelay = cast ? AnimationTiming.spellContactSeconds : AnimationTiming.weaponContactSeconds;
        const AnimState state = cast ? AnimState::Cast : AnimState::Attack;
        int facing = track.visual.facing;
        if (!hurt.empty())
          facing = Facing(actor->pos, hurt.front()->pos, facing, rotation_);
        Queue(track, {state, Duration(state), {}, {}, facing});
        if (hint && *hint == Ability::Smoke)
          effects_.push_back({EffectType::Smoke, actor->pos, actor->pos, Height(after, actor->pos), Height(after, actor->pos), 0.F, 0.65F, actorDelay, 0, actor->owner, ""});
      }
    }
    for (const auto *unit : hurt) {
      const auto *old = before.Find(unit->id);
      auto &track = tracks_.at(unit->id);
      const bool enemyAttack = actor && actor->owner != unit->owner && old->hp - unit->hp > EnvironmentalLoss(before, after, *unit);
      const float delay = !events.empty() ? environmentDelay : enemyAttack ? actorDelay + contactDelay
                                                                           : 0.F;
      const float wait = delay - pending(track);
      if (wait > 0.F)
        Queue(track, {AnimState::Idle, wait, {}, {}, -1});
      track.dead = !unit->Alive();
      const auto state = unit->Alive() ? AnimState::Hurt : AnimState::Death;
      Queue(track, {state, Duration(state), {}, {}, -1});
      effects_.push_back({EffectType::Damage, unit->pos, unit->pos, Height(after, unit->pos), Height(after, unit->pos), 0.F, 0.85F, delay, old->hp - unit->hp - combatLoss[unit->id], unit->owner, "-" + std::to_string(old->hp - unit->hp - combatLoss[unit->id])});
      if (enemyAttack)
        effects_.push_back({AttackEffect(actor->type, hint), actor->pos, unit->pos,
                            Height(after, actor->pos), Height(after, unit->pos), 0.F, 0.48F, actorDelay + contactDelay, old->hp - unit->hp, actor->owner, ""});
    }
    for (const auto *unit : healed) {
      const auto *old = before.Find(unit->id);
      effects_.push_back({EffectType::Heal, actor ? actor->pos : unit->pos, unit->pos,
                          actor ? Height(after, actor->pos) : Height(after, unit->pos), Height(after, unit->pos), 0.F, 0.75F, actorDelay, unit->hp - old->hp, unit->owner, "+" + std::to_string(unit->hp - old->hp)});
    }
    for (const auto &source : after.state.sources)
      if (std::ranges::none_of(before.state.sources, [&](const Source &old) { return old.id == source.id; }))
        effects_.push_back({EffectType::RotBurst, source.pos, source.pos, Height(after, source.pos), Height(after, source.pos), 0.F, 0.80F, 0.F, 0, -1, "PUTREFACCION"});
    if (before.state.round == after.state.round) {
      for (const auto &source : before.state.sources)
        if (std::ranges::none_of(after.state.sources, [&](const Source &next) { return next.id == source.id; }))
          effects_.push_back({EffectType::Purify, actor ? actor->pos : source.pos, source.pos,
                              actor ? Height(after, actor->pos) : Height(after, source.pos), Height(after, source.pos), 0.F, 0.65F, actorDelay, 0, actor ? actor->owner : -1, "PURIFICADO"});
      for (const auto &corpse : before.state.corpses)
        if (std::ranges::none_of(after.state.corpses, [&](const Corpse &next) { return next.id == corpse.id; }))
          effects_.push_back({EffectType::Fire, actor ? actor->pos : corpse.pos, corpse.pos,
                              actor ? Height(after, actor->pos) : Height(after, corpse.pos), Height(after, corpse.pos), 0.F, 0.65F, actorDelay, 0, actor ? actor->owner : -1, "INCINERADO"});
    }
    for (const auto &cell : after.state.cells) {
      const auto *old = before.At(cell.pos);
      if (old && (cell.captureProgress != old->captureProgress || cell.owner != old->owner))
        effects_.push_back({EffectType::Capture, cell.pos, cell.pos, static_cast<float>(cell.height), static_cast<float>(cell.height), 0.F, 0.90F, 0.F, 0, cell.owner, cell.captureProgress > 0 ? "CAPTURA " + std::to_string(cell.captureProgress) + "/" + std::to_string(after.CaptureRequired(cell)) : "CONQUISTA"});
    }
  }
  void AnimationDirector::Update(float seconds) {
    if (!std::isfinite(seconds) || seconds <= 0.F)
      return;
    for (auto &[id, track] : tracks_) {
      (void)id;
      if (!track.visual.visible)
        continue;
      float remaining = seconds;
      while (remaining > 0.F) {
        if (track.clip.duration <= 0.F) {
          track.visual.elapsed += remaining;
          Sample(track);
          break;
        }
        const float advance = std::min(remaining, std::max(0.F, track.clip.duration - track.visual.elapsed));
        track.visual.elapsed += advance;
        remaining -= advance;
        Sample(track);
        if (track.visual.elapsed + 0.000001F < track.clip.duration)
          break;
        const bool died = track.clip.state == AnimState::Death;
        if (!track.queue.empty()) {
          Clip next = std::move(track.queue.front());
          track.queue.pop_front();
          Start(track, std::move(next));
        } else if (died || track.dead) {
          track.visual.visible = false;
          track.visual.state = AnimState::Death;
          track.visual.frame = 7;
          track.clip.duration = 0.F;
          break;
        } else {
          track.clip = {};
          track.visual.state = AnimState::Idle;
          track.visual.elapsed = 0.F;
          track.visual.path.clear();
          track.visual.heights.clear();
          track.visual.q = static_cast<float>(track.destination.q);
          track.visual.r = static_cast<float>(track.destination.r);
          track.visual.elevation = track.destinationHeight;
          Sample(track);
        }
      }
    }
    for (auto &effect : effects_) {
      float remaining = seconds;
      if (effect.delay > 0.F) {
        const float advance = std::min(remaining, effect.delay);
        effect.delay -= advance;
        remaining -= advance;
      }
      effect.elapsed += remaining;
    }
    std::erase_if(effects_, [](const VisualEffect &effect) { return effect.delay <= 0.F && effect.elapsed >= effect.lifetime; });
  }
  UnitVisual AnimationDirector::VisualFor(int unitId, const Unit &logical) const {
    const auto found = tracks_.find(unitId);
    if (found != tracks_.end())
      return found->second.visual;
    UnitVisual visual;
    visual.id = logical.id;
    visual.owner = logical.owner;
    visual.type = logical.type;
    visual.q = static_cast<float>(logical.pos.q);
    visual.r = static_cast<float>(logical.pos.r);
    visual.visible = logical.Alive();
    visual.facing = logical.owner == 0 ? 0 : 1;
    return visual;
  }
  bool AnimationDirector::Busy() const {
    return std::ranges::any_of(tracks_, [](const auto &entry) { return entry.second.visual.visible &&
                                                                       (entry.second.clip.duration > 0.F || !entry.second.queue.empty()); }) || !effects_.empty();
  }
} // espacio de nombres Plaguefront
