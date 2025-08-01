/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

/* COMPILER() - the compiler being used to build the project */
#define COMPILER(WTF_FEATURE) (defined WTF_COMPILER_##WTF_FEATURE  && WTF_COMPILER_##WTF_FEATURE)

/* COMPILER_SUPPORTS() - whether the compiler being used to build the project supports the given feature. */
#define COMPILER_SUPPORTS(WTF_COMPILER_FEATURE) (defined WTF_COMPILER_SUPPORTS_##WTF_COMPILER_FEATURE  && WTF_COMPILER_SUPPORTS_##WTF_COMPILER_FEATURE)

/* COMPILER_QUIRK() - whether the compiler being used to build the project requires a given quirk. */
#define COMPILER_QUIRK(WTF_COMPILER_QUIRK) (defined WTF_COMPILER_QUIRK_##WTF_COMPILER_QUIRK  && WTF_COMPILER_QUIRK_##WTF_COMPILER_QUIRK)

/* COMPILER_HAS_ATTRIBUTE() - whether the compiler supports a particular attribute. */
/* https://clang.llvm.org/docs/LanguageExtensions.html#has-attribute */
/* https://gcc.gnu.org/onlinedocs/cpp/_005f_005fhas_005fattribute.html */
#ifdef __has_attribute
#define COMPILER_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
#define COMPILER_HAS_ATTRIBUTE(x) 0
#endif

/* COMPILER_HAS_ATTRIBUTE() - whether the compiler supports a particular c++ attribute. */
/* https://clang.llvm.org/docs/LanguageExtensions.html#has-cpp-attribute */
/* https://gcc.gnu.org/onlinedocs/cpp/_005f_005fhas_005fcpp_005fattribute.html */
#ifdef __has_cpp_attribute
#define COMPILER_HAS_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#else
#define COMPILER_HAS_CPP_ATTRIBUTE(x) 0
#endif

/* COMPILER_HAS_CLANG_BUILTIN() - whether the compiler supports a particular clang builtin. */
#ifdef __has_builtin
#define COMPILER_HAS_CLANG_BUILTIN(x) __has_builtin(x)
#else
#define COMPILER_HAS_CLANG_BUILTIN(x) 0
#endif

/* COMPILER_HAS_CLANG_FEATURE() - whether the compiler supports a particular language or library feature. */
/* https://clang.llvm.org/docs/LanguageExtensions.html#has-feature-and-has-extension */
#ifdef __has_feature
#define COMPILER_HAS_CLANG_FEATURE(x) __has_feature(x)
#else
#define COMPILER_HAS_CLANG_FEATURE(x) 0
#endif

/* COMPILER_HAS_CLANG_DECLSPEC() - whether the compiler supports a Microsoft style __declspec attribute. */
/* https://clang.llvm.org/docs/LanguageExtensions.html#has-declspec-attribute */
#ifdef __has_declspec_attribute
#define COMPILER_HAS_CLANG_DECLSPEC(x) __has_declspec_attribute(x)
#else
#define COMPILER_HAS_CLANG_DECLSPEC(x) 0
#endif

/* ==== COMPILER() - primary detection of the compiler being used to build the project, in alphabetical order ==== */

/* COMPILER(CLANG) - Clang  */

#if defined(__clang__)
#define WTF_COMPILER_CLANG 1
#define WTF_COMPILER_SUPPORTS_BLOCKS COMPILER_HAS_CLANG_FEATURE(blocks)
#define WTF_COMPILER_SUPPORTS_C_STATIC_ASSERT COMPILER_HAS_CLANG_FEATURE(c_static_assert)
#define WTF_COMPILER_SUPPORTS_CXX_EXCEPTIONS COMPILER_HAS_CLANG_FEATURE(cxx_exceptions)
#define WTF_COMPILER_SUPPORTS_BUILTIN_IS_TRIVIALLY_COPYABLE COMPILER_HAS_CLANG_FEATURE(is_trivially_copyable)

#if defined(__apple_build_version__)
#define WTF_COMPILER_APPLE_CLANG 1
#endif

#ifdef __cplusplus
#if __cplusplus <= 201103L
#define WTF_CPP_STD_VER 11
#elif __cplusplus <= 201402L
#define WTF_CPP_STD_VER 14
#elif __cplusplus <= 201703L
#define WTF_CPP_STD_VER 17
#endif
#endif

#endif // defined(__clang__)

/* COMPILER(GCC_COMPATIBLE) - GNU Compiler Collection or compatibles */
#if defined(__GNUC__)
#define WTF_COMPILER_GCC_COMPATIBLE 1
#endif

/* COMPILER(GCC) - GNU Compiler Collection */
/* Note: This section must come after the Clang section since we check !COMPILER(CLANG) here. */
#if COMPILER(GCC_COMPATIBLE) && !COMPILER(CLANG)
#define WTF_COMPILER_GCC 1

#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#define GCC_VERSION_AT_LEAST(major, minor, patch) (GCC_VERSION >= (major * 10000 + minor * 100 + patch))

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define WTF_COMPILER_SUPPORTS_C_STATIC_ASSERT 1
#endif

#endif /* COMPILER(GCC) */

#if COMPILER(GCC_COMPATIBLE) && !defined(__clang_tapi__) && defined(NDEBUG) && !defined(__OPTIMIZE__) && !defined(RELEASE_WITHOUT_OPTIMIZATIONS)
#error "Building release without compiler optimizations: WebKit will be slow. Set -DRELEASE_WITHOUT_OPTIMIZATIONS if this is intended."
#endif

/* COMPILER(MSVC) - Microsoft Visual C++ */

#if defined(_MSC_VER)

#define WTF_COMPILER_MSVC 1

#if _MSC_VER < 1910
#error "Please use a newer version of Visual Studio. WebKit requires VS2017 or newer to compile."
#endif

#endif

#if !COMPILER(CLANG)
#define WTF_COMPILER_QUIRK_CONSIDERS_UNREACHABLE_CODE 1
#endif

/* ==== COMPILER_SUPPORTS - additional compiler feature detection, in alphabetical order ==== */

/* COMPILER_SUPPORTS(EABI) */

#if defined(__ARM_EABI__) || defined(__EABI__)
#define WTF_COMPILER_SUPPORTS_EABI 1
#endif

/* ASAN_ENABLED and SUPPRESS_ASAN */

#ifdef __SANITIZE_ADDRESS__
#define ASAN_ENABLED 1
#else
#define ASAN_ENABLED COMPILER_HAS_CLANG_FEATURE(address_sanitizer)
#endif

#if ASAN_ENABLED
#define SUPPRESS_ASAN __attribute__((no_sanitize_address))
#else
#define SUPPRESS_ASAN
#endif

/* TSAN_ENABLED and SUPPRESS_TSAN */

#ifdef __SANITIZE_THREAD__
#define TSAN_ENABLED 1
#else
#define TSAN_ENABLED COMPILER_HAS_CLANG_FEATURE(thread_sanitizer)
#endif

#if TSAN_ENABLED
#define SUPPRESS_TSAN __attribute__((no_sanitize_thread))
#else
#define SUPPRESS_TSAN
#endif

/* COVERAGE_ENABLED and SUPPRESS_COVERAGE */

#define COVERAGE_ENABLED COMPILER_HAS_CLANG_FEATURE(coverage_sanitizer)

#if COVERAGE_ENABLED
#define SUPPRESS_COVERAGE __attribute__((no_sanitize("coverage")))
#else
#define SUPPRESS_COVERAGE
#endif

/* ==== Compiler-independent macros for various compiler features, in alphabetical order ==== */

/* ALWAYS_INLINE */

/* In GCC functions marked with no_sanitize_address cannot call functions that are marked with always_inline and not marked with no_sanitize_address.
 * Therefore we need to give up on the enforcement of ALWAYS_INLINE when building with ASAN. https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67368 */
#if !defined(ALWAYS_INLINE) && defined(NDEBUG) && !(COMPILER(GCC) && ASAN_ENABLED)
#define ALWAYS_INLINE inline __attribute__((__always_inline__))
#endif

#if !defined(ALWAYS_INLINE)
#define ALWAYS_INLINE inline
#endif

/* ALWAYS_INLINE_LAMBDA */

/* In GCC functions marked with no_sanitize_address cannot call functions that are marked with always_inline and not marked with no_sanitize_address.
 * Therefore we need to give up on the enforcement of ALWAYS_INLINE_LAMBDA when building with ASAN. https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67368 */
#if !defined(ALWAYS_INLINE_LAMBDA) && defined(NDEBUG) && !(COMPILER(GCC) && ASAN_ENABLED)
#define ALWAYS_INLINE_LAMBDA __attribute__((__always_inline__))
#endif

#if !defined(ALWAYS_INLINE_LAMBDA)
#define ALWAYS_INLINE_LAMBDA
#endif

/* WTF_EXTERN_C_{BEGIN, END} */

#ifdef __cplusplus
#define WTF_EXTERN_C_BEGIN extern "C" {
#define WTF_EXTERN_C_END }
#else
#define WTF_EXTERN_C_BEGIN
#define WTF_EXTERN_C_END
#endif

/* FALLTHROUGH */

#if !defined(FALLTHROUGH) && !defined(__cplusplus)

#if COMPILER_HAS_ATTRIBUTE(fallthrough)
#define FALLTHROUGH __attribute__ ((fallthrough))
#endif

#if !defined(FALLTHROUGH)
#define FALLTHROUGH
#endif

#endif // !defined(FALLTHROUGH) && !defined(__cplusplus)

/* LIFETIME_BOUND */

#if !defined(LIFETIME_BOUND) && defined(__cplusplus)
#if COMPILER_HAS_CPP_ATTRIBUTE(clang::lifetimebound)
#define LIFETIME_BOUND [[clang::lifetimebound]]
#elif COMPILER_HAS_CPP_ATTRIBUTE(msvc::lifetimebound)
#define LIFETIME_BOUND [[msvc::lifetimebound]]
#elif COMPILER_HAS_CPP_ATTRIBUTE(lifetimebound)
#define LIFETIME_BOUND [[lifetimebound]]
#endif
#endif

#if !defined(LIFETIME_BOUND)
#define LIFETIME_BOUND
#endif

/* NEVER_INLINE */

#if !defined(NEVER_INLINE)
#define NEVER_INLINE __attribute__((__noinline__))
#endif

/* NOT_TAIL_CALLED */

#if !defined(NOT_TAIL_CALLED)
#if COMPILER_HAS_ATTRIBUTE(not_tail_called)
#define NOT_TAIL_CALLED __attribute__((not_tail_called))
#endif
#endif

#if !defined(NOT_TAIL_CALLED)
#define NOT_TAIL_CALLED
#endif

/* MUST_TAIL_CALL */

// 32-bit platforms use different calling conventions, so a MUST_TAIL_CALL function
// written for 64-bit may fail to tail call on 32-bit.
// It also doesn't work on ppc64le: https://github.com/llvm/llvm-project/issues/98859
// and on Windows: https://github.com/llvm/llvm-project/issues/116568
#if !defined(MUST_TAIL_CALL) && COMPILER(CLANG) && __SIZEOF_POINTER__ == 8 && defined(__cplusplus) && COMPILER_HAS_CPP_ATTRIBUTE(clang::musttail) && !defined(__powerpc__) && !defined(_WIN32)
#define MUST_TAIL_CALL [[clang::musttail]]
#define HAVE_MUST_TAIL_CALL 1
#endif

#if !defined(MUST_TAIL_CALL)
#define MUST_TAIL_CALL
#define HAVE_MUST_TAIL_CALL 0
#endif

/* RETURNS_NONNULL */
#if !defined(RETURNS_NONNULL)
#define RETURNS_NONNULL __attribute__((returns_nonnull))
#endif

/* OBJC_CLASS */

#if !defined(OBJC_CLASS) && defined(__OBJC__)
#define OBJC_CLASS @class
#endif

#if !defined(OBJC_CLASS)
#define OBJC_CLASS class
#endif

/* OBJC_PROTOCOL */

#if !defined(OBJC_PROTOCOL) && defined(__OBJC__)
/* This forward-declares a protocol, then also creates a type of the same name based on NSObject.
 * This allows us to use "NSObject<MyProtocol> *" or "MyProtocol *" more-or-less interchangably. */
#define OBJC_PROTOCOL(protocolName) @protocol protocolName; using protocolName = NSObject<protocolName>
#endif

#if !defined(OBJC_PROTOCOL)
#define OBJC_PROTOCOL(protocolName) class protocolName
#endif

// https://clang.llvm.org/docs/AttributeReference.html#preferred-type
#if COMPILER_HAS_ATTRIBUTE(preferred_type)
#define PREFERRED_TYPE(T) __attribute__((preferred_type(T)))
#else
#define PREFERRED_TYPE(T)
#endif

/* PURE_FUNCTION */

#if !defined(PURE_FUNCTION)
#define PURE_FUNCTION __attribute__((__pure__))
#endif

/* WK_UNUSED_INSTANCE_VARIABLE */

#if !defined(WK_UNUSED_INSTANCE_VARIABLE)
#define WK_UNUSED_INSTANCE_VARIABLE __attribute__((unused))
#endif

/* UNUSED_FUNCTION */

#if !defined(UNUSED_FUNCTION)
#define UNUSED_FUNCTION __attribute__((unused))
#endif

/* UNUSED_MEMBER_VARIABLE */

#if !defined(UNUSED_MEMBER_VARIABLE)
#define UNUSED_MEMBER_VARIABLE __attribute__((unused))
#endif

/* TRIVIAL_ABI */

#if !defined(TRIVIAL_ABI)
#if COMPILER(CLANG)
#define TRIVIAL_ABI __attribute__((trivial_abi))
#else
#define TRIVIAL_ABI
#endif
#endif

/* UNUSED_TYPE_ALIAS */

#if !defined(UNUSED_TYPE_ALIAS)
#define UNUSED_TYPE_ALIAS __attribute__((unused))
#endif

/* REFERENCED_FROM_ASM */

#if !defined(REFERENCED_FROM_ASM)
#define REFERENCED_FROM_ASM __attribute__((__used__))
#endif

/* NO_REORDER */

#if !defined(NO_REORDER) && COMPILER(GCC)
#define NO_REORDER \
    __attribute__((__no_reorder__))
#endif

#if !defined(NO_REORDER)
#define NO_REORDER
#endif

/* UNUSED_LABEL */

/* Keep the compiler from complaining for a local label that is defined but not referenced. */
/* Helpful when mixing hand-written and autogenerated code. */

#if !defined(UNUSED_LABEL)
#define UNUSED_LABEL(label) UNUSED_PARAM(&& label)
#endif

/* UNUSED_PARAM */

#if !defined(UNUSED_PARAM)
#define UNUSED_PARAM(variable) (void)variable
#endif

/* UNUSED_VARIABLE */
#if !defined(UNUSED_VARIABLE)
#define UNUSED_VARIABLE(variable) UNUSED_PARAM(variable)
#endif

/* UNUSED_VARIADIC_PARAMS */

#if !defined(UNUSED_VARIADIC_PARAMS)
#define UNUSED_VARIADIC_PARAMS __attribute__((unused))
#endif

/* WARN_UNUSED_RETURN */

#if !defined(WARN_UNUSED_RETURN)
#define WARN_UNUSED_RETURN __attribute__((__warn_unused_result__))
#endif

/* DEBUGGER_ANNOTATION_MARKER */

#if !defined(DEBUGGER_ANNOTATION_MARKER) && COMPILER(GCC)
#define DEBUGGER_ANNOTATION_MARKER(name) \
    __attribute__((__no_reorder__)) void name(void) { __asm__(""); }
#endif

#if !defined(DEBUGGER_ANNOTATION_MARKER)
#define DEBUGGER_ANNOTATION_MARKER(name)
#endif

/* IGNORE_WARNINGS */

/* Can't use WTF_CONCAT() and STRINGIZE() because they are defined in
 * StdLibExtras.h, which includes this file. */
#define _COMPILER_CONCAT_I(a, b) a ## b
#define _COMPILER_CONCAT(a, b) _COMPILER_CONCAT_I(a, b)

#define _COMPILER_STRINGIZE(exp) #exp

#define _COMPILER_WARNING_NAME(warning) "-W" warning

#define IGNORE_WARNINGS_BEGIN_COND(cond, compiler, warning) \
    _Pragma(_COMPILER_STRINGIZE(compiler diagnostic push)) \
    _COMPILER_CONCAT(IGNORE_WARNINGS_BEGIN_IMPL_, cond)(compiler, warning)

/* Condition is either 1 (true) or 0 (false, do nothing). */
#define IGNORE_WARNINGS_BEGIN_IMPL_1(compiler, warning) \
    _Pragma(_COMPILER_STRINGIZE(compiler diagnostic ignored warning))
#define IGNORE_WARNINGS_BEGIN_IMPL_0(compiler, warning)

#if defined(__has_warning)
#define IGNORE_WARNINGS_END_IMPL(compiler) _Pragma(_COMPILER_STRINGIZE(compiler diagnostic pop))
#else
#define IGNORE_WARNINGS_END_IMPL(compiler) \
    _Pragma(_COMPILER_STRINGIZE(compiler diagnostic pop)) \
    _Pragma(_COMPILER_STRINGIZE(compiler diagnostic pop))
#endif

#if defined(__has_warning)
#define _IGNORE_WARNINGS_BEGIN_IMPL(compiler, warning) \
    IGNORE_WARNINGS_BEGIN_COND(__has_warning(warning), compiler, warning)
#else
/* Suppress -Wpragmas to dodge warnings about attempts to suppress unrecognized warnings. */
#define _IGNORE_WARNINGS_BEGIN_IMPL(compiler, warning) \
    IGNORE_WARNINGS_BEGIN_COND(1, compiler, "-Wpragmas") \
    IGNORE_WARNINGS_BEGIN_COND(1, compiler, warning)
#endif

#define IGNORE_WARNINGS_BEGIN_IMPL(compiler, warning) \
    _IGNORE_WARNINGS_BEGIN_IMPL(compiler, _COMPILER_WARNING_NAME(warning))


#if COMPILER(GCC)
#define IGNORE_GCC_WARNINGS_BEGIN(warning) IGNORE_WARNINGS_BEGIN_IMPL(GCC, warning)
#define IGNORE_GCC_WARNINGS_END IGNORE_WARNINGS_END_IMPL(GCC)
#else
#define IGNORE_GCC_WARNINGS_BEGIN(warning)
#define IGNORE_GCC_WARNINGS_END
#endif

#if COMPILER(CLANG)
#define IGNORE_CLANG_WARNINGS_BEGIN(warning) IGNORE_WARNINGS_BEGIN_IMPL(clang, warning)
#define IGNORE_CLANG_WARNINGS_END IGNORE_WARNINGS_END_IMPL(clang)
#else
#define IGNORE_CLANG_WARNINGS_BEGIN(warning)
#define IGNORE_CLANG_WARNINGS_END
#endif

#define IGNORE_WARNINGS_BEGIN(warning) IGNORE_WARNINGS_BEGIN_IMPL(GCC, warning)
#define IGNORE_WARNINGS_END IGNORE_WARNINGS_END_IMPL(GCC)

/* IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_BEGIN() - whether the compiler supports suppressing static analysis warnings. */
/* https://clang.llvm.org/docs/AttributeReference.html#suppress */
#if COMPILER_HAS_ATTRIBUTE(suppress)
#define IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE(warning, ...) [[clang::suppress]]
#define IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_BEGIN(warning, ...) [[clang::suppress]] {
#else
#define IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE(warning, ...)
#define IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_BEGIN(warning, ...) {
#endif
#define IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_END }

#if COMPILER(APPLE_CLANG) || defined(CLANG_WEBKIT_BRANCH) || !defined __clang_major__ || __clang_major__ >= 19
#define IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE_ON_MEMBER(...) IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE(__VA_ARGS__)
#define IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE_ON_CLASS(...) IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE(__VA_ARGS__)
#define IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE_ON_FUNCTION(...) IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE(__VA_ARGS__)
#else
#define IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE_ON_MEMBER(...)
#define IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE_ON_CLASS(...)
#define IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE_ON_FUNCTION(...)
#endif

#define ALLOW_DEPRECATED_DECLARATIONS_BEGIN IGNORE_WARNINGS_BEGIN("deprecated-declarations")
#define ALLOW_DEPRECATED_DECLARATIONS_END IGNORE_WARNINGS_END

#define ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN IGNORE_WARNINGS_BEGIN("deprecated-implementations")
#define ALLOW_DEPRECATED_IMPLEMENTATIONS_END IGNORE_WARNINGS_END

#define ALLOW_DEPRECATED_PRAGMA_BEGIN IGNORE_WARNINGS_BEGIN("deprecated-pragma")
#define ALLOW_DEPRECATED_PRAGMA_END IGNORE_WARNINGS_END

#define ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN IGNORE_CLANG_WARNINGS_BEGIN("unguarded-availability-new")
#define ALLOW_NEW_API_WITHOUT_GUARDS_END IGNORE_CLANG_WARNINGS_END

#define ALLOW_UNUSED_PARAMETERS_BEGIN IGNORE_WARNINGS_BEGIN("unused-parameter")
#define ALLOW_UNUSED_PARAMETERS_END IGNORE_WARNINGS_END

#define ALLOW_COMMA_BEGIN IGNORE_WARNINGS_BEGIN("comma")
#define ALLOW_COMMA_END IGNORE_WARNINGS_END

#define ALLOW_NONLITERAL_FORMAT_BEGIN IGNORE_WARNINGS_BEGIN("format-nonliteral")
#define ALLOW_NONLITERAL_FORMAT_END IGNORE_WARNINGS_END

#define SUPPRESS_USE_AFTER_MOVE \
    IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE("cplusplus.Move")

#define SUPPRESS_UNCHECKED_ARG \
    IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE_ON_MEMBER("alpha.webkit.UncheckedCallArgsChecker")

#define SUPPRESS_UNCHECKED_LOCAL \
    IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE("alpha.webkit.UncheckedLocalVarsChecker")

#define SUPPRESS_UNCHECKED_MEMBER \
    IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE_ON_MEMBER("webkit.NoUncheckedPtrMemberChecker")

#define SUPPRESS_UNCOUNTED_ARG \
    IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE_ON_MEMBER("alpha.webkit.UncountedCallArgsChecker")

#define SUPPRESS_UNCOUNTED_LOCAL \
    IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE("alpha.webkit.UncountedLocalVarsChecker")

#define SUPPRESS_UNCOUNTED_MEMBER \
    IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE_ON_MEMBER("webkit.NoUncountedMemberChecker")

#if COMPILER(APPLE_CLANG) || defined(CLANG_WEBKIT_BRANCH) || !defined __clang_major__ || __clang_major__ >= 19
#define SUPPRESS_UNCOUNTED_LAMBDA_CAPTURE \
    IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE("webkit.UncountedLambdaCapturesChecker")
#define SUPPRESS_UNRETAINED_LOCAL \
    IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE("alpha.webkit.UnretainedLocalVarsChecker")
#define SUPPRESS_UNRETAINED_ARG \
    IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE("alpha.webkit.UnretainedCallArgsChecker")
#else
#define SUPPRESS_UNCOUNTED_LAMBDA_CAPTURE
#define SUPPRESS_UNRETAINED_LOCAL
#define SUPPRESS_UNRETAINED_ARG
#endif

// To suppress webkit.RefCntblBaseVirtualDtor, use NoVirtualDestructorBase instead.

#define SUPPRESS_MEMORY_UNSAFE_CAST \
    IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE("alpha.webkit.MemoryUnsafeCastChecker")

#if defined(CLANG_WEBKIT_BRANCH) || !defined __clang_major__ || __clang_major__ >= 21
#define SUPPRESS_FORWARD_DECL_MEMBER \
    IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE_ON_MEMBER("alpha.webkit.ForwardDeclChecker")
#define SUPPRESS_FORWARD_DECL_ARG \
    IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE_ON_FUNCTION("alpha.webkit.ForwardDeclChecker")
#else
#define SUPPRESS_FORWARD_DECL_MEMBER
#define SUPPRESS_FORWARD_DECL_ARG
#endif

#define IGNORE_RETURN_TYPE_WARNINGS_BEGIN IGNORE_WARNINGS_BEGIN("return-type")
#define IGNORE_RETURN_TYPE_WARNINGS_END IGNORE_WARNINGS_END

#define IGNORE_NULL_CHECK_WARNINGS_BEGIN IGNORE_WARNINGS_BEGIN("nonnull")
#define IGNORE_NULL_CHECK_WARNINGS_END IGNORE_WARNINGS_END

#if COMPILER(CLANG)
#define DECLARE_SYSTEM_HEADER _Pragma(_COMPILER_STRINGIZE(clang system_header))
#else
#define DECLARE_SYSTEM_HEADER
#endif

/* NOESCAPE */
/* This attribute promises that a function argument will only be used for the duration of the function,
   and not stored to the heap or in a global state for later use. The compiler does not verify this claim. */

#if !defined(NOESCAPE)
#if COMPILER_HAS_CPP_ATTRIBUTE(clang::noescape)
#define NOESCAPE [[clang::noescape]]
#else
#define NOESCAPE
#endif
#endif

/* NO_UNIQUE_ADDRESS */

#if !defined(NO_UNIQUE_ADDRESS)
#if COMPILER_HAS_CPP_ATTRIBUTE(no_unique_address)
#define NO_UNIQUE_ADDRESS [[no_unique_address]] // NOLINT: check-webkit-style does not understand annotations.
#else
#define NO_UNIQUE_ADDRESS
#endif
#endif

/* TLS_MODEL_INITIAL_EXEC */

#if !defined(TLS_MODEL_INITIAL_EXEC)
#if COMPILER_HAS_ATTRIBUTE(tls_model)
#define TLS_MODEL_INITIAL_EXEC __attribute__((tls_model("initial-exec")))
#endif
#endif

#if !defined(TLS_MODEL_INITIAL_EXEC)
#define TLS_MODEL_INITIAL_EXEC
#endif

/* UNREACHABLE */

#define WTF_UNREACHABLE(...) __builtin_unreachable();

/* WTF_ALLOW_UNSAFE_BUFFER_USAGE */

#if COMPILER(CLANG)
#define WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN \
    _Pragma("clang diagnostic push") \
    _Pragma("clang diagnostic ignored \"-Wunknown-warning-option\"") \
    _Pragma("clang diagnostic ignored \"-Wunsafe-buffer-usage\"") \
    _Pragma("clang diagnostic ignored \"-Wunsafe-buffer-usage-in-libc-call\"")

#define WTF_ALLOW_UNSAFE_BUFFER_USAGE_END \
    _Pragma("clang diagnostic pop")
#else
#define WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
#define WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
#endif

/* WTF_UNSAFE_BUFFER_USAGE */

#if COMPILER(CLANG)
#if COMPILER_HAS_CPP_ATTRIBUTE(clang::unsafe_buffer_usage)
#define WTF_UNSAFE_BUFFER_USAGE [[clang::unsafe_buffer_usage]]
#elif COMPILER_HAS_ATTRIBUTE(unsafe_buffer_usage)
#define WTF_UNSAFE_BUFFER_USAGE __attribute__((__unsafe_buffer_usage__))
#else
#define WTF_UNSAFE_BUFFER_USAGE
#endif
#else
#define WTF_UNSAFE_BUFFER_USAGE
#endif

/* WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE */

#define WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN \
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN \
    ALLOW_COMMA_BEGIN \
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN \
    ALLOW_UNUSED_PARAMETERS_BEGIN \
    IGNORE_WARNINGS_BEGIN("cast-align") \
    IGNORE_CLANG_WARNINGS_BEGIN("thread-safety-reference-return")

#define WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END \
    IGNORE_CLANG_WARNINGS_END \
    IGNORE_WARNINGS_END \
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END \
    ALLOW_COMMA_END \
    ALLOW_DEPRECATED_DECLARATIONS_END \
    ALLOW_UNUSED_PARAMETERS_END

// Used to indicate that a class member has a specialized implementation in Swift. See
// "SwiftCXXThunk.h".
#define HAS_SWIFTCXX_THUNK  NS_REFINED_FOR_SWIFT

// This comment is incremented each time we add or remove a modulemap file, to force
// rebuild of all WTF's dependencies. This is a workaround for rdar://151920332.
// Current increment: 2.
