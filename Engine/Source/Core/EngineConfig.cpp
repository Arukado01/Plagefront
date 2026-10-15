#include "Core/EngineConfig.h"
#include <cassert>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <algorithm>
#include <array>
#include <nlohmann/json.hpp>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

namespace {
  sf::Color ReadColor(const nlohmann::json &palette, const char *name, sf::Color fallback) {
    const auto found = palette.find(name);
    if (found == palette.end() || !found->is_array() || found->size() != 4)
      return fallback;
    std::array<std::uint8_t, 4> channels{};
    for (std::size_t i = 0; i < channels.size(); ++i) {
      const auto &value = (*found)[i];
      if (!value.is_number_integer() || value < 0 || value > 255)
        return fallback;
      channels[i] = static_cast<std::uint8_t>(value.get<unsigned>());
    }
    return {channels[0], channels[1], channels[2], channels[3]};
  }
}

RotPaletteConfig EngineConfig::ReadRotPalette(const nlohmann::json &document) {
  RotPaletteConfig palette;
  const auto found = document.find("rotPalette");
  if (found == document.end() || !found->is_object())
    return palette;
  palette.sourceShadow = ReadColor(*found, "sourceShadow", palette.sourceShadow);
  palette.sourceBase = ReadColor(*found, "sourceBase", palette.sourceBase);
  palette.sourceLight = ReadColor(*found, "sourceLight", palette.sourceLight);
  palette.sourceGlow = ReadColor(*found, "sourceGlow", palette.sourceGlow);
  palette.localFill = ReadColor(*found, "localFill", palette.localFill);
  palette.localEdge = ReadColor(*found, "localEdge", palette.localEdge);
  palette.globalFill = ReadColor(*found, "globalFill", palette.globalFill);
  palette.spores = ReadColor(*found, "spores", palette.spores);
  palette.uiText = ReadColor(*found, "uiText", palette.uiText);
  return palette;
}

sf::Color RotPaletteConfig::SpriteColor(sf::Color source) const {
  if (source.a == 0)
    return source;
  const auto mapped = [&](sf::Color color) {
    color.a = static_cast<std::uint8_t>(static_cast<unsigned>(color.a) * source.a / 255U);
    return color;
  };
  if (source.r == 51 && source.g == 69 && source.b == 53)
    return mapped(sourceShadow);
  if (source.r == 100 && source.g == 125 && source.b == 60)
    return mapped(sourceBase);
  if (source.r == 159 && source.g == 179 && source.b == 79)
    return mapped(sourceLight);
  if (source.r == 213 && source.g == 212 && source.b == 122)
    return mapped(sourceGlow);
  return source;
}

EngineConfig::EngineConfig() {
  // Localiza Content antes de cargar recursos relativos, incluso al abrir build/Plaguefront.exe desde el explorador.
  if (!std::filesystem::exists("Content/Config.json")) {
#ifdef _WIN32
    std::wstring executable(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (length > 0 && length < executable.size()) {
      executable.resize(length);
      auto directory = std::filesystem::path(executable).parent_path();
      for (int level = 0; level < 3; ++level) {
        if (std::filesystem::exists(directory / "Content/Config.json")) {
          std::filesystem::current_path(directory);
          break;
        }
        directory = directory.parent_path();
      }
    }
#endif
  }
  std::ifstream file("Content/Config.json");
  if (!file)
    throw std::runtime_error("No se encuentra Content/Config.json junto al proyecto o al ejecutable.");

  nlohmann::json json = nlohmann::json::parse(file);
  windowTitle = json["windowTitle"];
  windowSize = {json["windowSize"][0], json["windowSize"][1]};
  const auto reference = json.value("referenceSize", std::array<float, 2>{windowSize.x, windowSize.y});
  referenceSize = {std::max(1.f, reference[0]), std::max(1.f, reference[1])};
  const auto minimum = json.value("minimumWindowSize", std::array<unsigned, 2>{960, 600});
  minimumWindowSize = {std::max(1u, minimum[0]), std::max(1u, minimum[1])};
  windowIcon = json.value("windowIcon", std::string{});
  disableSfmlLogs = json["disableSfmlLogs"];
  maximumDeltaTime = sf::seconds(json["maximumDeltaTime"]);
  globalVolume = json["globalVolume"];
  backgroundColor = {
      json["backgroundColor"][0],
      json["backgroundColor"][1],
      json["backgroundColor"][2],
  };
  cursorRadius = json["cursorRadius"];
  cursorSpeed = json["cursorSpeed"];
  cursorColor = {json["cursorColor"][0], json["cursorColor"][1],
                 json["cursorColor"][2]};
  joystickDeadzone = json["joystickDeadzone"];
  rotPalette = ReadRotPalette(json);
}
