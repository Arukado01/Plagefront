#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <optional>
#include <string_view>

class SaveManager {
private:
  nlohmann::json values_;
  std::filesystem::path filename_;
  bool dirty_{};

public:
  explicit SaveManager(std::filesystem::path filename = "Content/Save.json");
  ~SaveManager();

  void Set(std::string_view key, auto value) {
    values_[key] = value;
    dirty_ = true;
  }
  auto Get(std::string_view key, auto defatultValue = {}) const {
    try {
      return values_.value(key, defatultValue);
    } catch (const nlohmann::json::exception &) {
      return defatultValue;
    }
  }

  // Los documentos comparten persistencia JSON local; cada juego conserva la responsabilidad sobre el formato de su partida.
  std::optional<nlohmann::json> ReadDocument(const std::filesystem::path &filename) const;
  bool WriteDocument(const std::filesystem::path &filename, const nlohmann::json &values) const;
  bool Flush();

  bool Has(std::string_view key) const;
  void Erase(std::string_view key);
  void Clear();
};
