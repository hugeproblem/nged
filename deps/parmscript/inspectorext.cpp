#include "parmscript.h"
#include "parminspector.h"
#include "inspectorext.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <nfd.h>

namespace parmscript {

bool inspectFilePath(Parm& parm)
{
  string path = parm.as<string>();
  auto label = "##" + parm.path();
  bool mod = false;

  auto filters = parm.getMeta<string>("filters", "");
  auto dlg = NFD_OpenDialog;
  if (parm.getMeta<string>("dialog", "open") == "save")
    dlg = NFD_SaveDialog;

  if (ImGui::InputText(label.c_str(), &path, ImGuiInputTextFlags_EnterReturnsTrue)) {
    mod = true;
  }

  ImGui::SameLine();
  auto btlabel = "...##" + parm.path();
  if (ImGui::Button(btlabel.c_str())) {
    char* cpath  = nullptr;
    auto  result = dlg(filters.c_str(), nullptr, &cpath);
    if (result == NFD_OKAY && cpath) {
      if (*cpath) {
        path = cpath;
        mod = true;
      }
      free(cpath);
    }
  }

  ImGui::SameLine();
  ImGui::TextUnformatted(parm.label().c_str());

  if (mod)
    parm.set<string>(path);
  return mod;
}

bool inspectDirPath(Parm& parm)
{
  string path = parm.as<string>();
  string defaultpath = parm.getMeta<string>("defaultpath", "");
  auto label = "##" + parm.path();
  bool mod = false;

  if (ImGui::InputText(label.c_str(), &path, ImGuiInputTextFlags_EnterReturnsTrue)) {
    mod = true;
  }

  ImGui::SameLine();
  auto btlabel = "...##" + parm.path();
  if (ImGui::Button(btlabel.c_str())) {
    char* cpath  = nullptr;
    auto  result = NFD_PickFolder(defaultpath.c_str(), &cpath);
    if (result == NFD_OKAY && cpath) {
      if (*cpath) {
        path = cpath;
        mod = true;
      }
      free(cpath);
    }
  }

  ImGui::SameLine();
  ImGui::TextUnformatted(parm.label().c_str());

  if (mod)
    parm.set<string>(path);
  return mod;
}

static int keepIndentCallback(ImGuiInputTextCallbackData* data)
{
  if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit) {
    if (!data->HasSelection() && ImGui::IsKeyPressed(ImGuiKey_Enter)) {
      char const* lastnonspace = data->Buf + data->CursorPos - 1;
      for (; lastnonspace != data->Buf; --lastnonspace) {
        if (char c = *lastnonspace;
            c != '\n' && c != '\r' && c != '\t' && c != ' ')
          break;
      }
      int space = 0;
      char const* linestart = lastnonspace;
      for (; linestart > data->Buf && *linestart != '\n'; --linestart)
        ;
      if (linestart > data->Buf && *linestart == '\n')
        ++linestart;
      for (; *linestart == ' ' || *linestart == '\t'; ++linestart) {
        if (*linestart == ' ') space += 1;
        else if (*linestart == '\t') space += 4;
        else break;
      }
      if (data->CursorPos >= 1 && (*lastnonspace == ':' || *lastnonspace == '{'))
        space += 2;
      if (space > 0) {
        std::string indent = std::string(space, ' ');
        data->InsertChars(data->CursorPos, indent.c_str(), indent.c_str()+indent.size());
      }
    }
  }
  return 0;
}

bool inspectCode(Parm& parm)
{
  auto label = parm.label() + "##" + parm.path();
  auto* v = parm.getPtr<string>();
  // TODO: use a syntax-highlighting editor
  return ImGui::InputTextMultiline(label.c_str(), v, ImVec2(0,0), ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_CallbackEdit, keepIndentCallback);
}

void addExtensions()
{
  ParmSetInspector::setFieldInspector("file", inspectFilePath);
  ParmSetInspector::setFieldInspector("dir", inspectDirPath);
  ParmSetInspector::setFieldInspector("code", inspectCode);

  ParmSet::preloadScript() += "\nlocal file = alias(text, 'file')\nlocal dir = alias(text, 'dir')\nlocal code = alias(text, 'code', {font='mono', width=-16})";
}

} // namespace parmscript

