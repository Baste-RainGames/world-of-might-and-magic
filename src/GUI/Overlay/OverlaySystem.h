#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <utility>
#include <functional>

#include "Library/Logger/LogCategory.h"
#include "Utility/String/TransparentFunctors.h"

class Overlay;
class GameConfig;
class PlatformApplication;
class Renderer;
struct nk_context;

/** 
 * @brief An Overlay is a screen built in Immediate Mode GUI ( nuklear ) which is displayed on top of any other ui and it's 
 * mainly used for debug purposes ( Console, Render Stats, Debug View Buttons, etc... ).
 * 
 * The Overlay System is the class responsible to keep track of all the registered Overlays and update them accordingly.
 */
class OverlaySystem {
 public:
    OverlaySystem(Renderer& render, PlatformApplication &platformApplication);
    ~OverlaySystem();

    void addOverlay(std::string_view name, std::unique_ptr<Overlay> overlay);
    void removeOverlay(std::string_view name);

    bool isEnabled() const;
    void setEnabled(bool enable);

    void drawOverlays();

    static LogCategory OverlayLogCategory;

 private:
    void _update();

    std::unordered_map<TransparentString, std::unique_ptr<Overlay>, TransparentStringHash, TransparentStringEquals> _overlays;
    Renderer &_renderer;
    PlatformApplication &_application;
    std::unique_ptr<nk_context> _nuklearContext;
    std::function<void()> _unregisterDependencies;
    bool _isEnabled{};
};
