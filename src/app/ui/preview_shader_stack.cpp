// LibreSprite preview shader stack
// Copyright (C) 2026
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/ui/preview_shader_stack.h"

#include "app/pref/preferences.h"
#include "doc/color.h"
#include "doc/image.h"
#include "doc/pixel_format.h"
#include "render/zoom.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>
#include <vector>

namespace app {

namespace {

int clamp_channel(int value)
{
  return std::max(0, std::min(255, value));
}

double clamp_strength(double value)
{
  return std::max(0.0, std::min(2.0, value));
}

double clamp_param(double value, double minValue, double maxValue)
{
  return std::max(minValue, std::min(maxValue, value));
}

std::string to_lower(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return char(std::tolower(ch));
  });
  return value;
}

void put_rgb(doc::Image* image, int x, int y, int r, int g, int b)
{
  image->putPixel(x, y, doc::rgba(
    uint8_t(clamp_channel(r)),
    uint8_t(clamp_channel(g)),
    uint8_t(clamp_channel(b)),
    255));
}

struct RgbF {
  double r;
  double g;
  double b;
};

double screen_radius(double sourceRadius, const render::Zoom& zoom)
{
  return clamp_param(sourceRadius * zoom.scale(), 0.0, 256.0);
}

void apply_preferences_to_effect(PreviewShaderStack::Effect& effect)
{
  auto& preview = Preferences::instance().preview;
  effect.enabled = preview.crtEnabled();
  effect.strength = clamp_strength(preview.crtStrength());
  effect.horizontalBlurEnabled = preview.crtHorizontalBlurEnabled();
  effect.horizontalBlurRadius = clamp_param(preview.crtHorizontalBlurRadius(), 0.0, 32.0);
  effect.scanlineEnabled = preview.crtScanlineEnabled();
  effect.scanlineStrength = clamp_param(preview.crtScanlineStrength(), 0.0, 1.0);
  effect.scanlinePower = clamp_param(preview.crtScanlinePower(), 0.1, 8.0);
  effect.bloomEnabled = preview.crtBloomEnabled();
  effect.bloomStrength = clamp_param(preview.crtBloomStrength(), 0.0, 1.5);
  effect.bloomRadius = clamp_param(preview.crtBloomRadius(), 0.0, 32.0);
  effect.bloomThreshold = clamp_param(preview.crtBloomThreshold(), 0.0, 255.0);
}

void save_effect_to_preferences(const PreviewShaderStack::Effect& effect)
{
  auto& preview = Preferences::instance().preview;
  preview.crtEnabled(effect.enabled);
  preview.crtStrength(clamp_strength(effect.strength));
  preview.crtHorizontalBlurEnabled(effect.horizontalBlurEnabled);
  preview.crtHorizontalBlurRadius(clamp_param(effect.horizontalBlurRadius, 0.0, 32.0));
  preview.crtScanlineEnabled(effect.scanlineEnabled);
  preview.crtScanlineStrength(clamp_param(effect.scanlineStrength, 0.0, 1.0));
  preview.crtScanlinePower(clamp_param(effect.scanlinePower, 0.1, 8.0));
  preview.crtBloomEnabled(effect.bloomEnabled);
  preview.crtBloomStrength(clamp_param(effect.bloomStrength, 0.0, 1.5));
  preview.crtBloomRadius(clamp_param(effect.bloomRadius, 0.0, 32.0));
  preview.crtBloomThreshold(clamp_param(effect.bloomThreshold, 0.0, 255.0));
}

void apply_horizontal_blur(doc::Image* image, double radius)
{
  radius = clamp_param(radius, 0.0, 256.0);
  if (radius < 0.5)
    return;

  const int w = image->width();
  const int h = image->height();
  const int r = int(std::ceil(radius));
  std::unique_ptr<doc::Image> src(doc::Image::createCopy(image));
  std::vector<double> prefixR(w+1);
  std::vector<double> prefixG(w+1);
  std::vector<double> prefixB(w+1);

  for (int y=0; y<h; ++y) {
    prefixR[0] = 0.0;
    prefixG[0] = 0.0;
    prefixB[0] = 0.0;
    for (int x=0; x<w; ++x) {
      uint32_t c = src->getPixel(x, y);
      prefixR[x+1] = prefixR[x] + doc::rgba_getr(c);
      prefixG[x+1] = prefixG[x] + doc::rgba_getg(c);
      prefixB[x+1] = prefixB[x] + doc::rgba_getb(c);
    }

    for (int x=0; x<w; ++x) {
      int x0 = std::max(0, x-r);
      int x1 = std::min(w-1, x+r);
      double count = double(x1 - x0 + 1);

      put_rgb(image, x, y,
        int((prefixR[x1+1] - prefixR[x0]) / count),
        int((prefixG[x1+1] - prefixG[x0]) / count),
        int((prefixB[x1+1] - prefixB[x0]) / count));
    }
  }
}

int source_pixel_height_at(const render::Zoom& zoom, int spriteY)
{
  int y0 = zoom.apply(spriteY);
  int y1 = zoom.apply(spriteY+1);
  return std::max(1, y1-y0);
}

double pixel_row_taper_amount(
  const render::Zoom& zoom, int zoomedY, double strength, double power)
{
  if (zoom.scale() < 2.0)
    return 1.0;

  int spriteY = zoom.remove(zoomedY);
  int y0 = zoom.apply(spriteY);
  int pixelHeight = source_pixel_height_at(zoom, spriteY);
  double phase = (double(zoomedY - y0) + 0.5) / double(pixelHeight);
  double edge = std::abs(phase - 0.5) * 2.0;
  double amount = clamp_param(strength, 0.0, 1.0) *
    std::pow(clamp_param(edge, 0.0, 1.0), std::max(0.1, power));
  return 1.0 - amount;
}

void apply_row_taper(
  doc::Image* image,
  const gfx::Point& zoomedSpriteOrigin,
  const render::Zoom& zoom,
  double strength,
  double power)
{
  const int w = image->width();
  const int h = image->height();

  for (int y=0; y<h; ++y) {
    double row = pixel_row_taper_amount(
      zoom, zoomedSpriteOrigin.y + y, strength, power);
    for (int x=0; x<w; ++x) {
      uint32_t c = image->getPixel(x, y);
      put_rgb(image, x, y,
        int(doc::rgba_getr(c) * row),
        int(doc::rgba_getg(c) * row),
        int(doc::rgba_getb(c) * row));
    }
  }
}

void apply_bloom(doc::Image* image, double radius, double strength, double threshold)
{
  radius = clamp_param(radius, 0.0, 256.0);
  strength = clamp_param(strength, 0.0, 1.5);
  threshold = clamp_param(threshold, 0.0, 255.0);
  if (radius < 0.5 || strength <= 0.0)
    return;

  const int w = image->width();
  const int h = image->height();
  const int r = int(std::ceil(radius));
  std::vector<RgbF> bright(w*h);
  std::vector<RgbF> temp(w*h);
  std::vector<RgbF> blur(w*h);
  std::vector<double> prefixR(std::max(w, h)+1);
  std::vector<double> prefixG(std::max(w, h)+1);
  std::vector<double> prefixB(std::max(w, h)+1);

  for (int y=0; y<h; ++y) {
    for (int x=0; x<w; ++x) {
      uint32_t c = image->getPixel(x, y);
      int luma = doc::rgb_luma(doc::rgba_getr(c), doc::rgba_getg(c), doc::rgba_getb(c));
      double gate = threshold >= 255.0 ? 0.0:
        clamp_param((double(luma) - threshold) / (255.0 - threshold), 0.0, 1.0);
      bright[y*w+x] = {
        doc::rgba_getr(c) * gate,
        doc::rgba_getg(c) * gate,
        doc::rgba_getb(c) * gate
      };
    }
  }

  for (int y=0; y<h; ++y) {
    prefixR[0] = 0.0;
    prefixG[0] = 0.0;
    prefixB[0] = 0.0;
    for (int x=0; x<w; ++x) {
      RgbF c = bright[y*w+x];
      prefixR[x+1] = prefixR[x] + c.r;
      prefixG[x+1] = prefixG[x] + c.g;
      prefixB[x+1] = prefixB[x] + c.b;
    }
    for (int x=0; x<w; ++x) {
      int x0 = std::max(0, x-r);
      int x1 = std::min(w-1, x+r);
      double count = double(x1 - x0 + 1);
      temp[y*w+x] = {
        (prefixR[x1+1] - prefixR[x0]) / count,
        (prefixG[x1+1] - prefixG[x0]) / count,
        (prefixB[x1+1] - prefixB[x0]) / count
      };
    }
  }

  for (int x=0; x<w; ++x) {
    prefixR[0] = 0.0;
    prefixG[0] = 0.0;
    prefixB[0] = 0.0;
    for (int y=0; y<h; ++y) {
      RgbF c = temp[y*w+x];
      prefixR[y+1] = prefixR[y] + c.r;
      prefixG[y+1] = prefixG[y] + c.g;
      prefixB[y+1] = prefixB[y] + c.b;
    }

    for (int y=0; y<h; ++y) {
      int y0 = std::max(0, y-r);
      int y1 = std::min(h-1, y+r);
      double count = double(y1 - y0 + 1);
      blur[y*w+x] = {
        (prefixR[y1+1] - prefixR[y0]) / count,
        (prefixG[y1+1] - prefixG[y0]) / count,
        (prefixB[y1+1] - prefixB[y0]) / count
      };
    }
  }

  for (int y=0; y<h; ++y) {
    for (int x=0; x<w; ++x) {
      uint32_t c = image->getPixel(x, y);
      RgbF b = blur[y*w+x];
      put_rgb(image, x, y,
        int(doc::rgba_getr(c) + b.r * strength),
        int(doc::rgba_getg(c) + b.g * strength),
        int(doc::rgba_getb(c) + b.b * strength));
    }
  }
}

void apply_crt(
  doc::Image* image,
  const gfx::Point& zoomedSpriteOrigin,
  const render::Zoom& zoom,
  const PreviewShaderStack::Effect& effect)
{
  if (effect.horizontalBlurEnabled)
    apply_horizontal_blur(image, screen_radius(effect.horizontalBlurRadius, zoom));
  if (effect.scanlineEnabled)
    apply_row_taper(
      image,
      zoomedSpriteOrigin,
      zoom,
      effect.scanlineStrength * clamp_strength(effect.strength),
      effect.scanlinePower);
  if (effect.bloomEnabled)
    apply_bloom(
      image,
      screen_radius(effect.bloomRadius, zoom),
      effect.bloomStrength * clamp_strength(effect.strength),
      effect.bloomThreshold);
}

} // anonymous namespace

PreviewShaderStack::PreviewShaderStack()
  : m_enabled(true)
{
}

PreviewShaderStack::Effect PreviewShaderStack::preset(EffectType type)
{
  Effect effect;
  effect.name = typeName(type);
  effect.type = type;
  effect.enabled = true;
  effect.strength = 1.0;
  effect.horizontalBlurEnabled = true;
  effect.scanlineEnabled = true;
  effect.bloomEnabled = true;
  effect.horizontalBlurRadius = 1.0;
  effect.scanlineStrength = 0.50;
  effect.scanlinePower = 5.83;
  effect.bloomStrength = 0.40;
  effect.bloomRadius = 2.6;
  effect.bloomThreshold = 27.0;
  return effect;
}

const char* PreviewShaderStack::typeName(EffectType type)
{
  switch (type) {
    case EffectType::Crt: return "CRT";
    case EffectType::Scanlines: return "Scanlines";
    case EffectType::LcdMatrix: return "LCD Matrix";
    case EffectType::Vignette: return "Vignette";
    case EffectType::Grayscale: return "Grayscale";
    case EffectType::Invert: return "Invert";
  }
  return "CRT";
}

void PreviewShaderStack::addPreset(EffectType type)
{
  Effect effect = preset(type);
  if (type == EffectType::Crt)
    apply_preferences_to_effect(effect);
  m_effects.push_back(effect);
}

const PreviewShaderStack::Effect& PreviewShaderStack::crtEffect()
{
  return ensureCrtEffect();
}

PreviewShaderStack::Effect& PreviewShaderStack::ensureCrtEffect()
{
  for (Effect& effect : m_effects) {
    if (effect.type == EffectType::Crt)
      return effect;
  }

  Effect effect = preset(EffectType::Crt);
  apply_preferences_to_effect(effect);
  m_effects.push_back(effect);
  return m_effects.back();
}

void PreviewShaderStack::clear()
{
  m_effects.clear();
}

void PreviewShaderStack::remove(int index)
{
  if (index >= 0 && index < size())
    m_effects.erase(m_effects.begin()+index);
}

void PreviewShaderStack::moveUp(int index)
{
  if (index > 0 && index < size())
    std::swap(m_effects[index], m_effects[index-1]);
}

void PreviewShaderStack::moveDown(int index)
{
  if (index >= 0 && index+1 < size())
    std::swap(m_effects[index], m_effects[index+1]);
}

void PreviewShaderStack::toggle(int index)
{
  if (index >= 0 && index < size())
    m_effects[index].enabled = !m_effects[index].enabled;
}

void PreviewShaderStack::setStrength(int index, double strength)
{
  if (index >= 0 && index < size())
    m_effects[index].strength = clamp_strength(strength);
}

void PreviewShaderStack::setCrtEnabled(bool enabled)
{
  m_enabled = true;
  ensureCrtEffect().enabled = enabled;
}

void PreviewShaderStack::setCrtLayerEnabled(const std::string& layer, bool enabled)
{
  Effect& effect = ensureCrtEffect();
  std::string lower = to_lower(layer);
  if (lower == "blur")
    effect.horizontalBlurEnabled = enabled;
  else if (lower == "scanlines")
    effect.scanlineEnabled = enabled;
  else if (lower == "bloom")
    effect.bloomEnabled = enabled;
}

void PreviewShaderStack::setCrtParameter(const std::string& key, double value)
{
  Effect& effect = ensureCrtEffect();
  std::string lower = to_lower(key);
  if (lower == "strength")
    effect.strength = clamp_strength(value);
  else if (lower == "horizontal_blur_radius")
    effect.horizontalBlurRadius = clamp_param(value, 0.0, 32.0);
  else if (lower == "scanline_strength")
    effect.scanlineStrength = clamp_param(value, 0.0, 1.0);
  else if (lower == "scanline_power")
    effect.scanlinePower = clamp_param(value, 0.1, 8.0);
  else if (lower == "bloom_strength")
    effect.bloomStrength = clamp_param(value, 0.0, 1.5);
  else if (lower == "bloom_radius")
    effect.bloomRadius = clamp_param(value, 0.0, 32.0);
  else if (lower == "bloom_threshold")
    effect.bloomThreshold = clamp_param(value, 0.0, 255.0);
}

int PreviewShaderStack::requiredMargin(const render::Zoom& zoom) const
{
  int margin = 0;
  for (const Effect& effect : m_effects) {
    if (!effect.enabled)
      continue;

    switch (effect.type) {
      case EffectType::Crt:
        margin = std::max(
          margin,
          int(std::ceil((effect.horizontalBlurRadius + 2.0 * effect.bloomRadius) * zoom.scale())));
        break;
      default:
        break;
    }
  }
  return margin;
}

int PreviewShaderStack::dirtyPixelPadding() const
{
  return int(clamp_param(
    Preferences::instance().preview.crtRedrawPadding(), 0, 8));
}

void PreviewShaderStack::apply(
  doc::Image* image,
  const gfx::Point& zoomedSpriteOrigin,
  const render::Zoom& zoom) const
{
  if (m_effects.empty() || !image ||
      image->pixelFormat() != doc::IMAGE_RGB)
    return;

  for (const Effect& effect : m_effects) {
    if (!effect.enabled)
      continue;
    if (effect.type != EffectType::Crt)
      continue;

    apply_crt(image, zoomedSpriteOrigin, zoom, effect);
  }
}

void PreviewShaderStack::loadConfig()
{
  loadPreferences();
}

void PreviewShaderStack::loadPreferences()
{
  m_enabled = true;
  m_effects.clear();

  Effect effect = preset(EffectType::Crt);
  apply_preferences_to_effect(effect);
  m_effects.push_back(effect);
}

void PreviewShaderStack::saveConfig() const
{
  for (const Effect& effect : m_effects) {
    if (effect.type == EffectType::Crt) {
      save_effect_to_preferences(effect);
      break;
    }
  }
}

} // namespace app
