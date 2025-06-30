#ifndef PLASMA_COMPAT_H
#define PLASMA_COMPAT_H

#include "plasma_config.h"
#include <functional>
#include <algorithm>

// Define macros to check for specific standard library implementations
#if defined(__GLIBCXX__)
  #define PLASMA_USING_LIBSTDCXX
#elif defined(_LIBCPP_VERSION)
  #define PLASMA_USING_LIBCXX
#elif defined(_MSC_VER)
  #define PLASMA_USING_MSVC
#endif

// Check if deprecated functions are available
// For GCC/libstdc++, these functions are still available even in C++17 mode
// but are marked as deprecated
#ifdef PLASMA_LEGACY_BUILD
  // In legacy mode, we know we're using C++11 where these are available
  #define PLASMA_HAS_DEPRECATED_FUNCTIONAL 1
#elif defined(PLASMA_USING_LIBSTDCXX) && defined(_GLIBCXX_FUNCTIONAL)
  // libstdc++ still has these functions, just deprecated
  #define PLASMA_HAS_DEPRECATED_FUNCTIONAL 1
#elif defined(PLASMA_USING_LIBCXX) && __cplusplus >= 201703L
  // libc++ removes them in C++17
  #define PLASMA_HAS_DEPRECATED_FUNCTIONAL 0
#elif defined(PLASMA_USING_MSVC) && _MSVC_LANG >= 201703L
  // MSVC removes them in C++17
  #define PLASMA_HAS_DEPRECATED_FUNCTIONAL 0
#else
  // Default: assume they're available pre-C++17
  #define PLASMA_HAS_DEPRECATED_FUNCTIONAL (__cplusplus < 201703L)
#endif

#if PLASMA_HAS_DEPRECATED_FUNCTIONAL
  // Use standard library functions but suppress deprecation warnings
  #if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  #elif defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4996)
  #endif
#else
  // Provide replacements for removed functions
  
  namespace std {
    // Custom implementation of bind2nd
    template<typename BinaryOp, typename T>
    class plasma_binder2nd {
        BinaryOp op;
        T value;
    public:
        plasma_binder2nd(const BinaryOp& x, const T& y) : op(x), value(y) {}
        
        template<typename Arg>
        auto operator()(Arg&& arg) const 
            -> decltype(op(std::forward<Arg>(arg), value)) {
            return op(std::forward<Arg>(arg), value);
        }
    };
    
    template<typename BinaryOp, typename T>
    plasma_binder2nd<BinaryOp, T> bind2nd(const BinaryOp& op, const T& value) {
        return plasma_binder2nd<BinaryOp, T>(op, value);
    }
    
    // Custom implementation of mem_fun_ref for const member functions
    template<typename S, typename T, typename A>
    class plasma_const_mem_fun1_ref_t {
        S (T::*pmf)(A) const;
    public:
        explicit plasma_const_mem_fun1_ref_t(S (T::*p)(A) const) : pmf(p) {}
        S operator()(const T& p, A x) const { return (p.*pmf)(x); }
    };
    
    template<typename S, typename T>
    class plasma_const_mem_fun_ref_t {
        S (T::*pmf)() const;
    public:
        explicit plasma_const_mem_fun_ref_t(S (T::*p)() const) : pmf(p) {}
        S operator()(const T& p) const { return (p.*pmf)(); }
    };
    
    template<typename S, typename T, typename A>
    plasma_const_mem_fun1_ref_t<S, T, A> mem_fun_ref(S (T::*f)(A) const) {
        return plasma_const_mem_fun1_ref_t<S, T, A>(f);
    }
    
    template<typename S, typename T>
    plasma_const_mem_fun_ref_t<S, T> mem_fun_ref(S (T::*f)() const) {
        return plasma_const_mem_fun_ref_t<S, T>(f);
    }
  }
#endif

// Macro to restore warnings at the end of files
#if PLASMA_HAS_DEPRECATED_FUNCTIONAL
  #if defined(__GNUC__) || defined(__clang__)
    #define PLASMA_RESTORE_WARNINGS _Pragma("GCC diagnostic pop")
  #elif defined(_MSC_VER)
    #define PLASMA_RESTORE_WARNINGS __pragma(warning(pop))
  #else
    #define PLASMA_RESTORE_WARNINGS
  #endif
#else
  #define PLASMA_RESTORE_WARNINGS
#endif

#endif // PLASMA_COMPAT_H
