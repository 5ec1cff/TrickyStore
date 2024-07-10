#pragma once

#include <concepts>
#include <android/log.h>

#include "type_traits.hpp"

#if defined(__LP64__)
#define LP_SELECT(lp32, lp64) lp64
#else
#define LP_SELECT(lp32, lp64) lp32
#endif

#define CONCATENATE(a, b) a##b

#define CREATE_HOOK_STUB_ENTRY(SYM, RET, FUNC, PARAMS, DEF)                                        \
    inline static struct : public hook_helper::Hooker<RET PARAMS, decltype(CONCATENATE(SYM, _tstr))>{  \
                               inline static RET replace PARAMS DEF} FUNC

#define CREATE_MEM_HOOK_STUB_ENTRY(SYM, RET, FUNC, PARAMS, DEF)                                    \
    static struct : public hook_helper::MemHooker<RET PARAMS,                                   \
                                                     decltype(CONCATENATE(SYM, _tstr))>{           \
                               inline static RET replace PARAMS DEF} FUNC

#define RETRIEVE_FUNC_SYMBOL(name, ...)                                                            \
    (name##Sym = reinterpret_cast<name##Type>(hook_helper::Dlsym(handler, __VA_ARGS__)))

#define RETRIEVE_FUNC_SYMBOL_OR_FAIL(name, ...)                                                            \
    RETRIEVE_FUNC_SYMBOL(name, __VA_ARGS__); if (!name##Sym) return false;

#define RETRIEVE_MEM_FUNC_SYMBOL(name, ...)                                                        \
    (name##Sym = reinterpret_cast<name##Type::FunType>(hook_helper::Dlsym(handler, __VA_ARGS__)))

#define RETRIEVE_MEM_FUNC_SYMBOL_OR_FAIL(name, ...)                                                        \
    RETRIEVE_MEM_FUNC_SYMBOL(name, __VA_ARGS__); if (!name##Sym) return false;

#define RETRIEVE_FIELD_SYMBOL(name, ...)                                                           \
    (name = reinterpret_cast<decltype(name)>(hook_helper::Dlsym(handler, __VA_ARGS__)))

#define CREATE_FUNC_SYMBOL_ENTRY(ret, func, ...)                                                   \
    typedef ret (*func##Type)(__VA_ARGS__);                                                        \
    inline static ret (*func##Sym)(__VA_ARGS__);                                                   \
    inline static ret func(__VA_ARGS__)

#define CREATE_MEM_FUNC_SYMBOL_ENTRY(ret, func, thiz, ...)                                         \
    using func##Type = hook_helper::MemberFunction<ret(__VA_ARGS__)>;                                  \
    inline static func##Type func##Sym;                                                            \
    inline static ret func(thiz, ##__VA_ARGS__)

namespace hook_helper {
    struct HookHandler {
        virtual void *get_symbol(const char *name) const = 0;

        virtual void *get_symbol_prefix(const char *prefix) const = 0;

        // return backup
        virtual void *hook(void *original, void *replacement) const = 0;

        virtual std::pair<uintptr_t, size_t> get_symbol_info(const char *name) const = 0;
    };

    inline namespace literals {
        template<char... chars>
        struct tstring : public std::integer_sequence<char, chars...> {
            inline constexpr static const char *c_str() { return str_; }

            inline constexpr operator std::string_view() const {
                return {c_str(), sizeof...(chars)};
            }

        private:
            inline static constexpr char str_[]{chars..., '\0'};
        };

        template<typename T, T... chars>
        inline constexpr tstring<chars...> operator ""_tstr() {
            return {};
        }

        template<char... as, char... bs>
        inline constexpr tstring<as..., bs...>
        operator+(const tstring<as...> &, const tstring<bs...> &) {
            return {};
        }

        template<char... as>
        inline constexpr auto operator+(const std::string &a, const tstring<as...> &) {
            char b[]{as..., '\0'};
            return a + b;
        }

        template<char... as>
        inline constexpr auto operator+(const tstring<as...> &, const std::string &b) {
            char a[]{as..., '\0'};
            return a + b;
        }
    }

    inline void *Dlsym(const HookHandler &handle, const char *name, bool match_prefix = false) {
        if (auto match = handle.get_symbol(name); match) {
            return match;
        } else if (match_prefix) {
            return handle.get_symbol_prefix(name);
        }
        return nullptr;
    }

    template<typename Class, typename Return, typename T, typename... Args>
    requires(std::is_same_v<T, void> ||
             std::is_same_v<Class, T>) inline static auto
    memfun_cast(Return (*func)(T *, Args...)) {
        union {
            Return (Class::*f)(Args...);

            struct {
                decltype(func) p;
                std::ptrdiff_t adj;
            } data;
        } u{.data = {func, 0}};
        static_assert(sizeof(u.f) == sizeof(u.data), "Try different T");
        return u.f;
    }

    template<typename Class, typename Return, typename... Args>
    inline static auto
    memfun_addr(Return (Class::*func)(Args...)) {
        union {
            Return (Class::*f)(Args...);

            struct {
                void *p;
                std::ptrdiff_t adj;
            } data;
        } u{.f = func};
        static_assert(sizeof(u.f) == sizeof(u.data), "Try different T");
        return u.data.p;
    }

    template<std::same_as<void> T, typename Return, typename... Args>
    inline auto memfun_cast(Return (*func)(T *, Args...)) {
        return memfun_cast<T>(func);
    }

    template<typename, typename = void>
    class MemberFunction;

    template<typename This, typename Return, typename... Args>
    class MemberFunction<Return(Args...), This> {
        using SelfType = MemberFunction<Return(This *, Args...), This>;
        using ThisType = std::conditional_t<std::is_same_v<This, void>, SelfType, This>;
        using MemFunType = Return (ThisType::*)(Args...);

    public:
        using FunType = Return (*)(This *, Args...);

    private:
        MemFunType f_ = nullptr;

    public:
        MemberFunction() = default;

        MemberFunction(FunType f) : f_(memfun_cast<ThisType>(f)) {}

        MemberFunction(MemFunType f) : f_(f) {}

        Return operator()(This *thiz, Args... args) {
            return (reinterpret_cast<ThisType *>(thiz)->*f_)(std::forward<Args>(args)...);
        }

        inline operator bool() { return f_ != nullptr; }

        inline void *addr() {
            return memfun_addr(f_);
        }
    };

// deduction guide
    template<typename This, typename Return, typename... Args>
    MemberFunction(Return (*f)(This *, Args...)) -> MemberFunction<Return(Args...), This>;

    template<typename This, typename Return, typename... Args>
    MemberFunction(Return (This::*f)(Args...)) -> MemberFunction<Return(Args...), This>;

    template<typename, typename>
    struct Hooker;

    template<typename Ret, typename... Args, char... cs>
    struct Hooker<Ret(Args...), tstring<cs...>> {
        inline static Ret (*backup)(Args...) = nullptr;

        inline static constexpr std::string_view sym = tstring<cs...>{};
    };

    template<typename, typename>
    struct MemHooker;
    template<typename Ret, typename This, typename... Args, char... cs>
    struct MemHooker<Ret(This, Args...), tstring<cs...>> {
        inline static MemberFunction<Ret(Args...)> backup;
        inline static constexpr std::string_view sym = tstring<cs...>{};
    };

    template<typename T>
    concept HookerType = requires(T a) {
        a.backup;
        a.replace;
    };

    template<HookerType T>
    inline static bool HookSymNoHandle(const HookHandler &handler, void *original, T &arg) {
        if (original) {
            if constexpr (is_instance_v<decltype(arg.backup), MemberFunction>) {
                void *backup = handler.hook(original, reinterpret_cast<void *>(arg.replace));
                arg.backup = reinterpret_cast<typename decltype(arg.backup)::FunType>(backup);
            } else {
                arg.backup = reinterpret_cast<decltype(arg.backup)>(
                        handler.hook(original, reinterpret_cast<void *>(arg.replace)));
            }
            return true;
        } else {
            return false;
        }
    }

    template<HookerType T>
    inline static bool HookSym(const HookHandler &handler, T &arg) {
        auto original = handler.get_symbol(arg.sym.data());
        return HookSymNoHandle(handler, original, arg);
    }

    template<HookerType T, HookerType... Args>
    inline static bool HookSyms(const HookHandler &handle, T &first, Args &...rest) {
        if (!(HookSym(handle, first) || ... || HookSym(handle, rest))) {
            __android_log_print(ANDROID_LOG_ERROR,
#ifdef LOG_TAG
                                LOG_TAG,
#else
                    "HookHelper",
#endif
                                "Hook Fails: %*s", static_cast<int>(first.sym.size()),
                                first.sym.data());
            return false;
        }
        return true;
    }

}  // namespace hook_helper