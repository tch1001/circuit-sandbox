#pragma once
/**
 * A very simple point class
 */

#include <cstddef>
#include <algorithm>

#include "integral_division.hpp"

namespace ext {
    struct point {
        int32_t x, y;

        point() = default;
        point(const point&) = default;
        point(point&&) = default;
        point& operator=(const point&) = default;
        point& operator=(point&&) = default;
        constexpr point(int32_t x, int32_t y) :x(x), y(y) {}

        // + and - compound assignment operators
        constexpr point& operator+=(const point& other) noexcept {
            x += other.x;
            y += other.y;
            return *this;
        }
        constexpr point& operator-=(const point& other) noexcept {
            x -= other.x;
            y -= other.y;
            return *this;
        }
        constexpr point& operator*=(const int32_t& scale) noexcept {
            x *= scale;
            y *= scale;
            return *this;
        }
        constexpr point& operator/=(const int32_t& scale) noexcept {
            x /= scale;
            y /= scale;
            return *this;
        }

        // arithmetic operators
        constexpr point operator+(const point& other) const noexcept {
            point ret = *this;
            ret += other;
            return ret;
        }
        constexpr point operator-(const point& other) const noexcept {
            point ret = *this;
            ret -= other;
            return ret;
        }
        constexpr point operator*(const int32_t& scale) const noexcept {
            point ret = *this;
            ret *= scale;
            return ret;
        }
        constexpr point operator/(const int32_t& scale) const noexcept {
            point ret = *this;
            ret /= scale;
            return ret;
        }

        // unary operators
        constexpr point operator+() const noexcept {
            return *this;
        }
        constexpr point operator-() const noexcept {
            return { -x, -y };
        }

        // equality operators
        constexpr bool operator==(const point& other) const noexcept {
            return x == other.x && y == other.y;
        }
        constexpr bool operator!=(const point& other) const noexcept {
            return !(*this == other);
        }

#ifdef SDL_h_ // additional convenience conversion functions if we are using SDL
        point(const SDL_Point& pt) noexcept : x(pt.x), y(pt.y) {}
        point(const SDL_MouseButtonEvent& ev) noexcept : x(ev.x), y(ev.y) {}
        point(const SDL_MouseMotionEvent& ev) noexcept : x(ev.x), y(ev.y) {}
#endif // SDL_h_

    };

    // integral division functions
    inline point div_floor(point pt, int32_t scale) noexcept {
        return { ext::div_floor(pt.x, scale), ext::div_floor(pt.y, scale) };
    }
    inline point div_ceil(point pt, int32_t scale) noexcept {
        return { ext::div_ceil(pt.x, scale), ext::div_ceil(pt.y, scale) };
    }

    // top-left bound of two points
    constexpr inline point min(const point& pt1, const point& pt2) noexcept {
        return { std::min(pt1.x, pt2.x), std::min(pt1.y, pt2.y) };
    }
    // bottom-right bound of two points
    constexpr inline point max(const point& pt1, const point& pt2) noexcept {
        return { std::max(pt1.x, pt2.x), std::max(pt1.y, pt2.y) };
    }

#ifdef SDL_h_ // additional convenience conversion functions if we are using SDL
    inline bool point_in_rect(const point& pt, const SDL_Rect& rect) noexcept {
        return pt.x >= rect.x && pt.x < rect.x + rect.w && pt.y >= rect.y && pt.y < rect.y + rect.h;
    }
#endif // SDL_h_
}
