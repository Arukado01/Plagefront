#include "Managers/AudioManager.h"
#include <SFML/Audio/Listener.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace {
  float Volume(float v) { return std::isfinite(v) ? std::clamp(v, 0.f, 100.f) : 0.f; }
} // espacio de nombres
AudioManager::~AudioManager() { StopAll(); }

void AudioManager::SetGlobalVolume(float volume) {
  masterVolume_ = Volume(volume);
  sf::Listener::setGlobalVolume(muted_ ? 0.f : masterVolume_);
}

float AudioManager::GetGlobalVolume() const {
  return masterVolume_;
}

void AudioManager::SetChannelVolumes(float master, float music, float effects, bool muted) {
  masterVolume_ = Volume(master);
  musicVolume_ = Volume(music);
  effectsVolume_ = Volume(effects);
  muted_ = muted;
  ApplyVolumes();
}
void AudioManager::ApplyVolumes() {
  sf::Listener::setGlobalVolume(muted_ ? 0.f : masterVolume_);
  for (auto &[name, cue] : cues_) {
    (void)name;
    if (cue.stream)
      cue.stream->setVolume(cue.gain * (cue.bus == AudioBus::Music ? musicVolume_ : effectsVolume_) / 100.f);
  }
  for (auto &voice : voices_)
    if (voice.sound)
      voice.sound->setVolume(voice.gain * effectsVolume_ / 100.f);
}
bool AudioManager::LoadCue(const std::string &name, const std::string &filename, AudioBus bus, float gain, bool loop, float cooldown) {
  if (cues_.contains(name))
    return true;
  Cue cue;
  cue.bus = bus;
  cue.gain = Volume(gain);
  cue.loop = loop;
  cue.cooldown = std::isfinite(cooldown) ? std::clamp(cooldown, 0.f, 30.f) : 0.08f;
  if (bus == AudioBus::Effects) {
    cue.buffer = std::make_unique<sf::SoundBuffer>();
    if (!cue.buffer->loadFromFile(filename)) {
      std::cerr << "Audio: no se puede cargar " << filename << '\n';
      return false;
    }
  } else {
    cue.stream = std::make_unique<sf::Music>();
    if (!cue.stream->openFromFile(filename)) {
      std::cerr << "Audio: no se puede abrir " << filename << '\n';
      return false;
    }
    cue.stream->setLooping(loop);
    cue.stream->setVolume(cue.gain * (bus == AudioBus::Music ? musicVolume_ : effectsVolume_) / 100.f);
  }
  cues_.emplace(name, std::move(cue));
  return true;
}
bool AudioManager::PlayCue(const std::string &name) {
  const auto found = cues_.find(name);
  if (found == cues_.end())
    return false;
  auto &cue = found->second;
  if (cue.bus != AudioBus::Effects) {
    auto &current = cue.bus == AudioBus::Music ? currentMusic_ : currentAmbient_;
    if (current == name)
      return true;
    if (!current.empty()) {
      const auto previous = cues_.find(current);
      if (previous != cues_.end() && previous->second.stream)
        previous->second.stream->stop();
    }
    current = name;
    cue.stream->play();
    ++cue.starts;
    if (cue.bus == AudioBus::Music)
      ++musicStarts_;
    return true;
  }
  if (elapsed_ - cue.lastPlayed < cue.cooldown)
    return false;
  cue.lastPlayed = elapsed_;
  auto slot = std::find_if(voices_.begin(), voices_.end(), [](const Voice &v) { return !v.sound || v.sound->getStatus() == sf::SoundSource::Status::Stopped; });
  if (slot == voices_.end())
    slot = std::min_element(voices_.begin(), voices_.end(), [](const Voice &a, const Voice &b) { return a.serial < b.serial; });
  if (!slot->sound)
    slot->sound = std::make_unique<sf::Sound>(*cue.buffer);
  else {
    slot->sound->stop();
    slot->sound->setBuffer(*cue.buffer);
  }
  slot->gain = cue.gain;
  slot->serial = ++voiceSerial_;
  slot->sound->setLooping(false);
  slot->sound->setVolume(cue.gain * effectsVolume_ / 100.f);
  slot->sound->play();
  ++cue.starts;
  ++effectStarts_;
  return true;
}
void AudioManager::StopMusic() {
  if (auto it = cues_.find(currentMusic_); it != cues_.end() && it->second.stream)
    it->second.stream->stop();
  currentMusic_.clear();
}
void AudioManager::StopAmbient() {
  if (auto it = cues_.find(currentAmbient_); it != cues_.end() && it->second.stream)
    it->second.stream->stop();
  currentAmbient_.clear();
}
void AudioManager::StopAll() {
  StopMusic();
  StopAmbient();
  for (auto &v : voices_)
    if (v.sound)
      v.sound->stop();
}
void AudioManager::Update(float seconds) {
  if (std::isfinite(seconds) && seconds > 0)
    elapsed_ += std::min(seconds, 10.f);
}
std::size_t AudioManager::ActiveVoiceCount() const {
  return static_cast<std::size_t>(std::count_if(voices_.begin(), voices_.end(), [](const Voice &v) { return v.sound && v.sound->getStatus() == sf::SoundSource::Status::Playing; }));
}
std::size_t AudioManager::VoicePoolSize() const {
  return static_cast<std::size_t>(std::count_if(voices_.begin(), voices_.end(), [](const Voice &v) { return static_cast<bool>(v.sound); }));
}
std::size_t AudioManager::ActiveMusicCount() const {
  std::size_t count = 0;
  for (const auto &[name, c] : cues_) {
    (void)name;
    if (c.bus == AudioBus::Music && c.stream && c.stream->getStatus() == sf::SoundSource::Status::Playing)
      ++count;
  }
  return count;
}
std::uint64_t AudioManager::CueStarts(const std::string &name) const {
  const auto it = cues_.find(name);
  return it == cues_.end() ? 0 : it->second.starts;
}

void AudioManager::SetListenerPosition(sf::Vector3f position) {
  sf::Listener::setPosition(position);
}

sf::Vector3f AudioManager::GetListenerPosition() const {
  return sf::Listener::getPosition();
}

void AudioManager::SetListenerDirection(sf::Vector3f direction) {
  sf::Listener::setDirection(direction);
}

sf::Vector3f AudioManager::GetListenerDirection() const {
  return sf::Listener::getDirection();
}
