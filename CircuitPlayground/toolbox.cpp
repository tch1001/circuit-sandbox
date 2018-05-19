#include <stdexcept>
#include <type_traits>
#include <string>
#include <cstring>

#include <SDL.h>
#include <SDL_ttf.h>

#include "toolbox.hpp"
#include "mainwindow.hpp"
#include "tag_tuple.hpp"


using namespace std::literals::string_literals; // gives the 's' suffix for strings


Toolbox::Toolbox(MainWindow& main_window) : mainWindow(main_window) {};


void Toolbox::updateDpi() {
    BUTTON_HEIGHT = mainWindow.logicalToPhysicalSize(LOGICAL_BUTTON_HEIGHT);
    PADDING_HORIZONTAL = mainWindow.logicalToPhysicalSize(LOGICAL_PADDING_HORIZONTAL);
    PADDING_VERTICAL = mainWindow.logicalToPhysicalSize(LOGICAL_PADDING_VERTICAL);
}


void Toolbox::render(SDL_Renderer* renderer) const {
    // draw a grey border around the toolbox
    SDL_SetRenderDrawColor(renderer, 0x66, 0x66, 0x66, 0xFF);
    SDL_RenderDrawRect(renderer, &renderArea);

    // load the font (TODO: this should be made so that we don't load the font at every render call)
    TTF_Font* button_font;
    {
        char* cwd = SDL_GetBasePath();
        if (cwd == nullptr) {
            throw std::runtime_error("SDL_GetBasePath() failed:  "s + SDL_GetError());
        }
        const char* font_name = "OpenSans-Bold.ttf";
        char* font_path = new char[std::strlen(cwd) + std::strlen(font_name) + 1];
        std::strcpy(font_path, cwd);
        std::strcat(font_path, font_name);
        SDL_free(cwd);
        button_font = TTF_OpenFont(font_path, mainWindow.logicalToPhysicalSize(12));
        delete[] font_path;
    }
    if (button_font == nullptr) {
        throw std::runtime_error("TTF_OpenFont() failed:  "s + TTF_GetError());
    }


    // draw the buttons to the screen one-by-one
    MainWindow::element_tags::for_each([this, renderer, button_font](const auto element_tag, const auto index_tag) {
        // 'Element' is the type of element (e.g. ConductiveWire)
        using Element = typename decltype(element_tag)::type;
        // 'index' is the index of this element inside the element_tags
        constexpr size_t index = decltype(index_tag)::value;

        SDL_Color backgroundColorForText = MainWindow::backgroundColor;

        // Make a grey rectangle if the element is being moused over
        if (mouseoverElementIndex == index) { // <-- test that current index is the index being mouseovered
            backgroundColorForText = SDL_Color{0x44, 0x44, 0x44, 0xFF};
            SDL_SetRenderDrawColor(renderer, backgroundColorForText.r, backgroundColorForText.g, backgroundColorForText.b, backgroundColorForText.a);
            const SDL_Rect destRect{renderArea.x + PADDING_HORIZONTAL, renderArea.y + PADDING_VERTICAL + BUTTON_HEIGHT * static_cast<int>(index), renderArea.w - 2 * PADDING_HORIZONTAL, BUTTON_HEIGHT};
            SDL_RenderFillRect(renderer, &destRect);
        }

        // Render the text
        {
            SDL_Surface* surface = TTF_RenderText_Shaded(button_font, Element::displayName, Element::displayColor, backgroundColorForText);
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
            int textureWidth, textureHeight;
            SDL_QueryTexture(texture, nullptr, nullptr, &textureWidth, &textureHeight);
            // the complicated calculation here makes the text vertically-centered and horizontally displaced by 2 logical pixels
            const SDL_Rect destRect{renderArea.x + PADDING_HORIZONTAL + mainWindow.logicalToPhysicalSize(2), renderArea.y + PADDING_VERTICAL + BUTTON_HEIGHT * static_cast<int>(index) + (BUTTON_HEIGHT - textureHeight) / 2, textureWidth, textureHeight};
            SDL_RenderCopy(renderer, texture, nullptr, &destRect);
            SDL_DestroyTexture(texture);
        }
    });

    // free the font so we don't leak memory
    TTF_CloseFont(button_font);
}


void Toolbox::processMouseMotionEvent(const SDL_MouseMotionEvent& event) {
    // offset relative to top-left of toolbox (in physical size; both event and renderArea are in physical size units)
    int offsetX = event.x - renderArea.x;
    int offsetY = event.y - renderArea.y;

    // reset the mouseover context
    mouseoverElementIndex = MainWindow::EMPTY_INDEX;

    // check left/right out of bounds
    if(offsetX < PADDING_HORIZONTAL || offsetX >= renderArea.w - PADDING_HORIZONTAL) return;

    // element index
    size_t index = static_cast<size_t>((offsetY - PADDING_VERTICAL) / BUTTON_HEIGHT);
    if (index >= MainWindow::element_tags::size) return;

    // save the index since it is valid
    mouseoverElementIndex = index;
}


void Toolbox::processMouseButtonDownEvent(const SDL_MouseButtonEvent& event) {
    // offset relative to top-left of toolbox (in physical size; both event and renderArea are in physical size units)
    int offsetX = event.x - renderArea.x;
    int offsetY = event.y - renderArea.y;

    // check left/right out of bounds
    if(offsetX < PADDING_HORIZONTAL || offsetX >= renderArea.w - PADDING_HORIZONTAL) return;

    // element index
    size_t index = static_cast<size_t>((offsetY - PADDING_VERTICAL) / BUTTON_HEIGHT);
    if (index >= MainWindow::element_tags::size) return;

    // save the index since it is valid
    mainWindow.selectedElementIndex = index;

    // make a message box pop up
    MainWindow::element_tags::get(index, [this](const auto element_tag) {
        // 'Element' is the type of element (e.g. ConductiveWire)
        using Element = typename decltype(element_tag)::type;

        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Button clicked", Element::displayName, mainWindow.window);
    });
}


void Toolbox::processMouseLeave() {
    mouseoverElementIndex = MainWindow::EMPTY_INDEX;
}
