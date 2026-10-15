#include "Managers/RenderManager.h"
#include "Managers/ResourceManager.h"
#include "Managers/SaveManager.h"
#include "Managers/ScreenshotManager.h"
#include "Managers/WindowManager.h"
#include "Core/EngineConfig.h"
#include <SFML/Audio/Listener.hpp>
#include <cmath>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <SFML/Graphics/Image.hpp>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace {
  int checks{};
  void Check(bool value, const char *message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
  }
  bool Close(sf::Vector2f left, sf::Vector2f right) {
    return std::abs(left.x-right.x)<0.02f && std::abs(left.y-right.y)<0.02f;
  }
#ifdef _WIN32
  struct MonitorSnapshot {
    HMONITOR handle{};
    MONITORINFOEXW info{};
    DEVMODEW mode{};
  };

  BOOL CALLBACK CollectMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
    reinterpret_cast<std::vector<HMONITOR> *>(data)->push_back(monitor);
    return TRUE;
  }

  bool SameRectangle(const RECT &left, const RECT &right) {
    return left.left==right.left && left.top==right.top && left.right==right.right && left.bottom==right.bottom;
  }

  RECT NativeRectangle(const sf::RenderWindow &window) {
    RECT result{};
    Check(GetWindowRect(window.getNativeHandle(), &result)!=FALSE,"Could not read the native window rectangle");
    return result;
  }

  void VerifyDisplayModes(const std::vector<MonitorSnapshot> &monitors) {
    for (const auto &monitor : monitors) {
      DEVMODEW current{};
      current.dmSize=sizeof(current);
      Check(EnumDisplaySettingsExW(monitor.info.szDevice,ENUM_CURRENT_SETTINGS,&current,0)!=FALSE,
            "Could not read the current physical display mode");
      Check(current.dmPelsWidth==monitor.mode.dmPelsWidth && current.dmPelsHeight==monitor.mode.dmPelsHeight &&
            current.dmBitsPerPel==monitor.mode.dmBitsPerPel && current.dmDisplayFrequency==monitor.mode.dmDisplayFrequency &&
            current.dmPosition.x==monitor.mode.dmPosition.x && current.dmPosition.y==monitor.mode.dmPosition.y &&
            current.dmDisplayOrientation==monitor.mode.dmDisplayOrientation && current.dmDisplayFlags==monitor.mode.dmDisplayFlags,
            "Fullscreen changed a monitor's physical display mode");
    }
  }

  void MonitorFullscreenRegression() {
    std::vector<HMONITOR> handles;
    Check(EnumDisplayMonitors(nullptr,nullptr,CollectMonitor,reinterpret_cast<LPARAM>(&handles))!=FALSE,
          "Could not enumerate display monitors");
    Check(!handles.empty(),"No monitor was available for fullscreen regression checks");
    std::vector<MonitorSnapshot> monitors;
    for (const auto handle : handles) {
      MonitorSnapshot monitor;
      monitor.handle=handle;
      monitor.info.cbSize=sizeof(monitor.info);
      monitor.mode.dmSize=sizeof(monitor.mode);
      Check(GetMonitorInfoW(handle,reinterpret_cast<MONITORINFO *>(&monitor.info))!=FALSE,"Could not read monitor geometry");
      Check(EnumDisplaySettingsExW(monitor.info.szDevice,ENUM_CURRENT_SETTINGS,&monitor.mode,0)!=FALSE,
            "Could not read the monitor's physical display mode");
      monitors.push_back(monitor);
    }

    for (const auto &monitor : monitors) {
      sf::RenderWindow window(sf::VideoMode({960,600}),"Plaguefront monitor regression checks");
      WindowManager manager(window,false);
      const auto pump=[&] {
        while (const auto event=window.pollEvent())
          if (const auto *resized=event->getIf<sf::Event::Resized>())
            manager.OnResized(resized->size);
      };
      pump();
      const auto initial=NativeRectangle(window);
      const auto &work=monitor.info.rcWork;
      window.setPosition({static_cast<int>(work.left+(work.right-work.left-(initial.right-initial.left))/2),
                          static_cast<int>(work.top+(work.bottom-work.top-(initial.bottom-initial.top))/2)});
      pump();
      Check(MonitorFromWindow(window.getNativeHandle(),MONITOR_DEFAULTTONEAREST)==monitor.handle,
            "Test window did not move to the requested monitor");
      const auto normalRectangle=NativeRectangle(window);
      const auto normalPosition=window.getPosition();
      const auto normalSize=window.getSize();
      Check(normalSize==sf::Vector2u{960,600},"Monitor regression did not start with the requested client size");
      const sf::Vector2u physicalSize{static_cast<unsigned>(monitor.mode.dmPelsWidth),static_cast<unsigned>(monitor.mode.dmPelsHeight)};
      manager.SetFullscreenResolution(physicalSize);
      Check(manager.FullscreenResolution()==physicalSize,"Current monitor's physical resolution was rejected");

      const auto verifyFullscreen=[&] {
        Check(manager.IsFullscreen(),"Monitor regression did not enter fullscreen");
        Check(MonitorFromWindow(window.getNativeHandle(),MONITOR_DEFAULTTONEAREST)==monitor.handle,
              "Fullscreen moved the window to another monitor");
        Check(SameRectangle(NativeRectangle(window),monitor.info.rcMonitor),
              "Fullscreen did not exactly cover the selected monitor's native rectangle");
        const auto style=GetWindowLongPtrW(window.getNativeHandle(),GWL_STYLE);
        Check((style&(WS_CAPTION|WS_THICKFRAME|WS_BORDER|WS_DLGFRAME))==0,
              "Fullscreen retained native window borders");
        Check(!IsWindowVisible(window.getNativeHandle()),"Fullscreen made a hidden window visible");
        VerifyDisplayModes(monitors);
      };
      const auto verifyWindowed=[&](sf::Vector2u size,const RECT &rectangle) {
        Check(!manager.IsFullscreen(),"Monitor regression did not leave fullscreen");
        Check(!manager.IsMaximized(),"Normal window returned from fullscreen maximized");
        Check(window.getSize()==size && manager.WindowedSize()==size,
              "Leaving fullscreen lost the requested windowed client size");
        Check(window.getPosition()==normalPosition && SameRectangle(NativeRectangle(window),rectangle),
              "Leaving fullscreen lost the window's normal position or native rectangle");
        Check(!IsWindowVisible(window.getNativeHandle()),"Leaving fullscreen made a hidden window visible");
        VerifyDisplayModes(monitors);
      };

      Check(manager.ToggleFullscreen() && manager.ApplyPending(),"Could not enter fullscreen on a monitor");
      pump();
      verifyFullscreen();
      Check(manager.ToggleFullscreen() && manager.ApplyPending(),"Could not leave fullscreen on a monitor");
      pump();
      verifyWindowed(normalSize,normalRectangle);

      Check(manager.Maximize(),"Could not maximize the monitor regression window");
      pump();
      Check(manager.IsMaximized(),"Monitor regression window was not maximized");
      const auto maximizedRectangle=NativeRectangle(window);
      Check(manager.ToggleFullscreen() && manager.ApplyPending(),"Could not enter fullscreen from maximized");
      pump();
      verifyFullscreen();
      Check(manager.ToggleFullscreen() && manager.ApplyPending(),"Could not leave fullscreen to maximized");
      pump();
      Check(manager.IsMaximized(),"Leaving fullscreen lost the maximized state");
      Check(SameRectangle(NativeRectangle(window),maximizedRectangle),
            "Leaving fullscreen changed the maximized window rectangle");
      Check(!IsWindowVisible(window.getNativeHandle()),"Maximized fullscreen return made a hidden window visible");
      Check(manager.Restore(),"Could not restore the normal window after fullscreen from maximized");
      pump();
      verifyWindowed(normalSize,normalRectangle);

      Check(manager.SetResolution({1000,640}),"Could not queue a windowed resolution before fullscreen");
      Check(manager.SetFullscreen(true) && manager.ApplyPending(),"Combined resolution and fullscreen request failed");
      pump();
      verifyFullscreen();
      Check(manager.WindowedSize()==sf::Vector2u{1000,640},"Fullscreen discarded a pending windowed resolution");
      Check(manager.SetFullscreen(false) && manager.ApplyPending(),"Could not leave combined resolution/fullscreen request");
      pump();
      RECT resizedRectangle=normalRectangle;
      resizedRectangle.right+=40;
      resizedRectangle.bottom+=40;
      verifyWindowed({1000,640},resizedRectangle);

      Check(manager.SetFullscreen(true) && manager.ApplyPending(),"Could not enter fullscreen before loading preferences");
      pump();
      verifyFullscreen();
      manager.LoadPreferences(nlohmann::json{{"fullscreen",false},{"window_width",1040},{"window_height",680},
                                           {"fullscreen_width",physicalSize.x},{"fullscreen_height",physicalSize.y}});
      Check(manager.ApplyPending(),"Windowed preferences did not queue a fullscreen exit");
      pump();
      resizedRectangle.right+=40;
      resizedRectangle.bottom+=40;
      verifyWindowed({1040,680},resizedRectangle);
      window.close();
      VerifyDisplayModes(monitors);
    }
    std::cout<<"Windows fullscreen monitor regression: "<<monitors.size()<<" monitor(s) checked\n";
  }
#endif
  void RotPaletteConfiguration() {
    using Json = nlohmann::json;
    const RotPaletteConfig defaults;
    struct Property {
      const char *name;
      sf::Color RotPaletteConfig::*member;
    };
    const std::array<Property, 9> properties{{
      {"sourceShadow", &RotPaletteConfig::sourceShadow}, {"sourceBase", &RotPaletteConfig::sourceBase},
      {"sourceLight", &RotPaletteConfig::sourceLight}, {"sourceGlow", &RotPaletteConfig::sourceGlow},
      {"localFill", &RotPaletteConfig::localFill}, {"localEdge", &RotPaletteConfig::localEdge},
      {"globalFill", &RotPaletteConfig::globalFill}, {"spores", &RotPaletteConfig::spores},
      {"uiText", &RotPaletteConfig::uiText}
    }};
    for (const auto &document : {Json{}, Json::object(), Json{{"rotPalette", nullptr}}, Json{{"rotPalette", 7}}, Json{{"rotPalette", Json::array()}}}) {
      const auto palette = EngineConfig::ReadRotPalette(document);
      for (const auto &property : properties)
        Check(palette.*(property.member) == defaults.*(property.member), "Missing or invalid palette did not preserve defaults");
    }
    for (const auto &property : properties) {
      const auto custom = EngineConfig::ReadRotPalette(Json{{"rotPalette", {{property.name, {0, 127, 255, 0}}}}});
      Check(custom.*(property.member) == sf::Color(0, 127, 255, 0), "RGBA boundaries or alpha were not loaded");
      for (const auto &invalid : {Json{}, Json::object(), Json::array(), Json{1, 2, 3}, Json{1, 2, 3, 4, 5}, Json{-1, 2, 3, 4}, Json{256, 2, 3, 4}, Json{1.5, 2, 3, 4}, Json{true, 2, 3, 4}, Json{"1", 2, 3, 4}, Json{std::numeric_limits<std::uint64_t>::max(), 2, 3, 4}}) {
        auto document = Json{{"rotPalette", {{property.name, invalid}}}};
        const auto result = EngineConfig::ReadRotPalette(document);
        for (const auto &other : properties)
          Check(result.*(other.member) == defaults.*(other.member), "Invalid color altered a property instead of its fallback");
      }
    }
    const auto independent = EngineConfig::ReadRotPalette(Json{{"rotPalette", {{"sourceBase", {999, 0, 0, 255}}, {"uiText", {9, 8, 7, 6}}}}});
    Check(independent.sourceBase == defaults.sourceBase && independent.uiText == sf::Color(9, 8, 7, 6), "Fallback leaked from one color into another");
    const auto alpha = EngineConfig::ReadRotPalette(Json{{"rotPalette", {{"sourceBase", {120, 40, 210, 128}}}}});
    Check(alpha.SpriteColor({100, 125, 60, 128}) == sf::Color(120, 40, 210, 64), "Configured sprite alpha was not composed with asset alpha");
    Check(defaults.SpriteColor({100, 125, 60, 0}) == sf::Color(100, 125, 60, 0), "Palette changed transparent pixels");
    for (const auto material : {sf::Color{16, 27, 37}, sf::Color{25, 44, 50}, sf::Color{229, 204, 107}, sf::Color{101, 177, 216}})
      Check(defaults.SpriteColor(material) == material, "Palette altered unrelated material or selection/team color");
    const std::array<std::pair<sf::Color, sf::Color>, 4> materials{{
      {{51, 69, 53}, defaults.sourceShadow}, {{100, 125, 60}, defaults.sourceBase},
      {{159, 179, 79}, defaults.sourceLight}, {{213, 212, 122}, defaults.sourceGlow}
    }};
    for (const auto &[original, replacement] : materials)
      Check(defaults.SpriteColor(original) == replacement, "An original rot sprite material remained green");
    struct Region {
      const char *file;
      sf::Vector2u position, size;
    };
    for (const auto &region : {Region{"Content/Textures/Plaguefront/Pixel/props.png", {448, 0}, {64, 320}},
                               Region{"Content/Textures/Plaguefront/Pixel/effects.png", {288, 0}, {48, 192}},
                               Region{"Content/Textures/Plaguefront/Pixel/icons.png", {64, 32}, {16, 16}}}) {
      const sf::Image source(region.file);
      unsigned changed = 0;
      for (unsigned y = 0; y < region.size.y; ++y)
        for (unsigned x = 0; x < region.size.x; ++x) {
          const auto original = source.getPixel(region.position + sf::Vector2u{x, y});
          const auto mapped = defaults.SpriteColor(original);
          if (original.a > 0 && original.g > original.r && original.g > original.b)
            Check(mapped != original, "An opaque green rot material has no palette entry");
          if (mapped != original) {
            ++changed;
            Check(mapped.r > mapped.g && mapped.b > mapped.g, "Default sprite remap is not purple");
            Check(mapped.a == original.a, "Default sprite remap lost original opacity");
          }
        }
      Check(changed > 0, "Rot atlas region had no recolored material");
    }
  }
}

int main() {
  try {
    sf::Listener::setGlobalVolume(0);
    RotPaletteConfiguration();
    const std::filesystem::path directory = "build/test-output/phase5-engine";
    std::filesystem::create_directories(directory);
    const auto settings = directory / "preferences.json";
    {
      SaveManager save(settings);
      save.Clear();
      save.Set("Clicker:Best Time", 7.25f);
      save.Set("Bounce:High Score", 42);
      Check(save.Flush(), "SaveManager flush failed");
      Check(!std::filesystem::exists(settings.string()+".tmp"), "Atomic save left temporary document");
    }
    {
      SaveManager save(settings);
      Check(save.Get<float>("Clicker:Best Time",0.f)==7.25f, "Clicker record compatibility failed");
      Check(save.Get<int>("Bounce:High Score",0)==42, "Bounce record compatibility failed");
      Check(save.Get<int>("Clicker:Best Time",0)==7, "Existing arithmetic JSON conversion changed");
      save.Set("invalid", "string");
      Check(save.Get<int>("invalid",19)==19, "Wrong preference type must use fallback");
      auto values=nlohmann::json{{"window_width",1280},{"window_height",720},{"fullscreen",false}};
      Check(save.WriteDocument(directory/"legacy-settings.json",values), "Named settings write failed");
      Check(save.ReadDocument(directory/"legacy-settings.json")==values, "Named settings round trip failed");
      Check(save.WriteDocument(directory/"legacy-settings.json",nlohmann::json{{"replacement",true}}), "Atomic replacement failed");
      Check(save.ReadDocument(directory/"legacy-settings.json")->value("replacement",false), "Atomic replacement did not replace old document");
      save.Erase("invalid");
      Check(!save.Has("invalid"), "Existing erase API changed");
      save.Clear();
      Check(save.Flush(), "Clearing save failed");
    }
    Check(SaveManager(settings).ReadDocument(settings)==nlohmann::json::object(), "Cleared save resurrected records");
    const auto malformed = directory/"malformed.json";
    { std::ofstream file(malformed); file<<"{invalid"; }
    {
      SaveManager save(malformed);
      Check(!save.ReadDocument(malformed), "Malformed document was accepted");
      Check(save.Get<int>("missing",23)==23, "Malformed legacy save did not recover safely");
    }

    sf::RenderWindow window(sf::VideoMode({1440,900}),"Plaguefront engine manager checks");
    WindowManager manager(window,false);
    RenderManager renderer;
    Check(!renderer.ResponsiveLayout(), "Legacy scenes must retain configured coordinates");
    renderer.ConfigureOutput({1280,720});
    Check(renderer.LogicalSize()==gConfig.windowSize, "Clicker and Bounce coordinate space changed");
    renderer.SetResponsiveLayout(true);
    const auto pump = [&] {
      while(const auto event=window.pollEvent()) {
        if(const auto *resized=event->getIf<sf::Event::Resized>()) {
          manager.OnResized(resized->size);
          renderer.ConfigureOutput(resized->size);
        }
      }
      renderer.ConfigureOutput(window.getSize());
    };
    const auto verifyMapping = [&] {
      const auto size=window.getSize();
      for(const sf::Vector2i pixel : {sf::Vector2i{0,0},sf::Vector2i{static_cast<int>(size.x/2),static_cast<int>(size.y/2)},sf::Vector2i{static_cast<int>(size.x-1),static_cast<int>(size.y-1)}}) {
        const auto logical=renderer.PixelToLogical(pixel);
        Check(renderer.LogicalToPixel(logical)==pixel,"Physical/logical coordinates did not round trip");
        Check(Close(window.mapPixelToCoords(pixel,renderer.View()),logical),"SFML and manager mouse mappings differ");
      }
      const auto logical=renderer.LogicalSize();
      Check(std::abs(logical.x/logical.y-static_cast<float>(size.x)/size.y)<0.001f,"Responsive interface was stretched");
      Check(logical.x>=gConfig.referenceSize.x-0.01f && logical.y>=gConfig.referenceSize.y-0.01f,"Responsive canvas clipped its reference bounds");
    };
    for(const sf::Vector2u size : {sf::Vector2u{1280,720},{1366,768},{1440,900},{1920,1080},{1000,1000}}) {
      manager.SetResolution(size); manager.ApplyPending(); pump();
      Check(window.getSize()==size,"Window manager did not apply the selected resolution");
      verifyMapping();
      const auto revision=renderer.Revision();
      renderer.ConfigureOutput(size);
      Check(renderer.Revision()==revision,"Unchanged frames unnecessarily invalidated layout");
    }
    for(unsigned step=0;step<24;++step) {
      const sf::Vector2u size{1000+step*17,700+step*7};
      window.setSize(size); pump();
      Check(manager.WindowedSize()==size,"Native resize event did not update preferences");
      verifyMapping();
    }
    const auto restored=window.getSize();
    Check(manager.Maximize(),"Native maximize did not apply"); pump(); verifyMapping();
    Check(manager.IsMaximized(),"Native maximized state was not recognized");
    Check(manager.Restore(),"Native restore did not apply"); pump(); verifyMapping();
    Check(window.getSize()==restored,"Restore did not preserve the prior native window dimensions");
    manager.SetFullscreen(true); manager.ApplyPending(); pump();
    Check(manager.IsFullscreen() && window.getSize()==manager.FullscreenResolution(),"Full-screen transition failed");
    verifyMapping();
    manager.SetFullscreen(false); manager.ApplyPending(); pump();
    Check(window.getSize()==restored,"Leaving full screen lost the window dimensions");
    verifyMapping();
    nlohmann::json preferences;
    manager.StorePreferences(preferences);
    manager.SetResolution({1280,720}); manager.ApplyPending(); pump();
    manager.LoadPreferences(preferences); manager.ApplyPending(); pump();
    Check(window.getSize()==restored,"Legacy graphic preference keys did not round trip through WindowManager");
#ifdef _WIN32
    MonitorFullscreenRegression();
#endif

    // SetView debe actualizar tanto las conversiones de entrada como el dibujo.
    sf::View view({40.f,60.f},{220.f,140.f});
    view.setRotation(sf::degrees(17)); view.setViewport({{0.1f,0.15f},{0.7f,0.75f}});
    renderer.SetView(view);
    for(const sf::Vector2i point : {sf::Vector2i{0,0},{200,300},{800,500}}) {
      Check(Close(renderer.PixelToLogical(point),window.mapPixelToCoords(point,view)),"Custom view conversion ignored its center, rotation or viewport");
      Check(renderer.LogicalToPixel(renderer.PixelToLogical(point))==point,"Custom view failed inverse conversion");
    }
    renderer.ConfigureOutput(window.getSize());
    Check(renderer.View().getRotation()==view.getRotation(),"Unchanged output discarded the caller's view");
    renderer.ResetView();
    Check(renderer.View().getRotation()==sf::Angle::Zero,"ResetView did not restore the default interface view");

    ScreenshotManager screenshot(window);
    sf::RenderTexture frame({144,90}); frame.clear(sf::Color(12,31,48)); frame.display();
    Check(screenshot.SaveFrame(frame.getTexture(),{1366,768},(directory/"composed.png").string(),[](sf::RenderTarget &target){
      sf::RectangleShape marker({20,20}); marker.setPosition({100,100}); marker.setFillColor(sf::Color::Red); target.draw(marker);
    }),"ScreenshotManager composition failed");
    sf::Image image(directory/"composed.png");
    Check(image.getSize()==sf::Vector2u{1366,768},"Screenshot ignored physical output resolution");
    Check(image.getPixel({400,400})==sf::Color(12,31,48),"Screenshot lost the application frame");
    Check(image.getPixel({110,110})==sf::Color::Red,"Screenshot lost the output-resolution overlay");

    ResourceManager resources;
    Check(!resources.FetchTexture("phase5-missing.png")&&!resources.FetchTexture("phase5-missing.png"),"Failed texture became a false successful cached resource");
    Check(!resources.FetchFont("phase5-missing.ttf")&&!resources.FetchFont("phase5-missing.ttf"),"Failed font became a false successful cached resource");
    Check(!resources.FetchSound("phase5-missing.wav")&&!resources.FetchSound("phase5-missing.wav"),"Failed sound became a false successful cached resource");
    const auto *texture=resources.FetchTexture("Target.png");
    Check(texture&&resources.FetchTexture("Target.png")==texture,"Clicker texture no longer uses the resource cache");
    const auto *font=resources.FetchFont("Relidux.otf");
    Check(font&&resources.FetchFont("Relidux.otf")==font,"Shared font no longer uses the resource cache");
    window.close();
    std::cout<<"Engine managers: "<<checks<<" checks passed; 24 native resize events, maximize/restore, fullscreen, 5 resolutions, view mappings, atomic preferences, cached resources and composed screenshot\n";
    return 0;
  } catch(const std::exception &error) {
    std::cerr<<"Engine manager check failed after "<<checks<<" checks: "<<error.what()<<'\n';
    return 1;
  }
}
