#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>

class SceneManager {
private:
  std::string currentScene_;
  std::optional<std::string> nextScene_;
  bool quitRequested_{};

public:
  void ChangeScene(std::string_view name);
  void RestartCurrentScene();
  void RequestQuit() { quitRequested_ = true; }

private:
  friend class Engine;

  std::optional<std::string> FetchNextScene();
  bool FetchQuitRequest() { return std::exchange(quitRequested_, false); }
};
