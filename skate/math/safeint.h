#ifndef SKATE_SAFEINT_H
#define SKATE_SAFEINT_H

#include <limits>
#include <cstdlib>
#include <string>
#include <exception>

namespace skate {
    template<typename L, typename R, typename std::enable_if<std::is_integral<L>::value && std::is_integral<R>::value && std::is_signed<L>::value == std::is_signed<R>::value, int>::type = 0>
    constexpr static bool safe_less_than(L l, R r) noexcept {
        return l < r;
    }

    template<typename L, typename R, typename std::enable_if<std::is_integral<L>::value && std::is_integral<R>::value && std::is_signed<L>::value && std::is_unsigned<R>::value, int>::type = 0>
    constexpr static bool safe_less_than(L l, R r) noexcept {
        return l < L(0) || static_cast<typename std::make_unsigned<L>::type>(l) < r;
    }

    template<typename L, typename R, typename std::enable_if<std::is_integral<L>::value && std::is_integral<R>::value && std::is_unsigned<L>::value && std::is_signed<R>::value, int>::type = 0>
    constexpr static bool safe_less_than(L l, R r) noexcept {
        return R(0) < r && l < static_cast<typename std::make_unsigned<L>::type>(r);
    }

    template<typename L, typename R>
    constexpr static int safe_compare(L l, R r) noexcept {
        return safe_less_than(l, r) ? -1 : safe_less_than(r, l) ? 1 : 0;
    }

    enum class safeint_mode {
        mask,           // Uses the lowest bits of the representation, just truncating higher bits as needed (i.e. an 8 bit unsigned safeint would transform 0xfecd to 0xcd)
        saturate,       // Saturates the representation to the high or low end of the range (i.e. an 8 bit unsigned safeint would transform 0xfecd to 0xff)
        error           // Throws an exception if an out of range error occurs (i.e. an 8 bit unsigned safeint would throw a safeint_exception with 0xfecd)
    };

    class safeint_exception : public std::exception {
    public:
        virtual const char *what() const noexcept { return "safe integer detected invalid value"; }
    };

    class safeint_not_implemented_exception : public std::exception {
    public:
        virtual const char *what() const noexcept { return "operation is not yet implemented for safe integer"; }
    };

    // Stores a logical value inside an underlying type
    // The logical value can be more restricted in size than what the underlying type supports (hence the Bits field in the template parameters)
    // The Mode specifies the operation performed when attempting clamping the value into range
    template<typename Underlying,
             safeint_mode Mode = std::is_signed<Underlying>::value? safeint_mode::saturate: safeint_mode::mask,
             int Bits = std::numeric_limits<Underlying>::digits + std::is_signed<Underlying>::value>
    class basic_safeint {
        constexpr static const int UnderlyingBits = std::numeric_limits<Underlying>::digits + (std::is_signed<Underlying>::value != 0);
        constexpr static const bool Signed = std::is_signed<Underlying>::value;

        static_assert(UnderlyingBits >= Bits, "basic_safeint cannot contain a larger number of bits than the underlying type");
        static_assert(Mode != safeint_mode::mask || !Signed, "basic_safeint cannot use mask mode with signed integers");

        // Maximum and minimum value of logical value
        constexpr static const Underlying Max = std::numeric_limits<Underlying>::max() / (UnderlyingBits == Bits? 1: 2 * (UnderlyingBits - Bits));
        constexpr static const Underlying Min = std::numeric_limits<Underlying>::min() / (UnderlyingBits == Bits? 1: 2 * (UnderlyingBits - Bits));

        template<typename L, typename R, typename std::enable_if<std::is_signed<L>::value == std::is_signed<R>::value, int>::type = 0>
        constexpr static bool LessThan(L l, R r) noexcept {
            return l < r;
        }

        template<typename L, typename R, typename std::enable_if<std::is_signed<L>::value && std::is_unsigned<R>::value, int>::type = 0>
        constexpr static bool LessThan(L l, R r) noexcept {
            return l < L(0) || static_cast<typename std::make_unsigned<L>::type>(l) < r;
        }

        template<typename L, typename R, typename std::enable_if<std::is_unsigned<L>::value && std::is_signed<R>::value, int>::type = 0>
        constexpr static bool LessThan(L l, R r) noexcept {
            return r >= R(0) && l < static_cast<typename std::make_unsigned<L>::type>(r);
        }

        template<typename T>
        constexpr static T Reduce(T value) noexcept { return value; }

        template<typename T, safeint_mode M, int B>
        constexpr static auto Reduce(basic_safeint<T, M, B> value) noexcept -> decltype(value.value()) { return value.value(); }

        template<typename T>
        constexpr static Underlying ClampHelper(T value) {
            return Mode == safeint_mode::mask? Underlying(value & Max): // TODO: fix mask mode for signed values
                   Mode == safeint_mode::saturate? LessThan(value, Min)? Min: LessThan(Max, value)? Max: value:
                   LessThan(value, Min) || LessThan(Max, value)? throw safeint_exception(): value;
        }

        template<typename T>
        constexpr static Underlying Clamp(T value) { return ClampHelper(Reduce(value)); }

    public:
        template<typename T = unsigned>
        constexpr basic_safeint(T v = 0) : x(Clamp(v)) {}
        constexpr basic_safeint(const basic_safeint &other) = default;

        template<typename T>
        basic_safeint &operator=(T v) {
            x = Clamp(v);
            return *this;
        }
        basic_safeint &operator=(const basic_safeint &other) = default;

        constexpr Underlying value() const noexcept { return x; }
        constexpr operator Underlying() const noexcept { return x; }

        template<typename T>
        constexpr bool operator<(T other) const noexcept {
            return LessThan(x, Reduce(other));
        }
        template<typename T>
        constexpr bool operator>(T other) const noexcept {
            return LessThan(Reduce(other), x);
        }
        template<typename T>
        constexpr bool operator<=(T other) const noexcept {
            return !operator>(other);
        }
        template<typename T>
        constexpr bool operator>=(T other) const noexcept {
            return !operator<(other);
        }

        template<typename T>
        constexpr bool operator!=(T other) const noexcept { return operator<(other) || operator>(other); }
        template<typename T>
        constexpr bool operator==(T other) const noexcept { return !operator==(other); }

        template<typename T>
        basic_safeint &operator+=(T other) {
            const auto other_reduced = Reduce(other);

            if ((x >= 0 && LessThan(Max - x, other_reduced)) ||
                (x < 0 && LessThan(other_reduced, Min - x))) {
                switch (Mode) {
                    case safeint_mode::mask: x &= Max; break;
                    case safeint_mode::saturate: x = x < 0? Min: Max; break;
                    default: throw safeint_exception(); break;
                }
            } else {
                x = Underlying(x + other_reduced);
            }

            return *this;
        }

        template<typename T>
        basic_safeint &operator-=(T other) {
            const auto other_reduced = Reduce(other);

            if ((x >= 0 && LessThan(other_reduced, Min + x)) ||
                (x < 0 && LessThan(Max + x, other_reduced))) {
                switch (Mode) {
                    case safeint_mode::mask: x &= Max; break;
                    case safeint_mode::saturate: x = x < 0? Max: Min; break;
                    default: throw safeint_exception(); break;
                }
            } else {
                x = Underlying(x - other_reduced);
            }

            return *this;
        }

#if 0
        template<typename T>
        basic_safeint &operator/=(T other) {
            const auto other_reduced = Reduce(other);

            if (Min < -Max && x == Min && other_reduced < 0 && other_reduced == -1) {
                // Overflow
            }
        }
#endif

        basic_safeint &operator++() { return *this += 1; }
        basic_safeint operator++(int) { const auto copy = *this; ++*this; return copy; }

        basic_safeint &operator--() { return *this -= 1; }
        basic_safeint operator--(int) { const auto copy = *this; --*this; return copy; }

        constexpr bool is_masking() const noexcept { return Mode == safeint_mode::mask; }
        constexpr bool is_saturating() const noexcept { return Mode == safeint_mode::saturate; }
        constexpr bool is_throwing() const noexcept { return Mode == safeint_mode::error; }

        std::string to_string(int base = 10) const {
            return std::to_string(x);
        }
        std::wstring to_wstring(int base = 10) const {
            return std::to_wstring(x);
        }

    private:
        Underlying x;
    };

    template<typename T, typename U, safeint_mode M, int B>
    constexpr bool operator<(T l, basic_safeint<U, M, B> r) noexcept { return r.operator>(l); }
    template<typename T, typename U, safeint_mode M, int B>
    constexpr bool operator>(T l, basic_safeint<U, M, B> r) noexcept { return r.operator<(l); }
    template<typename T, typename U, safeint_mode M, int B>
    constexpr bool operator<=(T l, basic_safeint<U, M, B> r) noexcept { return r.operator>=(l); }
    template<typename T, typename U, safeint_mode M, int B>
    constexpr bool operator>=(T l, basic_safeint<U, M, B> r) noexcept { return r.operator<=(l); }
    template<typename T, typename U, safeint_mode M, int B>
    constexpr bool operator==(T l, basic_safeint<U, M, B> r) noexcept { return r.operator==(l); }
    template<typename T, typename U, safeint_mode M, int B>
    constexpr bool operator!=(T l, basic_safeint<U, M, B> r) noexcept { return r.operator!=(l); }
}

#endif // SKATE_SAFEINT_H
