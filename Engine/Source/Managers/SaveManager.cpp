#include "Managers/SaveManager.h"
#include <fstream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

SaveManager::SaveManager(std::filesystem::path filename) : filename_(std::move(filename)) {
  const auto loaded = ReadDocument(filename_);
  values_ = loaded && loaded->is_object() ? *loaded : nlohmann::json::object();
}

SaveManager::~SaveManager() { Flush(); }

std::optional<nlohmann::json> SaveManager::ReadDocument(const std::filesystem::path &filename) const {
  try {
    std::ifstream file(filename);
    if (!file)
      return {};
    return nlohmann::json::parse(file);
  } catch (const std::exception &) {
    return {};
  }
}

bool SaveManager::WriteDocument(const std::filesystem::path &filename, const nlohmann::json &values) const {
  auto temporary = filename;
  temporary += ".tmp";
  try {
    if (!filename.parent_path().empty())
      std::filesystem::create_directories(filename.parent_path());
    {
      std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
      if (!file)
        return false;
      file << values.dump(2) << '\n';
      file.flush();
      if (!file) {
        file.close();
        std::filesystem::remove(temporary);
        return false;
      }
    }
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), filename.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
      std::filesystem::remove(temporary);
      return false;
    }
#else
    std::filesystem::rename(temporary, filename);
#endif
    return true;
  } catch (const std::exception &) {
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    return false;
  }
}

bool SaveManager::Flush() {
  if (!dirty_)
    return true;
  if (!WriteDocument(filename_, values_))
    return false;
  dirty_ = false;
  return true;
}

bool SaveManager::Has(std::string_view key) const {
  return values_.contains(key);
}

void SaveManager::Erase(std::string_view key) { dirty_ |= values_.erase(key) > 0; }

void SaveManager::Clear() {
  dirty_ |= !values_.empty();
  values_.clear();
}
