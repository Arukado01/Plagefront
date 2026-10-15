#pragma once
#include "Managers/AudioManager.h"
#include "Plaguefront/Rules.h"
#include <string>
#include <vector>

namespace Plaguefront {
  class AudioDirector {
  public:
    explicit AudioDirector(AudioManager &manager) : manager_(manager) {}
    bool Start(const std::string &manifest = "Content/Plaguefront/audio.json");
    void Update(float seconds);
    void SetScene(const std::string &scene);
    bool Play(const std::string &cue);
    void Observe(const Match &before, const Match &after, std::optional<Ability> hint = {});
    void SetVolumes(float master, float music, float effects, bool muted);
    float MasterVolume() const { return manager_.GetGlobalVolume(); }
    float MusicVolume() const { return manager_.GetMusicVolume(); }
    float EffectsVolume() const { return manager_.GetEffectsVolume(); }
    bool Muted() const { return manager_.IsMuted(); }
    bool Ready() const { return ready_; }

  private:
    AudioManager &manager_;
    std::string scene_;
    bool ready_{}, contaminated_{};
    float pulseTimer_{};
    struct DelayedCue {
      float delay{};
      std::string cue;
    };
    std::vector<DelayedCue> pending_;
  };
} // Espacio de nombres Plaguefront
