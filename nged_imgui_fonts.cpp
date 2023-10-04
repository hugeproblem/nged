#include "style.h"

#include "res/fa_icondef.h"
#include "res/fa_solid.hpp"
#include "res/roboto_medium.hpp"
#include "res/sourcecodepro.hpp"

#include <imgui.h>

namespace nged {
namespace detail {

void reloadImGuiFonts(ImFont* &sansSerif, ImFont* &mono, ImFont* &icon, ImFont* &large, ImFont* &largeIcon)
{
  auto* atlas = ImGui::GetIO().Fonts;
  auto const& style = UIStyle::instance();
  static const ImWchar rangesIcons[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

  atlas->Clear();
  sansSerif = atlas->AddFontFromMemoryCompressedTTF(roboto_medium_compressed_data, roboto_medium_compressed_size, style.normalFontSize, nullptr, atlas->GetGlyphRangesGreek());
  large = atlas->AddFontFromMemoryCompressedTTF(roboto_medium_compressed_data, roboto_medium_compressed_size, style.bigFontSize*2, nullptr, atlas->GetGlyphRangesGreek());
  mono = atlas->AddFontFromMemoryCompressedTTF(sourcecodepro_compressed_data, sourcecodepro_compressed_size, style.normalFontSize, nullptr, atlas->GetGlyphRangesGreek());
  icon = atlas->AddFontFromMemoryCompressedTTF(FontAwesomeSolid_compressed_data, FontAwesomeSolid_compressed_size, style.normalFontSize, nullptr, rangesIcons);
  largeIcon = atlas->AddFontFromMemoryCompressedTTF(FontAwesomeSolid_compressed_data, FontAwesomeSolid_compressed_size, style.bigFontSize*2, nullptr, rangesIcons);
}

} // namespace detail
} // namespace nged
