#pragma once

#include "Plaguefront/Rules.h"
#include "Config/PlaguefrontPresentationConfig.h"

#include <array>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Plaguefront {
  RotExposure RotPresentationAt(const Match &, Hex pos);
  float GroundDepth(Hex pos);
  std::vector<const Cell *> DrawOrder(const std::vector<Cell> &cells);
  bool InsideTop(Point point, const std::array<Point, 6> &polygon);

  class Camera {
  public:
    void Fit(const std::vector<Cell> &cells, float width = 486.F, float height = 320.F);
    // La vista de conjunto amplía el lienzo nativo con escala externa 1 y conserva el zoom entero de la cámara.
    void FrameAll(const std::vector<Cell> &cells, float width, float height);
    void CenterOn(Hex pos, float elevation, Point nativeCenter);
    // Gira los ejes axiales en pasos de 60 grados sin cambiar el mapa lógico.
    void SetRotation(int hexTurns);
    int Rotation() const { return rotation_; }
    Point Project(Hex pos, float elevation = 0.F) const;
    Point ProjectContinuous(float q, float r, float elevation = 0.F) const;
    float Depth(Hex pos) const;
    float DepthContinuous(float q, float r) const;
    Point Inverse(Point nativePoint, float elevation = 0.F) const;
    std::array<Point, 6> Surface(Hex pos, float elevation = 0.F) const;
    std::optional<Hex> Pick(const std::vector<Cell> &cells, Point nativePoint) const;
    Rect Bounds(const std::vector<Cell> &cells) const;
    bool SetZoom(int zoom, Point nativeAnchor);
    void PanBy(Point nativeDelta);
    void ResetPan();
    int Zoom() const { return zoom_; }
    Point Origin() const { return origin_; }
    Point Pan() const { return pan_; }

  private:
    Point origin_{243.F, 160.F}, pan_{};
    int zoom_{1};
    int rotation_{};
  };

  class AnimationDirector {
  public:
    void SetRotation(int hexTurns) { rotation_ = (hexTurns % 6 + 6) % 6; }
    void Reset(const Match &match);
    void Observe(const Match &before, const Match &after, std::optional<Ability> hint = {});
    void Update(float seconds);
    UnitVisual VisualFor(int unitId, const Unit &logical) const;
    UnitVisual VisualFor(const Unit &logical) const { return VisualFor(logical.id, logical); }
    const std::vector<VisualEffect> &Effects() const { return effects_; }
    bool Busy() const;

  private:
    struct Clip {
      AnimState state{AnimState::Idle};
      float duration{};
      std::vector<Hex> path;
      std::vector<float> heights;
      int facing{-1};
    };
    struct Track {
      UnitVisual visual;
      Clip clip;
      std::deque<Clip> queue;
      Hex destination{};
      float destinationHeight{};
      bool dead{};
    };
    std::map<int, Track> tracks_;
    std::vector<VisualEffect> effects_;
    int rotation_{};
    static UnitVisual Initial(const Match &match, const Unit &unit);
    void Queue(Track &track, Clip clip);
    void Start(Track &track, Clip clip);
    void Sample(Track &track);
  };
} // espacio de nombres Plaguefront
