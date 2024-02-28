#include "entry.h"
#include "imgui.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <iconv.h>
#endif

namespace nged {

void App::init()
{
  ImGui::GetIO().IniFilename = nullptr; // disable window size/pos/layout store

  ImGui::StyleColorsDark();
  ImVec4* colors = ImGui::GetStyle().Colors;
  colors[ImGuiCol_FrameBg]            = ImVec4(0.28f, 0.28f, 0.28f, 0.54f);
  colors[ImGuiCol_FrameBgHovered]     = ImVec4(0.68f, 0.67f, 0.64f, 0.40f);
  colors[ImGuiCol_FrameBgActive]      = ImVec4(0.45f, 0.45f, 0.45f, 0.67f);
  colors[ImGuiCol_TitleBgActive]      = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
  colors[ImGuiCol_CheckMark]          = ImVec4(0.67f, 0.67f, 0.67f, 1.00f);
  colors[ImGuiCol_SliderGrab]         = ImVec4(0.89f, 0.89f, 0.89f, 1.00f);
  colors[ImGuiCol_SliderGrabActive]   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
  colors[ImGuiCol_Button]             = ImVec4(0.47f, 0.46f, 0.45f, 0.40f);
  colors[ImGuiCol_ButtonHovered]      = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
  colors[ImGuiCol_ButtonActive]       = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
  colors[ImGuiCol_Header]             = ImVec4(0.33f, 0.31f, 0.28f, 0.31f);
  colors[ImGuiCol_HeaderHovered]      = ImVec4(0.26f, 0.26f, 0.26f, 0.80f);
  colors[ImGuiCol_HeaderActive]       = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
  colors[ImGuiCol_SeparatorHovered]   = ImVec4(0.57f, 0.59f, 0.61f, 0.78f);
  colors[ImGuiCol_SeparatorActive]    = ImVec4(0.58f, 0.58f, 0.58f, 1.00f);
  colors[ImGuiCol_ResizeGrip]         = ImVec4(0.48f, 0.48f, 0.48f, 0.25f);
  colors[ImGuiCol_ResizeGripHovered]  = ImVec4(0.60f, 0.60f, 0.60f, 0.67f);
  colors[ImGuiCol_ResizeGripActive]   = ImVec4(0.82f, 0.82f, 0.82f, 0.95f);
  colors[ImGuiCol_Tab]                = ImVec4(0.23f, 0.23f, 0.23f, 0.86f);
  colors[ImGuiCol_TabHovered]         = ImVec4(0.28f, 0.28f, 0.28f, 0.80f);
  colors[ImGuiCol_TabActive]          = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
  colors[ImGuiCol_TabUnfocused]       = ImVec4(0.05f, 0.05f, 0.05f, 0.97f);
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
  colors[ImGuiCol_DockingPreview]     = ImVec4(0.61f, 0.61f, 0.61f, 0.70f);
  colors[ImGuiCol_TextSelectedBg]     = ImVec4(1.00f, 1.00f, 1.00f, 0.35f);
  colors[ImGuiCol_NavHighlight]       = ImVec4(0.78f, 0.78f, 0.78f, 1.00f);
}

std::wstring utf8towstring(std::string_view str)
{
  std::wstring wstr;
#ifdef _WIN32
  wstr.resize(MultiByteToWideChar(CP_UTF8, 0, str.data(), int(str.size()), nullptr, 0));
  MultiByteToWideChar(CP_UTF8, 0, str.data(), int(str.size()), wstr.data(), int(wstr.size()));
#else
  iconv_t cd = iconv_open("WCHAR_T", "UTF-8");
  if (cd == (iconv_t)-1)
    return wstr;
  size_t inbytesleft = str.size();
  size_t outbytesleft = inbytesleft * sizeof(wchar_t);
  wstr.resize(outbytesleft / sizeof(wchar_t));
  char* inbuf = const_cast<char*>(str.data());
  char* outbuf = reinterpret_cast<char*>(wstr.data());
  size_t r = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
  iconv_close(cd);
  if (r == (size_t)-1)
    wstr.clear();
  else
    wstr.resize((outbytesleft / sizeof(wchar_t)) - 1);
#endif
  return wstr;
}

} // namespace nged
