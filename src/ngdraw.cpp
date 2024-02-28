#include <nged/ngdoc.h>
#include <nged/style.h>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace nged {

// Node {{{
void Node::draw(Canvas* canvas, GraphItemState state) const
{
  auto box       = aabb();
  bool highlight = false;
  auto hlcolor   = color_;
  auto style     = Canvas::ShapeStyle{
    true,                 // filled
    toUint32RGBA(color_), // fillColor
    0.f,                  // strokeWidth
    0                     // strokeColor
  };

  if (state == GraphItemState::SELECTED) {
    highlight = true;
    // auto hsl = toHSL(toLinear(color_));
    // hsl.s = clamp(hsl.s * 1.1f, 0.f, 1.f);
    // hsl.l = clamp(hsl.l * 1.4f, 0.f, 1.f);
    // hlcolor = toSRGB(toLinear(hsl));
    // style.fillColor = toUint32RGBA(hlcolor);
    hlcolor = {255, 255, 255, 255};
  } else if (state == GraphItemState::HOVERED) {
    highlight = true;
    hlcolor   = color_;
    hlcolor.a *= 0.5;
  } else if (state == GraphItemState::DESELECTED) {
    highlight = true;
    // auto hsv = toHSV(toLinear(color_));
    // hsv.s *= 0.7f;
    // hsv.v *= 0.7f;
    // hlcolor = toSRGB(toLinear(hsv));
    // style.fillColor = toUint32RGBA(hlcolor);
    hlcolor = {64, 64, 64, 255};
  }

  // the node
  float const limit = 0.2f;
  if (canvas->viewScale() < limit) {
    auto localbb = localBound();
    localbb.min /= canvas->viewScale() / limit;
    localbb.max /= canvas->viewScale() / limit;
    auto scaledbox = localbb.moved(pos());
    auto c         = color_;
    c.a *= 0.66f;
    style.fillColor = toUint32RGBA(c);
    canvas->drawRect(scaledbox.min, scaledbox.max, 0, style);
  } else {
    canvas->drawRect(box.min, box.max, 5, style);

    // highlights
    if (highlight) {
      auto hlstyle = Canvas::ShapeStyle{false, 0x0, 2.0f, toUint32RGBA(hlcolor)};
      canvas->drawRect(box.min - Vec2{4, 4}, box.max + Vec2{4, 4}, 9, hlstyle);
    }

    // pins
    if (numMaxInputs() > 0) {
      for (sint i = 0; i < numMaxInputs(); ++i) {
        auto pinstyle = style;
        if (InputConnection input; parent() && parent()->getLinkSource(id(), i, input)) {
          if (auto srcitem = parent()->get(input.sourceItem)) {
            if (auto* node = srcitem->asNode())
              pinstyle.fillColor = gmath::toUint32RGBA(node->outputPinColor(input.sourcePort));
            else if (auto* router = srcitem->asRouter())
              pinstyle.fillColor = gmath::toUint32RGBA(router->linkColor());
          }
        } else {
          pinstyle.fillColor = gmath::toUint32RGBA(inputPinColor(i));
        }
        canvas->drawCircle(inputPinPos(i), UIStyle::instance().nodePinRadius, 0, pinstyle);
      }
    } else if (numMaxInputs() < 0) {
      if (AABB bb; mergedInputBound(bb))
        canvas->drawRect(bb.min, bb.max, UIStyle::instance().nodePinRadius, style);
    }
    
    {
      auto pinstyle = style;
      for (sint i = 0; i < numOutputs(); ++i) {
        pinstyle.fillColor = gmath::toUint32RGBA(outputPinColor(i));
        canvas->drawCircle(outputPinPos(i), UIStyle::instance().nodePinRadius, 0, pinstyle);
      }
    }

    // label
    auto label = this->label();
    if (!label.empty() && canvas->viewScale() > 0.3f) {
      auto textStyle  = Canvas::defaultTextStyle;
      textStyle.color = style.fillColor;
      auto labelpos   = Vec2{box.max.x + 8, box.center().y};
      canvas->drawText(labelpos, label, textStyle);
    }

    // icon
    IconType   iconType;
    StringView iconData;
    if (getIcon(iconType, iconData)) {
      auto iconStyle = Canvas::defaultTextStyle;
      iconStyle.align = Canvas::TextAlign::Center;
      iconStyle.style = Canvas::FontStyle::Strong;
      if (iconType == IconType::IconFont) {
        iconStyle.font = Canvas::FontFamily::Icon;
      } else if (iconType == IconType::Text) {
        iconStyle.font = Canvas::FontFamily::SansSerif;
      }
      if (int(color_.r)+int(color_.g)+int(color_.b) >= 128*3)
        iconStyle.color = 0x000000ff;
      else
        iconStyle.color = 0xffffffff;
      canvas->drawText(pos(), iconData, iconStyle);
    }
  }
}
// }}} Node

// Link {{{
void Link::draw(Canvas* canvas, GraphItemState state) const
{
  auto style = Canvas::ShapeStyle{
    false,                               // filled
    0,                                   // fillColor
    UIStyle::instance().linkStrokeWidth, // strokeWidth
    UIStyle::instance().linkDefaultColor};
  if (auto srcitem = parent()->get(input_.sourceItem)) {
    if (auto* dye = srcitem->asDyeable(); dye && !canvas->displayTypeHint()) {
      style.strokeColor = gmath::toUint32RGBA(dye->color());
    } else if (auto* node = srcitem->asNode()) {
      style.strokeColor = gmath::toUint32RGBA(node->outputPinColor(input_.sourcePort));
    } else if (auto* router = srcitem->asRouter()) {
      style.strokeColor = gmath::toUint32RGBA(router->linkColor());
    } else if (auto* dye = srcitem->asDyeable()) {
      style.strokeColor = gmath::toUint32RGBA(dye->color());
    }
  }
  canvas->pushLayer(Canvas::Layer::Low);
  if (state == GraphItemState::SELECTED) {
    const Canvas::ShapeStyle hlstyle = {
      false, 0, UIStyle::instance().linkSelectedWidth, UIStyle::instance().linkSelectedColor};
    canvas->drawPoly(path_.data(), path_.size(), false, hlstyle);
  }
  canvas->drawPoly(path_.data(), path_.size(), false, style);
  canvas->popLayer();
}
// }}} Link

// Router {{{
void Router::draw(Canvas* canvas, GraphItemState state) const
{
  auto style = Canvas::ShapeStyle{
    true,                 // filled
    toUint32RGBA(color_), // fillColor
    0.f,                  // strokeWidth
    0                     // strokeColor
  };
  if (linkColor() != color_) {
    style.strokeWidth = 1;
    style.strokeColor = toUint32RGBA(linkColor_.value());
  }
  if (state == GraphItemState::HOVERED) {
    style.strokeWidth = 2.f;
    style.strokeColor = 0xaaaaaaff;
  } else if (state == GraphItemState::SELECTED) {
    style.strokeWidth = 2.f;
    style.strokeColor = 0xffffffff;
  }

  canvas->drawCircle(pos_, UIStyle::instance().routerRadius, 0, style);
}
// }}} Router

// GroupBox {{{
void GroupBox::draw(Canvas* canvas, GraphItemState state) const
{
  auto borderColor = color();
  borderColor.a = 126;
  if ((int(borderColor.r)+borderColor.g+borderColor.b)>128*3) {
    borderColor.r /= 2;
    borderColor.g /= 2;
    borderColor.b /= 2;
  } else {
    borderColor.r = std::max(255, 2*borderColor.r);
    borderColor.g = std::max(255, 2*borderColor.g);
    borderColor.b = std::max(255, 2*borderColor.b);
  }
  auto bgstyle = Canvas::ShapeStyle{
    true,                     // filled
    toUint32RGBA(color()),    // fillColor
    1.f,                      // strokeWidth
    toUint32RGBA(borderColor) // strokeColor
  };
  if (state == GraphItemState::HOVERED) {
    bgstyle.strokeWidth = 3.f;
    bgstyle.strokeColor = 0xaaaaaaff;
  } else if (state == GraphItemState::SELECTED) {
    bgstyle.strokeWidth = 4.f;
    bgstyle.strokeColor = 0xffffffff;
  }
  auto box = aabb();

  auto headerBR = box.max;
  headerBR.y = box.min.y + UIStyle::instance().groupboxHeaderHeight;
  canvas->pushLayer(Canvas::Layer::Lower);
  canvas->drawRect(box.min, headerBR, 0, bgstyle); // the header
  canvas->drawRect(box.min, box.max, 0, bgstyle);
  canvas->popLayer();
}
// }}} GroupBox

// Comment Box {{{
void CommentBox::draw(Canvas* canvas, GraphItemState state) const
{
  auto bgstyle = Canvas::ShapeStyle{
    true,                           // filled
    toUint32RGBA(backgroundColor_), // fillColor
    0.f,                            // strokeWidth
    0                               // strokeColor
  };
  if (state == GraphItemState::HOVERED) {
    bgstyle.strokeWidth = 2.f;
    bgstyle.strokeColor = 0xaaaaaaff;
  } else if (state == GraphItemState::SELECTED) {
    bgstyle.strokeWidth = 2.f;
    bgstyle.strokeColor = 0xffffffff;
  }

  textSize_ = canvas->measureTextSize(text_, Canvas::defaultTextStyle);
  auto box  = aabb();

  canvas->pushLayer(Canvas::Layer::Low);
  canvas->drawRect(box.min, box.max, 0, bgstyle);

  if (canvas->viewScale() > 0.25f) {
    auto textStyle   = Canvas::defaultTextStyle;
    textStyle.color  = toUint32RGBA(color_);
    textStyle.align  = Canvas::TextAlign::Center;
    textStyle.valign = Canvas::TextVerticalAlign::Center;
    canvas->drawText(pos(), text_, textStyle);
  }
  canvas->popLayer();
}
// }}} Comment Box

// Arrow {{{
void Arrow::draw(Canvas* canvas, GraphItemState state) const
{
  if (thickness_ * canvas->viewScale() < 0.1f)
    return;

  Vec2 line[] = {start(), end()};
  Mat3 rleft = Mat3::fromSRT({1,1}, gmath::pi/6, end());
  Mat3 rright = Mat3::fromSRT({1,1}, gmath::pi/-6, end());
  Vec2 d = (start_ - end_) * 0.25f;
  if (length2(d) > tipSize_*tipSize_)
    d = normalize(d) * tipSize_;

  Vec2 tip[] = {rleft.transformPoint(d), end(), rright.transformPoint(d)};

  canvas->pushLayer(Canvas::Layer::Low);
  if (state == GraphItemState::SELECTED) {
    auto const hlstyle = Canvas::ShapeStyle{
      false, 0,
      thickness_*2, UIStyle::instance().arrowSelectedColor};
    line[1] += normalize(d)*thickness_/2;
    canvas->drawPoly(line, 2, false, hlstyle);
    canvas->drawPoly(tip, 3, false, hlstyle);
  }
  auto const style = Canvas::ShapeStyle{
    false, 0,
    thickness_, toUint32RGBA(color_)};
  if (state == GraphItemState::SELECTED) {
    line[0] -= normalize(d)*thickness_/2;
    tip[0] += normalize(tip[1]-tip[0])*thickness_/2;
    tip[2] += normalize(tip[1]-tip[2])*thickness_/2;
  }
  canvas->drawPoly(line, 2, false, style);
  canvas->drawPoly(tip, 3, false, style);
  canvas->popLayer();
}
// }}} Arrow

} // namespace nged
