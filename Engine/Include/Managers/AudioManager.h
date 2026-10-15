#pragma once
#include <SFML/Audio/Music.hpp>
#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/System/Vector3.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

enum class AudioBus { Music,
                      Effects,
                      Ambient };

class AudioManager {
public:
  static constexpr std::size_t VoiceLimit = 12;
  AudioManager() = default;
  ~AudioManager();
  AudioManager(const AudioManager &) = delete;
  AudioManager &operator=(const AudioManager &) = delete;
  void SetGlobalVolume(float volume);
  float GetGlobalVolume() const;
  void SetChannelVolumes(float master, float music, float effects, bool muted);
  float GetMusicVolume() const { return musicVolume_; }
  float GetEffectsVolume() const { return effectsVolume_; }
  bool IsMuted() const { return muted_; }
  bool LoadCue(const std::string &name, const std::string &filename, AudioBus bus,
               float gain = 100.f, bool loop = false, float cooldown = 0.08f);
  bool PlayCue(const std::string &name);
  void StopMusic();
  void StopAmbient();
  void StopAll();
  void Update(float seconds);
  std::size_t LoadedCueCount() const { return cues_.size(); }
  std::size_t ActiveVoiceCount() const;
  std::size_t VoicePoolSize() const;
  std::size_t ActiveMusicCount() const;
  const std::string &CurrentMusic() const { return currentMusic_; }
  const std::string &CurrentAmbient() const { return currentAmbient_; }
  std::uint64_t MusicStarts() const { return musicStarts_; }
  std::uint64_t EffectStarts() const { return effectStarts_; }
  std::uint64_t CueStarts(const std::string &name) const;

  void SetListenerPosition(sf::Vector3f position);
  sf::Vector3f GetListenerPosition() const;

  void SetListenerDirection(sf::Vector3f direction);
  sf::Vector3f GetListenerDirection() const;

private:
  struct Cue {
    AudioBus bus{AudioBus::Effects};
    float gain{100.f}, cooldown{0.08f};
    bool loop{};
    double lastPlayed{-10000.0};
    std::uint64_t starts{};
    std::unique_ptr<sf::SoundBuffer> buffer;
    std::unique_ptr<sf::Music> stream;
  };
  struct Voice {
    std::unique_ptr<sf::Sound> sound;
    float gain{100.f};
    std::uint64_t serial{};
  };
  std::unordered_map<std::string, Cue> cues_;
  std::array<Voice, VoiceLimit> voices_;
  std::string currentMusic_, currentAmbient_;
  float masterVolume_{60.f}, musicVolume_{65.f}, effectsVolume_{75.f};
  bool muted_{};
  double elapsed_{};
  std::uint64_t voiceSerial_{}, musicStarts_{}, effectStarts_{};
  void ApplyVolumes();
};
