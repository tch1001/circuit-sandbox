#include <exception>
#include <stdexcept>
#include <iostream>

#include <SDL.h>
#include <SDL_ttf.h>

#include "mainwindow.hpp"


using namespace std::literals::string_literals; // gives the 's' suffix for strings

// guard object used to initialize and de-initialize program-wide stuff
struct InitGuard {
    InitGuard() {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            throw std::runtime_error("SDL_Init() failed:  "s + SDL_GetError());
        }

        if (TTF_Init() != 0) {
            throw std::runtime_error("TTF_Init() failed:  "s + TTF_GetError());
        }
    }
    ~InitGuard() {
        TTF_Init();
        SDL_Quit();
    }
};

int main(int argc, char* argv[]) {
    try {
        InitGuard init_guard; // this ensures that all the program-wide init and de-init works even if exceptions are thrown
        MainWindow main_window;
        main_window.start(); // this method will block until the window closes (or some exception is thrown)
    }
    catch (const std::exception& err) {
        std::cerr << "Error thrown out of MainWindow:  " << err.what() << std::endl;
        return 1;
    }

    return 0;
}
