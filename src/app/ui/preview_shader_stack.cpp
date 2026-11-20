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

std::vector<std::string> crt_layer_order(const std::string& value)
{
  const char* defaults[] = { "blur", "scanlines", "bloom", "rgb_cells" };
  std::vector<std::string> result;
  std::string token;

  auto add = [&result](const std::string& name) {
    std::string lower = to_lower(name);
    if (lower == "rgb" || lower == "rgbcells")
      lower = "rgb_cells";
    if ((lower == "blur" || lower == "scanlines" ||
         lower == "bloom" || lower == "rgb_cells") &&
        std::find(result.begin(), result.end(), lower) == result.end())
      result.push_back(lower);
  };

  for (char ch : value) {
    if (ch == ',') {
      add(token);
      token.clear();
    }
    else if (!std::isspace(static_cast<unsigned char>(ch)))
      token += ch;
  }
  add(token);
  for (const char* name : defaults)
    add(name);
  return result;
}

std::string join_crt_layer_order(const std::vector<std::string>& order)
{
  std::string result;
  for (const std::string& name : order) {
    if (!result.empty())
      result += ",";
    result += name;
  }
  return result;
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
  effect.horizontalBlurRadius = clamp_param(preview.crtHorizontalBlurRadius(), 0.0, 5.0);
  effect.scanlineEnabled = preview.crtScanlineEnabled();
  effect.scanlineStrength = clamp_param(preview.crtScanlineStrength(), 0.0, 1.0);
  effect.scanlinePower = clamp_param(preview.crtScanlinePower(), 0.1, 8.0);
  effect.scanlineBrightnessCompensation = clamp_param(preview.crtScanlineBrightnessCompensation(), 0.0, 1.0);
  effect.bloomEnabled = preview.crtBloomEnabled();
  effect.bloomStrength = clamp_param(preview.crtBloomStrength(), 0.0, 1.5);
  effect.bloomRadius = clamp_param(preview.crtBloomRadius(), 0.0, 32.0);
  effect.bloomThreshold = clamp_param(preview.crtBloomThreshold(), 0.0, 255.0);
  effect.rgbCellsEnabled = preview.crtRgbCellsEnabled();
  effect.rgbCellsFadeStartZoom = clamp_param(preview.crtRgbCellsFadeStartZoom(), 1.0, 64.0);
  effect.rgbCellsFadePerZoom = clamp_param(preview.crtRgbCellsFadePerZoom(), 0.0, 10.0);
  effect.rgbCellsMinStrength = clamp_param(preview.crtRgbCellsMinStrength(), 0.0, 1.0);
  effect.rgbCellsBrightnessCompensation = clamp_param(preview.crtRgbCellsBrightnessCompensation(), 0.0, 1.0);
  effect.layerOrder = join_crt_layer_order(crt_layer_order(preview.crtLayerOrder()));
}

void save_effect_to_preferences(const PreviewShaderStack::Effect& effect)
{
  auto& preview = Preferences::instance().preview;
  preview.crtEnabled(effect.enabled);
  preview.crtStrength(clamp_strength(effect.strength));
  preview.crtHorizontalBlurEnabled(effect.horizontalBlurEnabled);
  preview.crtHorizontalBlurRadius(clamp_param(effect.horizontalBlurRadius, 0.0, 5.0));
  preview.crtScanlineEnabled(effect.scanlineEnabled);
  preview.crtScanlineStrength(clamp_param(effect.scanlineStrength, 0.0, 1.0));
  preview.crtScanlinePower(clamp_param(effect.scanlinePower, 0.1, 8.0));
  preview.crtScanlineBrightnessCompensation(clamp_param(effect.scanlineBrightnessCompensation, 0.0, 1.0));
  preview.crtBloomEnabled(effect.bloomEnabled);
  preview.crtBloomStrength(clamp_param(effect.bloomStrength, 0.0, 1.5));
  preview.crtBloomRadius(clamp_param(effect.bloomRadius, 0.0, 32.0));
  preview.crtBloomThreshold(clamp_param(effect.bloomThreshold, 0.0, 255.0));
  preview.crtRgbCellsEnabled(effect.rgbCellsEnabled);
  preview.crtRgbCellsFadeStartZoom(clamp_param(effect.rgbCellsFadeStartZoom, 1.0, 64.0));
  preview.crtRgbCellsFadePerZoom(clamp_param(effect.rgbCellsFadePerZoom, 0.0, 10.0));
  preview.crtRgbCellsMinStrength(clamp_param(effect.rgbCellsMinStrength, 0.0, 1.0));
  preview.crtRgbCellsBrightnessCompensation(clamp_param(effect.rgbCellsBrightnessCompensation, 0.0, 1.0));
  preview.crtLayerOrder(effect.layerOrder);
}

void apply_horizontal_blur(doc::Image* image, double radius)
{
  radius = clamp_param(radius, 0.0, 5.0);
  if (radius < 0.5)
    return;

  const int w = image->width();
  const int h = image->height();
  const int r = int(std::ceil(radius));
  std::unique_ptr<doc::Image> src(doc::Image::createCopy(image));
  std::vector<double> prefixWeight(w+1);
  std::vector<double> prefixWR(w+1);
  std::vector<double> prefixWG(w+1);
  std::vector<double> prefixWB(w+1);
  constexpr double kBrightBias = 0.20;

  for (int y=0; y<h; ++y) {
    prefixWeight[0] = 0.0;
    prefixWR[0] = 0.0;
    prefixWG[0] = 0.0;
    prefixWB[0] = 0.0;
    for (int x=0; x<w; ++x) {
      uint32_t c = src->getPixel(x, y);
      double luma = double(doc::rgb_luma(
        doc::rgba_getr(c), doc::rgba_getg(c), doc::rgba_getb(c))) / 255.0;
      double weight = 1.0 + kBrightBias * luma;
      prefixWeight[x+1] = prefixWeight[x] + weight;
      prefixWR[x+1] = prefixWR[x] + doc::rgba_getr(c) * weight;
      prefixWG[x+1] = prefixWG[x] + doc::rgba_getg(c) * weight;
      prefixWB[x+1] = prefixWB[x] + doc::rgba_getb(c) * weight;
    }

    for (int x=0; x<w; ++x) {
      int x0 = std::max(0, x-r);
      int x1 = std::min(w-1, x+r);
      double weight = prefixWeight[x1+1] - prefixWeight[x0];

      put_rgb(image, x, y,
        int((prefixWR[x1+1] - prefixWR[x0]) / weight),
        int((prefixWG[x1+1] - prefixWG[x0]) / weight),
        int((prefixWB[x1+1] - prefixWB[x0]) / weight));
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
  double power,
  double brightnessCompensation)
{
  const int w = image->width();
  const int h = image->height();
  const double compensation = clamp_param(brightnessCompensation, 0.0, 1.0);
  const double boost = 1.0 + compensation * clamp_param(strength, 0.0, 1.0);

  // Compensation belongs underneath the scanline mask.  Brighten the input
  // first, then apply the normal scanline taper so the taper itself is not
  // weakened by the compensation control.
  if (boost > 1.0) {
    for (int y=0; y<h; ++y) {
      for (int x=0; x<w; ++x) {
        uint32_t c = image->getPixel(x, y);
        put_rgb(image, x, y,
          int(doc::rgba_getr(c) * boost),
          int(doc::rgba_getg(c) * boost),
          int(doc::rgba_getb(c) * boost));
      }
    }
  }

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

void apply_rgb_cells(
  doc::Image* image,
  const gfx::Point& zoomedSpriteOrigin,
  const render::Zoom& zoom,
  double strength,
  double fadeStartZoom,
  double fadePerZoom,
  double minStrength,
  double brightnessCompensation)
{
  const int w = image->width();
  const int h = image->height();
  const double scale = zoom.scale();
  strength = clamp_strength(strength);
  fadeStartZoom = clamp_param(fadeStartZoom, 1.0, 64.0);
  fadePerZoom = clamp_param(fadePerZoom, 0.0, 10.0);
  minStrength = clamp_param(minStrength, 0.0, 1.0);
  const double zoomLoss = std::max(0.0, fadeStartZoom - scale) * fadePerZoom / 100.0;
  // The minimum is a floor only while the fade is still above zero.  With a
  // minimum of 0, the cells must really disappear once the configured fade
  // has consumed all of their strength; using max(0, ...) here also avoids
  // leaving a residual darkening pass at small zoom levels.
  const double fadedStrength = std::max(0.0, 1.0 - zoomLoss);
  const double cellStrength = strength * std::max(minStrength, fadedStrength);
  if (cellStrength <= 0.0)
    return;

  const double compensation = clamp_param(brightnessCompensation, 0.0, 1.0);
  const double boost = 1.0 + compensation * cellStrength;
  if (boost > 1.0) {
    for (int y=0; y<h; ++y) {
      for (int x=0; x<w; ++x) {
        uint32_t c = image->getPixel(x, y);
        put_rgb(image, x, y,
          int(doc::rgba_getr(c) * boost),
          int(doc::rgba_getg(c) * boost),
          int(doc::rgba_getb(c) * boost));
      }
    }
  }

  for (int y=0; y<h; ++y) {
    for (int x=0; x<w; ++x) {
      const double screenX = double(zoomedSpriteOrigin.x + x);
      double cellOrigin;
      double cellPitch;

      if (scale >= 3.0) {
        // At useful magnifications, align the phosphor triad to each
        // displayed source pixel. Fractional coverage avoids the uneven
        // 1/1/0 assignment produced by integer division.
        const int spriteX = zoom.remove(int(screenX));
        cellOrigin = zoom.apply(spriteX);
        cellPitch = scale / 3.0;
      }
      else {
        // Below 3x, a source pixel cannot contain three distinct output
        // pixels. Keep the physical triad screen-space aligned instead of
        // disabling it or forcing an unstable per-source-pixel mask.
        // Use the absolute preview coordinate so partial dirty redraws do
        // not shift the triad when renderRc starts at a different x.
        cellOrigin = 0.0;
        cellPitch = 1.0;
      }

      double channelCoverage[3] = { 0.0, 0.0, 0.0 };
      constexpr int kSamples = 4;
      for (int sample=0; sample<kSamples; ++sample) {
        double sampleX = screenX + (double(sample) + 0.5) / kSamples;
        double phase = std::fmod(sampleX - cellOrigin, 3.0 * cellPitch);
        if (phase < 0.0)
          phase += 3.0 * cellPitch;
        int subpixel = std::min(2, int(phase / cellPitch));
        channelCoverage[subpixel] += 1.0 / kSamples;
      }

      const double channel[3] = {
        1.0 - cellStrength * (1.0 - channelCoverage[0]),
        1.0 - cellStrength * (1.0 - channelCoverage[1]),
        1.0 - cellStrength * (1.0 - channelCoverage[2])
      };
      uint32_t c = image->getPixel(x, y);
      put_rgb(image, x, y,
        int(doc::rgba_getr(c) * channel[0]),
        int(doc::rgba_getg(c) * channel[1]),
        int(doc::rgba_getb(c) * channel[2]));
    }
  }

}

void apply_crt(
  doc::Image* image,
  const gfx::Point& zoomedSpriteOrigin,
  const render::Zoom& zoom,
  const PreviewShaderStack::Effect& effect)
{
  for (const std::string& layer : crt_layer_order(effect.layerOrder)) {
    if (layer == "blur" && effect.horizontalBlurEnabled)
      apply_horizontal_blur(image, screen_radius(effect.horizontalBlurRadius, zoom));
    else if (layer == "scanlines" && effect.scanlineEnabled)
      apply_row_taper(
        image,
        zoomedSpriteOrigin,
        zoom,
        effect.scanlineStrength * clamp_strength(effect.strength),
        effect.scanlinePower,
        effect.scanlineBrightnessCompensation);
    else if (layer == "bloom" && effect.bloomEnabled)
      apply_bloom(
        image,
        screen_radius(effect.bloomRadius, zoom),
        effect.bloomStrength * clamp_strength(effect.strength),
        effect.bloomThreshold);
    else if (layer == "rgb_cells" && effect.rgbCellsEnabled)
      apply_rgb_cells(
        image,
        zoomedSpriteOrigin,
        zoom,
        clamp_strength(effect.strength),
        effect.rgbCellsFadeStartZoom,
        effect.rgbCellsFadePerZoom,
        effect.rgbCellsMinStrength,
        effect.rgbCellsBrightnessCompensation);
  }
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
  effect.scanlineBrightnessCompensation = 0.0;
  effect.bloomStrength = 0.40;
  effect.bloomRadius = 2.6;
  effect.bloomThreshold = 27.0;
  effect.rgbCellsEnabled = true;
  effect.rgbCellsFadeStartZoom = 24.0;
  effect.rgbCellsFadePerZoom = 5.0;
  effect.rgbCellsMinStrength = 0.10;
  effect.rgbCellsBrightnessCompensation = 0.0;
  effect.layerOrder = "blur,scanlines,bloom,rgb_cells";
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
  else if (lower == "rgb_cells" || lower == "rgb")
    effect.rgbCellsEnabled = enabled;
}

void PreviewShaderStack::setCrtParameter(const std::string& key, double value)
{
  Effect& effect = ensureCrtEffect();
  std::string lower = to_lower(key);
  if (lower == "strength")
    effect.strength = clamp_strength(value);
  else if (lower == "horizontal_blur_radius")
    effect.horizontalBlurRadius = clamp_param(value, 0.0, 5.0);
  else if (lower == "scanline_strength")
    effect.scanlineStrength = clamp_param(value, 0.0, 1.0);
  else if (lower == "scanline_power")
    effect.scanlinePower = clamp_param(value, 0.1, 8.0);
  else if (lower == "scanline_brightness_compensation")
    effect.scanlineBrightnessCompensation = clamp_param(value, 0.0, 1.0);
  else if (lower == "bloom_strength")
    effect.bloomStrength = clamp_param(value, 0.0, 1.5);
  else if (lower == "bloom_radius")
    effect.bloomRadius = clamp_param(value, 0.0, 32.0);
  else if (lower == "bloom_threshold")
    effect.bloomThreshold = clamp_param(value, 0.0, 255.0);
  else if (lower == "rgb_cells_fade_start_zoom")
    effect.rgbCellsFadeStartZoom = clamp_param(value, 1.0, 64.0);
  else if (lower == "rgb_cells_fade_per_zoom")
    effect.rgbCellsFadePerZoom = clamp_param(value, 0.0, 10.0);
  else if (lower == "rgb_cells_min_strength")
    effect.rgbCellsMinStrength = clamp_param(value, 0.0, 1.0);
  else if (lower == "rgb_cells_brightness_compensation")
    effect.rgbCellsBrightnessCompensation = clamp_param(value, 0.0, 1.0);
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
