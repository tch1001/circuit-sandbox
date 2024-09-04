/*
 * Circuit Sandbox
 * Copyright 2018 National University of Singapore <enterprise@nus.edu.sg>
 *
 * This file is part of Circuit Sandbox.
 * Circuit Sandbox is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 * Circuit Sandbox is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with Circuit Sandbox.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdexcept>
#include <cstring>
#include <numeric>
#include <algorithm>

#include <SDL.h>
#include <SDL_ttf.h>

#include <iostream>

#include "reverse_adaptor.hpp"
#include "mainwindow.hpp"
#include "fileutils.hpp"
#include "selectionaction.hpp"
#include "filenewaction.hpp"
#include "fileopenaction.hpp"
#include "filesaveaction.hpp"
#include "historyaction.hpp"
#include "eyedropperaction.hpp"
#include "changesimulationspeedaction.hpp"
#include "clipboardaction.hpp"
#include "launch_browser.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <SDL_syswm.h>
#elif defined(__APPLE__)
#include <dispatch/dispatch.h>
#elif defined(__linux)
#include "x11_dpi_scaling.hpp"
#endif


using namespace std::literals::string_literals; // gives the 's' suffix for strings

#ifdef _WIN32
#define SIZE_MOVE_TIMER_ID 1
// hack for issue with window resizing on Windows not giving live events
int resizeEventForwarder(void* main_window_void_ptr, SDL_Event* event) {
    MainWindow* const main_window = static_cast<MainWindow*>(main_window_void_ptr);
    if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_RESIZED) {
        SDL_Window* event_window = SDL_GetWindowFromID(event->window.windowID);
        if (event_window == main_window->window) {
            // only do stuff if we are called from the main thread
            if (main_window->mainThreadId == GetCurrentThreadId()) {
                main_window->layoutComponents();
                main_window->render();
            }
        }
    }
    else if (event->type == SDL_SYSWMEVENT) {
        const auto& winMessage = event->syswm.msg->msg.win;
        if (winMessage.msg == WM_ENTERSIZEMOVE) {
            main_window->_sizeMoveTimerRunning = SetTimer(GetActiveWindow(), SIZE_MOVE_TIMER_ID, USER_TIMER_MINIMUM, nullptr);
        }
        else if (winMessage.msg == WM_TIMER) {
            if (winMessage.wParam == SIZE_MOVE_TIMER_ID) {
                // only do stuff if we are called from the main thread
                if (main_window->mainThreadId == GetCurrentThreadId()) {
                    main_window->render();
                }
            }
        }
    }
    return 1;
}
#endif // _WIN32

#ifdef __APPLE__
// hack for issue with window resizing on MacOS not giving live events
// https://stackoverflow.com/questions/34967628/sdl2-window-turns-black-on-resize

void dispatchRenderHandler(void* main_window_void_ptr) {
    MainWindow* const main_window = static_cast<MainWindow*>(main_window_void_ptr);
    if (main_window->_mainQueueDispatchRunning.load(std::memory_order_relaxed)) {
        main_window->render();
        dispatch_after_f(dispatch_time(DISPATCH_TIME_NOW, 2000000), dispatch_get_main_queue(), main_window_void_ptr, &dispatchRenderHandler);
    }
}

void dispatchLayoutHandler(void* main_window_void_ptr) {
    MainWindow* const main_window = static_cast<MainWindow*>(main_window_void_ptr);
    if (main_window->_mainQueueDispatchRunning.load(std::memory_order_relaxed)) {
        main_window->layoutComponents();
        main_window->render();
    }
}

int resizeEventForwarder(void* main_window_void_ptr, SDL_Event* event) {
    MainWindow* const main_window = static_cast<MainWindow*>(main_window_void_ptr);
    if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_RESIZED) {
        SDL_Window* event_window = SDL_GetWindowFromID(event->window.windowID);
        if (event_window == main_window->window) {
            // This is dangerous if SDL_PollEvent() can return while we are doing layout/rendering.
            // But it seems to work well in practice.
            dispatch_async_f(dispatch_get_main_queue(), main_window_void_ptr, &dispatchLayoutHandler);
            bool tmp = false;
            if (main_window->_mainQueueDispatchRunning.compare_exchange_strong(tmp, true)) {
                dispatch_async_f(dispatch_get_main_queue(), main_window_void_ptr, &dispatchRenderHandler);
            }
        }
    }
    return 1;
}
#endif // __APPLE__


MainWindow::MainWindow(const char* const processName) : stateManager(getSimulatorPeriodFromFPS(std::stold(displayedSimulationFPS))), closing(false), toolbox(*this), playArea(*this), buttonBar(*this, playArea), notificationDisplay(*this), currentEventTarget(nullptr), currentLocationTarget(nullptr), currentAction(*this), clipboard(notificationDisplay), interfaceFont("OpenSans-Bold.ttf", 12), processName(processName) {

    // unset all the input handle selection state
    std::fill_n(selectedToolIndices, NUM_INPUT_HANDLES, EMPTY_INDEX);

    // update dpi once first, so we can use it to create the properly sized window
    updateDpiFields(false);

    window = SDL_CreateWindow(WINDOW_TITLE_STRING, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, logicalToPhysicalSize(960), logicalToPhysicalSize(720), SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (window == nullptr) {
        throw std::runtime_error("SDL_CreateWindow() failed:  "s + SDL_GetError());
    }

    // Create the renderer.  SDL_RENDERER_PRESENTVSYNC turns on the monitor refresh rate synchronization.
    // For some reason my SDL2 doesn't have any software renderer, so I can't do "SDL_RENDERER_SOFTWARE | SDL_RENDERER_ACCELERATED"
    // So we try creating hardware renderer, and if it doesn't work then we try creating software renderer.
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_PRESENTVSYNC);
    }
    if (renderer == nullptr) {
        throw std::runtime_error("SDL_CreateRenderer() failed:  "s + SDL_GetError());
    }

    // if not already the default, set the blend mode to none
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    // update dpi again (in case the window was opened on something other than the default monitor)
    if (updateDpiFields()) {
        // resize the window, if the dpi changed
        SDL_SetWindowSize(window, logicalToPhysicalSize(960), logicalToPhysicalSize(720));
    }

    // Clear the window with a black background
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    // Show the window to the user
    SDL_ShowWindow(window);

    // do the layout (must be done after showing the window, otherwise pre-rendering done here won't work properly)
    layoutComponents(true);

    // render once first, because in case file loading takes long we don't want users to stare at black/white screen
    render();
    
#if defined(_WIN32)
    // save the current thread id on Windows, so the event watch can check it
    mainThreadId = GetCurrentThreadId();
#endif
}


MainWindow::~MainWindow() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}


void MainWindow::updateFonts() {
    interfaceFont.updateDPI(*this);
}


bool MainWindow::updateDpiFields(bool useWindow) {

    int display_index = 0;

    if (useWindow) {
        display_index = SDL_GetWindowDisplayIndex(window);
        if (display_index < 0) { // means that SDL_GetWindowDisplayIndex doesn't work, then we just use the default monitor
            display_index = 0;
        }
    }


    float dpi_float;
    // well expect horizontal and vertical dpis to be the same
    // this function might fail to get dpi on some setups
    int dpi;
// #if defined(__linux__)
//     if (DpiScaling::getDpi(useWindow ? window : nullptr, display_index, dpi_float)) {
//         // success
//         dpi = static_cast<int>(dpi_float + 0.5f); // round to nearest int
//     }
// #else
    if (SDL_GetDisplayDPI(display_index, nullptr, &dpi_float, nullptr) == 0) {
        // success
        dpi = static_cast<int>(dpi_float + 0.5f); // round to nearest int
    }
// #endif
    else {
        dpi = 96;
    }
    
    int default_dpi;
#if defined(__APPLE__)
    default_dpi = 96; // for some reason apple is 96 dpi
#elif defined(__linux__)
    default_dpi = 96;
#elif defined(_WIN32)

#if defined(USER_DEFAULT_SCREEN_DPI) // If we have the default DPI macro, use it
    default_dpi = USER_DEFAULT_SCREEN_DPI; // The Windows default DPI.
#else
    default_dpi = 96;
#endif // USER_DEFAULT_SCREEN_DPI

#else
    default_dpi = 96; // We don't know what OS we are on :(
#endif

    // use gcd, so the multipliers don't become too big
    int gcd = std::gcd(dpi, default_dpi);

    int tmp_physicalMultiplier = physicalMultiplier;
    int tmp_logicalMultiplier = logicalMultiplier;
    // update the fields
    physicalMultiplier = dpi / gcd;
    logicalMultiplier = default_dpi / gcd;

    // check if the fields changed
    bool fields_changed = tmp_physicalMultiplier != physicalMultiplier || tmp_logicalMultiplier != logicalMultiplier;

    if (fields_changed) {

        // remember to update my own pseudo-constants
        TOOLBOX_WIDTH = logicalToPhysicalSize(LOGICAL_TOOLBOX_WIDTH);
        BUTTONBAR_HEIGHT = logicalToPhysicalSize(LOGICAL_BUTTONBAR_HEIGHT);
    }

    // return true if the fields changed
    return fields_changed;
}



void MainWindow::updateTitleBar() {
    std::string title = ((unsaved) ? "* " : "") + (filePath.empty() ? "" : getFileName(filePath.c_str()) + " "s);
    if (title.empty()) title = WINDOW_TITLE_STRING;
    else {
        title += u8"\u2013 " WINDOW_TITLE_STRING;
    }
    SDL_SetWindowTitle(window, title.c_str());
}

void MainWindow::toggleBeginnerMode() {
    // get existing flags
    NotificationFlags::Type flags = notificationDisplay.getVisibleFlags();
    // toggle the BEGINNER flag
    notificationDisplay.setVisibleFlags(flags ^ NotificationFlags::BEGINNER);
    // write notifications
    if (flags & NotificationFlags::BEGINNER) {
        // beginner mode was disabled
        toggleBeginnerModeNotification = notificationDisplay.uniqueAdd(NotificationFlags::DEFAULT, 5s, NotificationDisplay::Data{ { "Beginner mode disabled", NotificationDisplay::TEXT_COLOR_CANCEL } });
    }
    else {
        // beginner mode was enabled
        toggleBeginnerModeNotification = notificationDisplay.uniqueAdd(NotificationFlags::DEFAULT, 5s, NotificationDisplay::Data{ { "Beginner mode enabled", NotificationDisplay::TEXT_COLOR_ACTION } });
    }
}


void MainWindow::layoutComponents(bool forceLayout) {

    // update the two dpi member fields
    bool dpiChanged = updateDpiFields();

    // get the size of the render target (this is a physical size)
    int32_t newW, newH;
    SDL_GetRendererOutputSize(renderer, &newW, &newH);
    bool sizeChanged = newW != renderArea.w || newH != renderArea.h;
    if (sizeChanged || forceLayout) {
        renderArea.x = renderArea.y = 0;
        renderArea.w = newW;
        renderArea.h = newH;
    }

    // position all the components:
    playArea.renderArea = SDL_Rect{0, 0, renderArea.w - TOOLBOX_WIDTH - HAIRLINE_WIDTH, renderArea.h - BUTTONBAR_HEIGHT - HAIRLINE_WIDTH};
    toolbox.renderArea = SDL_Rect{ renderArea.w - TOOLBOX_WIDTH, 0, TOOLBOX_WIDTH, renderArea.h - BUTTONBAR_HEIGHT - HAIRLINE_WIDTH};
    buttonBar.renderArea = SDL_Rect{0, renderArea.h - BUTTONBAR_HEIGHT, renderArea.w, BUTTONBAR_HEIGHT};
    notificationDisplay.renderArea = playArea.renderArea;


    if (dpiChanged || forceLayout) {
        // set min window size
        SDL_SetWindowMinimumSize(window, logicalToPhysicalSize(480), logicalToPhysicalSize(480));

        // update font sizes
        updateFonts();

    }

    // one-time initialization of renderer
    if (forceLayout) {
        playArea.initScale();
        clipboard.setRenderer(renderer);
    }

    if (dpiChanged || sizeChanged || forceLayout) {
        // tell children about the updated layout
        for (Drawable* drawable : drawables) {
            drawable->layoutComponents(renderer);
        }
    }
}


void MainWindow::start() {
    startEventLoop();
}

void MainWindow::start(const char* filePath) {
    loadFile(filePath);
    startEventLoop();
}

void MainWindow::startEventLoop() {

#if defined(_WIN32) || defined(__APPLE__)
    // On Windows and Mac, when the user is resizing the window, we don't get any events until the resize is complete.
    // This tries to fix this
    // Note that this has to come *after* layoutComponents(true), so that all layout-specific stuff (e.g. fonts) has been initialized.
    // Also, we have to remember to delete the event watch before closing the window as closing the window will resize it (by disengaging fullscreen mode).
    struct EventWatchGuard {
        MainWindow& mainWindow;
        EventWatchGuard(MainWindow& mainWindow) : mainWindow(mainWindow) {
            SDL_AddEventWatch(resizeEventForwarder, static_cast<void*>(&mainWindow));
        }
        ~EventWatchGuard() {
            SDL_DelEventWatch(resizeEventForwarder, static_cast<void*>(&mainWindow));
        }
    } eventWatchGuard(*this);
#endif
    
#ifdef _WIN32
    // on Windows we also need special handling of some windows messages
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
#endif // _WIN32

    // event/drawing loop:
    while (true) {

        // get the next event to process, if any
        while (true) {
            SDL_Event event;
            if (visible ? SDL_PollEvent(&event) : SDL_WaitEvent(&event)) {
#if defined(_WIN32)
                if (_sizeMoveTimerRunning) {
                    KillTimer(GetActiveWindow(), SIZE_MOVE_TIMER_ID);
                    _sizeMoveTimerRunning = false;
                }
                if (event.type == SDL_SYSWMEVENT) {
                    const auto& winMessage = event.syswm.msg->msg.win;
                    if (winMessage.msg == WM_LBUTTONDOWN) {
                        if (_suppressMouseUntilNextDown) {
                            _suppressMouseUntilNextDown = false;
                        }
                    }
#if defined(WM_DPICHANGED) // If we have the WM_DPICHANGED message, look out for it
                    else if (winMessage.msg == WM_DPICHANGED) {
                        layoutComponents(); // for safety, in case the window size didn't change, then we won't get SDL_WINDOWEVENT_RESIZED
                    }
#endif
                }
                else {
                    processEvent(event);
                }
#elif defined(__APPLE__)
                _mainQueueDispatchRunning.store(false, std::memory_order_release);
                processEvent(event);
#else
                processEvent(event);
#endif
                if (closing) return;
            }
            else break;
        }

        // draw everything onto the screen
        render();
    }
}


void MainWindow::processEvent(const SDL_Event& event) {
    switch (event.type) {
    case SDL_WINDOWEVENT:
        processWindowEvent(event.window);
        break;
    case SDL_MOUSEMOTION:
        processMouseMotionEvent(event.motion);
        break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        processMouseButtonEvent(event.button);
        break;
    case SDL_MOUSEWHEEL:
        processMouseWheelEvent(event.wheel);
        break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        processKeyboardEvent(event.key);
        break;
    case SDL_TEXTINPUT:
        processTextInputEvent(event.text);
        break;
    }
    registrar.update(*this);
}


void MainWindow::processWindowEvent(const SDL_WindowEvent& event) {
    switch (event.event) {
    case SDL_WINDOWEVENT_CLOSE: // close button was pressed (or some other close command like Alt-F4)
        closing = true;
        break;
    case SDL_WINDOWEVENT_RESIZED: // window got resized by window manager or by user (will NOT be triggered by programmatic resize, e.g. SDL_SetWindowSize)
        layoutComponents();
        break;
    case SDL_WINDOWEVENT_ENTER:
        {
            int32_t x, y;
            auto state = SDL_GetMouseState(&x, &y);
            processMouseMotionEvent({
                SDL_MOUSEMOTION, /* type */
                event.timestamp, /* timestamp */
                event.windowID, /* windowID */
                0, /* which (we assume only one mouse) */
                state, /* state */
                x, /* x */
                y, /* y */
                0, /* xrel */
                0 /* yrel */
                });
        }
        break;
    case SDL_WINDOWEVENT_LEAVE:
        if (currentLocationTarget != nullptr) {
            currentLocationTarget->processMouseLeave();
            currentLocationTarget = nullptr;
        }
        break;
    case SDL_WINDOWEVENT_MINIMIZED:
        visible = false;
        break;
    case SDL_WINDOWEVENT_MAXIMIZED:
        // this is to fix a Windows issue where double click to maximize will trigger a mousedown.
        suppressMouseUntilNextDown();
        visible = true;
        break;
    case SDL_WINDOWEVENT_RESTORED:
        visible = true;
        break;
    }
}


void MainWindow::processMouseMotionEvent(const SDL_MouseMotionEvent& event) {

    // for processMouseDrag()
    if (currentEventTarget != nullptr) {
        currentEventTarget->processMouseDrag(event);
    }

    // check if the cursor left currentLocationTarget
    SDL_Point position{ event.x, event.y };
    if (currentLocationTarget != nullptr && !SDL_PointInRect(&position, &currentLocationTarget->renderArea)) {
        currentLocationTarget->processMouseLeave();
        currentLocationTarget = nullptr;
    }

    // determine new currentLocationTarget
    for (Control* control : ext::reverse(controls)) {
        if (control == currentLocationTarget) {
            break;
        }
        if (SDL_PointInRect(&position, &control->renderArea)) {
            // found a new Control that isn't currently being hovered
            if (currentLocationTarget != nullptr) {
                currentLocationTarget->processMouseLeave();
                currentLocationTarget = nullptr;
            }
            currentLocationTarget = control;
            break;
        }
    }

    // for processMouseHover()
    if (currentEventTarget == nullptr) {
        if (currentLocationTarget != nullptr) {
            currentLocationTarget->processMouseHover(event);
        }
    }
    else {
        if (SDL_PointInRect(&position, &currentEventTarget->renderArea)) {
            currentEventTarget->processMouseHover(event);
        }
    }

}


void MainWindow::processMouseButtonEvent(const SDL_MouseButtonEvent& event) {
#if defined(_WIN32)
    if (_suppressMouseUntilNextDown) return;
#endif

    SDL_Point position{event.x, event.y};
    size_t inputHandleIndex = resolveInputHandleIndex(event);

    if (event.type == SDL_MOUSEBUTTONDOWN) {
        // ensure only one input handle can be down at any moment
        stopMouseDrag();
        activeInputHandleIndex = inputHandleIndex;
        for (Control* control : ext::reverse(controls)) {
            if (SDL_PointInRect(&position, &control->renderArea)){
                currentEventTarget = control;
                if (control->processMouseButtonDown(event)) {
                    SDL_CaptureMouse(SDL_TRUE);
                    break;
                }
                else {
                    currentEventTarget = nullptr;
                }
            }
        }
    }
    else {
        if (activeInputHandleIndex == inputHandleIndex) {
            stopMouseDrag();
        }
    }
}

void MainWindow::stopMouseDrag() {
    if (currentEventTarget != nullptr) {
        SDL_CaptureMouse(SDL_FALSE);
        auto drawable = currentEventTarget;
        currentEventTarget = nullptr;
        drawable->processMouseButtonUp();
    }
}

void MainWindow::processMouseWheelEvent(const SDL_MouseWheelEvent& event) {
    SDL_Point position;
    // poll the mouse position since it's not reflected in the event
    SDL_GetMouseState(&position.x, &position.y);
    for (Control* control : ext::reverse(controls)) {
        if (SDL_PointInRect(&position, &control->renderArea) && control->processMouseWheel(event)) {
            break;
        }
    }
}


void MainWindow::processKeyboardEvent(const SDL_KeyboardEvent& event) {
    // invoke the handlers for all controls and see if they want to stop event propagation
    for (KeyboardEventReceiver* receiver : ext::reverse(keyboardEventReceivers)) {
        if (receiver->processKeyboard(event)) {
            return;
        }
    }

    if (event.type == SDL_KEYDOWN) {
        SDL_Keymod modifiers = static_cast<SDL_Keymod>(event.keysym.mod);
        if (modifiers & KMOD_CTRL) {
            // In alphabetical order
            if (!event.repeat) {
                switch (event.keysym.scancode) {
                case SDL_SCANCODE_A: // Select all
                    SelectionAction::startBySelectingAll(*this, currentAction.getStarter());
                    return;
                case SDL_SCANCODE_N: // Spawn new instance
                    FileNewAction::start(*this, currentAction.getStarter());
                    return;
                case SDL_SCANCODE_O: // Open file
                    FileOpenAction::start(*this, playArea, currentAction.getStarter());
                    return;
                case SDL_SCANCODE_S: // Save file
                    FileSaveAction::start(*this, modifiers, currentAction.getStarter());
                    return;
                case SDL_SCANCODE_Y: // Redo
                    HistoryAction::startByRedoing(*this, currentAction.getStarter());
                    return;
                case SDL_SCANCODE_Z: // Undo
                    HistoryAction::startByUndoing(*this, currentAction.getStarter());
                    return;
                case SDL_SCANCODE_SPACE: // Change simulation speed
                    ChangeSimulationSpeedAction::start(*this, renderer, currentAction.getStarter());
                    return;
                default:
                    break;
                }
            }
            switch (event.keysym.scancode) {
            case SDL_SCANCODE_V: // Paste
                if (modifiers & KMOD_SHIFT) {
                    ClipboardAction::startPasteDialog(*this, renderer, currentAction.getStarter());
                }
                else {
                    SelectionAction::startByPasting(*this, playArea, currentAction.getStarter());
                }
                return;
            default:
                break;
            }
        }
        else {
            if (!event.repeat) {
                // things that should only work if its a non-repeat keydown
                switch (event.keysym.scancode) { // using the scancode layout so that keys will be in the same position if the user has a non-qwerty keyboard
                case SDL_SCANCODE_R: // Reset simulator
                    currentAction.reset();
                    stateManager.resetSimulator(*this);
                    return;
                case SDL_SCANCODE_Z:
                    if (modifiers & KMOD_SHIFT) {
                        playArea.saveZoom();
                    }
                    else {
                        playArea.toggleZoom();
                    }
                    return;
                case SDL_SCANCODE_SPACE: // Start/stop simulator
                    currentAction.reset();
                    stateManager.startOrStopSimulator(*this);
                    return;
                case SDL_SCANCODE_E:
                    EyedropperAction::start(*this, renderer, currentAction.getStarter());
                    return;
                case SDL_SCANCODE_B:
                    toggleBeginnerMode();
                    return;
                case SDL_SCANCODE_F1: [[fallthrough]];
                case SDL_SCANCODE_HELP:
                    // if (WebResource::launch(WebResource::USER_MANUAL)) {
                    //     notificationDisplay.add(NotificationFlags::DEFAULT, 5s, "Opening user manual in browser ...");
                    // }
                    return;
                default:
                    break;
                }
            }
            switch (event.keysym.scancode) { // using the scancode layout so that keys will be in the same position if the user has a non-qwerty keyboard
            case SDL_SCANCODE_RIGHT: // Step simulator
                currentAction.reset();
                stateManager.stepSimulator();
                return;
            default:
                break;
            }
        }
    }
}

void MainWindow::processTextInputEvent(const SDL_TextInputEvent& event) {
    // invoke the handlers for all controls and see if they want to stop event propagation
    for (KeyboardEventReceiver* receiver : ext::reverse(keyboardEventReceivers)) {
        if (receiver->processTextInput(event)) {
            return;
        }
    }
}


// TODO: needs some way to use the old data when resizing, for consistency?
void MainWindow::render() {
    Drawable::renderTime = Drawable::RenderClock::now();

    // Clear the window with a black background
    SDL_SetRenderDrawColor(renderer, backgroundColor.r, backgroundColor.g, backgroundColor.b, 255);
    SDL_RenderClear(renderer);

    // draw the separators
    SDL_SetRenderDrawColor(renderer, DARK_GREY.r, DARK_GREY.g, DARK_GREY.b, DARK_GREY.a);
    SDL_Rect toolbarRect{ toolbox.renderArea.x - 1, 0, toolbox.renderArea.w, toolbox.renderArea.h };
    SDL_Rect buttonBarRect{ 0, buttonBar.renderArea.y - 1, buttonBar.renderArea.w, buttonBar.renderArea.h };
    SDL_RenderFillRect(renderer, &toolbarRect);
    SDL_RenderFillRect(renderer, &buttonBarRect);

    // draw everything to the screen - buttons, status info, play area, etc.
    for (Drawable* drawable : drawables) {
        // set clip rect to clip off parts of the surface outside renderArea
        SDL_RenderSetClipRect(renderer, &drawable->renderArea);
        // render the stuff
        drawable->render(renderer);
    }
    // reset the clip rect
    SDL_RenderSetClipRect(renderer, nullptr);

    // Then display to the user
    SDL_RenderPresent(renderer);
}


void MainWindow::loadFile(const char* filePath) {
    FileOpenAction::start(*this, playArea, currentAction.getStarter(), filePath);
}

void MainWindow::setUnsaved(bool unsaved) {
    if (this->unsaved != unsaved) {
        this->unsaved = unsaved;
        updateTitleBar();
    }
}


void MainWindow::setFilePath(const char* filePath) {
    if (this->filePath != filePath) {
        this->filePath = filePath;
        updateTitleBar();
    }
}


const char* MainWindow::getFilePath() const {
    return filePath.empty() ? nullptr : filePath.c_str();
}


bool MainWindow::hasFilePath() const {
    return !filePath.empty();
}

void MainWindow::bindTool(size_t inputHandleIndex, size_t tool_index) {
    auto it = std::find(selectedToolIndices, selectedToolIndices + NUM_INPUT_HANDLES, tool_index);
    if (it == selectedToolIndices + NUM_INPUT_HANDLES) {
        // cannot find an existing handle
        selectedToolIndices[inputHandleIndex] = tool_index;
    }
    else {
        // can find an existing handle
        using std::swap;
        swap(selectedToolIndices[inputHandleIndex], *it);
    }
}

// throws std::logic_error or its derived classes
Simulator::period_t MainWindow::getSimulatorPeriodFromFPS(long double fps) {
    // try to parse and save the fps of the simulator
    using rep = Simulator::period_t::rep;
    rep period;
    if (fps == 0.0) {
        period = 0;
    }
    else if (fps < 0) {
        throw std::logic_error("The FPS you have entered cannot be less than zero.");
    }
    else {
        long double _period = static_cast<long double>(Simulator::period_t::period::den / Simulator::period_t::period::num) / fps;
        if (_period == 0.0) {
            throw std::logic_error("The FPS you have entered is too large.");
        }
        if (_period == std::numeric_limits<long double>::infinity() || !(_period < std::numeric_limits<rep>::max())) {
            throw std::logic_error("The FPS you have entered is too small.");
        }
        period = static_cast<rep>(_period);
        if (period <= 0) throw std::logic_error("The FPS you have entered is too large.");
    }
    return Simulator::period_t(period);
}
