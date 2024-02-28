#include <nged/style.h>

namespace nged {

UIStyle& UIStyle::instance()
{
  static UIStyle theInstance_;
  return theInstance_;
}

void UIStyle::save()
{ }

void UIStyle::load()
{ }

} // namespace nged

