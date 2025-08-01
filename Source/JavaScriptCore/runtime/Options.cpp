/*
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "Options.h"

#include "CPU.h"
#include "JITOperationValidation.h"
#include "LLIntCommon.h"
#include "MacroAssembler.h"
#include "MinimumReservedZoneSize.h"
#include <algorithm>
#include <limits>
#include <mutex>
#include <stdlib.h>
#include <string.h>
#include <wtf/ASCIICType.h>
#include <wtf/BitSet.h>
#include <wtf/Compiler.h>
#include <wtf/DataLog.h>
#include <wtf/Gigacage.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/NumberOfCores.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TranslatedProcess.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/threads/Signals.h>

#if OS(DARWIN)
#include <wtf/darwin/OSLogPrintStream.h>
#endif

#if PLATFORM(COCOA)
#include <crt_externs.h>
#endif

#if ENABLE(JIT_CAGE)
#include <machine/cpu_capabilities.h>
#include <wtf/cocoa/Entitlements.h>
#endif

#if OS(LINUX)
#include <unistd.h>
extern "C" char **environ;
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

bool useOSLogOptionHasChanged = false;
Options::SandboxPolicy Options::machExceptionHandlerSandboxPolicy = Options::SandboxPolicy::Unknown;

namespace OptionsHelper {

// The purpose of Metadata is to hold transient info needed during initialization of
// Options. It will be released in Options::finalize(), and will not be kept during
// VM run time. For now, the only field it contains is a copy of Options defaults
// which are only used to provide more info for Options dumps.
struct Metadata {
    // This struct does not need to be TZONE_ALLOCATED because it is only used for transient memory
    // during Options initialization, and will not be re-allocated thereafter. See comment above.
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(Metadata);
public:
    OptionsStorage defaults;
};

static LazyNeverDestroyed<std::unique_ptr<Metadata>> g_metadata;
static LazyNeverDestroyed<WTF::BitSet<NumberOfOptions>> g_optionWasOverridden;

struct ConstMetaData {
    ASCIILiteral name;
    ASCIILiteral description;
    Options::Type type;
    Options::Availability availability;
    uint16_t offsetOfOption;
};

// Realize the names for each of the options:
static const ConstMetaData g_constMetaData[NumberOfOptions] = {
#define FILL_OPTION_INFO(type_, name_, defaultValue_, availability_, description_) \
    { #name_ ## _s, description_, Options::Type::type_, Options::Availability::availability_, offsetof(OptionsStorage, name_) },
    FOR_EACH_JSC_OPTION(FILL_OPTION_INFO)
#undef FILL_OPTION_INFO
};

class Option {
public:
    void dump(StringBuilder&) const;

    bool operator==(const Option&) const;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    ASCIILiteral name() const { return g_constMetaData[m_id].name; }
    ASCIILiteral description() const { return g_constMetaData[m_id].description; }
    Options::Type type() const { return g_constMetaData[m_id].type; }
    Options::Availability availability() const { return g_constMetaData[m_id].availability; }
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

    Option(Options::ID id, void* addressOfValue)
        : m_id(id)
    {
        initValue(addressOfValue);
    }

    void initValue(void* addressOfValue);

    Options::ID m_id;
    union {
        bool m_bool;
        unsigned m_unsigned;
        double m_double;
        int32_t m_int32;
        size_t m_size;
        OptionRange m_optionRange;
        const char* m_optionString;
        GCLogging::Level m_gcLogLevel;
        OSLogType m_osLogType;
    };
};

static void initialize()
{
    g_optionWasOverridden.construct();

    // Make a transient copy of the default option values into g_metadata before they get
    // modified. The defaults are only needed to provide more info when dumping options.
    // g_metadata will be released in Options::finalize() (see releaseMetadata()).
    g_metadata.construct();
    auto metadata = makeUnique<Metadata>();
    memcpy(&metadata->defaults, &g_jscConfig.options, sizeof(OptionsStorage));
    g_metadata.get() = WTFMove(metadata);
}

static void releaseMetadata()
{
    g_metadata.get() = nullptr;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

static const Option defaultFor(Options::ID id)
{
    auto offset = g_constMetaData[id].offsetOfOption;
    void* addressOfDefault = reinterpret_cast<uint8_t*>(&g_metadata.get()->defaults) + offset;
    return Option(id, addressOfDefault);
}

inline static void* addressOfOption(Options::ID id)
{
    auto offset = g_constMetaData[id].offsetOfOption;
    return reinterpret_cast<uint8_t*>(&g_jscConfig.options) + offset;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

static const Option optionFor(Options::ID id)
{
    return Option(id, addressOfOption(id));
}

inline static bool hasMetadata()
{
    return !!g_metadata.get();
}

inline static bool wasOverridden(Options::ID id)
{
    ASSERT(id < NumberOfOptions);
    return g_optionWasOverridden->get(id);
}

inline static void setWasOverridden(Options::ID id)
{
    ASSERT(id < NumberOfOptions);
    g_optionWasOverridden->set(id);
}

} // namespace OptionsHelper

template<typename T>
std::optional<T> parse(const char* string);

template<>
std::optional<OptionsStorage::Bool> parse(const char* string)
{
    auto span = unsafeSpan(string);
    if (equalLettersIgnoringASCIICase(span, "true"_s) || equalLettersIgnoringASCIICase(span, "yes"_s) || !strcmp(string, "1"))
        return true;
    if (equalLettersIgnoringASCIICase(span, "false"_s) || equalLettersIgnoringASCIICase(span, "no"_s) || !strcmp(string, "0"))
        return false;
    return std::nullopt;
}

template<>
std::optional<OptionsStorage::Int32> parse(const char* string)
{
    int32_t value;
    if (sscanf(string, "%d", &value) == 1)
        return value;
    return std::nullopt;
}

template<>
std::optional<OptionsStorage::Unsigned> parse(const char* string)
{
    unsigned value;
    if (sscanf(string, "%u", &value) == 1)
        return value;
    return std::nullopt;
}

#if CPU(ADDRESS64) || OS(DARWIN) || OS(HAIKU)
template<>
std::optional<OptionsStorage::Size> parse(const char* string)
{
    size_t value;
    if (sscanf(string, "%zu", &value) == 1)
        return value;
    return std::nullopt;
}
#endif // CPU(ADDRESS64) || OS(DARWIN) || OS(HAIKU)

template<>
std::optional<OptionsStorage::Double> parse(const char* string)
{
    double value;
    if (sscanf(string, "%lf", &value) == 1)
        return value;
    return std::nullopt;
}

template<>
std::optional<OptionsStorage::OptionRange> parse(const char* string)
{
    OptionRange range;
    if (range.init(string))
        return range;
    return std::nullopt;
}

template<>
std::optional<OptionsStorage::OptionString> parse(const char* string)
{
    const char* value = nullptr;
    if (!strlen(string))
        return value;

    // FIXME <https://webkit.org/b/169057>: This could leak if this option is set more than once.
    // Given that Options are typically used for testing, this isn't considered to be a problem.
    value = WTF::fastStrDup(string);
    return value;
}

template<>
std::optional<OptionsStorage::GCLogLevel> parse(const char* string)
{
    auto span = unsafeSpan(string);
    if (equalLettersIgnoringASCIICase(span, "none"_s) || equalLettersIgnoringASCIICase(span, "no"_s) || equalLettersIgnoringASCIICase(span, "false"_s) || !strcmp(string, "0"))
        return GCLogging::None;

    if (equalLettersIgnoringASCIICase(span, "basic"_s) || equalLettersIgnoringASCIICase(span, "yes"_s) || equalLettersIgnoringASCIICase(span, "true"_s) || !strcmp(string, "1"))
        return GCLogging::Basic;

    if (equalLettersIgnoringASCIICase(span, "verbose"_s) || !strcmp(string, "2"))
        return GCLogging::Verbose;

    return std::nullopt;
}

template<>
std::optional<OptionsStorage::OSLogType> parse(const char* string)
{
    std::optional<OptionsStorage::OSLogType> result;

    auto span = unsafeSpan(string);
    if (equalLettersIgnoringASCIICase(span, "none"_s) || equalLettersIgnoringASCIICase(span, "false"_s) || !strcmp(string, "0"))
        result = OSLogType::None;
    else if (equalLettersIgnoringASCIICase(span, "true"_s) || !strcmp(string, "1"))
        result = OSLogType::Error;
    else if (equalLettersIgnoringASCIICase(span, "default"_s))
        result = OSLogType::Default;
    else if (equalLettersIgnoringASCIICase(span, "info"_s))
        result = OSLogType::Info;
    else if (equalLettersIgnoringASCIICase(span, "debug"_s))
        result = OSLogType::Debug;
    else if (equalLettersIgnoringASCIICase(span, "error"_s))
        result = OSLogType::Error;
    else if (equalLettersIgnoringASCIICase(span, "fault"_s))
        result = OSLogType::Fault;

    if (result && result.value() != Options::useOSLog())
        useOSLogOptionHasChanged = true;
    return result;
}

#if OS(DARWIN)
static os_log_type_t asDarwinOSLogType(OSLogType type)
{
    switch (type) {
    case OSLogType::None:
        RELEASE_ASSERT_NOT_REACHED();
    case OSLogType::Default:
        return OS_LOG_TYPE_DEFAULT;
    case OSLogType::Info:
        return OS_LOG_TYPE_INFO;
    case OSLogType::Debug:
        return OS_LOG_TYPE_DEBUG;
    case OSLogType::Error:
        return OS_LOG_TYPE_ERROR;
    case OSLogType::Fault:
        return OS_LOG_TYPE_FAULT;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return OS_LOG_TYPE_DEFAULT;
}

static void initializeDatafileToUseOSLog()
{
    static bool alreadyInitialized = false;
    RELEASE_ASSERT(!alreadyInitialized);
    WTF::setDataFile(OSLogPrintStream::open("com.apple.JavaScriptCore", "DataLog", asDarwinOSLogType(Options::useOSLog())));
    alreadyInitialized = true;
    // Make sure no one jumped here for nefarious reasons...
    RELEASE_ASSERT(Options::useOSLog() != OSLogType::None);
}
#endif // OS(DARWIN)

static ASCIILiteral asString(OSLogType type)
{
    switch (type) {
    case OSLogType::None:
        return "none"_s;
    case OSLogType::Default:
        return "default"_s;
    case OSLogType::Info:
        return "info"_s;
    case OSLogType::Debug:
        return "debug"_s;
    case OSLogType::Error:
        return "error"_s;
    case OSLogType::Fault:
        return "fault"_s;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

bool Options::isAvailable(Options::ID id, Options::Availability availability)
{
    if (availability == Availability::Restricted)
        return g_jscConfig.restrictedOptionsEnabled;
    ASSERT(availability == Availability::Configurable);
    
    UNUSED_PARAM(id);
#if !defined(NDEBUG)
    if (id == maxSingleAllocationSizeID)
        return true;
#endif
    if (id == traceLLIntExecutionID)
        return !!LLINT_TRACING;
    if (id == traceLLIntSlowPathID)
        return !!LLINT_TRACING;
    if (id == traceWasmLLIntExecutionID)
        return !!LLINT_TRACING;

    if (id == validateVMEntryCalleeSavesID)
        return !!ASSERT_ENABLED;
    return false;
}

#if !PLATFORM(COCOA)

template<typename T>
bool overrideOptionWithHeuristic(T& variable, Options::ID id, const char* name, Options::Availability availability)
{
    bool available = (availability == Options::Availability::Normal)
        || Options::isAvailable(id, availability);

    const char* stringValue = getenv(name);
    if (!stringValue)
        return false;
    
    if (available) {
        std::optional<T> value = parse<T>(stringValue);
        if (value) {
            variable = value.value();
            return true;
        }
    }
    
    fprintf(stderr, "WARNING: failed to parse %s=%s\n", name, stringValue);
    return false;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

bool Options::overrideAliasedOptionWithHeuristic(const char* name)
{
    const char* stringValue = getenv(name);
    if (!stringValue)
        return false;

    auto aliasedOption = makeString(unsafeSpan(&name[4]), '=', unsafeSpan(stringValue));
    if (Options::setOption(aliasedOption.utf8().data()))
        return true;

    fprintf(stderr, "WARNING: failed to parse %s=%s\n", name, stringValue);
    return false;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // !PLATFORM(COCOA)

unsigned Options::computeNumberOfWorkerThreads(int maxNumberOfWorkerThreads, int minimum)
{
    int cpusToUse = std::min(kernTCSMAwareNumberOfProcessorCores(), maxNumberOfWorkerThreads);

    // Be paranoid, it is the OS we're dealing with, after all.
    ASSERT(cpusToUse >= 1);
    return std::max(cpusToUse, minimum);
}

int32_t Options::computePriorityDeltaOfWorkerThreads(int32_t twoCorePriorityDelta, int32_t multiCorePriorityDelta)
{
    if (kernTCSMAwareNumberOfProcessorCores() <= 2)
        return twoCorePriorityDelta;

    return multiCorePriorityDelta;
}

unsigned Options::computeNumberOfGCMarkers(unsigned maxNumberOfGCMarkers)
{
    return computeNumberOfWorkerThreads(maxNumberOfGCMarkers);
}

bool Options::defaultTCSMValue()
{
    return true;
}

const char* const OptionRange::s_nullRangeStr = "<null>";

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

bool OptionRange::init(const char* rangeString)
{
    // rangeString should be in the form of [!]<low>[:<high>]
    // where low and high are unsigned

    bool invert = false;

    if (!rangeString) {
        m_state = InitError;
        return false;
    }

    if (!strcmp(rangeString, s_nullRangeStr)) {
        m_state = Uninitialized;
        return true;
    }
    
    const char* p = rangeString;

    if (*p == '!') {
        invert = true;
        p++;
    }

    int scanResult = sscanf(p, " %u:%u", &m_lowLimit, &m_highLimit);

    if (!scanResult || scanResult == EOF) {
        m_state = InitError;
        return false;
    }

    if (scanResult == 1)
        m_highLimit = m_lowLimit;

    if (m_lowLimit > m_highLimit) {
        m_state = InitError;
        return false;
    }

    // FIXME <https://webkit.org/b/169057>: This could leak if this particular option is set more than once.
    // Given that these options are used for testing, this isn't considered to be problem.
    m_rangeString = WTF::fastStrDup(rangeString);
    m_state = invert ? Inverted : Normal;

    return true;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

bool OptionRange::isInRange(unsigned count) const
{
    if (m_state < Normal)
        return true;

    if ((m_lowLimit <= count) && (count <= m_highLimit))
        return m_state == Normal ? true : false;

    return m_state == Normal ? false : true;
}

void OptionRange::dump(PrintStream& out) const
{
    out.print(m_rangeString);
}

static void scaleJITPolicy()
{
    auto& scaleFactor = Options::jitPolicyScale();
    if (scaleFactor > 1.0)
        scaleFactor = 1.0;
    else if (scaleFactor < 0.0)
        scaleFactor = 0.0;

    auto scaleOption = [&] (int32_t& optionValue, int32_t minValue) {
        optionValue *= scaleFactor;
        optionValue = std::max(optionValue, minValue);
    };

    scaleOption(Options::thresholdForJITAfterWarmUp(), 0);
    scaleOption(Options::thresholdForJITSoon(), 0);
    scaleOption(Options::thresholdForOptimizeAfterWarmUp(), 1);
    scaleOption(Options::thresholdForOptimizeAfterLongWarmUp(), 1);
    scaleOption(Options::thresholdForOptimizeSoon(), 1);
    scaleOption(Options::thresholdForFTLOptimizeSoon(), 2);
    scaleOption(Options::thresholdForFTLOptimizeAfterWarmUp(), 2);

    scaleOption(Options::thresholdForBBQOptimizeAfterWarmUp(), 0);
    scaleOption(Options::thresholdForBBQOptimizeSoon(), 0);
    scaleOption(Options::thresholdForOMGOptimizeAfterWarmUp(), 1);
    scaleOption(Options::thresholdForOMGOptimizeSoon(), 1);
}

#if OS(DARWIN)
static void disableAllSignalHandlerBasedOptions();
#endif

static void overrideDefaults()
{
#if OS(DARWIN)
    if (Options::machExceptionHandlerSandboxPolicy == Options::SandboxPolicy::Block)
        disableAllSignalHandlerBasedOptions();
#endif

#if !PLATFORM(IOS_FAMILY)
    if (WTF::numberOfProcessorCores() < 4)
#endif
    {
        Options::maximumMutatorUtilization() = 0.6;
        Options::concurrentGCMaxHeadroom() = 1.4;
        Options::minimumGCPauseMS() = 1;
        Options::useStochasticMutatorScheduler() = false;
        if (WTF::numberOfProcessorCores() <= 1)
            Options::gcIncrementScale() = 1;
        else
            Options::gcIncrementScale() = 0;
    }

#if OS(DARWIN) && CPU(ARM64)
    Options::numberOfGCMarkers() = std::min<unsigned>(4, kernTCSMAwareNumberOfProcessorCores());

    Options::minNumberOfWorklistThreads() = 1;
    Options::maxNumberOfWorklistThreads() = std::min<unsigned>(3, kernTCSMAwareNumberOfProcessorCores());
    Options::numberOfBaselineCompilerThreads() = std::min<unsigned>(3, kernTCSMAwareNumberOfProcessorCores());
    Options::numberOfDFGCompilerThreads() = std::min<unsigned>(3, kernTCSMAwareNumberOfProcessorCores());
    Options::numberOfFTLCompilerThreads() = std::min<unsigned>(3, kernTCSMAwareNumberOfProcessorCores());
    Options::worklistLoadFactor() = 20;
    Options::worklistBaselineLoadWeight() = 2;
    Options::worklistDFGLoadWeight() = 5;
    // Set the FTL load weight equal to the load-factor so that a new thread is started for each FTL plan
    Options::worklistFTLLoadWeight() = 20;
#endif

#if OS(LINUX) && CPU(ARM)
    Options::maximumFunctionForCallInlineCandidateBytecodeCostForDFG() = 77;
    Options::maximumOptimizationCandidateBytecodeCost() = 42403;
    Options::maximumFunctionForClosureCallInlineCandidateBytecodeCostForDFG() = 68;
    Options::maximumInliningCallerBytecodeCost() = 9912;
    Options::maximumInliningDepth() = 8;
    Options::maximumInliningRecursion() = 3;
#endif

#if USE(BMALLOC_MEMORY_FOOTPRINT_API)
    // On iOS and conditionally Linux, we control heap growth using process memory footprint. Therefore these values can be agressive.
    Options::smallHeapRAMFraction() = 0.8;
    Options::mediumHeapRAMFraction() = 0.9;
#endif

#if !ENABLE(SIGNAL_BASED_VM_TRAPS)
    Options::usePollingTraps() = true;
#endif

#if !ENABLE(WEBASSEMBLY)
    Options::useWasmFastMemory() = false;
    Options::useWasmFaultSignalHandler() = false;
#endif

#if !HAVE(MACH_EXCEPTIONS)
    Options::useMachForExceptions() = false;
#endif

#if ASAN_ENABLED
    // This is a heuristic because ASAN builds are memory hogs in terms of stack frame usage.
    // So, we need a much larger ReservedZoneSize to allow stack overflow handlers to execute.
    Options::reservedZoneSize() = 3 * Options::reservedZoneSize();
#endif

#if PLATFORM(IOS_FAMILY)
    // This is used to mitigate performance regression rdar://150522186.
    if (Options::usePartialLoopUnrolling())
        Options::maxPartialLoopUnrollingBodyNodeSize() = 50;
#endif
}

bool Options::setAllJITCodeValidations(const char* valueStr)
{
    auto value = parse<OptionsStorage::Bool>(valueStr);
    if (!value)
        return false;
    setAllJITCodeValidations(value.value());
    return true;
}

void Options::setAllJITCodeValidations(bool value)
{
    Options::validateDFGClobberize() = value;
    Options::validateDFGExceptionHandling() = value;
    Options::validateDFGMayExit() = value;
    Options::validateDoesGC() = value;
    Options::useJITAsserts() = value;
}

static inline void disableAllWasmJITOptions()
{
    Options::useLLInt() = true;
    Options::useBBQJIT() = false;
    Options::useOMGJIT() = false;

    Options::useWasmSIMD() = false;

    Options::dumpWasmDisassembly() = false;
    Options::dumpBBQDisassembly() = false;
    Options::dumpOMGDisassembly() = false;
}

static inline void disableAllWasmOptions()
{
    disableAllWasmJITOptions();

    Options::useWasm() = false;
    Options::useWasmIPInt() = false;
    Options::useWasmLLInt() = false;
    Options::failToCompileWasmCode() = true;

    Options::useWasmFastMemory() = false;
    Options::useWasmFaultSignalHandler() = false;
    Options::numberOfWasmCompilerThreads() = 0;

    // SIMD is already disabled by JITOptions
    Options::useWasmRelaxedSIMD() = false;
    Options::useWasmTailCalls() = false;
}

static inline void disableAllJITOptions()
{
    Options::useLLInt() = true;
    Options::useJIT() = false;
    disableAllWasmJITOptions();

    Options::useBaselineJIT() = false;
    Options::useDFGJIT() = false;
    Options::useFTLJIT() = false;
    Options::useDOMJIT() = false;
    Options::useRegExpJIT() = false;
    Options::useJITCage() = false;
    Options::useConcurrentJIT() = false;

    Options::usePollingTraps() = true;

    Options::dumpDisassembly() = false;
    Options::asyncDisassembly() = false;
    Options::dumpBaselineDisassembly() = false;
    Options::dumpDFGDisassembly() = false;
    Options::dumpFTLDisassembly() = false;
    Options::dumpRegExpDisassembly() = false;
    Options::needDisassemblySupport() = false;
}

#if OS(DARWIN)
static void disableAllSignalHandlerBasedOptions()
{
    Options::usePollingTraps() = true;
    Options::useSharedArrayBuffer() = false;
    Options::useWasmFastMemory() = false;
    Options::useWasmFaultSignalHandler() = false;
}
#endif

void Options::executeDumpOptions()
{
    if (!Options::dumpOptions()) [[likely]]
        return;

    DumpLevel level = static_cast<DumpLevel>(Options::dumpOptions());
    if (level > DumpLevel::Verbose)
        level = DumpLevel::Verbose;

    ASCIILiteral title;
    switch (level) {
    case DumpLevel::None:
        break;
    case DumpLevel::Overridden:
        title = "Modified JSC options:"_s;
        break;
    case DumpLevel::All:
        title = "All JSC options:"_s;
        break;
    case DumpLevel::Verbose:
        title = "All JSC options with descriptions:"_s;
        break;
    }

    StringBuilder builder;
    dumpAllOptions(builder, level, title, nullptr, "   "_s, "\n"_s, DumpDefaults);
    dataLog(builder.toString());
}


WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

void Options::notifyOptionsChanged()
{
    AllowUnfinalizedAccessScope scope;

    unsigned thresholdForGlobalLexicalBindingEpoch = Options::thresholdForGlobalLexicalBindingEpoch();
    if (thresholdForGlobalLexicalBindingEpoch == 0 || thresholdForGlobalLexicalBindingEpoch == 1)
        Options::thresholdForGlobalLexicalBindingEpoch() = UINT_MAX;

#if !ENABLE(OFFLINE_ASM_ALT_ENTRY)
    if (Options::useGdbJITInfo())
        dataLogLn("useGdbJITInfo should be used with OFFLINE_ASM_ALT_ENTRY");
#endif

#if !ENABLE(JIT)
    Options::useJIT() = false;
#endif
#if !ENABLE(CONCURRENT_JS)
    Options::useConcurrentJIT() = false;
#endif
#if !ENABLE(YARR_JIT)
    Options::useRegExpJIT() = false;
#endif
#if !ENABLE(DFG_JIT)
    Options::useDFGJIT() = false;
    Options::useFTLJIT() = false;
#endif
#if !ENABLE(FTL_JIT)
    Options::useFTLJIT() = false;
#endif

#if CPU(RISCV64)
    // On RISCV64, JIT levels are enabled at build-time to simplify building JSC, avoiding
    // otherwise rare combinations of build-time configuration. FTL on RISCV64 is disabled
    // at runtime for now, until it gets int a proper working state.
    // https://webkit.org/b/239707
    Options::useFTLJIT() = false;
#endif

#if !CPU(X86_64) && !CPU(ARM64)
    Options::useConcurrentGC() = false;
    Options::forceUnlinkedDFG() = false;
    Options::useWasmSIMD() = false;
    Options::useWasmIPInt() = false;
#if !CPU(ARM_THUMB2)
    Options::useBBQJIT() = false;
#endif
#endif

#if !CPU(ARM64)
    Options::useRandomizingExecutableIslandAllocation() = false;
#endif

    Options::useDataICInFTL() = false; // Currently, it is not completed. Disable forcefully.
    Options::forceUnlinkedDFG() = false; // Currently, IC is rapidly changing. We disable this until we get the final form of Data IC.

    if (!Options::allowDoubleShape())
        Options::useJIT() = false; // We don't support JIT with !allowDoubleShape. So disable it.

    if (!Options::useWasm())
        disableAllWasmOptions();

    if (!Options::useJIT())
        disableAllWasmJITOptions();

    if (!Options::useWasmLLInt() && !Options::useWasmIPInt())
        Options::thresholdForBBQOptimizeAfterWarmUp() = 0; // Trigger immediate BBQ tier up.

    // At initialization time, we may decide that useJIT should be false for any
    // number of reasons (including failing to allocate JIT memory), and therefore,
    // will / should not be able to enable any JIT related services.
    if (!Options::useJIT()) {
        disableAllJITOptions();
#if OS(DARWIN)
        // If we don't know what the sandbox policy is on mach exception handler use is, we'll
        // take the default behavior of blocking its use if the JIT is disabled. JIT disablement
        // is a good proxy indicator for when mach exception handler use would also be blocked.
        if (machExceptionHandlerSandboxPolicy == SandboxPolicy::Unknown)
            disableAllSignalHandlerBasedOptions();
#endif
    } else {
        if (WTF::isX86BinaryRunningOnARM()) {
            Options::useBaselineJIT() = false;
            Options::useDFGJIT() = false;
            Options::useFTLJIT() = false;
        }

        if (Options::dumpDisassembly()
            || Options::asyncDisassembly()
            || Options::dumpBaselineDisassembly()
            || Options::dumpDFGDisassembly()
            || Options::dumpFTLDisassembly()
            || Options::dumpRegExpDisassembly()
            || Options::dumpWasmDisassembly()
            || Options::dumpBBQDisassembly()
            || Options::dumpOMGDisassembly())
            Options::needDisassemblySupport() = true;

        if (OptionsHelper::wasOverridden(jitPolicyScaleID))
            scaleJITPolicy();

        if (Options::forceEagerCompilation()) {
            Options::thresholdForJITAfterWarmUp() = 10;
            Options::thresholdForJITSoon() = 10;
            Options::thresholdForOptimizeAfterWarmUp() = 20;
            Options::thresholdForOptimizeAfterLongWarmUp() = 20;
            Options::thresholdForOptimizeSoon() = 20;
            Options::thresholdForFTLOptimizeAfterWarmUp() = 20;
            Options::thresholdForFTLOptimizeSoon() = 20;
            Options::maximumEvalCacheableSourceLength() = 150000;
            Options::useConcurrentJIT() = false;
        }

        // Compute the maximum value of the reoptimization retry counter. This is simply
        // the largest value at which we don't overflow the execute counter, when using it
        // to left-shift the execution counter by this amount. Currently the value ends
        // up being 18, so this loop is not so terrible; it probably takes up ~100 cycles
        // total on a 32-bit processor.
        Options::reoptimizationRetryCounterMax() = 0;
        while ((static_cast<int64_t>(Options::thresholdForOptimizeAfterLongWarmUp()) << (Options::reoptimizationRetryCounterMax() + 1)) <= static_cast<int64_t>(std::numeric_limits<int32_t>::max()))
            Options::reoptimizationRetryCounterMax()++;

        ASSERT((static_cast<int64_t>(Options::thresholdForOptimizeAfterLongWarmUp()) << Options::reoptimizationRetryCounterMax()) > 0);
        ASSERT((static_cast<int64_t>(Options::thresholdForOptimizeAfterLongWarmUp()) << Options::reoptimizationRetryCounterMax()) <= static_cast<int64_t>(std::numeric_limits<int32_t>::max()));

        if (isX86_64() && !isX86_64_AVX())
            Options::useWasmSIMD() = false;

        if (Options::forceAllFunctionsToUseSIMD() && !Options::useWasmSIMD())
            Options::forceAllFunctionsToUseSIMD() = false;

        if (Options::useWasmSIMD() && !(Options::useWasmLLInt() || Options::useWasmIPInt())) {
            // The LLInt is responsible for discovering if functions use SIMD.
            // If we can't run using it, then we should be conservative.
            Options::forceAllFunctionsToUseSIMD() = true;
        }
    }

    if (!Options::useConcurrentGC())
        Options::collectContinuously() = false;

    if (Options::useProfiler())
        Options::useConcurrentJIT() = false;

    if (Options::alwaysUseShadowChicken())
        Options::maximumInliningDepth() = 1;
        
#if !defined(NDEBUG)
    if (Options::maxSingleAllocationSize())
        fastSetMaxSingleAllocationSize(Options::maxSingleAllocationSize());
    else
        fastSetMaxSingleAllocationSize(std::numeric_limits<size_t>::max());
#endif

    if (Options::useZombieMode()) {
        Options::sweepSynchronously() = true;
        Options::scribbleFreeCells() = true;
    }

    if (Options::reservedZoneSize() < minimumReservedZoneSize)
        Options::reservedZoneSize() = minimumReservedZoneSize;
    if (Options::softReservedZoneSize() < Options::reservedZoneSize() + minimumReservedZoneSize)
        Options::softReservedZoneSize() = Options::reservedZoneSize() + minimumReservedZoneSize;

    if (!Options::useCodeCache())
        Options::diskCachePath() = nullptr;

    if (Options::randomIntegrityAuditRate() < 0)
        Options::randomIntegrityAuditRate() = 0;
    else if (Options::randomIntegrityAuditRate() > 1.0)
        Options::randomIntegrityAuditRate() = 1.0;

    if (!Options::allowUnsupportedTiers()) {
#define DISABLE_TIERS(option, flags, ...) do { \
            if (!Options::option())            \
                break;                         \
            if (!(flags & SupportsDFG))        \
                Options::useDFGJIT() = false;  \
            if (!(flags & SupportsFTL))        \
                Options::useFTLJIT() = false;  \
        } while (false);

        FOR_EACH_JSC_EXPERIMENTAL_OPTION(DISABLE_TIERS);
    }

#if OS(DARWIN)
    if (useOSLogOptionHasChanged) {
        initializeDatafileToUseOSLog();
        useOSLogOptionHasChanged = false;
    }
#endif

    if (Options::verboseVerifyGC())
        Options::verifyGC() = true;

#if ASAN_ENABLED && OS(LINUX)
    if (Options::useWasmFaultSignalHandler()) {
        const char* asanOptions = getenv("ASAN_OPTIONS");
        bool okToUseWasmFastMemory = asanOptions
            && (strstr(asanOptions, "allow_user_segv_handler=1") || strstr(asanOptions, "handle_segv=0"));
        if (!okToUseWasmFastMemory) {
            dataLogLn("WARNING: ASAN interferes with JSC signal handlers; useWasmFastMemory and useWasmFaultSignalHandler will be disabled.");
            Options::useWasmFaultSignalHandler() = false;
        }
    }
#endif

    // We can't use our pacibsp system while using posix signals because the signal handler could trash our stack during reifyInlinedCallFrames.
    // If we have JITCage we don't need to restrict ourselves to pacibsp.
    if (!Options::useMachForExceptions() || Options::useJITCage())
        Options::allowNonSPTagging() = true;

    if (!Options::useWasmFaultSignalHandler())
        Options::useWasmFastMemory() = false;

    if (Options::dumpOptimizationTracing()) {
        Options::printEachDFGFTLInlineCall() = true;
        Options::printEachUnrolledLoop() = true;
        // FIXME: Should support for OSR exit as well.
    }

#if CPU(ADDRESS32) || PLATFORM(PLAYSTATION)
    Options::useWasmFastMemory() = false;
#endif

    // Do range checks where needed and make corrections to the options:
    ASSERT(Options::thresholdForOptimizeAfterLongWarmUp() >= Options::thresholdForOptimizeAfterWarmUp());
    ASSERT(Options::thresholdForOptimizeAfterWarmUp() >= 0);
    ASSERT(Options::criticalGCMemoryThreshold() > 0.0 && Options::criticalGCMemoryThreshold() < 1.0);
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#if OS(WINDOWS)
// FIXME: Use equalLettersIgnoringASCIICase.
inline bool strncasecmp(const char* str1, const char* str2, size_t n)
{
    return _strnicmp(str1, str2, n);
}
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

void Options::initialize()
{
    static std::once_flag initializeOptionsOnceFlag;
    
    std::call_once(
        initializeOptionsOnceFlag,
        [] {
            AllowUnfinalizedAccessScope scope;

            // Sanity check that options address computation is working.
            RELEASE_ASSERT(OptionsHelper::addressOfOption(useKernTCSMID) ==  &Options::useKernTCSM());
            RELEASE_ASSERT(OptionsHelper::addressOfOption(gcMaxHeapSizeID) ==  &Options::gcMaxHeapSize());
            RELEASE_ASSERT(OptionsHelper::addressOfOption(forceOSRExitToLLIntID) ==  &Options::forceOSRExitToLLInt());

#if ENABLE(JSC_RESTRICTED_OPTIONS_BY_DEFAULT)
            Config::enableRestrictedOptions();
#endif

            // Initialize each of the options with their default values:
#define INIT_OPTION(type_, name_, defaultValue_, availability_, description_) { \
                name_() = defaultValue_; \
            }
            FOR_EACH_JSC_OPTION(INIT_OPTION)
#undef INIT_OPTION
            OptionsHelper::initialize();

            overrideDefaults();

            // Allow environment vars to override options if applicable.
            // The env var should be the name of the option prefixed with
            // "JSC_".
#if PLATFORM(COCOA) || OS(LINUX)
            bool hasBadOptions = false;
#if PLATFORM(COCOA)
            char** envp = *_NSGetEnviron();
#else
            char** envp = environ;
#endif

            for (; *envp; envp++) {
                const char* env = *envp;
                if (!strncmp("JSC_", env, 4)) {
                    if (!Options::setOption(&env[4])) {
                        dataLog("ERROR: invalid option: ", *envp, "\n");
                        hasBadOptions = true;
                    }
                }
            }
            if (hasBadOptions && Options::validateOptions())
                CRASH();
#endif // PLATFORM(COCOA) || OS(LINUX)

#if !PLATFORM(COCOA)
#define OVERRIDE_OPTION_WITH_HEURISTICS(type_, name_, defaultValue_, availability_, description_) \
            overrideOptionWithHeuristic(name_(), name_##ID, "JSC_" #name_, Availability::availability_);
            FOR_EACH_JSC_OPTION(OVERRIDE_OPTION_WITH_HEURISTICS)
#undef OVERRIDE_OPTION_WITH_HEURISTICS

#define OVERRIDE_ALIASED_OPTION_WITH_HEURISTICS(aliasedName_, unaliasedName_, equivalence_) \
            overrideAliasedOptionWithHeuristic("JSC_" #aliasedName_);
            FOR_EACH_JSC_ALIASED_OPTION(OVERRIDE_ALIASED_OPTION_WITH_HEURISTICS)
#undef OVERRIDE_ALIASED_OPTION_WITH_HEURISTICS

#endif // !PLATFORM(COCOA)

#if 0
                ; // Deconfuse editors that do auto indentation
#endif

#if CPU(X86_64) && OS(DARWIN)
            Options::dumpZappedCellCrashData() =
                (hwPhysicalCPUMax() >= 4) && (hwL3CacheSize() >= static_cast<int64_t>(6 * MB));
#endif

            // No more options changes after this point. notifyOptionsChanged() will
            // do sanity checks and fix up options as needed.
            notifyOptionsChanged();

            // The code below acts on options that have been finalized.
            // Do not change any options here.
#if HAVE(MACH_EXCEPTIONS)
            if (Options::useMachForExceptions())
                handleSignalsWithMach();
#endif

    });
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

void Options::finalize()
{
    ASSERT(!g_jscConfig.options.allowUnfinalizedAccess);
    g_jscConfig.options.isFinalized = true;

    // The following should only be done at the end after all options
    // have been initialized.
    assertOptionsAreCoherent();
    if (Options::dumpOptions()) [[unlikely]]
        executeDumpOptions();

#if USE(LIBPAS)
    if (Options::libpasForcePGMWithRate())
        WTF::forceEnablePGM(Options::libpasForcePGMWithRate());
#endif

    OptionsHelper::releaseMetadata();
}

static bool isSeparator(char c)
{
    return isUnicodeCompatibleASCIIWhitespace(c) || (c == ',');
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

bool Options::setOptions(const char* optionsStr)
{
    AllowUnfinalizedAccessScope scope;
    RELEASE_ASSERT(!g_jscConfig.isPermanentlyFrozen());
    Vector<char*> options;

    size_t length = strlen(optionsStr);
    char* optionsStrCopy = WTF::fastStrDup(optionsStr);
    char* end = optionsStrCopy + length;
    char* p = optionsStrCopy;

    while (p < end) {
        // Skip separators (white space or commas).
        while (p < end && isSeparator(*p))
            p++;
        if (p == end)
            break;

        char* optionStart = p;
        p = strchr(p, '=');
        if (!p) {
            dataLogF("'=' not found in option string: %p\n", optionStart);
            WTF::fastFree(optionsStrCopy);
            return false;
        }
        p++;

        char* valueBegin = p;
        bool hasStringValue = false;
        const int minStringLength = 2; // The min is an empty string i.e. 2 double quotes.
        if ((p + minStringLength < end) && (*p == '"')) {
            p = strstr(p + 1, "\"");
            if (!p) {
                dataLogF("Missing trailing '\"' in option string: %p\n", optionStart);
                WTF::fastFree(optionsStrCopy);
                return false; // End of string not found.
            }
            hasStringValue = true;
        }

        // Find next separator (white space or commas).
        while (p < end && !isSeparator(*p))
            p++;
        if (!p)
            p = end; // No more " " separator. Hence, this is the last arg.

        // If we have a well-formed string value, strip the quotes.
        if (hasStringValue) {
            char* valueEnd = p;
            ASSERT((*valueBegin == '"') && ((valueEnd - valueBegin) >= minStringLength) && (valueEnd[-1] == '"'));
            memmove(valueBegin, valueBegin + 1, valueEnd - valueBegin - minStringLength);
            valueEnd[-minStringLength] = '\0';
        }

        // Strip leading -- if present.
        if ((p -  optionStart > 2) && optionStart[0] == '-' && optionStart[1] == '-')
            optionStart += 2;

        *p++ = '\0';
        options.append(optionStart);
    }

    bool success = true;
    for (auto& option : options) {
        bool optionSuccess = setOption(option);
        if (!optionSuccess) {
            dataLogF("Failed to set option : %s\n", option);
            success = false;
        }
    }

    notifyOptionsChanged();
    WTF::fastFree(optionsStrCopy);

    return success;
}

// Parses a single command line option in the format "<optionName>=<value>"
// (no spaces allowed) and set the specified option if appropriate.
bool Options::setOptionWithoutAlias(const char* arg, bool verify)
{
    // arg should look like this:
    //   <jscOptionName>=<appropriate value>
    const char* equalStr = strchr(arg, '=');
    if (!equalStr)
        return false;

    const char* valueStr = equalStr + 1;

    // For each option, check if the specified arg is a match. If so, set the arg
    // if the value makes sense. Otherwise, move on to checking the next option.
#define SET_OPTION_IF_MATCH(type_, name_, defaultValue_, availability_, description_) \
    if (strlen(#name_) == static_cast<size_t>(equalStr - arg)      \
        && !strncasecmp(arg, #name_, equalStr - arg)) {            \
        if (Availability::availability_ != Availability::Normal    \
            && !isAvailable(name_##ID, Availability::availability_)) \
            return false;                                          \
        std::optional<OptionsStorage::type_> value;                \
        value = parse<OptionsStorage::type_>(valueStr);            \
        if (value) {                                               \
            OptionsHelper::setWasOverridden(name_##ID);            \
            name_() = value.value();                               \
            if (verify) notifyOptionsChanged();                    \
            return true;                                           \
        }                                                          \
        return false;                                              \
    }

    FOR_EACH_JSC_OPTION(SET_OPTION_IF_MATCH)
#undef SET_OPTION_IF_MATCH

    return false; // No option matched.
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

static ASCIILiteral invertBoolOptionValue(const char* valueStr)
{
    std::optional<OptionsStorage::Bool> value = parse<OptionsStorage::Bool>(valueStr);
    if (!value)
        return { };
    return value.value() ? "false"_s : "true"_s;
}


WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

bool Options::setAliasedOption(const char* arg, bool verify)
{
    // arg should look like this:
    //   <jscOptionName>=<appropriate value>
    const char* equalStr = strchr(arg, '=');
    if (!equalStr)
        return false;

    IGNORE_WARNINGS_BEGIN("tautological-compare")

    // For each option, check if the specify arg is a match. If so, set the arg
    // if the value makes sense. Otherwise, move on to checking the next option.
#define FOR_EACH_OPTION(aliasedName_, unaliasedName_, equivalence) \
    if (strlen(#aliasedName_) == static_cast<size_t>(equalStr - arg)    \
        && !strncasecmp(arg, #aliasedName_, equalStr - arg)) {          \
        auto unaliasedOption = String::fromLatin1(#unaliasedName_);     \
        if (equivalence == SameOption)                                  \
            unaliasedOption = makeString(unaliasedOption, unsafeSpan(equalStr)); \
        else {                                                          \
            ASSERT(equivalence == InvertedOption);                      \
            auto invertedValueStr = invertBoolOptionValue(equalStr + 1); \
            if (invertedValueStr.isNull())                                      \
                return false;                                           \
            unaliasedOption = makeString(unaliasedOption, '=', invertedValueStr); \
        }                                                               \
        return setOptionWithoutAlias(unaliasedOption.utf8().data(), verify);    \
    }

    FOR_EACH_JSC_ALIASED_OPTION(FOR_EACH_OPTION)
#undef FOR_EACH_OPTION

    IGNORE_WARNINGS_END

    return false; // No option matched.
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

bool Options::setOption(const char* arg, bool verify)
{
    AllowUnfinalizedAccessScope scope;
    bool success = setOptionWithoutAlias(arg, verify);
    if (success)
        return true;
    return setAliasedOption(arg, verify);
}


void Options::dumpAllOptions(StringBuilder& builder, DumpLevel level, ASCIILiteral title,
    ASCIILiteral separator, ASCIILiteral optionHeader, ASCIILiteral optionFooter, DumpDefaultsOption dumpDefaultsOption)
{
    AllowUnfinalizedAccessScope scope;
    if (!title.isNull()) {
        builder.append(title);
        builder.append('\n');
    }

    for (size_t id = 0; id < NumberOfOptions; ++id) {
        if (separator && id)
            builder.append(separator);
        dumpOption(builder, level, static_cast<ID>(id), optionHeader, optionFooter, dumpDefaultsOption);
    }
}

void Options::dumpAllOptionsInALine(StringBuilder& builder)
{
    dumpAllOptions(builder, DumpLevel::All, { }, " "_s, { }, { }, DontDumpDefaults);
}

void Options::dumpAllOptions(DumpLevel level, ASCIILiteral title)
{
    StringBuilder builder;
    dumpAllOptions(builder, level, title, { }, "   "_s, "\n"_s, DumpDefaults);
    dataLog(builder.toString().utf8().data());
}

void Options::dumpOption(StringBuilder& builder, DumpLevel level, Options::ID id,
    ASCIILiteral header, ASCIILiteral footer, DumpDefaultsOption dumpDefaultsOption)
{
    RELEASE_ASSERT(static_cast<size_t>(id) < NumberOfOptions);

    auto option = OptionsHelper::optionFor(id);
    Availability availability = option.availability();
    if (availability != Availability::Normal && !isAvailable(id, availability))
        return;

    bool wasOverridden = OptionsHelper::wasOverridden(id);
    bool needsDescription = (level == DumpLevel::Verbose && option.description());

    if (level == DumpLevel::Overridden && !wasOverridden)
        return;

    if (!header.isNull())
        builder.append(header);
    builder.append(option.name(), '=');
    option.dump(builder);

    if (wasOverridden && (dumpDefaultsOption == DumpDefaults) && OptionsHelper::hasMetadata()) {
        auto defaultOption = OptionsHelper::defaultFor(id);
        builder.append(" (default: "_s);
        defaultOption.dump(builder);
        builder.append(')');
    }

    if (needsDescription)
        builder.append("   ... "_s, option.description());

    builder.append(footer);
}

void Options::assertOptionsAreCoherent()
{
    AllowUnfinalizedAccessScope scope;
    bool coherent = true;
    if (!(useLLInt() || useJIT())) {
        coherent = false;
        dataLog("INCOHERENT OPTIONS: at least one of useLLInt or useJIT must be true\n");
    }
    if (useWasm() && !(useWasmIPInt() || useWasmLLInt() || useBBQJIT())) {
        coherent = false;
        dataLog("INCOHERENT OPTIONS: at least one of useWasmIPInt, useWasmLLInt, or useBBQJIT must be true\n");
    }
    if (useProfiler() && useConcurrentJIT()) {
        coherent = false;
        dataLogLn("Bytecode profiler is not concurrent JIT safe.");
    }
    if (!allowNonSPTagging() && !useMachForExceptions()) {
        coherent = false;
        dataLog("INCOHERENT OPTIONS: can't restrict pointer tagging to pacibsp and use posix signals");
    }

    if (!coherent)
        CRASH();
}

namespace OptionsHelper {

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

void Option::initValue(void* addressOfValue)
{
    Options::Type type = g_constMetaData[m_id].type;
    switch (type) {
    case Options::Type::Bool:
        memcpy(&m_bool, addressOfValue, sizeof(OptionsStorage::Bool));
        break;
    case Options::Type::Unsigned:
        memcpy(&m_unsigned, addressOfValue, sizeof(OptionsStorage::Unsigned));
        break;
    case Options::Type::Double:
        memcpy(&m_double, addressOfValue, sizeof(OptionsStorage::Double));
        break;
    case Options::Type::Int32:
        memcpy(&m_int32, addressOfValue, sizeof(OptionsStorage::Int32));
        break;
    case Options::Type::Size:
        memcpy(&m_size, addressOfValue, sizeof(OptionsStorage::Size));
        break;
    case Options::Type::OptionRange:
        memcpy(&m_optionRange, addressOfValue, sizeof(OptionsStorage::OptionRange));
        break;
    case Options::Type::OptionString:
        memcpy(&m_optionString, addressOfValue, sizeof(OptionsStorage::OptionString));
        break;
    case Options::Type::GCLogLevel:
        memcpy(&m_gcLogLevel, addressOfValue, sizeof(OptionsStorage::GCLogLevel));
        break;
    case Options::Type::OSLogType:
        memcpy(&m_osLogType, addressOfValue, sizeof(OptionsStorage::OSLogType));
        break;
    }
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

void Option::dump(StringBuilder& builder) const
{
    switch (type()) {
    case Options::Type::Bool:
        builder.append(m_bool ? "true"_s : "false"_s);
        break;
    case Options::Type::Unsigned:
        builder.append(m_unsigned);
        break;
    case Options::Type::Size:
        builder.append(m_size);
        break;
    case Options::Type::Double:
        builder.append(m_double);
        break;
    case Options::Type::Int32:
        builder.append(m_int32);
        break;
    case Options::Type::OptionRange:
        builder.append(unsafeSpan(m_optionRange.rangeString()));
        break;
    case Options::Type::OptionString:
        builder.append('"', m_optionString ? unsafeSpan8(m_optionString) : ""_span8, '"');
        break;
    case Options::Type::GCLogLevel:
        builder.append(m_gcLogLevel);
        break;
    case Options::Type::OSLogType:
        builder.append(asString(m_osLogType));
        break;
    }
}

bool Option::operator==(const Option& other) const
{
    ASSERT(type() == other.type());
    switch (type()) {
    case Options::Type::Bool:
        return m_bool == other.m_bool;
    case Options::Type::Unsigned:
        return m_unsigned == other.m_unsigned;
    case Options::Type::Size:
        return m_size == other.m_size;
    case Options::Type::Double:
        return (m_double == other.m_double) || (std::isnan(m_double) && std::isnan(other.m_double));
    case Options::Type::Int32:
        return m_int32 == other.m_int32;
    case Options::Type::OptionRange:
        return m_optionRange.rangeString() == other.m_optionRange.rangeString();
    case Options::Type::OptionString:
        return (m_optionString == other.m_optionString)
            || (m_optionString && other.m_optionString && !strcmp(m_optionString, other.m_optionString));
    case Options::Type::GCLogLevel:
        return m_gcLogLevel == other.m_gcLogLevel;
    case Options::Type::OSLogType:
        return m_osLogType == other.m_osLogType;
    }
    return false;
}

} // namespace OptionsHelper

#if ENABLE(JIT_CAGE)
SUPPRESS_ASAN bool canUseJITCage()
{
    if (JSC_FORCE_USE_JIT_CAGE)
        return true;
    return JSC_JIT_CAGE_VERSION() && !ASAN_ENABLED && WTF::processHasEntitlement("com.apple.private.verified-jit"_s);
}
#else
bool canUseJITCage() { return false; }
#endif

bool canUseHandlerIC()
{
#if USE(JSVALUE64)
    return true;
#else
    return false;
#endif
}

bool canUseWasm()
{
#if ENABLE(WEBASSEMBLY) && !PLATFORM(WATCHOS)
    return true;
#else
    return false;
#endif
}

bool hasCapacityToUseLargeGigacage()
{
    // Gigacage::hasCapacityToUseLargeGigacage is determined based on EFFECTIVE_ADDRESS_WIDTH.
    // If we have enough address range to potentially use a large gigacage,
    // then we have enough address range to useWasmFastMemory.
    return Gigacage::hasCapacityToUseLargeGigacage;
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
