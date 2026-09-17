// LibreSprite preview shader stack
// Copyright (C) 2026
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#pragma once

#include "gfx/point.h"

#include <string>
#include <vector>

namespace doc {
  class Image;
}

namespace render {
  class Zoom;
}

namespace app {

  class PreviewShaderStack {
  public:
    enum class EffectType {
      Crt,
      Scanlines,
      LcdMatrix,
      Vignette,
      Grayscale,
      Invert
    };

    struct Effect {
      std::string name;
      EffectType type;
      bool enabled;
      double strength;
      bool horizontalBlurEnabled;
      bool scanlineEnabled;
      bool bloomEnabled;
      double horizontalBlurRadius;
      double scanlineStrength;
      double scanlinePower;
      double bloomStrength;
      double bloomRadius;
      double bloomThreshold;
    };

    PreviewShaderStack();

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool empty() const { return m_effects.empty(); }
    int size() const { return int(m_effects.size()); }
    const Effect& effect(int index) const { return m_effects[index]; }
    const Effect& crtEffect();

    void addPreset(EffectType type);
    void clear();
    void remove(int index);
    void moveUp(int index);
    void moveDown(int index);
    void toggle(int index);
    void setStrength(int index, double strength);
    void setCrtEnabled(bool enabled);
    void setCrtLayerEnabled(const std::string& layer, bool enabled);
    void setCrtParameter(const std::string& key, double value);
    int dirtyPixelPadding() const;
    int requiredMargin(const render::Zoom& zoom) const;
    void apply(doc::Image* image,
               const gfx::Point& zoomedSpriteOrigin,
               const render::Zoom& zoom) const;

    void loadConfig();
    void loadPreferences();
    void saveConfig() const;

  private:
    static Effect preset(EffectType type);
    static const char* typeName(EffectType type);
    Effect& ensureCrtEffect();

    std::vector<Effect> m_effects;
    bool m_enabled;
  };

} // namespace app
