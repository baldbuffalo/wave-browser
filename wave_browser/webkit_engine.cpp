// webkit_engine.cpp — WKC peer implementations + SDL integration
// All signatures match vendor/webkit-wiiu/include/wkc/wkc*.h exactly.

#include "webkit_engine.h"
#ifdef WKC_AVAILABLE
#include "font_data.h"

#include <SDL.h>
#include <SDL_ttf.h>

extern "C" {
#include <coreinit/time.h>
#include <coreinit/thread.h>
#include <coreinit/mutex.h>
}

#include <wkc/wkcbase.h>
#include <wkc/wkcpeer.h>   // defines wkcTimeoutProc, wkc_uint64, wkc_int64, all file/timer/thread peers
#include <wkc/wkcgpeer.h>  // defines fontPeerMalloc, fontPeerFree, wkcFontDrawTextPeer
#include <wkc/wkcmpeer.h>
#include <wkc/wkcmediapeer.h>
#include <wkc/wkcsocket.h>
#include <wkc/wkcheappeer.h>
#include <wkc/WKCWebView.h>
#include <wkc/WKCClientBuilders.h>
#include <wkc/helpers/ChromeClientIf.h>
#include <wkc/helpers/FrameLoaderClientIf.h>
#include <wkc/helpers/EditorClientIf.h>
#include <wkc/helpers/ContextMenuClientIf.h>
#include <wkc/helpers/DragClientIf.h>
#include <wkc/helpers/DropDownListClientIf.h>
#include <wkc/helpers/WKCString.h>
#include <wkc/helpers/WKCResourceError.h>


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <math.h>

// ─── State ───────────────────────────────────────────────────────────────────

static bool           s_available = false;
static int            s_w = 1280, s_h = 720;
static unsigned char* s_pixels   = nullptr;
static SDL_Renderer*  s_renderer = nullptr;
static SDL_Texture*   s_texture  = nullptr;
static bool           s_dirty    = false;
static char           s_url[512] = {};
static WKC::WKCWebView* s_webview = nullptr;
static bool s_webkit_initialized = false;

// ─── Timer ───────────────────────────────────────────────────────────────────
// wkcTimeoutProc returns bool (from wkcpeer.h)

struct WKCTimer {
    wkcTimeoutProc proc;
    void*          data;
    unsigned int   fire_at;
    unsigned int   interval;
    bool           active;
    bool           wkc_wakeup;
    WKCTimer*      next;
};
static WKCTimer* s_timers     = nullptr;
static OSMutex   s_timer_mtx;
static bool      s_timer_init = false;
static unsigned int now_ms();

static void schedule_wkc_wakeup(unsigned int ms, void* data)
{
    if (!s_timer_init) {
        OSInitMutex(&s_timer_mtx);
        s_timer_init = true;
    }

    const bool cancel = ms == 0xFFFFFFFFu;
    OSLockMutex(&s_timer_mtx);

    // WebKit frequently reschedules a wake-up for the same client data. Reuse
    // its existing queue node instead of allocating another node on every
    // animation/network/timer update.
    for (WKCTimer* t = s_timers; t; t = t->next) {
        if (!t->wkc_wakeup || t->data != data)
            continue;

        if (cancel) {
            t->active = false;
        } else {
            t->fire_at = now_ms() + ms;
            t->active = true;
        }
        OSUnlockMutex(&s_timer_mtx);
        return;
    }

    if (cancel) {
        OSUnlockMutex(&s_timer_mtx);
        return;
    }

    auto* t = (WKCTimer*)calloc(1, sizeof(WKCTimer));
    if (t) {
        t->data = data;
        t->fire_at = now_ms() + ms;
        t->active = true;
        t->wkc_wakeup = true;
        t->next = s_timers;
        s_timers = t;
    }
    OSUnlockMutex(&s_timer_mtx);
}


static unsigned int now_ms() {
    return (unsigned int)OSTicksToMilliseconds(OSGetTick());
}

class WaveMemoryEventHandler final : public WKC::WKCMemoryEventHandler {
public:
    bool checkMemoryAvailability(unsigned int, bool) override { return true; }
    bool checkMemoryAllocatable(unsigned int, AllocationReason) override { return true; }
    void* notifyMemoryExhaust(unsigned int requested, unsigned int& allocated) override {
        allocated = requested;
        return malloc(requested);
    }
    void notifyMemoryAllocationError(unsigned int, AllocationReason) override {}
    void notifyCrash(const char*, int, const char*, const char*) override {}
    void notifyStackOverflow(bool, unsigned int, unsigned int, unsigned int,
                             void*, void*, void*, const char*, int, const char*) override {}
};

class WaveTimerEventHandler final : public WKC::WKCTimerEventHandler {
public:
    bool requestWakeUp(unsigned int ms, void* data) override {
        schedule_wkc_wakeup(ms, data);
        return true;
    }
};


/* Default embedder clients required to create and navigate a WKC page. */
static WKC::ResourceError makeWaveDefaultResourceError()
{
    WKC::String empty;
    return WKC::ResourceError(empty, 0, empty, empty, nullptr);
}

class WaveChromeClient final : public WKC::ChromeClientIf {
public:
    void chromeDestroyed() override { }
    void setWindowRect(const WKCFloatRect&) override { }
    WKCFloatRect windowRect() override { WKCFloatRect rect{}; WKCFloatRect_SetXYWH(&rect, 0, 0, static_cast<float>(s_w), static_cast<float>(s_h)); return rect; }
    WKCFloatRect pageRect() override { WKCFloatRect rect{}; WKCFloatRect_SetXYWH(&rect, 0, 0, static_cast<float>(s_w), static_cast<float>(s_h)); return rect; }
    void focus() override { }
    void unfocus() override { }
    bool canTakeFocus(WKC::WKCFocusDirection) override { return false; }
    void takeFocus(WKC::WKCFocusDirection) override { }
    void focusedNodeChanged(WKC::Node*) override { }
    WKC::Page* createWindow(WKC::Frame*, const WKC::FrameLoadRequest&, const WKC::WindowFeatures&) override { return {}; }
    void show() override { }
    bool canRunModal() override { return false; }
    void runModal() override { }
    void setToolbarsVisible(bool) override { }
    bool toolbarsVisible() override { return {}; }
    void setStatusbarVisible(bool) override { }
    bool statusbarVisible() override { return {}; }
    void setScrollbarsVisible(bool) override { }
    bool scrollbarsVisible() override { return {}; }
    void setMenubarVisible(bool) override { }
    bool menubarVisible() override { return {}; }
    void setResizable(bool) override { }
    void addMessageToConsole(WKC::MessageSource source, WKC::MessageType type, WKC::MessageLevel level, const WKC::String& message, unsigned int lineNumber, const WKC::String& sourceID) override { }
    bool canRunBeforeUnloadConfirmPanel() override { return false; }
    bool runBeforeUnloadConfirmPanel(const WKC::String& message, WKC::Frame* frame) override { return false; }
    void closeWindowSoon() override { }
    void runJavaScriptAlert(WKC::Frame*, const WKC::String&) override { }
    bool runJavaScriptConfirm(WKC::Frame*, const WKC::String&) override { return false; }
    bool runJavaScriptPrompt(WKC::Frame*, const WKC::String& message, const WKC::String& defaultValue, WKC::String& result) override { return false; }
    void setStatusbarText(const WKC::String&) override { }
    bool shouldInterruptJavaScript() override { return false; }
    void registerProtocolHandler(const WKC::String& scheme, const WKC::String& baseURL, const WKC::String& url, const WKC::String& title) override { }
    WKCRect windowResizerRect() const override { return {}; }
    void repaint(const WKCRect& rect, bool contentChanged, bool immediate = false, bool repaintContentOnly = false) override { s_dirty = true; }
    void invalidateContentsForSlowScroll(const WKCRect& rect, bool immediate) override { s_dirty = true; }
    void scroll(const WKCSize& scrollDelta, const WKCRect& rectToScroll, const WKCRect& clipRect) override { s_dirty = true; }
    WKCPoint screenToWindow(const WKCPoint&) const override { return {}; }
    WKCRect windowToScreen(const WKCRect&) const override { return {}; }
    PlatformPageClient platformPageClient() const override { return {}; }
    void contentsSizeChanged(WKC::Frame*, const WKCSize&) const override { s_dirty = true; }
    void scheduleAnimation() override { s_dirty = true; }
    void scrollbarsModeDidChange() const override { }
    void mouseDidMoveOverElement(const WKC::HitTestResult&, unsigned modifierFlags) override { }
    void setToolTip(const WKC::String&, WKC::TextDirection) override { }
    void print(WKC::Frame*) override { }
    void exceededDatabaseQuota(WKC::Frame*, const WKC::String&) override { }
    void reachedMaxAppCacheSize(int64_t spaceNeeded) override { }
    void reachedApplicationCacheOriginQuota(WKC::SecurityOrigin* origin, int64_t totalSpaceNeeded) override { }
    void runOpenPanel(WKC::Frame*, WKC::FileChooser*) override { }
    void enumerateChosenDirectory(WKC::FileChooser*) override { }
    void formStateDidChange(const WKC::Node*) override { }
    void elementDidFocus(const WKC::Node*) override { }
    void elementDidBlur(const WKC::Node*) override { }
    WKC::HTMLParserQuirks* createHTMLParserQuirks() override { return {}; }
    void setCursor(WKC::PlatformCursorHandle) override { }
    void dispatchViewportDataDidChange(const WKC::ViewportArguments&) override { }
    bool shouldUnavailablePluginMessageBeButton(int) const override { return false; }
    void unavailablePluginButtonClicked(WKC::Element* element, int reason) const override { }
    void scrollRectIntoView(const WKCRect&) const override { }
    void focusedFrameChanged(WKC::Frame*) override { }
    WKC::KeyboardUIMode keyboardUIMode() override { return {}; }
    bool shouldRubberBandInDirection(WKC::ScrollDirection) const override { return false; }
    bool selectItemWritingDirectionIsNatural() override { return false; }
    bool selectItemAlignmentFollowsMenuWritingDirection() override { return false; }
    void numWheelEventHandlersChanged(unsigned num) override { }
    void numTouchEventHandlersChanged(unsigned num) override { }
    bool hasOpenedPopup() override { return false; }
    void populateVisitedLinks() override { }
    WKCFloatRect customHighlightRect(WKC::Node*, const WKC::String& type, const WKCFloatRect& lineRect) override { return {}; }
    void paintCustomHighlight(WKC::Node*, const WKC::String& type, const WKCFloatRect& boxRect, const WKCFloatRect& lineRect, bool behindText, bool entireLine) override { }
    bool shouldReplaceWithGeneratedFileForUpload(const WKC::String& path, WKC::String& generatedFilename) override { return false; }
    WKC::String generateReplacementFile(const WKC::String& path) override { return WKC::String(); }
    void attachRootGraphicsLayer(WKC::Frame*, WKC::GraphicsLayer*) override { }
    void setNeedsOneShotDrawingSynchronization() override { s_dirty = true; }
    void scheduleCompositingLayerSync() override { s_dirty = true; }
    bool allowsAcceleratedCompositing() const override { return false; }
    CompositingTriggerFlags allowedCompositingTriggers() const override { return {}; }
    bool supportsFullscreenForNode(const WKC::Node*) override { return false; }
    void enterFullscreenForNode(WKC::Node*) override { }
    void exitFullscreenForNode(WKC::Node*) override { }
    bool requiresFullscreenForVideoPlayback() override { return false; }
    bool supportsFullScreenForElement(const WKC::Element*, bool) override { return false; }
    void enterFullScreenForElement(WKC::Element*) override { }
    void exitFullScreenForElement(WKC::Element*) override { }
    void notifyScrollerThumbIsVisibleInRect(const WKCRect&) override { }
    void recommendedScrollbarStyleDidChange(int) override { }
    void needTouchEvents(bool) override { }
};

class WaveFrameLoaderClient final : public WKC::FrameLoaderClientIf {
public:
    void frameLoaderDestroyed() override { }
    bool hasWebView() const override { return true; }
    void makeRepresentation(WKC::DocumentLoader*) override { }
    void forceLayout() override { }
    void forceLayoutForNonHTML() override { }
    void setCopiesOnScroll() override { }
    void detachedFromParent2() override { }
    void detachedFromParent3() override { }
    void assignIdentifierToInitialRequest(unsigned long identifier, WKC::DocumentLoader*, const WKC::ResourceRequest&) override { }
    void dispatchWillSendRequest(WKC::DocumentLoader*, unsigned long identifier, WKC::ResourceRequest&, const WKC::ResourceResponse& redirectResponse) override { }
    bool shouldUseCredentialStorage(WKC::DocumentLoader*, unsigned long identifier) override { return true; }
    void dispatchDidReceiveAuthenticationChallenge(WKC::DocumentLoader*, unsigned long identifier, const WKC::AuthenticationChallenge&) override { }
    void dispatchDidCancelAuthenticationChallenge(WKC::DocumentLoader*, unsigned long identifier, const WKC::AuthenticationChallenge&) override { }
    void dispatchDidReceiveResponse(WKC::DocumentLoader*, unsigned long identifier, const WKC::ResourceResponse&) override { s_dirty = true; }
    void dispatchDidReceiveContentLength(WKC::DocumentLoader*, unsigned long identifier, int lengthReceived) override { }
    void dispatchDidFinishLoading(WKC::DocumentLoader*, unsigned long identifier) override { }
    void dispatchDidFailLoading(WKC::DocumentLoader*, unsigned long identifier, const WKC::ResourceError&) override { }
    bool dispatchDidLoadResourceFromMemoryCache(WKC::DocumentLoader*, const WKC::ResourceRequest&, const WKC::ResourceResponse&, int length) override { return false; }
    void dispatchDidHandleOnloadEvents() override { }
    void dispatchDidReceiveServerRedirectForProvisionalLoad() override { }
    void dispatchDidCancelClientRedirect() override { }
    void dispatchWillPerformClientRedirect(const WKC::URL&, double, double) override { }
    void dispatchDidChangeLocationWithinPage() override { s_dirty = true; }
    void dispatchDidNavigateWithinPage() override { s_dirty = true; }
    void dispatchDidPushStateWithinPage() override { s_dirty = true; }
    void dispatchDidReplaceStateWithinPage() override { s_dirty = true; }
    void dispatchDidPopStateWithinPage() override { s_dirty = true; }
    void dispatchWillClose() override { }
    void dispatchDidReceiveIcon() override { }
    void dispatchDidStartProvisionalLoad() override { s_dirty = true; }
    void dispatchDidReceiveTitle(const WKC::String&) override { s_dirty = true; }
    void dispatchDidCommitLoad() override { s_dirty = true; }
    void dispatchDidFailProvisionalLoad(const WKC::ResourceError&) override { s_dirty = true; }
    void dispatchDidFailLoad(const WKC::ResourceError&) override { s_dirty = true; }
    void dispatchDidFinishDocumentLoad() override { s_dirty = true; }
    void dispatchDidFinishLoad() override { s_dirty = true; }
    void dispatchDidFirstLayout() override { s_dirty = true; }
    void dispatchDidFirstVisuallyNonEmptyLayout() override { s_dirty = true; }
    void dispatchDidNewFirstVisuallyNonEmptyLayout() override { s_dirty = true; }
    void dispatchDidLayout() override { s_dirty = true; }
    WKC::Frame* dispatchCreatePage() override { return {}; }
    void dispatchShow() override { }
    void dispatchDecidePolicyForNewWindowAction(WKC::FramePolicyFunction& function, const WKC::NavigationAction&, const WKC::ResourceRequest&, WKC::FormState*, const WKC::String& frameName) override { function.reply(WKC::PolicyUse); }
    void dispatchDecidePolicyForNavigationAction(WKC::FramePolicyFunction& function, const WKC::NavigationAction&, const WKC::ResourceRequest&, WKC::FormState*) override { function.reply(WKC::PolicyUse); }
    void dispatchDecidePolicyForResponse(WKC::FramePolicyFunction& function, const WKC::ResourceResponse&, const WKC::ResourceRequest&) override { function.reply(WKC::PolicyUse); }
    void cancelPolicyCheck() override { }
    void dispatchUnableToImplementPolicy(const WKC::ResourceError&) override { }
    void dispatchWillSendSubmitEvent(WKC::FormState*) override { }
    void dispatchWillSubmitForm(WKC::FramePolicyFunction& function, WKC::FormState*) override { function.reply(WKC::PolicyUse); }
    void revertToProvisionalState(WKC::DocumentLoader*) override { }
    void setMainDocumentError(WKC::DocumentLoader*, const WKC::ResourceError&) override { }
    void willChangeEstimatedProgress() override { }
    void didChangeEstimatedProgress() override { }
    void postProgressStartedNotification() override { }
    void postProgressEstimateChangedNotification() override { }
    void postProgressFinishedNotification() override { }
    void setMainFrameDocumentReady(bool) override { s_dirty = true; }
    void startDownload(const WKC::ResourceRequest&, const WKC::String& suggestedName) override { }
    void willChangeTitle(WKC::DocumentLoader*) override { }
    void didChangeTitle(WKC::DocumentLoader*) override { }
    void committedLoad(WKC::DocumentLoader*, const char*, int) override { s_dirty = true; }
    void finishedLoading(WKC::DocumentLoader*) override { s_dirty = true; }
    void updateGlobalHistory() override { }
    void updateGlobalHistoryRedirectLinks() override { }
    bool shouldGoToHistoryItem(WKC::HistoryItem*) const override { return true; }
    bool shouldStopLoadingForHistoryItem(WKC::HistoryItem*) const override { return false; }
    void updateGlobalHistoryItemForPage() override { }
    void didDisplayInsecureContent() override { }
    void didRunInsecureContent(WKC::SecurityOrigin*, const WKC::URL& uri) override { }
    void didDetectXSS(const WKC::URL&, bool didBlockEntirePage) override { }
    WKC::ResourceError cancelledError(const WKC::ResourceRequest&) override { return makeWaveDefaultResourceError(); }
    WKC::ResourceError blockedError(const WKC::ResourceRequest&) override { return makeWaveDefaultResourceError(); }
    WKC::ResourceError cannotShowURLError(const WKC::ResourceRequest&) override { return makeWaveDefaultResourceError(); }
    WKC::ResourceError interruptForPolicyChangeError(const WKC::ResourceRequest&) override { return makeWaveDefaultResourceError(); }
    WKC::ResourceError cannotShowMIMETypeError(const WKC::ResourceResponse&) override { return makeWaveDefaultResourceError(); }
    WKC::ResourceError fileDoesNotExistError(const WKC::ResourceResponse&) override { return makeWaveDefaultResourceError(); }
    bool shouldFallBack(const WKC::ResourceError&) override { return false; }
    bool canHandleRequest(const WKC::ResourceRequest&) const override { return true; }
    bool canShowMIMEType(const WKC::String&) const override { return true; }
    bool canShowMIMETypeAsHTML(const WKC::String&) const override { return true; }
    bool representationExistsForURLScheme(const WKC::String&) const override { return {}; }
    WKC::String generatedMIMETypeForURLScheme(const WKC::String&) const override { return {}; }
    void frameLoadCompleted() override { }
    void saveViewStateToItem(WKC::HistoryItem*) override { }
    void restoreViewState() override { }
    void provisionalLoadStarted() override { }
    void didFinishLoad() override { }
    void prepareForDataSourceReplacement() override { }
    void setTitle(const WKC::String& title, const WKC::URL&) override { }
    WKC::String userAgent(const WKC::URL&) override { return WKC::String("Mozilla/5.0 (Nintendo Wii U) AppleWebKit/605.1.15 (KHTML, like Gecko) Safari/605.1.15"); }
    void savePlatformDataToCachedFrame(WKC::CachedFrame*) override { }
    void transitionToCommittedFromCachedFrame(WKC::CachedFrame*) override { }
    void transitionToCommittedForNewPage() override { }
    void didSaveToPageCache() override { }
    void didRestoreFromPageCache() override { }
    void dispatchDidBecomeFrameset(bool) override { }
    bool canCachePage() const override { return false; }
    void download(WKC::ResourceHandle*, const WKC::ResourceRequest&, const WKC::ResourceResponse&) override { }
    WKC::Widget* createPlugin(const WKCSize&, WKC::HTMLPlugInElement*, const WKC::URL&, const WKC::String**, const WKC::String**, const WKC::String&, bool) override { return {}; }
    void redirectDataToPlugin(WKC::Widget* pluginWidget) override { }
    WKC::Widget* createJavaAppletWidget(const WKCSize&, WKC::HTMLAppletElement*, const WKC::URL& baseURL, const WKC::String** paramNames, const WKC::String** paramValues) override { return {}; }
    WKC::ObjectContentType objectContentType(const WKC::URL&, const WKC::String&, bool) override { return {}; }
    void dispatchDidClearWindowObjectInWorld(WKC::DOMWrapperWorld*) override { }
    void documentElementAvailable() override { }
    void didPerformFirstNavigation() const override { }
    void registerForIconNotification(bool) override { }
    void didChangeScrollOffset() override { }
    bool allowScript(bool enabledPerSettings) override { return true; }
    bool allowScriptFromSource(bool enabledPerSettings, const URL&) override { return true; }
    bool allowPlugins(bool enabledPerSettings) override { return false; }
    bool allowImage(bool enabledPerSettings, const URL&) override { return enabledPerSettings; }
    bool allowDisplayingInsecureContent(bool enabledPerSettings, WKC::SecurityOrigin*, const URL&) override { return false; }
    bool allowRunningInsecureContent(bool enabledPerSettings, WKC::SecurityOrigin*, const URL&) override { return false; }
    void didNotAllowScript() override { }
    void didNotAllowPlugins() override { }
    bool shouldForceUniversalAccessFromLocalURL(const URL&) override { return false; }
    void dispatchGlobalObjectAvailable(WKC::DOMWrapperWorld*) override { }
    bool dispatchWillAcceptCookie(bool income, WKC::ResourceHandle* handle, const char* url, const char* firstparty_host, const char* cookie_domain) override { return true; }
    bool dispatchWillReceiveData(WKC::ResourceHandle* handle, int length) override { return true; }
    bool notifySSLHandshakeStatus(WKC::ResourceHandle* handle, SSLHandshakeStatus status) override { return false; }
    int dispatchWillPermitSendRequest(void* handle, const char* url, WKC::ContentComposition composition, bool isSync, const WKC::ResourceResponse& redirectResponse) override { return 1; }
#ifdef WKC_ENABLE_CUSTOMJS
    bool dispatchWillCallCustomJS(WKCCustomJSAPIList* api, void** context) override { return {}; }
#endif
};

class WaveEditorClient final : public WKC::EditorClientIf {
public:
    void pageDestroyed() override { }
    bool shouldDeleteRange(WKC::Range*) override { return true; }
    bool shouldShowDeleteInterface(WKC::HTMLElement*) override { return true; }
    bool smartInsertDeleteEnabled() override { return {}; }
    bool isSelectTrailingWhitespaceEnabled() override { return {}; }
    bool isContinuousSpellCheckingEnabled() override { return {}; }
    void toggleContinuousSpellChecking() override { }
    bool isGrammarCheckingEnabled() override { return {}; }
    void toggleGrammarChecking() override { }
    int spellCheckerDocumentTag() override { return {}; }
    bool shouldBeginEditing(WKC::Range*) override { return true; }
    bool shouldEndEditing(WKC::Range*) override { return true; }
    bool shouldInsertNode(WKC::Node*, WKC::Range*, WKC::EditorInsertAction) override { return true; }
    bool shouldInsertText(const WKC::String&, WKC::Range*, WKC::EditorInsertAction) override { return true; }
    bool shouldChangeSelectedRange(WKC::Range* fromRange, WKC::Range* toRange, WKC::EAffinity, bool stillSelecting) override { return true; }
    bool shouldApplyStyle(WKC::CSSStyleDeclaration*, WKC::Range*) override { return true; }
    bool shouldMoveRangeAfterDelete(WKC::Range*, WKC::Range*) override { return true; }
    void didBeginEditing() override { }
    void respondToChangedContents() override { }
    void respondToChangedSelection(WKC::Frame*) override { }
    void didEndEditing() override { }
    void didWriteSelectionToPasteboard() override { }
    void didSetSelectionTypesForPasteboard() override { }
    void registerUndoStep(WKC::EditCommand*) override { }
    void registerRedoStep(WKC::EditCommand*) override { }
    void clearUndoRedoOperations() override { }
    bool canCopyCut(Frame*, bool) override { return false; }
    bool canPaste(Frame*, bool) override { return false; }
    bool canUndo() const override { return {}; }
    bool canRedo() const override { return {}; }
    void undo() override { }
    void redo() override { }
    void handleKeyboardEvent(WKC::KeyboardEvent*) override { }
    void handleInputMethodKeydown(WKC::KeyboardEvent*) override { }
    void textFieldDidBeginEditing(WKC::Element*) override { }
    void textFieldDidEndEditing(WKC::Element*) override { }
    void textDidChangeInTextField(WKC::Element*) override { }
    bool doTextFieldCommandFromEvent(WKC::Element*, WKC::KeyboardEvent*) override { return {}; }
    void textWillBeDeletedInTextField(WKC::Element*) override { }
    void textDidChangeInTextArea(WKC::Element*) override { }
    void updateSpellingUIWithGrammarString(const WKC::String&, const WKC::GrammarDetail&) override { }
    void updateSpellingUIWithMisspelledWord(const WKC::String&) override { }
    void showSpellingUI(bool show) override { }
    bool spellingUIIsShowing() override { return {}; }
    void willSetInputMethodState() override { }
    void setInputMethodState(bool enabled) override { }
};

class WaveContextMenuClient final : public WKC::ContextMenuClientIf {
public:
    void contextMenuDestroyed() override { }
    WKC::PlatformMenuDescription getCustomMenuFromDefaultItems(WKC::ContextMenu*) override { return nullptr; }
    void contextMenuItemSelected(WKC::ContextMenuItem*, const WKC::ContextMenu*) override { }
    void downloadURL(const WKC::URL& url) override { }
    void searchWithGoogle(const WKC::Frame*) override { }
    void lookUpInDictionary(WKC::Frame*) override { }
    void speak(const WKC::String&) override { }
    void stopSpeaking() override { }
    bool isSpeaking() override { return false; }
};

class WaveDragClient final : public WKC::DragClientIf {
public:
    void willPerformDragDestinationAction(WKC::DragDestinationAction, WKC::DragData*) override { }
    void willPerformDragSourceAction(WKC::DragSourceAction, const WKCPoint&, WKC::DataTransfer*) override { }
    WKC::DragSourceAction dragSourceActionMaskForPoint(const WKCPoint& windowPoint) override { return {}; }
    void startDrag(WKC::DragImageRef dragImage, const WKCPoint& dragImageOrigin, const WKCPoint& eventPos, WKC::DataTransfer*, WKC::Frame*, bool linkDrag = false) override { }
};

class WaveDropDownListClient final : public WKC::DropDownListClientIf {
public:
    void show(const WKCRect& r, WKC::FrameView* view, int index, WKC::PopupMenuClient *client) override { }
    void hide(WKC::PopupMenuClient *client) override { }
    void updateFromElement(WKC::PopupMenuClient *client) override { }
};

class WaveClientBuilders final : public WKC::WKCClientBuilders {
public:
    WKC::ChromeClientIf* createChromeClient(WKC::WKCWebView*) override { return new WaveChromeClient(); }
    void deleteChromeClient(WKC::ChromeClientIf* client) override { delete static_cast<WaveChromeClient*>(client); }
    WKC::ContextMenuClientIf* createContextMenuClient(WKC::WKCWebView*) override { return new WaveContextMenuClient(); }
    void deleteContextMenuClient(WKC::ContextMenuClientIf* client) override { delete static_cast<WaveContextMenuClient*>(client); }
    WKC::DragClientIf* createDragClient(WKC::WKCWebView*) override { return new WaveDragClient(); }
    void deleteDragClient(WKC::DragClientIf* client) override { delete static_cast<WaveDragClient*>(client); }
    WKC::EditorClientIf* createEditorClient(WKC::WKCWebView*) override { return new WaveEditorClient(); }
    void deleteEditorClient(WKC::EditorClientIf* client) override { delete static_cast<WaveEditorClient*>(client); }
    WKC::FrameLoaderClientIf* createFrameLoaderClient(WKC::WKCWebFrame*) override { return new WaveFrameLoaderClient(); }
    void deleteFrameLoaderClient(WKC::FrameLoaderClientIf* client) override { delete static_cast<WaveFrameLoaderClient*>(client); }
    WKC::InspectorClientIf* createInspectorClient(WKC::WKCWebView*) override { return nullptr; }
    void deleteInspectorClient(WKC::InspectorClientIf*) override {}
    WKC::DropDownListClientIf* createDropDownListClient(WKC::WKCWebView*) override { return new WaveDropDownListClient(); }
    void deleteDropDownListClient(WKC::DropDownListClientIf* client) override { delete static_cast<WaveDropDownListClient*>(client); }
    WKC::GeolocationClientIf* createGeolocationClient(WKC::WKCWebView*) override { return nullptr; }
    void deleteGeolocationClient(WKC::GeolocationClientIf*) override {}
    WKC::DeviceMotionClientIf* createDeviceMotionClient(WKC::WKCWebView*) override { return nullptr; }
    void deleteDeviceMotionClient(WKC::DeviceMotionClientIf*) override {}
    WKC::DeviceOrientationClientIf* createDeviceOrientationClient(WKC::WKCWebView*) override { return nullptr; }
    void deleteDeviceOrientationClient(WKC::DeviceOrientationClientIf*) override {}
    WKC::BackForwardClientIf* createBackForwardClient(WKC::WKCWebView*) override { return nullptr; }
    void deleteBackForwardClient(WKC::BackForwardClientIf*) override {}
    WKC::BatteryClientIf* createBatteryClient(WKC::WKCWebView*) override { return nullptr; }
    void deleteBatteryClient(WKC::BatteryClientIf*) override {}
};

static WaveMemoryEventHandler s_memory_event_handler;
static WaveTimerEventHandler s_timer_event_handler;
static WaveClientBuilders s_client_builders;

// ─── Thread ──────────────────────────────────────────────────────────────────

struct WKCThread {
    OSThread  ost;
    uint8_t*  stack;
    void*   (*fn)(void*);
    void*     arg;
    void*     result;
};

// Use OSThreadSpecificID slot 0 to store WKCThread pointer
#define WKC_THREAD_SLOT ((OSThreadSpecificID)0)

static int thread_trampoline(int, const char**) {
    WKCThread* t = (WKCThread*)OSGetThreadSpecific(WKC_THREAD_SLOT);
    if (t) t->result = t->fn(t->arg);
    return 0;
}

// ─── TLS ─────────────────────────────────────────────────────────────────────

#define TLS_KEYS  32
#define TLS_THRDS 16
struct TLSKey  { bool used; void(*dtor)(void*); };
struct ThrTLS  { void* thr; void* val[TLS_KEYS]; };
static TLSKey  s_tls_keys[TLS_KEYS] = {};
static ThrTLS  s_tls_thr[TLS_THRDS] = {};
static OSMutex s_tls_mtx;

static ThrTLS* tls_get(void* thr) {
    OSLockMutex(&s_tls_mtx);
    for (auto& t : s_tls_thr) if (t.thr==thr)  { OSUnlockMutex(&s_tls_mtx); return &t; }
    for (auto& t : s_tls_thr) if (!t.thr)       { t.thr=thr; OSUnlockMutex(&s_tls_mtx); return &t; }
    OSUnlockMutex(&s_tls_mtx);
    return nullptr;
}

// ─── Font / Offscreen ────────────────────────────────────────────────────────

struct WKCFont { TTF_Font* f; int size; };
struct WKCOff  { unsigned char* bmp; int rb, w, h; bool owned; };
static const unsigned char k_white[4] = {0xFF,0xFF,0xFF,0xFF};

// ─── UTF-16 → UTF-8 ──────────────────────────────────────────────────────────

static int u16_to_u8(const unsigned short* in, int len, char* out, int max) {
    int j=0;
    for (int i=0; i<len && j<max-4; i++) {
        unsigned short c=in[i];
        if      (c<0x80)  { out[j++]=(char)c; }
        else if (c<0x800) { out[j++]=0xC0|(c>>6); out[j++]=0x80|(c&0x3F); }
        else               { out[j++]=0xE0|(c>>12); out[j++]=0x80|((c>>6)&0x3F); out[j++]=0x80|(c&0x3F); }
    }
    out[j]='\0'; return j;
}

// =============================================================================
extern "C" {
// =============================================================================

// ── Timer ────────────────────────────────────────────────────────────────────

bool wkcTimerInitializePeer(bool(*)(unsigned int, void*)) {
    if (!s_timer_init) { OSInitMutex(&s_timer_mtx); s_timer_init=true; }
    return true;
}
void wkcTimerFinalizePeer(void)        {}
void wkcTimerForceTerminatePeer(void)  {}

void* wkcTimerNewPeer(void) {
    auto* t=(WKCTimer*)calloc(1,sizeof(WKCTimer));
    if (!t) return nullptr;
    OSLockMutex(&s_timer_mtx); t->next=s_timers; s_timers=t; OSUnlockMutex(&s_timer_mtx);
    return t;
}
void wkcTimerDeletePeer(void* p) {
    if (!p) return;
    auto* t=(WKCTimer*)p;
    OSLockMutex(&s_timer_mtx);
    t->active=false;
    WKCTimer** pp=&s_timers;
    while (*pp&&*pp!=t) pp=&(*pp)->next;
    if (*pp) *pp=t->next;
    OSUnlockMutex(&s_timer_mtx);
    free(t);
}
bool wkcTimerStartOneShotPeer(void* p, unsigned int ms, wkcTimeoutProc cb, void* d) {
    if (!p) return false;
    auto* t=(WKCTimer*)p;
    OSLockMutex(&s_timer_mtx);
    t->proc=cb; t->data=d; t->fire_at=now_ms()+ms; t->interval=0; t->active=true; t->wkc_wakeup=false;
    OSUnlockMutex(&s_timer_mtx);
    return true;
}
bool wkcTimerStartPeriodicPeer(void* p, unsigned int ms, wkcTimeoutProc cb, void* d) {
    if (!p) return false;
    auto* t=(WKCTimer*)p;
    OSLockMutex(&s_timer_mtx);
    t->proc=cb; t->data=d; t->fire_at=now_ms()+ms; t->interval=ms; t->active=true; t->wkc_wakeup=false;
    OSUnlockMutex(&s_timer_mtx);
    return true;
}
void wkcTimerCancelPeer(void* p)       { if (p) ((WKCTimer*)p)->active=false; }
void wkcTimerWakeUpPeer(void*)         {}
unsigned int wkcGetTickCountPeer(void) { return now_ms(); }

// ── Thread ───────────────────────────────────────────────────────────────────

bool wkcThreadInitializePeer(void) {
    OSInitMutex(&s_tls_mtx);
    return true;
}
void wkcThreadFinalizePeer(void)             {}
void wkcThreadForceTerminatePeer(void)       {}
void wkcThreadForceTerminateThreadsPeer(void){}
void wkcThreadForceTerminateExceptThreadsPeer(void){}

void* wkcThreadCreatePeer(void*(*fn)(void*), void* arg) {
    auto* t=(WKCThread*)calloc(1,sizeof(WKCThread));
    if (!t) return nullptr;
    t->fn=fn; t->arg=arg;
    t->stack=(uint8_t*)malloc(64*1024);
    if (!t->stack) { free(t); return nullptr; }
    OSCreateThread(&t->ost, thread_trampoline, 0, nullptr,
                   t->stack+64*1024, 64*1024, 16, OS_THREAD_ATTRIB_AFFINITY_ANY);
    OSSetThreadSpecific(WKC_THREAD_SLOT, t);
    OSResumeThread(&t->ost);
    return t;
}
void* wkcThreadCreateForMultiCorePeer(void*(*fn)(void*), void* a) { return wkcThreadCreatePeer(fn,a); }

int wkcThreadJoinPeer(void* p, void** res) {
    if (!p) return -1;
    auto* t=(WKCThread*)p;
    OSJoinThread(&t->ost, nullptr);
    if (res) *res=t->result;
    free(t->stack); free(t);
    return 0;
}
void wkcThreadDetachPeer(void* p)             { if (p) OSDetachThread(&((WKCThread*)p)->ost); }
void wkcThreadSetCurrentThreadNamePeer(const char* n) { if(n) OSSetThreadName(OSGetCurrentThread(),n); }
void* wkcThreadCurrentThreadPeer(void)        { return OSGetCurrentThread(); }
void* wkcThreadGetStackBasePeer(void*)        { return nullptr; }  // not accessible via WUT
unsigned int wkcThreadGetStackSizePeer(void*) { return 64*1024; }
void wkcThreadSetStackSizePeer(void*,unsigned int) {}
int  wkcThreadEqualPeer(void* a, void* b)     { return a==b ? 1 : 0; }
void wkcThreadSuspendPeer(void* t)            { if(t) OSSuspendThread((OSThread*)t); }
void wkcThreadResumePeer(void* t)             { if(t) OSResumeThread((OSThread*)t); }
void wkcThreadSuspendAllThreadsPeer(void)     {}
bool wkcThreadTimedSleepPeer(unsigned int ms) { OSSleepTicks(OSMillisecondsToTicks(ms)); return true; }
void wkcThreadYieldPeer(void)                 { OSYieldThread(); }
void wkcThreadSetMainThreadInfoPeer(void*,void*) {}
void* wkcThreadGetMainThreadPeer(void)        { return nullptr; }
bool wkcThreadIsSuspendedPeer(void*)          { return false; }
void wkcThreadSetMutexPeer(void*,void*)       {}
void* wkcThreadGetMutexPeer(void*)            { return nullptr; }
void wkcThreadSetCondPeer(void*,void*)        {}
void* wkcThreadGetCondPeer(void*)             { return nullptr; }
void wkcThreadStoreRegistersPeer(void*,int)   {}
int  wkcThreadStoreThreadRegistersPeer(void*,void*,int) { return 0; }
void* wkcThreadGetStackPointerPeer(const void*) { return nullptr; }
int  wkcThreadAtomicIncrementPeer(int volatile* v) { return ++(*v); }
int  wkcThreadAtomicDecrementPeer(int volatile* v) { return --(*v); }

int wkcThreadKeyNewPeer(void** out, void(*dtor)(void*)) {
    OSLockMutex(&s_tls_mtx);
    for (int i=0; i<TLS_KEYS; i++) {
        if (!s_tls_keys[i].used) {
            s_tls_keys[i]={true,dtor};
            *out=(void*)(uintptr_t)(i+1);
            OSUnlockMutex(&s_tls_mtx);
            return 0;
        }
    }
    OSUnlockMutex(&s_tls_mtx);
    return -1;
}
void wkcThreadKeyDeletePeer(void* k) {
    int i=(int)(uintptr_t)k-1;
    if (i>=0&&i<TLS_KEYS) { OSLockMutex(&s_tls_mtx); s_tls_keys[i]={false,nullptr}; OSUnlockMutex(&s_tls_mtx); }
}
void wkcThreadKeySetSpecificPeer(void* k, void* v) {
    int i=(int)(uintptr_t)k-1;
    if (i<0||i>=TLS_KEYS) return;
    if (auto* t=tls_get(OSGetCurrentThread())) t->val[i]=v;
}
void* wkcThreadKeyGetSpecificPeer(void* k) {
    int i=(int)(uintptr_t)k-1;
    if (i<0||i>=TLS_KEYS) return nullptr;
    auto* t=tls_get(OSGetCurrentThread());
    return t ? t->val[i] : nullptr;
}

// ── Memory ───────────────────────────────────────────────────────────────────

bool wkcMemoryInitializePeer(wkcPeerMallocProc,wkcPeerFreeProc,wkcPeerReallocProc){ return true; }
bool wkcMemoryIsInitializedPeer(void)    { return true; }
void wkcMemoryFinalizePeer(void)         {}
void wkcMemoryForceTerminatePeer(void)   {}
void wkcMemorySetNotifyNoMemoryProcPeer(void*(*)(unsigned int)){}
void wkcMemorySetNotifyMemoryAllocationErrorProcPeer(void(*)(unsigned int,int)){}
void wkcMemorySetNotifyCrashProcPeer(void(*)(const char*,int,const char*,const char*)){}
void wkcMemorySetNotifyStackOverflowProcPeer(void(*)(bool,unsigned int,unsigned int,unsigned int,void*,void*,void*,const char*,int,const char*)){}
void wkcMemorySetCheckAvailabilityProcPeer(bool(*)(unsigned int,bool)){}
void wkcMemorySetCheckMemoryAllocatableProcPeer(bool(*)(unsigned int,int)){}
void* wkcMemoryNotifyNoMemoryPeer(unsigned int)    { return nullptr; }
void  wkcMemorySetCrashReasonPeer(const char*,int,const char*,const char*){}
void  wkcMemoryNotifyCrashPeer(void)               {}
void  wkcMemoryNotifyStackOverflowPeer(bool,unsigned int,unsigned int,void*,const char*,int,const char*){}
bool  wkcMemoryIsStackOverflowPeer(unsigned int,unsigned int*,void**) { return false; }
bool  wkcMemoryCheckAvailabilityPeer(unsigned int)  { return true; }
void  wkcMemorySetAllocatingForSVGPeer(bool)        {}
void  wkcMemorySetAllocatingForImagesPeer(bool)     {}
void  wkcMemorySetAllocationForAnimatedImage(bool)  {}
bool  wkcMemoryIsAllocatingForSVGPeer(void)         { return false; }
int   wkcMemoryGetAllocationStatePeer(void)         { return 0; }
bool  wkcMemoryIsCrashingPeer(void)                 { return false; }
bool  wkcMemoryCheckMemoryAllocatablePeer(unsigned int,int){ return true; }
void  wkcMemoryNotifyMemoryAllocationErrorPeer(unsigned int,int){}
void  wkcMemoryRegisterGlobalObjPeer(volatile void*,int){}
size_t wkcMemoryGetPageSizePeer(void)               { return 4096; }
void  wkcMemorySetExecutablePeer(void*,size_t,bool) {}
void  wkcMemoryCacheFlushPeer(void*,size_t)         {}
void* wkcMemoryFillMem16Peer(void* d,unsigned short v,unsigned int n){
    auto* p=(unsigned short*)d; for(unsigned int i=0;i<n/2;i++) p[i]=v; return d;
}
unsigned short* wkcMemoryFillRange16Peer(unsigned short* d,unsigned short s,unsigned int x,unsigned int y,unsigned int w,unsigned int h,unsigned int v){
    for(unsigned int r=0;r<h;r++){auto* row=d+(y+r)*s+x;for(unsigned int c=0;c<w;c++)row[c]=(unsigned short)v;}return d;
}
void* wkcMemoryFillMem32Peer(void* d,unsigned int v,unsigned int n){
    auto* p=(unsigned int*)d; for(unsigned int i=0;i<n/4;i++) p[i]=v; return d;
}
unsigned int* wkcMemoryFillRange32Peer(unsigned int* d,unsigned int s,unsigned int x,unsigned int y,unsigned int w,unsigned int h,unsigned int v){
    for(unsigned int r=0;r<h;r++){auto* row=d+(y+r)*s+x;for(unsigned int c=0;c<w;c++)row[c]=v;}return d;
}
void* wkcMemoryMoveMemPeer(void* d,void* s,unsigned int n)        { return memmove(d,s,n); }
void* wkcMemoryCopyAlignedMemPeer(void* d,void* s,unsigned int n) { return memcpy(d,s,n); }

// ── File ─────────────────────────────────────────────────────────────────────
// Signatures match wkcpeer.h exactly: int usage, wkc_uint64, wkc_int64

void*      wkcFileFOpenPeer(int /*usage*/, const char* p, const char* m) { return fopen(p,m); }
int        wkcFileFClosePeer(void* f)                    { return fclose((FILE*)f); }
wkc_uint64 wkcFileFReadPeer(void* b, wkc_uint64 s, wkc_uint64 n, void* f) { return (wkc_uint64)fread(b,(size_t)s,(size_t)n,(FILE*)f); }
wkc_uint64 wkcFileFWritePeer(const void* b, wkc_uint64 s, wkc_uint64 n, void* f) { return (wkc_uint64)fwrite(b,(size_t)s,(size_t)n,(FILE*)f); }
int        wkcFileFSeekPeer(void* f, wkc_int64 o, int w) { return fseek((FILE*)f,(long)o,w); }
wkc_int64  wkcFileFTellPeer(void* f)                     { return (wkc_int64)ftell((FILE*)f); }
bool       wkcFileFEofPeer(void* f)                      { return feof((FILE*)f)!=0; }
char*      wkcFileFGetsPeer(char* s, int n, void* f)     { return fgets(s,n,(FILE*)f); }
int        wkcFileFPutsPeer(const char* s, void* f)      { return fputs(s,(FILE*)f); }
int        wkcFileFFlushPeer(void* f)                    { return fflush((FILE*)f); }
int        wkcFileFErrorPeer(void* f)                    { return ferror((FILE*)f); }
int        wkcFileAccessPeer(const char* p, int m)       { return access(p,m); }
bool       wkcFileUnlinkPeer(const char* p)              { return remove(p)==0; }
int        wkcFileStatPeer(const char* p, struct stat* s){ return stat(p,s); }
int        wkcFileErrnoPeer(void)                        { return errno; }

bool wkcFileMakeAllDirectoriesPeer(const char* path) {
    char t[512]; strncpy(t,path,511);
    for (char* p=t+1;*p;p++) if(*p=='/'){*p='\0';mkdir(t,0777);*p='/';}
    return mkdir(t,0777)==0||errno==EEXIST;
}
int wkcFilePathGetFileNamePeer(const char* p,char* out,int mx){
    const char* s=strrchr(p,'/'); s=s?s+1:p;
    strncpy(out,s,mx-1); return (int)strlen(out);
}
int wkcFilePathByAppendingComponentPeer(const char* p,const char* c,char* out,int mx){
    snprintf(out,mx,"%s/%s",p,c); return (int)strlen(out);
}
int wkcFileHomeDirectoryPathPeer(char* out,int mx){
    strncpy(out,"fs:/vol/external01/wiiu/apps/WaveBrowser",mx-1); return (int)strlen(out);
}
int  wkcFileListDirectoryPeer(const char*,const char*,char**,int,int){ return 0; }
int  wkcFileDirectoryNamePeer(const char* p,char* out,int mx){
    const char* s=strrchr(p,'/');
    if(!s){strncpy(out,".",mx-1);return 1;}
    int n=(int)(s-p);if(n>=mx)n=mx-1;
    strncpy(out,p,n);out[n]='\0';return n;
}
void* wkcFileOpenTemporaryFilePeer(const char* prefix,char* out,int mx){
    if (!out || mx <= 1) return nullptr;

    // WebKit uses temporary files for cache and intermediate resource data.
    // Return an actual open FILE* as required by the peer API; returning a
    // directory name with a null handle silently breaks those code paths.
    static unsigned int serial = 0;
    const char* dir = "fs:/vol/external01/wiiu/apps/WaveBrowser/tmp";
    (void)mkdir("fs:/vol/external01/wiiu/apps/WaveBrowser", 0777);
    (void)mkdir(dir, 0777);

    for (unsigned int attempt = 0; attempt < 32; ++attempt) {
        const unsigned int id = ++serial;
        const char* safePrefix = (prefix && prefix[0]) ? prefix : "wkc";
        int n = snprintf(out, (size_t)mx, "%s/%s-%08x.tmp", dir, safePrefix, id);
        if (n < 0 || n >= mx) {
            out[0] = '\0';
            return nullptr;
        }
        FILE* file = fopen(out, "w+b");
        if (file) return file;
        if (errno != EEXIST) break;
    }

    out[0] = '\0';
    return nullptr;
}
void* wkcFileOpenDirectoryPeer(int,const char*,const char*){ return nullptr; }
void  wkcFileCloseDirectoryPeer(void*)      {}
int   wkcFileReadDirectoryPeer(void*,char*,int){ return 0; }

bool wkcFileInitializePeer(void)     { return true; }
void wkcFileFinalizePeer(void)       {}
void wkcFileForceTerminatePeer(void) {}

// ── Debug ────────────────────────────────────────────────────────────────────

bool wkcDebugPrintInitializePeer(void)     { return true; }
void wkcDebugPrintFinalizePeer(void)       {}
void wkcDebugPrintForceTerminatePeer(void) {}
void wkcDebugPrintfPeer(const char* fmt,...){ va_list v;va_start(v,fmt);vprintf(fmt,v);va_end(v); }
void wkcDebugPutsPeer(const char* s)       { puts(s); }
int  wkcDebugGetBacktracePeer(void**,int)  { return 0; }
void wkcDebugPrintBacktracePeer(void**,int){}
void peerDebugPrintf(const char* fmt,...)  { va_list v;va_start(v,fmt);vprintf(fmt,v);va_end(v); }

// ── System ───────────────────────────────────────────────────────────────────

bool wkcSystemInitializePeer(void)  { return true; }
void wkcSystemFinalizePeer(void)    {}

static const unsigned short kPlatform[]={'W','i','i','U',0};
static const unsigned short kProduct[] ={'G','e','c','k','o',0};
static const unsigned short kVendor[]  ={'W','a','v','e','B','r','o','w','s','e','r',0};
static const unsigned short kLang[]    ={'e','n',0};
static const unsigned short kEmpty[]   ={0};
static const unsigned short kSubmit[]  ={'S','u','b','m','i','t',0};
static const unsigned short kReset[]   ={'R','e','s','e','t',0};
static const unsigned short kFile[]    ={'C','h','o','o','s','e',' ','F','i','l','e',0};

const unsigned short* wkcSystemGetNavigatorPlatformPeer(void)   { return kPlatform; }
const unsigned short* wkcSystemGetNavigatorProductPeer(void)    { return kProduct; }
const unsigned short* wkcSystemGetNavigatorProductSubPeer(void) { return kEmpty; }
const unsigned short* wkcSystemGetNavigatorVendorPeer(void)     { return kVendor; }
const unsigned short* wkcSystemGetNavigatorVendorSubPeer(void)  { return kEmpty; }
const unsigned short* wkcSystemGetLanguagePeer(void)            { return kLang; }
const unsigned short* wkcSystemGetButtonLabelSubmitPeer(void)   { return kSubmit; }
const unsigned short* wkcSystemGetButtonLabelResetPeer(void)    { return kReset; }
const unsigned short* wkcSystemGetButtonLabelFilePeer(void)     { return kFile; }
void wkcSystemSetNavigatorPlatformPeer(const unsigned short*)   {}
void wkcSystemSetNavigatorProductPeer(const unsigned short*)    {}
void wkcSystemSetNavigatorProductSubPeer(const unsigned short*) {}
void wkcSystemSetNavigatorVendorPeer(const unsigned short*)     {}
void wkcSystemSetNavigatorVendorSubPeer(const unsigned short*)  {}
void wkcSystemSetLanguagePeer(const unsigned short*)            {}
void wkcSystemSetButtonLabelSubmitPeer(const unsigned short*)   {}
void wkcSystemSetButtonLabelResetPeer(const unsigned short*)    {}
void wkcSystemSetButtonLabelFilePeer(const unsigned short*)     {}
bool wkcSystemSetSystemStringPeer(const unsigned char*,const unsigned char*){ return true; }
const unsigned char* wkcSystemGetSystemStringPeer(const unsigned char*)     { return nullptr; }

// ── SSL ──────────────────────────────────────────────────────────────────────

bool wkcHWOffscreenInitializePeer(void) { return true; }
void wkcHWOffscreenFinalizePeer(void) {}
void wkcHWOffscreenForceTerminatePeer(void) {}
bool wkcAudioInitializePeer(void) { return true; }
void wkcAudioFinalizePeer(void) {}
void wkcAudioForceTerminatePeer(void) {}
bool wkcMediaPlayerInitializePeer(void) { return true; }
void wkcMediaPlayerFinalizePeer(void) {}
void wkcMediaPlayerForceTerminatePeer(void) {}
bool wkcNetInitializePeer(void) { return true; }
void wkcNetFinalizePeer(void) {}
void wkcNetForceTerminatePeer(void) {}
void wkcHeapInitializePeer(void*, size_t) {}
void wkcHeapFinalizePeer(void) {}
void wkcHeapForceTerminatePeer(void) {}

bool  wkcSSLInitializePeer(void)           { return true; }
void  wkcSSLFinalizePeer(void)             {}
void  wkcSSLForceTerminatePeer(void)       {}
void* wkcSSLRegisterRootCAPeer(const char*,int)  { return (void*)1; }
int   wkcSSLUnregisterRootCAPeer(void*)    { return 0; }
void  wkcSSLRootCADeleteAllPeer(void)      {}
void* wkcSSLRegisterCRLPeer(const char*,int)     { return (void*)1; }
int   wkcSSLUnregisterCRLPeer(void*)       { return 0; }
void  wkcSSLCRLDeleteAllPeer(void)         {}

FILE* wkcOsslCertfOpenPeer(const char* p,const char* m)        { return fopen(p,m); }
int   wkcOsslCertfClosePeer(FILE* f)                           { return fclose(f); }
size_t wkcOsslCertfReadPeer(void* b,size_t s,size_t n,FILE* f) { return fread(b,s,n,f); }
size_t wkcOsslCertfWritePeer(const void* b,size_t s,size_t n,FILE* f){ return fwrite(b,s,n,f); }
char*  wkcOsslCertfGetsPeer(char* s,int n,FILE* f)             { return fgets(s,n,f); }
size_t wkcOsslCertfFlushPeer(FILE* f)                          { return fflush(f); }
int    wkcOsslCertfSeekPeer(FILE* f,long o,int w)              { return fseek(f,o,w); }
long   wkcOsslCertfTellPeer(FILE* f)                           { return ftell(f); }
bool   wkcOsslCertfIsRegistPeer(void)                          { return false; }
bool   wkcOsslCRLIsRegistPeer(void)                            { return false; }

FILE*  wkcOsslRandFilefOpenPeer(const char* p,const char* m)         { return fopen(p,m); }
int    wkcOsslRandFilefClosePeer(FILE* f)                            { return fclose(f); }
int    wkcOsslRandFileStatPeer(const char* p,struct stat* s)         { return stat(p,s); }
size_t wkcOsslRandFilefReadPeer(void* b,size_t s,size_t n,FILE* f)  { return fread(b,s,n,f); }
size_t wkcOsslRandFilefWritePeer(const void* b,size_t s,size_t n,FILE* f){ return fwrite(b,s,n,f); }

int          wkcNetCheckCorrectIPAddressPeer(const char*) { return 1; }
unsigned int wkcRandomNumberPeer(void)            { return (unsigned int)rand(); }
int          wkcRandomNumbersPeer(unsigned char* b,int n){ for(int i=0;i<n;i++)b[i]=(unsigned char)rand();return n; }
unsigned int wkcRandomMaxPeer(void)               { return RAND_MAX; }

// ── PCAP ─────────────────────────────────────────────────────────────────────

bool wkcPCAPInitializePeer(const char*) { return true; }
void wkcPCAPFinalizePeer(void)          {}
void wkcPCAPForceTerminatePeer(void)    {}

// ── Pasteboard ───────────────────────────────────────────────────────────────

void wkcPasteboardCallbackSetPeer(const WKCPasteboardProcs*) {}
void wkcPasteboardClearPeer(void)                {}
void wkcPasteboardWriteHTMLPeer(const char*,int,const char*,int,const char*,int,bool){}
void wkcPasteboardWritePlainTextPeer(const char*,int) {}
void wkcPasteboardWriteURIPeer(const char*,int,const char*,int) {}
void wkcPasteboardWriteImagePeer(int,void*,int,void*,int,const WKCSize*) {}
int  wkcPasteboardReadHTMLPeer(char*,int)        { return 0; }
int  wkcPasteboardReadHTMLURIPeer(char*,int)     { return 0; }
int  wkcPasteboardReadPlainTextPeer(char*,int)   { return 0; }
bool wkcPasteboardIsFormatAvailablePeer(int)     { return false; }

// ── Misc ─────────────────────────────────────────────────────────────────────

const char* wkcGuessMIMETypeByContentPeer(const unsigned char*,size_t){ return "application/octet-stream"; }

// ── Font engine ──────────────────────────────────────────────────────────────
// fontPeerMalloc/fontPeerFree already typedef'd in wkcgpeer.h — do NOT redefine

bool wkcFontEngineInitializePeer(void*,int,fontPeerMalloc,fontPeerFree,bool){ return true; }
void wkcFontEngineFinalizePeer(void)       {}
void wkcFontEngineForceTerminatePeer(void) {}
bool wkcFontEngineCanSuspendPeer(void)     { return false; }
bool wkcFontEngineSupportsNonSpacingMarksPeer(void){ return false; }
bool wkcFontEngineCanSupportPeer(const unsigned char*,unsigned int){ return true; }
bool wkcFontEngineCanSupportByFormatNamePeer(const unsigned char*,unsigned int){ return true; }
int  wkcFontEngineRegisterSystemFontPeer(int,const unsigned char*,unsigned int){ return 0; }
int  wkcFontEngineRegisterFontPeer(int,const unsigned char*,unsigned int)      { return 1; }
void wkcFontEngineUnregisterFontPeer(int)  {}
void wkcFontEngineUnregisterFontsPeer()    {}
bool wkcFontEngineGetFamilyNamePeer(int,char* out,int mx){ strncpy(out,"Roboto",mx-1);return true; }
bool wkcFontEngineSetPrimaryFontPeer(int)  { return true; }
int  wkcFontEngineHeapSizePeer(void)       { return 0; }
bool wkcFontEngineSetFontScalePeer(int,float){ return true; }

void* wkcFontNewPeer(int size,int,bool,bool,bool,int,const char*){
    auto* f=(WKCFont*)malloc(sizeof(WKCFont));
    if (!f) return nullptr;
    f->size=size;
    f->f=TTF_OpenFontRW(SDL_RWFromConstMem(font_data,font_data_len),0,size);
    return f;
}
void* wkcFontNewCopyPeer(void* p){
    return p ? wkcFontNewPeer(((WKCFont*)p)->size,0,false,false,false,0,nullptr) : nullptr;
}
void wkcFontDeletePeer(void* p){ if(p){auto*f=(WKCFont*)p;TTF_CloseFont(f->f);free(f);} }
int  wkcFontSizeOfFontInstancePeer(void*)    { return sizeof(WKCFont); }
bool wkcFontCanScalePeer(void*)              { return true; }
bool wkcFontCanSupportDrawComplexPeer(void*) { return false; }
bool wkcFontCanTransformPeer(void*)          { return false; }
bool wkcFontIsEqualPeer(void* a,void* b){
    if(!a||!b) return a==b;
    return ((WKCFont*)a)->size==((WKCFont*)b)->size;
}
void wkcFontSetMatrixPeer(void*,float,float,float,float,float,float){}
int  wkcFontGetSizePeer(void* p)     { return p?((WKCFont*)p)->size:0; }
int  wkcFontGetAscentPeer(void* p)   { return (p&&((WKCFont*)p)->f)?TTF_FontAscent(((WKCFont*)p)->f):0; }
int  wkcFontGetDescentPeer(void* p)  { return (p&&((WKCFont*)p)->f)?-TTF_FontDescent(((WKCFont*)p)->f):0; }
int  wkcFontGetLineSpacingPeer(void* p){ return (p&&((WKCFont*)p)->f)?TTF_FontLineSkip(((WKCFont*)p)->f):0; }
int  wkcFontGetLineGapPeer(void* p)  {
    if(!p||!((WKCFont*)p)->f) return 0;
    return TTF_FontLineSkip(((WKCFont*)p)->f)-TTF_FontHeight(((WKCFont*)p)->f);
}
int wkcFontGetXHeightPeer(void* p)        { int w,h=0;if(p&&((WKCFont*)p)->f)TTF_SizeUTF8(((WKCFont*)p)->f,"x",&w,&h);return h; }
int wkcFontGetAverageCharWidthPeer(void* p){ int w=0,h;if(p&&((WKCFont*)p)->f)TTF_SizeUTF8(((WKCFont*)p)->f,"M",&w,&h);return w; }
int wkcFontGetMaxCharWidthPeer(void* p)   { int w=0,h;if(p&&((WKCFont*)p)->f)TTF_SizeUTF8(((WKCFont*)p)->f,"W",&w,&h);return w; }
bool wkcFontIsFixedFontPeer(void*)        { return false; }

int wkcFontGetTextWidthPeer(void* p,int,const unsigned short* str,int len,int* clip){
    if(!p||!str||!((WKCFont*)p)->f) return 0;
    char buf[512]; u16_to_u8(str,len,buf,512);
    int w=0,h; TTF_SizeUTF8(((WKCFont*)p)->f,buf,&w,&h);
    if(clip) *clip=w;
    return w;
}

// Exact signature from wkcgpeer.h line 460
int wkcFontDrawTextPeer(void* in_font, int /*flags*/,
                        const unsigned short* in_str, int in_strlen,
                        void* in_offscreen, const WKCSize* /*in_offscreensize*/,
                        int in_rowbytes, int /*in_fmt*/,
                        const WKCFloatRect* in_textbox, const WKCFloatRect* /*in_clip*/,
                        int /*in_drawingmode*/, unsigned int in_color, unsigned int /*strokecolor*/,
                        int /*strokestyle*/, float /*strokethickness*/, float /*zoomlevel*/)
{
    if (!in_font||!in_str||!in_offscreen||!((WKCFont*)in_font)->f) return 0;
    char buf[512]; u16_to_u8(in_str,in_strlen,buf,512);
    SDL_Color sc={(uint8_t)((in_color>>16)&0xFF),(uint8_t)((in_color>>8)&0xFF),(uint8_t)(in_color&0xFF),0xFF};
    SDL_Surface* surf=TTF_RenderUTF8_Blended(((WKCFont*)in_font)->f,buf,sc);
    if (!surf) return 0;
    int dx=in_textbox?(int)in_textbox->fPoint.fX:0;
    int dy=in_textbox?(int)in_textbox->fPoint.fY:0;
    SDL_LockSurface(surf);
    for (int y=0;y<surf->h;y++) {
        int by=dy+y; if(by<0||by>=s_h) continue;
        auto* sr=(uint8_t*)surf->pixels+y*surf->pitch;
        auto* dr=(uint8_t*)in_offscreen+by*in_rowbytes+dx*4;
        for (int x=0;x<surf->w;x++) {
            auto* s=sr+x*4; auto* d=dr+x*4;
            uint8_t a=s[3];
            d[0]=(uint8_t)(s[0]*a/255+d[0]*(255-a)/255);
            d[1]=(uint8_t)(s[1]*a/255+d[1]*(255-a)/255);
            d[2]=(uint8_t)(s[2]*a/255+d[2]*(255-a)/255);
            d[3]=255;
        }
    }
    SDL_UnlockSurface(surf);
    SDL_FreeSurface(surf);
    return in_strlen;
}

// ── Stock image ───────────────────────────────────────────────────────────────

const unsigned char* wkcStockImageGetBitmapPeer(int)               { return k_white; }
void wkcStockImageGetSizePeer(int,unsigned int* w,unsigned int* h)  { *w=1;*h=1; }
const WKCPoint* wkcStockImageGetLayoutPointsPeer(int)              { return nullptr; }
unsigned int wkcStockImageGetSkinColorPeer(int)                    { return 0xFFFFFFFF; }
float wkcStockImageGetSystemFontSizePeer(int)                      { return 13.f; }
const char* wkcStockImageGetSystemFontFamilyNamePeer(int)          { return "Roboto"; }
void wkcStockImageRegisterSkinPeer(const wkcSkin*)                 {}
const char* wkcStockImageGetDefaultStyleSheetPeer(void)            { return nullptr; }
const char* wkcStockImageGetQuirksStyleSheetPeer(void)             { return nullptr; }
const unsigned char* wkcStockImageGetPlatformResourceImagePeer(const char*,unsigned int* w,unsigned int* h)
{ *w=1;*h=1; return k_white; }

// ── Offscreen ────────────────────────────────────────────────────────────────
// WKCRect: { WKCPoint fPoint{fX,fY}; WKCSize fSize{fWidth,fHeight}; }

void* wkcOffscreenNewPeer(int,void* bmp,int rb,const WKCSize* sz){
    const int w=sz?sz->fWidth:s_w, h=sz?sz->fHeight:s_h;
    if(w<=0||h<=0||w>INT_MAX/4) return nullptr;
    const int stride=rb?rb:w*4;
    if(stride<w*4) return nullptr;
    const size_t bytes=(size_t)stride*(size_t)h;
    if(bytes/(size_t)h!=(size_t)stride) return nullptr;
    auto* o=(WKCOff*)calloc(1,sizeof(WKCOff));
    if(!o) return nullptr;
    o->w=w; o->h=h; o->rb=stride;
    if(bmp){o->bmp=(unsigned char*)bmp;o->owned=false;}
    else{o->bmp=(unsigned char*)malloc(bytes);o->owned=true;if(o->bmp)memset(o->bmp,0xFF,bytes);}
    if(!o->bmp){free(o);return nullptr;}
    return o;
}bool wkcOffscreenIsAcceleratedPeer(void*) { return false; }
void* wkcTextureNewPeer(const WKCSize*)   { return nullptr; }
void wkcTextureSetImagePeer(void*,void*)  {}
void wkcTextureClearImagePeer(void*,void*){}
void wkcOffscreenDeletePeer(void* p)      { if(p){auto*o=(WKCOff*)p;if(o->owned)free(o->bmp);free(o);} }
void wkcTextureDeletePeer(void*)          {}
void wkcOffscreenForceTerminatePeer(void) {}
void wkcOffscreenResizePeer(void* p,const WKCSize* sz){
    if(!p||!sz||sz->fWidth<=0||sz->fHeight<=0||sz->fWidth>INT_MAX/4) return;
    auto* o=(WKCOff*)p;
    const int stride=sz->fWidth*4;
    const size_t bytes=(size_t)stride*(size_t)sz->fHeight;
    if(bytes/(size_t)sz->fHeight!=(size_t)stride) return;
    unsigned char* next=(unsigned char*)malloc(bytes);
    if(!next) return;
    memset(next,0xFF,bytes);
    if(o->owned) free(o->bmp);
    o->bmp=next; o->w=sz->fWidth; o->h=sz->fHeight; o->rb=stride; o->owned=true;
}
void wkcOffscreenGetSizePeer(void* p,WKCSize* sz){
    if(p&&sz){sz->fWidth=((WKCOff*)p)->w;sz->fHeight=((WKCOff*)p)->h;}
}
void wkcOffscreenScrollPeer(void* p,const WKCRect* r,const WKCSize* d){
    // Clip source and translated destination before calculating any copy size.
    if(!p||!r||!d) return;
    auto* o=(WKCOff*)p;
    if(!o->bmp||o->w<=0||o->h<=0||o->rb<o->w*4) return;
    int rx=r->fPoint.fX, ry=r->fPoint.fY;
    int rw=r->fSize.fWidth, rh=r->fSize.fHeight;
    int dx=d->fWidth, dy=d->fHeight;
    if(rw<=0||rh<=0||(dx==0&&dy==0)) return;
    int sx0=rx<0?0:rx, sy0=ry<0?0:ry;
    int sx1=rx+rw, sy1=ry+rh;
    if(sx1>o->w) sx1=o->w;
    if(sy1>o->h) sy1=o->h;
    if(dx<0&&sx0 < -dx) sx0=-dx;
    if(dy<0&&sy0 < -dy) sy0=-dy;
    if(dx>0&&sx1>o->w-dx) sx1=o->w-dx;
    if(dy>0&&sy1>o->h-dy) sy1=o->h-dy;
    if(sx0>=sx1||sy0>=sy1) return;
    size_t bytes=(size_t)(sx1-sx0)*4;
    int y=dy>0?sy1-1:sy0, end=dy>0?sy0-1:sy1, step=dy>0?-1:1;
    for(;y!=end;y+=step){
        unsigned char* src=o->bmp+(size_t)y*o->rb+(size_t)sx0*4;
        unsigned char* dst=o->bmp+(size_t)(y+dy)*o->rb+(size_t)(sx0+dx)*4;
        if(dx>0){ for(size_t x=bytes;x>0;x-=4) memcpy(dst+x-4,src+x-4,4); }
        else if(dx<0){ for(size_t x=0;x<bytes;x+=4) memcpy(dst+x,src+x,4); }
        else memmove(dst,src,bytes);
    }
}
void wkcOffscreenSetOpticalZoomPeer(void*,float,const WKCFloatPoint*){}
void wkcOffscreenSetUseInterpolationForImagePeer(void*,bool){}
void wkcOffscreenSetUseAntiAliasForPolygonPeer(void*,bool)  {}
void* wkcOffscreenBitmapPeer(void* p,int* rb){
    if(!p) return nullptr;
    auto*o=(WKCOff*)p;
    if(rb) *rb=o->rb;
    if(!o->bmp||o->w<=0||o->h<=0||o->rb<o->w*4) return nullptr;
    if(s_pixels&&o->w==s_w&&o->h==s_h){
        // The SDL framebuffer is tightly packed; WKC may supply a padded
        // row stride. Copy only visible pixel bytes to avoid overrunning it.
        const size_t rowBytes=(size_t)o->w*4;
        for(int y=0;y<o->h;y++)
            memcpy(s_pixels+(size_t)y*rowBytes,o->bmp+(size_t)y*o->rb,rowBytes);
        s_dirty=true;
    }
    return o->bmp;
}

// =============================================================================
} // extern "C"
// =============================================================================

bool webkit_engine_init(int w, int h)
{
    if (w <= 0 || h <= 0) return false;
    s_w=w; s_h=h;
    s_pixels=(unsigned char*)malloc((size_t)w*h*4);
    if(!s_pixels) return false;
    memset(s_pixels,0xFF,(size_t)w*h*4);

    if (!WKC::WKCWebKitInitialize(nullptr, 0, nullptr, 0,
                                  s_memory_event_handler, s_timer_event_handler)) {
        free(s_pixels);
        s_pixels = nullptr;
        return false;
    }
    s_webkit_initialized = true;

    s_webview = WKC::WKCWebView::create(s_client_builders);
    if (!s_webview) {
        WKC::WKCWebKitFinalize();
        s_webkit_initialized = false;
        free(s_pixels);
        s_pixels = nullptr;
        return false;
    }

    WKC::WKCSize size = { s_w, s_h };
    if (!s_webview->setOffscreen(WKC::EOffscreenFormatARGB8888,
                                 s_pixels, s_w * 4, size, size, true, &size, true)) {
        WKC::WKCWebView::deleteWKCWebView(s_webview);
        s_webview = nullptr;
        WKC::WKCWebKitFinalize();
        s_webkit_initialized = false;
        free(s_pixels);
        s_pixels = nullptr;
        return false;
    }

    s_available = true;
    s_dirty = true;
    return true;
}

void webkit_engine_set_renderer(SDL_Renderer* ren)
{
    s_renderer=ren;
    if(s_renderer&&!s_texture)
        s_texture=SDL_CreateTexture(s_renderer,SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING,s_w,s_h);
}

void webkit_engine_navigate(const char* url)
{
    if(!url || !s_webview || !s_available) return;
    strncpy(s_url,url,sizeof(s_url)-1);
    s_url[sizeof(s_url)-1] = '\0';
    s_webview->loadURLOrSearch(url);
    s_dirty = true;
}

void webkit_engine_tick(void)
{
    // A WebKit view must be serviced every frame, even before its first timer
    // request. Returning here when the timer queue is empty can prevent the
    // initial navigation, layout and paint pipeline from making progress.
    if (s_timer_init) {
        unsigned int now = now_ms();
        OSLockMutex(&s_timer_mtx);
        for (WKCTimer* t = s_timers; t; t = t->next) {
            if (!t->active || now < t->fire_at)
                continue;

            t->active = false;
            wkcTimeoutProc cb = t->proc;
            void* data = t->data;
            unsigned int interval = t->interval;
            bool wake_up = t->wkc_wakeup;
            if (interval) {
                t->fire_at = now + interval;
                t->active = true;
            }

            // Callbacks may schedule or cancel timers; never invoke them while
            // holding the timer-list mutex.
            OSUnlockMutex(&s_timer_mtx);
            if (wake_up)
                WKC::WKCWebKitWakeUp(data);
            else if (cb)
                cb(data);
            OSLockMutex(&s_timer_mtx);
        }
        OSUnlockMutex(&s_timer_mtx);
    }

    if (s_webview && s_available) {
        WKC::WKCWebKitWakeUp(nullptr);
        s_webview->notifyServiceScriptedAnimations();
        WKC::WKCRect rect = { 0, 0, s_w, s_h };
        s_webview->notifyPaintOffscreen(rect);
        s_dirty = true;
    }
}

void webkit_engine_draw(int cx,int cy,int cw,int ch)
{
    if(!s_renderer||!s_texture||!s_pixels) return;
    if(s_dirty||s_available){
        SDL_UpdateTexture(s_texture,nullptr,s_pixels,s_w*4);
        s_dirty=false;
    }
    SDL_Rect src={0,0,s_w,s_h};
    SDL_Rect dst={cx,cy,cw,ch};
    SDL_RenderCopy(s_renderer,s_texture,&src,&dst);
}

void webkit_engine_input_text(const char* u)
{
    if (!u || !s_webview || !s_available) return;

    // Decode only when the entire UTF-8 sequence is present. Checking p[1]
    // and p[2] directly can read past the NUL terminator on truncated input.
    const unsigned char* p = reinterpret_cast<const unsigned char*>(u);
    size_t remaining = strlen(u);
    while (remaining) {
        unsigned int ch = *p;
        size_t consumed = 1;
        if (ch < 0x80) {
            // ASCII
        } else if (ch >= 0xC2 && ch <= 0xDF && remaining >= 2 &&
                   (p[1] & 0xC0) == 0x80) {
            ch = ((ch & 0x1F) << 6) | (p[1] & 0x3F);
            consumed = 2;
        } else if (ch >= 0xE0 && ch <= 0xEF && remaining >= 3 &&
                   (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
            ch = ((ch & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
            if (ch >= 0x800 && !(ch >= 0xD800 && ch <= 0xDFFF))
                consumed = 3;
            else
                ch = 0xFFFD;
        } else if (ch >= 0xF0 && ch <= 0xF4 && remaining >= 4 &&
                   (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 &&
                   (p[3] & 0xC0) == 0x80) {
            ch = ((ch & 0x07) << 18) | ((p[1] & 0x3F) << 12) |
                 ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
            if (ch >= 0x10000 && ch <= 0x10FFFF)
                consumed = 4;
            else
                ch = 0xFFFD;
        } else {
            // Malformed or truncated input: replace one byte and keep going.
            ch = 0xFFFD;
        }
        s_webview->notifyKeyChar(ch);
        p += consumed;
        remaining -= consumed;
    }
}
void webkit_engine_scroll(int x,int y)
{
    if (s_webview && s_available) s_webview->notifyScroll(x,y);
}
void webkit_engine_touch_down(int x,int y)
{
    if (!s_webview || !s_available) return;
    WKC::WKCPoint p = { x, y };
    s_webview->notifyMouseDown(p, WKC::EMouseButtonLeft, WKC::EModifierNone);
}
void webkit_engine_touch_up(int x,int y)
{
    if (!s_webview || !s_available) return;
    WKC::WKCPoint p = { x, y };
    s_webview->notifyMouseUp(p, WKC::EMouseButtonLeft, WKC::EModifierNone);
}

bool webkit_engine_available(void){ return s_available; }

void webkit_engine_shutdown(void)
{
    if(s_texture){SDL_DestroyTexture(s_texture);s_texture=nullptr;}
    if (s_webview) {
        WKC::WKCWebView::deleteWKCWebView(s_webview);
        s_webview = nullptr;
    }
    if (s_webkit_initialized) {
        WKC::WKCWebKitFinalize();
        s_webkit_initialized = false;
    }
    s_available=false;
    if (s_timer_init) {
        OSLockMutex(&s_timer_mtx);
        WKCTimer* t = s_timers;
        s_timers = nullptr;
        while (t) { WKCTimer* next=t->next; free(t); t=next; }
        OSUnlockMutex(&s_timer_mtx);
    }
    free(s_pixels);s_pixels=nullptr;
}

void webkit_engine_key_down(int keycode)
{
    if (!s_webview || !s_available) return;

    // The public Wave API specifies browser/virtual-key codes, matching WKC::Key
    // values (for example Backspace=0x08, Enter=0x0D, arrows=0x25..0x28).
    // Do not pass raw SDL scancode-masked key symbols through this interface.
    if (keycode < 0 || keycode > 0xFF) return;
    s_webview->notifyKeyPress(static_cast<WKC::Key>(keycode),
                              WKC::EModifierNone, false);
}

void webkit_engine_key_up(int keycode)
{
    if (!s_webview || !s_available) return;
    if (keycode < 0 || keycode > 0xFF) return;
    s_webview->notifyKeyRelease(static_cast<WKC::Key>(keycode),
                                WKC::EModifierNone);
}

#else

// Stub implementation used when the optional WebKit-WiiU/WKC library is not
// present. This keeps the main Wave Browser build working without WKC headers
// or a prebuilt static WebKit archive.

bool webkit_engine_init(int, int) { return true; }
void webkit_engine_set_renderer(SDL_Renderer*) {}
void webkit_engine_navigate(const char*) {}
void webkit_engine_tick(void) {}
void webkit_engine_draw(int, int, int, int) {}
void webkit_engine_input_text(const char*) {}
void webkit_engine_scroll(int, int) {}
void webkit_engine_touch_down(int, int) {}
void webkit_engine_touch_up(int, int) {}
bool webkit_engine_available(void) { return false; }
void webkit_engine_shutdown(void) {}
void webkit_engine_key_down(int) {}
void webkit_engine_key_up(int) {}

#endif
