#include "ngs7.h"

#include "../nged.h"
#include "../nged_imgui.h"
#include "../style.h"
#include "../entry/entry.h"

#include <spdlog/spdlog.h>
#ifdef _WIN32
#include <spdlog/sinks/wincolor_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <spdlog/sinks/ansicolor_sink.h>
#endif

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <misc/cpp/imgui_stdlib.cpp>

#include <chrono>


class S7App: public nged::App
{
  nged::EditorPtr editor = nullptr;
public:
  void init() override
  {
#ifdef _WIN32
    auto hIcon = LoadIcon(GetModuleHandle(nullptr), TEXT("0"));
    auto setIcon = [](HWND hWnd, LPARAM hIcon) -> BOOL {
      SendMessage(hWnd, WM_SETICON, ICON_BIG, hIcon);
      SendMessage(hWnd, WM_SETICON, ICON_SMALL, hIcon);
      SendMessage(hWnd, WM_SETICON, ICON_SMALL2, hIcon);
      return TRUE;
    };
    if (hIcon)
      EnumThreadWindows(GetCurrentThreadId(), setIcon, (LPARAM)hIcon);

    spdlog::set_default_logger(std::make_shared<spdlog::logger>("", std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>()));
    spdlog::default_logger()->sinks().emplace_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#else
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("", std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>()));
#endif

#ifdef DEBUG
    spdlog::set_level(spdlog::level::trace);
#else
    spdlog::set_level(spdlog::level::warn);
#endif

    nged::App::init();

    editor = nged::newImGuiNodeGraphEditor();
    
    ngs7::initEditor(editor.get());

    editor->createNewDocAndDefaultViews();
  }

  char const* title() { return "S7"; }
  bool agreeToQuit()
  {
    return editor->agreeToQuit();
  }
  void update()
  {
    static auto prev= std::chrono::system_clock::now();
    auto now = std::chrono::system_clock::now();
    auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - prev);
    ImGui::PushFont(nged::ImGuiResource::instance().sansSerifFont);
    editor->update(dt.count()/1000.f);
    editor->draw();
    ImGui::PopFont();
    prev = now;
  }
  void quit()
  {
  }

}; // class S7App

int main()
{
  nged::startApp(new S7App);
  return 0;
}
