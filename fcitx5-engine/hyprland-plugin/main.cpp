// xtype-inline — Hyprland plugin for XType's popup ghost style.
//
// Hyprland places input-method popups *below* the text caret (see
// CInputPopup::updateBox). With XType's popup ghost style that popup holds the
// suggestion, so moving it onto the caret's own line — starting right after the
// caret and vertically centred on it — makes it read as inline ghost text.
//
// Every consumer of the popup position (rendering, damage, pointer hit-tests)
// goes through CInputPopup::globalBox(), so hooking that one function is
// enough. isVecInPopup() is hooked too so the popup is click-through and never
// steals clicks from the text underneath it.
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/plugins/HookSystem.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/input/InputMethodPopup.hpp>
#include <hyprland/src/managers/input/TextInput.hpp>
#include <hyprland/src/desktop/view/WLSurface.hpp>

#include <stdexcept>
#include <string>

inline HANDLE         PHANDLE           = nullptr;
static CFunctionHook* g_globalBoxHook   = nullptr;
static CFunctionHook* g_isVecInPopupHook = nullptr;

using GlobalBoxFn    = CBox (*)(CInputPopup*);
using IsVecInPopupFn = bool (*)(CInputPopup*, const Vector2D&);

static CBox hkGlobalBox(CInputPopup* self) {
    CBox box = reinterpret_cast<GlobalBoxFn>(g_globalBoxHook->m_original)(self);

    const auto TI = g_pInputManager->m_relay.getFocusedTextInput();
    if (!TI || !TI->hasCursorRectangle())
        return box;   // no caret position known: keep Hyprland's placement

    const auto SURF = Desktop::View::CWLSurface::fromResource(TI->focusedSurface());
    if (!SURF)
        return box;
    const auto PARENT = SURF->getSurfaceBoxGlobal();
    if (!PARENT)
        return box;

    const CBox CARET = TI->cursorBox();   // surface-local
    box.x = PARENT->x + CARET.x + CARET.w;
    box.y = PARENT->y + CARET.y + (CARET.h - box.h) / 2.0;
    return box;
}

static bool hkIsVecInPopup(CInputPopup*, const Vector2D&) {
    return false;
}

static void* findMember(const std::string& name, const std::string& qualified) {
    for (const auto& m : HyprlandAPI::findFunctionsByName(PHANDLE, name))
        if (m.demangled.find(qualified) != std::string::npos)
            return m.address;
    return nullptr;
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    if (std::string(__hyprland_api_get_hash()) != __hyprland_api_get_client_hash()) {
        HyprlandAPI::addNotification(PHANDLE, "[xtype-inline] Version mismatch: rebuild against the running Hyprland",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[xtype-inline] version mismatch");
    }

    void* globalBox    = findMember("globalBox", "CInputPopup::globalBox");
    void* isVecInPopup = findMember("isVecInPopup", "CInputPopup::isVecInPopup");
    if (!globalBox || !isVecInPopup) {
        HyprlandAPI::addNotification(PHANDLE, "[xtype-inline] CInputPopup functions not found; plugin inactive",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[xtype-inline] symbols not found");
    }

    g_globalBoxHook    = HyprlandAPI::createFunctionHook(PHANDLE, globalBox, reinterpret_cast<void*>(&hkGlobalBox));
    g_isVecInPopupHook = HyprlandAPI::createFunctionHook(PHANDLE, isVecInPopup, reinterpret_cast<void*>(&hkIsVecInPopup));
    if (!g_globalBoxHook->hook() || !g_isVecInPopupHook->hook())
        throw std::runtime_error("[xtype-inline] failed to hook");

    HyprlandAPI::addNotification(PHANDLE, "[xtype-inline] loaded: input-method popups now sit on the caret line",
                                 CHyprColor{0.52, 0.44, 0.9, 1.0}, 3000);
    return {"xtype-inline", "Places input-method popups on the caret line (XType popup suggestions)", "XType", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    // Hooks created through HyprlandAPI are removed automatically on unload.
}
