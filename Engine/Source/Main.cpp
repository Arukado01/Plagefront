#include <SFML/GpuPreference.hpp>
#include <SFML/System/Sleep.hpp>

#include "Core/Engine.h"
#include "SFML/System/Time.hpp"
#include <filesystem>
#include <iostream>
#include <string_view>

SFML_DEFINE_DISCRETE_GPU_PREFERENCE

int main(int argc, char **argv) {
  const bool smoke = argc > 1 && std::string_view(argv[1]) == "--smoke";
  Engine engine(!smoke);

  if (smoke) {
    std::cout << std::unitbuf;
    engine.ProcessEvent();
    std::filesystem::create_directories("build/screenshots");
    for (int step = 0; step < 71; ++step) {
      try {
        engine.SmokeStep(step);
        engine.Render();
        if (!engine.SaveScreenshot((step < 28 ? "build/screenshots/screen-" + std::to_string(step) : step < 49 ? "build/screenshots/phase4-" + std::to_string(step - 27)
                                                                                                               : "build/screenshots/phase5-" + std::to_string(step - 48)) +
                                   ".png"))
          return 2;
      } catch (const std::exception &error) {
        std::cerr << "Plaguefront smoke step " << step << ": " << error.what() << '\n';
        return 3;
      }
    }
    std::cout << "Plaguefront UI smoke: 71 application screens (49 regression +22 phase5) saved at their actual output dimensions\n";
    return 0;
  }

  while (engine.IsRunning()) {
    engine.ProcessEvent();

    if (!engine.HasFocus()) {
      sf::sleep(sf::milliseconds(10));
      continue;
    }

    engine.Update();
    engine.Render();
  }
}
