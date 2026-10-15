#include "Managers/InputManager.h"
#include "Managers/SaveManager.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
int checks=0,groups=0;
void Check(bool value,const char *message){++checks;if(!value)throw std::runtime_error(message);}
Input::DeviceCapabilities Capabilities(unsigned vendor=0x045e,unsigned buttons=10) {
  Input::DeviceCapabilities info;info.connected=true;info.vendorId=vendor;info.buttonCount=buttons;info.name="Simulated SFML event device";
  for(auto axis:{sf::Joystick::Axis::X,sf::Joystick::Axis::Y,sf::Joystick::Axis::Z,sf::Joystick::Axis::PovX,sf::Joystick::Axis::PovY})info.axes[static_cast<unsigned>(axis)]=true;
  return info;
}
void Press(InputManager &input,unsigned button,unsigned id=0) {input.ProcessEvent(sf::Event::JoystickButtonPressed{id,button});input.Update(.016f);}
void Release(InputManager &input,unsigned button,unsigned id=0) {input.ProcessEvent(sf::Event::JoystickButtonReleased{id,button});input.Update(.016f);}
void Axis(InputManager &input,sf::Joystick::Axis axis,float position,unsigned id=0,float dt=.016f){input.ProcessEvent(sf::Event::JoystickMoved{id,axis,position});input.Update(dt);}
void DevicesAndCompatibility() {
  ++groups;InputManager input(false);Check(!input.GamepadAvailable()&&input.Device()==Input::Device::KeyboardMouse,"No-device fallback unavailable");
  input.Bind(7,Input::Gamepad{GamepadButton::South,3});input.RegisterDevice(3,Capabilities());
  Check(input.ActiveGamepad()==3u&&input.DeviceInfo(3)->buttonCount==10,"Actual connected id/capability not retained");
  Check(input.HardwareButton(GamepadButton::South,3)==0u,"Xbox lower button mapping incorrect");
  Check(input.HardwareButton(GamepadButton::West,3)==2u,"Xbox left button mapping incorrect");
  Check(!input.HardwareButton(GamepadButton::L2,3),"Xbox combined trigger invented a hardware button");
  Press(input,0,3);Check(input.Pressed(7),"Clicker existing Bind/Pressed interface broken");
  Check(input.Consume(Input::PadAction::Confirm)&&!input.Consume(Input::PadAction::Confirm),"Confirm edge consumed more than once");
  for(int i=0;i<100;++i){input.Update(.02f);Check(!input.Consume(Input::PadAction::Confirm),"Held button repeats confirm");}
  input.ProcessEvent(sf::Event::JoystickButtonPressed{3,0});input.Update(.016f);Check(!input.Consume(Input::PadAction::Confirm),"Repeated physical press produced a held edge");
  Release(input,0,3);Check(!input.Pressed(7),"Release did not clear legacy Pressed");Press(input,0,3);Check(input.Consume(Input::PadAction::Confirm),"Release/repress did not restore edge");
  input.ProcessEvent(sf::Event::JoystickDisconnected{3});input.Update(.016f);
  Check(!input.GamepadAvailable()&&!input.Pressed(7)&&input.Device()==Input::Device::KeyboardMouse,"Disconnect left controls blocked");
  input.RegisterDevice(5,Capabilities(0x054c,4));Press(input,1,5);Check(input.Consume(Input::PadAction::Confirm),"Sony profile/hotplug did not confirm");
  Check(!input.HardwareButton(GamepadButton::R3,5),"Small device manufactured absent buttons");
  input.ProcessEvent(sf::Event::JoystickButtonPressed{5,31});input.ProcessEvent(sf::Event::JoystickMoved{8,sf::Joystick::Axis::X,80});input.Update(.016f);
  Check(!input.Consume(Input::PadAction::Pause),"Invalid button/id changed controller state");
  Check(!Input::LogicalToHardware(GamepadButton::South,-1)&&!Input::HardwareToLogical(0,99),"Legacy mapping accepts invalid devices");
}
void DeadzoneAndRepeat() {
  ++groups;InputManager input(false);input.RegisterDevice(0,Capabilities());
  Axis(input,sf::Joystick::Axis::X,12);Check(input.PointerDirection()==sf::Vector2f{}&&input.NavigationPulse()==sf::Vector2i{},"Deadzone drift moved cursor");
  Axis(input,sf::Joystick::Axis::X,75);Check(input.NavigationPulse()==sf::Vector2i{1,0},"Initial navigation pulse absent");
  Check(input.PointerDirection().x>0&&input.PointerDirection().x<=1,"Analog normalization invalid");
  input.Update(.18f);Check(input.NavigationPulse()==sf::Vector2i{},"Stick repeated before initial delay");
  input.Update(.15f);Check(input.NavigationPulse()==sf::Vector2i{1,0},"Held stick did not repeat after delay");
  input.Update(.08f);Check(input.NavigationPulse()==sf::Vector2i{},"Repeat interval ignored");
  input.Update(.05f);Check(input.NavigationPulse()==sf::Vector2i{1,0},"Timed repeat absent");
  Axis(input,sf::Joystick::Axis::X,0);Check(input.NavigationPulse()==sf::Vector2i{},"Centered stick kept repeating");
  Axis(input,sf::Joystick::Axis::PovY,100);Check(input.NavigationPulse()==sf::Vector2i{0,-1},"POV up orientation incorrect");
  Axis(input,sf::Joystick::Axis::PovY,0);auto controls=input.Controls();controls.invertX=true;controls.invertY=true;input.Configure(controls);input.Update(.016f);
  Axis(input,sf::Joystick::Axis::X,80);Check(input.NavigationPulse()==sf::Vector2i{-1,0}&&input.PointerDirection().x<0,"Configured axis inversion ignored");
  Axis(input,sf::Joystick::Axis::Y,80);Check(input.NavigationPulse()==sf::Vector2i{-1,-1},"Simultaneous direction input lost");
}
void SuppressionAndModeSwitch() {
  ++groups;InputManager input(false);input.RegisterDevice(0,Capabilities());Press(input,0);Check(input.Consume(Input::PadAction::Confirm),"Initial confirm missing");
  input.SuppressUntilReleased();for(int i=0;i<30;++i){input.Update(.03f);Check(!input.Consume(Input::PadAction::Confirm),"Handoff carried held confirmation");}
  input.ProcessEvent(sf::Event::JoystickButtonPressed{0,0});input.Update(.016f);Check(!input.Consume(Input::PadAction::Confirm),"Held duplicate bypassed handoff suppression");
  Release(input,0);Press(input,0);Check(input.Consume(Input::PadAction::Confirm),"Neutral release did not unblock handoff");Release(input,0);
  Axis(input,sf::Joystick::Axis::X,90);input.SuppressUntilReleased();input.Update(.5f);Check(input.NavigationPulse()==sf::Vector2i{},"Handoff carried held stick");
  Axis(input,sf::Joystick::Axis::X,0);Axis(input,sf::Joystick::Axis::X,90);Check(input.NavigationPulse().x==1,"Neutral stick did not rearm");
  input.ProcessEvent(sf::Event::MouseMoved{{20,30}});input.Update(.02f);Check(input.Device()==Input::Device::KeyboardMouse&&input.NavigationPulse()==sf::Vector2i{},"Mouse did not reclaim input");
  Axis(input,sf::Joystick::Axis::X,-90);Check(input.Device()==Input::Device::Gamepad,"Controller did not reclaim input");
  input.ProcessEvent(sf::Event::KeyPressed{});input.Update(.02f);Check(input.Device()==Input::Device::KeyboardMouse,"Keyboard did not reclaim input");
  input.ProcessEvent(sf::Event::FocusLost{});Press(input,0);Check(!input.Consume(Input::PadAction::Confirm),"Unfocused window accepted gamepad command");
  input.ProcessEvent(sf::Event::FocusGained{});Axis(input,sf::Joystick::Axis::X,0);Release(input,0);Press(input,0);Check(input.Consume(Input::PadAction::Confirm),"Focus regain did not rearm controls");
}
void TriggersAndCustomMapping() {
  ++groups;InputManager input(false);input.RegisterDevice(0,Capabilities());
  Axis(input,sf::Joystick::Axis::Z,90);Check(input.Consume(Input::PadAction::ZoomOut),"Combined left trigger unavailable");
  input.Update(.5f);Check(!input.Consume(Input::PadAction::ZoomOut),"Held trigger repeated edge");
  Axis(input,sf::Joystick::Axis::Z,0);Axis(input,sf::Joystick::Axis::Z,-90);Check(input.Consume(Input::PadAction::ZoomIn),"Combined right trigger unavailable");
  Axis(input,sf::Joystick::Axis::Z,0);auto controls=input.Controls();controls.hardwareButtons[static_cast<unsigned>(GamepadButton::South)]=4;input.Configure(controls);input.Update(.016f);
  Press(input,0);Check(!input.Consume(Input::PadAction::Confirm),"Physical override retained old button");Release(input,0);
  Press(input,4);Check(input.Consume(Input::PadAction::Confirm),"Physical override not honored");Release(input,4);
  controls.actionButtons[static_cast<unsigned>(Input::PadAction::EndTurn)]=GamepadButton::North;input.Configure(controls);input.Update(.016f);Press(input,3);
  Check(input.Consume(Input::PadAction::EndTurn),"Configurable action button ignored");Release(input,3);
  controls.profile=Input::GamepadProfile::PlayStation;input.Configure(controls);input.Update(.016f);Check(input.HardwareButton(GamepadButton::East,0)==2u,"Explicit profile override ignored");
}
void PreferenceRoundTrip() {
  ++groups;const auto path=std::filesystem::path("build/test-output/phase5-input-preferences.json");std::filesystem::create_directories(path.parent_path());
  Input::GamepadConfig expected;
  {
    SaveManager save(path);InputManager input(false);auto controls=input.Controls();controls.deadzone=.35f;controls.sensitivity=1.75f;controls.invertY=true;
    controls.profile=Input::GamepadProfile::Generic;controls.hardwareButtons[2]=13;controls.actionButtons[0]=GamepadButton::North;
    input.Configure(controls);expected=input.Controls();input.SavePreferences(save);Check(save.Flush(),"Common SaveManager failed to persist input");
  }
  {
    SaveManager save(path);InputManager restored(false);restored.LoadPreferences(save);Check(restored.Controls()==expected,"Input preferences did not round trip through common persistence");
    save.Set(Input::PreferencesKey,nlohmann::json{{"deadzone","invalid"}});restored.LoadPreferences(save);Check(restored.Controls()==Input::GamepadConfig{},"Malformed preferences did not use defaults");
    save.Set(Input::PreferencesKey,nlohmann::json{{"deadzone",9.f},{"sensitivity",-.5f},{"repeat_interval",0.f},{"profile",42},{"stick_x",99}});
    restored.LoadPreferences(save);Check(restored.Controls().deadzone==.8f&&restored.Controls().sensitivity==.25f&&restored.Controls().repeatInterval==.06f,"Input bounds not sanitized");
    Check(restored.Controls().profile==Input::GamepadProfile::Auto&&restored.Controls().stickX==sf::Joystick::Axis::X,"Invalid enum not sanitized");
    save.Erase(Input::PreferencesKey);
  }
  std::filesystem::remove(path);
}
}
int main() {
  try {DevicesAndCompatibility();DeadzoneAndRepeat();SuppressionAndModeSwitch();TriggersAndCustomMapping();PreferenceRoundTrip();
    std::cout<<"InputManager: "<<groups<<" groups / "<<checks<<" checks, actual SFML event types; no physical controller assumed\n";return 0;
  }catch(const std::exception &error){std::cerr<<"InputManager failed after "<<checks<<" checks: "<<error.what()<<'\n';return 1;}
}
