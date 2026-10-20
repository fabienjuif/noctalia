#pragma once

#include "render/animation/animation_manager.h"
#include "wayland/layer_surface.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class ConfigService;
struct Config;
struct OsdConfig;
class CompositorPlatform;
class IpcService;
class Box;
class Flex;
class Glyph;
class Image;
class Label;
class ProgressBar;
class RenderContext;
class WaylandConnection;
struct WaylandOutput;
struct PointerEvent;
struct wl_output;
struct wl_surface;

enum class OsdKind : std::uint8_t {
  Volume,
  Microphone,
  Brightness,
  Wifi,
  Bluetooth,
  PowerProfile,
  Caffeine,
  NightLight,
  Dnd,
  LockKeys,
  KeyboardLayout,
  Media,
  Privacy,
  KeyboardBacklight,
  Notification
};

struct OsdContent {
  OsdKind kind = OsdKind::Volume;
  std::string icon;
  std::string value;
  float progress = 0.0F;
  bool showProgress = true;
  bool overLimit = false;
  bool inactive = false;
  // Stays up until hide() instead of timing out (notification island).
  bool persistent = false;
  // When set and loadable, shown in place of the glyph (notification app icon).
  std::string imagePath;
  // Icon pinned to the leading edge, text centered in the remaining width (notification island).
  bool leadingIcon = false;
};

class OsdOverlay {
public:
  OsdOverlay();
  ~OsdOverlay();

  OsdOverlay(const OsdOverlay&) = delete;
  OsdOverlay& operator=(const OsdOverlay&) = delete;

  // Local patch: the selector picks which OsdConfig drives this overlay, so a second instance can render
  // notifications from [notification.island] with the same look.
  using ConfigSelector = const OsdConfig& (*)(const Config&);
  static const OsdConfig& selectOsdConfig(const Config& config);
  void initialize(
      WaylandConnection& wayland, CompositorPlatform& platform, ConfigService* config, RenderContext* renderContext,
      ConfigSelector configSelector = &OsdOverlay::selectOsdConfig
  );
  void registerIpc(IpcService& ipc);
  void onOutputChange();
  void onConfigReload();
  void requestLayout();
  void requestRedraw();

  void show(const OsdContent& content);
  void hide();
  // Makes the card clickable; without it the overlay stays input-transparent.
  void setOnClick(std::function<void()> onClick);
  bool onPointerEvent(const PointerEvent& event);
  // Supplies the output-local x range fill_bar_gap stretches over; re-queried by syncSpan().
  void setSpanProvider(std::function<std::optional<std::pair<float, float>>()> provider);
  void syncSpan();
  [[nodiscard]] bool isEnabled() const noexcept;

  // True while any instance is on screen or animating into view. Callers use this to correct
  // already-visible content in place without popping up a fresh OSD.
  [[nodiscard]] bool isVisible() const;

private:
  struct SurfaceMargins {
    std::int32_t top = 0;
    std::int32_t right = 0;
    std::int32_t bottom = 0;
    std::int32_t left = 0;
  };

  struct Instance {
    wl_output* output = nullptr;
    std::int32_t scale = 1;
    float uiLayoutScale = 1.0F;
    std::unique_ptr<LayerSurface> surface;
    // sceneRoot must be destroyed before `animations` — ~Node() calls cancelForOwner().
    AnimationManager animations;
    std::unique_ptr<Node> sceneRoot;
    wl_surface* wlSurface = nullptr;

    Node* card = nullptr;
    Flex* row = nullptr;
    Box* background = nullptr;
    Glyph* glyph = nullptr;
    Image* image = nullptr;
    std::string loadedImagePath;
    Label* value = nullptr;
    ProgressBar* progress = nullptr;
    float progressValueMinWidth = 0.0F;
    float rowBaseX = 0.0F;
    float rowBaseY = 0.0F;
    AnimationManager::Id showAnimId = 0;
    AnimationManager::Id hideAnimId = 0;
    bool showPending = false;
    bool visible = false;
    float appliedCornerRadiusScale = -1.0F;
  };

  [[nodiscard]] SurfaceMargins surfaceMarginsForPosition(const std::string& position) const;
  [[nodiscard]] std::vector<std::string> osdMonitors() const;
  [[nodiscard]] bool shouldRenderOnOutput(const WaylandOutput& output) const;
  void ensureSurfaces();
  void destroySurfaces();
  void setEnabledOverride(bool enabled);
  void prepareFrame(Instance& inst, bool needsUpdate, bool needsLayout);
  void buildScene(Instance& inst, std::uint32_t width, std::uint32_t height);
  void updateInstanceContent(Instance& inst);
  void updateBlurRegion(Instance& inst) const;
  [[nodiscard]] const OsdConfig& osdConfig() const;
  [[nodiscard]] const OsdConfig* osdConfigOrNull() const;
  [[nodiscard]] float lengthScale() const;
  [[nodiscard]] float fontScale() const;
  [[nodiscard]] float effectiveCardWidth(float s) const;
  [[nodiscard]] std::optional<std::pair<float, float>> querySpan() const;
  [[nodiscard]] bool islandStyle() const;
  void applyReveal(Instance& inst, float reveal);
  void animateInstance(Instance& inst);
  void startHideAnimation(Instance& inst);

  WaylandConnection* m_wayland = nullptr;
  CompositorPlatform* m_platform = nullptr;
  ConfigService* m_config = nullptr;
  ConfigSelector m_configSelector = &OsdOverlay::selectOsdConfig;
  RenderContext* m_renderContext = nullptr;
  OsdContent m_content;
  std::string m_lastPosition;
  std::string m_lastOrientation;
  bool m_lastShowProgress = true;
  float m_lastLayoutScale = 1.0F;
  float m_lastCornerRadiusScale = 1.0F;
  std::vector<std::string> m_lastMonitorSelectors;
  bool m_followFocusedOutput = false;
  wl_output* m_targetOutput = nullptr;
  std::optional<bool> m_runtimeEnabledOverride;
  bool m_lastConfiguredEnabled = true;
  std::vector<std::unique_ptr<Instance>> m_instances;
  std::function<void()> m_onClick;
  std::function<std::optional<std::pair<float, float>>()> m_spanProvider;
  std::optional<std::pair<float, float>> m_span;
  float m_spanCardWidth = 0.0F;
};
