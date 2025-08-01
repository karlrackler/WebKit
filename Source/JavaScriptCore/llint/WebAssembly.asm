# Copyright (C) 2019-2024 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

# Calling conventions
const CalleeSaveSpaceAsVirtualRegisters = constexpr Wasm::numberOfLLIntCalleeSaveRegisters + constexpr Wasm::numberOfLLIntInternalRegisters
const CalleeSaveSpaceStackAligned = (CalleeSaveSpaceAsVirtualRegisters * SlotSize + StackAlignment - 1) & ~StackAlignmentMask
const WasmEntryPtrTag = constexpr WasmEntryPtrTag
const UnboxedWasmCalleeStackSlot = CallerFrame - constexpr Wasm::numberOfLLIntCalleeSaveRegisters * SlotSize - MachineRegisterSize
const WasmToJSScratchSpaceSize = constexpr Wasm::WasmToJSScratchSpaceSize
const WasmToJSCallableFunctionSlot = constexpr Wasm::WasmToJSCallableFunctionSlot

if HAVE_FAST_TLS
    const WTF_WASM_CONTEXT_KEY = constexpr WTF_WASM_CONTEXT_KEY
end

# Must match GPRInfo.h
if X86_64
    const NumberOfWasmArgumentJSRs = 6
elsif ARM64 or ARM64E or RISCV64
    const NumberOfWasmArgumentJSRs = 8
elsif ARMv7
    const NumberOfWasmArgumentJSRs = 2
else
    error
end

const NumberOfWasmArgumentFPRs = 8

const NumberOfWasmArguments = NumberOfWasmArgumentJSRs + NumberOfWasmArgumentFPRs

# All callee saves must match the definition in WasmCallee.cpp

# These must match the definition in GPRInfo.h
if X86_64 or ARM64 or ARM64E or RISCV64
    const wasmInstance = csr0
    const memoryBase = csr3
    const boundsCheckingSize = csr4
elsif ARMv7
    const wasmInstance = csr0
    const memoryBase = invalidGPR
    const boundsCheckingSize = invalidGPR
else
    error
end

# This must match the definition in LowLevelInterpreter.asm
if X86_64
    const PB = csr2
elsif ARM64 or ARM64E or RISCV64
    const PB = csr7
elsif ARMv7
    const PB = csr1
else
    error
end

# Helper macros

# On JSVALUE64, each 64-bit argument GPR holds one whole Wasm value.
# On JSVALUE32_64, a consecutive pair of even/odd numbered GPRs hold a single
# Wasm value (even if that value is i32/f32, the odd numbered GPR holds the
# more significant word).
macro forEachArgumentJSR(fn)
    if ARM64 or ARM64E
        fn(0 * 8, wa0, wa1)
        fn(2 * 8, wa2, wa3)
        fn(4 * 8, wa4, wa5)
        fn(6 * 8, wa6, wa7)
    elsif JSVALUE64
        fn(0 * 8, wa0)
        fn(1 * 8, wa1)
        fn(2 * 8, wa2)
        fn(3 * 8, wa3)
        fn(4 * 8, wa4)
        fn(5 * 8, wa5)
    else
        fn(0 * 8, wa1, wa0)
        fn(1 * 8, wa3, wa2)
    end
end

macro forEachArgumentFPR(fn)
    if ARM64 or ARM64E
        fn((NumberOfWasmArgumentJSRs + 0) * 8, wfa0, wfa1)
        fn((NumberOfWasmArgumentJSRs + 2) * 8, wfa2, wfa3)
        fn((NumberOfWasmArgumentJSRs + 4) * 8, wfa4, wfa5)
        fn((NumberOfWasmArgumentJSRs + 6) * 8, wfa6, wfa7)
    else
        fn((NumberOfWasmArgumentJSRs + 0) * 8, wfa0)
        fn((NumberOfWasmArgumentJSRs + 1) * 8, wfa1)
        fn((NumberOfWasmArgumentJSRs + 2) * 8, wfa2)
        fn((NumberOfWasmArgumentJSRs + 3) * 8, wfa3)
        fn((NumberOfWasmArgumentJSRs + 4) * 8, wfa4)
        fn((NumberOfWasmArgumentJSRs + 5) * 8, wfa5)
        fn((NumberOfWasmArgumentJSRs + 6) * 8, wfa6)
        fn((NumberOfWasmArgumentJSRs + 7) * 8, wfa7)
    end
end

# FIXME: Eventually this should be unified with the JS versions
# https://bugs.webkit.org/show_bug.cgi?id=203656

macro wasmDispatch(advanceReg)
    addp advanceReg, PC
    wasmNextInstruction()
end

macro wasmDispatchIndirect(offsetReg)
    wasmDispatch(offsetReg)
end

macro wasmNextInstruction()
    loadb [PB, PC, 1], t0
    leap _g_opcodeMap, t1
    jmp NumberOfJSOpcodeIDs * PtrSize[t1, t0, PtrSize], BytecodePtrTag, AddressDiversified
end

macro wasmNextInstructionWide16()
    loadb OpcodeIDNarrowSize[PB, PC, 1], t0
    leap _g_opcodeMapWide16, t1
    jmp NumberOfJSOpcodeIDs * PtrSize[t1, t0, PtrSize], BytecodePtrTag, AddressDiversified
end

macro wasmNextInstructionWide32()
    loadb OpcodeIDNarrowSize[PB, PC, 1], t0
    leap _g_opcodeMapWide32, t1
    jmp NumberOfJSOpcodeIDs * PtrSize[t1, t0, PtrSize], BytecodePtrTag, AddressDiversified
end

macro checkSwitchToJIT(increment, action)
    loadp UnboxedWasmCalleeStackSlot[cfr], ws0
    baddis increment, Wasm::LLIntCallee::m_tierUpCounter + Wasm::LLIntTierUpCounter::m_counter[ws0], .continue
    action()
    .continue:
end

macro checkSwitchToJITForPrologue(codeBlockRegister)
    if WEBASSEMBLY_BBQJIT or WEBASSEMBLY_OMGJIT
    checkSwitchToJIT(
        5,
        macro()
            move cfr, a0
            move PC, a1
            move wasmInstance, a2
            cCall4(_slow_path_wasm_prologue_osr)
            btpz r0, .recover
            move r0, ws0

if ARM64 or ARM64E
            forEachArgumentJSR(macro (offset, gpr1, gpr2)
                loadpairq -offset - 16 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpr2, gpr1
            end)
elsif JSVALUE64
            forEachArgumentJSR(macro (offset, gpr)
                loadq -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpr
            end)
else
            forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
                load2ia -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpLsw, gprMsw
            end)
end
if ARM64 or ARM64E
            forEachArgumentFPR(macro (offset, fpr1, fpr2)
                loadpaird -offset - 16 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], fpr2, fpr1
            end)
else
            forEachArgumentFPR(macro (offset, fpr)
                loadd -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], fpr
            end)
end

            restoreCalleeSavesUsedByWasm()
            restoreCallerPCAndCFR()
            if ARM64E
                leap _g_config, ws1
                jmp JSCConfigGateMapOffset + (constexpr Gate::wasmOSREntry) * PtrSize[ws1], NativeToJITGatePtrTag # WasmEntryPtrTag
            else
                jmp ws0, WasmEntryPtrTag
            end
        .recover:
            loadp UnboxedWasmCalleeStackSlot[cfr], codeBlockRegister
        end)
    end
end

macro checkSwitchToJITForLoop()
    if WEBASSEMBLY_BBQJIT or WEBASSEMBLY_OMGJIT
    checkSwitchToJIT(
        1,
        macro()
            storei PC, CallSiteIndex[cfr]
            prepareStateForCCall()
            move cfr, a0
            move PC, a1
            move wasmInstance, a2
            cCall4(_slow_path_wasm_loop_osr)
            btpz r1, .recover
            restoreCalleeSavesUsedByWasm()
            restoreCallerPCAndCFR()
            move r0, a0
            if ARM64E
                move r1, ws0
                leap _g_config, ws1
                jmp JSCConfigGateMapOffset + (constexpr Gate::wasmOSREntry) * PtrSize[ws1], NativeToJITGatePtrTag # WasmEntryPtrTag
            else
                jmp r1, WasmEntryPtrTag
            end
        .recover:
            loadi CallSiteIndex[cfr], PC
        end)
    end
end

macro checkSwitchToJITForEpilogue()
    if WEBASSEMBLY_BBQJIT or WEBASSEMBLY_OMGJIT
    checkSwitchToJIT(
        10,
        macro ()
            callWasmSlowPath(_slow_path_wasm_epilogue_osr)
        end)
    end
end

# Wasm specific helpers

macro preserveCalleeSavesUsedByWasm()
    # NOTE: We intentionally don't save memoryBase and boundsCheckingSize here. See the comment
    # in restoreCalleeSavesUsedByWasm() below for why.
    subp CalleeSaveSpaceStackAligned, sp
    if ARM64 or ARM64E
        storepairq wasmInstance, PB, -16[cfr]
    elsif X86_64 or RISCV64
        storep PB, -0x8[cfr]
        storep wasmInstance, -0x10[cfr]
    elsif ARMv7
        storep PB, -4[cfr]
        storep wasmInstance, -8[cfr]
    else
        error
    end
end

macro restoreCalleeSavesUsedByWasm()
    # NOTE: We intentionally don't restore memoryBase and boundsCheckingSize here. These are saved
    # and restored when entering Wasm by the JSToWasm wrapper and changes to them are meant
    # to be observable within the same Wasm module.
    if ARM64 or ARM64E
        loadpairq -16[cfr], wasmInstance, PB
    elsif X86_64 or RISCV64
        loadp -0x8[cfr], PB
        loadp -0x10[cfr], wasmInstance
    elsif ARMv7
        loadp -4[cfr], PB
        loadp -8[cfr], wasmInstance
    else
        error
    end
end

macro preserveGPRsUsedByTailCall(gpr0, gpr1)
    storep gpr0, ThisArgumentOffset[sp]
    storep gpr1, ArgumentCountIncludingThis[sp]
end

macro restoreGPRsUsedByTailCall(gpr0, gpr1)
    loadp ThisArgumentOffset[sp], gpr0
    loadp ArgumentCountIncludingThis[sp], gpr1
end

macro preserveReturnAddress(scratch)
if X86_64
    loadp ReturnPC[cfr], scratch
    storep scratch, ReturnPC[sp]
elsif ARM64 or ARM64E or ARMv7 or RISCV64
    loadp ReturnPC[cfr], lr
end
end

macro usePreviousFrame()
    if ARM64 or ARM64E
        loadpairq -PtrSize[cfr], PB, cfr
    elsif ARMv7 or X86_64 or RISCV64
        loadp -PtrSize[cfr], PB
        loadp [cfr], cfr
    else
        error
    end
end

macro reloadMemoryRegistersFromInstance(instance, scratch1)
if not ARMv7
    loadp JSWebAssemblyInstance::m_cachedMemory[instance], memoryBase
    loadp JSWebAssemblyInstance::m_cachedBoundsCheckingSize[instance], boundsCheckingSize
    cagedPrimitiveMayBeNull(memoryBase, scratch1) # If boundsCheckingSize is 0, pointer can be a nullptr.
end
end

macro throwException(exception)
    storei constexpr Wasm::ExceptionType::%exception%, ArgumentCountIncludingThis + PayloadOffset[cfr]
    jmp _wasm_throw_from_slow_path_trampoline
end

macro callWasmSlowPath(slowPath)
    storei PC, CallSiteIndex[cfr]
    prepareStateForCCall()
    move cfr, a0
    move PC, a1
    move wasmInstance, a2
    cCall3(slowPath)
    restoreStateAfterCCall()
end

macro callWasmCallSlowPath(slowPath, action)
    storei PC, CallSiteIndex[cfr]
    prepareStateForCCall()
    move cfr, a0
    move PC, a1
    move wasmInstance, a2
    cCall3(slowPath)
    action(r0, r1)
end

macro restoreStackPointerAfterCall()
    loadp UnboxedWasmCalleeStackSlot[cfr], ws1
    loadi Wasm::LLIntCallee::m_numCalleeLocals[ws1], ws1
    lshiftp 3, ws1
    addp maxFrameExtentForSlowPathCall, ws1
if ARMv7
    subp cfr, ws1, ws1
    move ws1, sp
else
    subp cfr, ws1, sp
end
end

macro wasmPrologue()
    # Set up the call frame and check if we should OSR.
    preserveCallerPCAndCFR()
    preserveCalleeSavesUsedByWasm()
    reloadMemoryRegistersFromInstance(wasmInstance, ws0)

    storep wasmInstance, CodeBlock[cfr]
    loadp Callee[cfr], ws0
if JSVALUE64
    andp ~(constexpr JSValue::NativeCalleeTag), ws0
end
    leap WTFConfig + constexpr WTF::offsetOfWTFConfigLowestAccessibleAddress, ws1
    loadp [ws1], ws1
    addp ws1, ws0
    storep ws0, UnboxedWasmCalleeStackSlot[cfr]

    # Get new sp in ws1 and check stack height.
    loadi Wasm::LLIntCallee::m_numCalleeLocals[ws0], ws1
    lshiftp 3, ws1
    addp maxFrameExtentForSlowPathCall, ws1
    subp cfr, ws1, ws1

if not JSVALUE64
    subp 8, ws1 # align stack pointer
end

if TRACING
    probe(
         macro()
             move cfr, a1
             move ws1, a2
             call _logWasmPrologue
         end
     )
end

if not ADDRESS64
    bpa ws1, cfr, .stackOverflow
end
    bplteq JSWebAssemblyInstance::m_softStackLimit[wasmInstance], ws1, .stackHeightOK

.stackOverflow:
    throwException(StackOverflow)

.stackHeightOK:
    move ws1, sp

if ARM64 or ARM64E
    forEachArgumentJSR(macro (offset, gpr1, gpr2)
        storepairq gpr2, gpr1, -offset - 16 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)
elsif JSVALUE64
    forEachArgumentJSR(macro (offset, gpr)
        storeq gpr, -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)
else
    forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
        store2ia gpLsw, gprMsw, -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)
end
if ARM64 or ARM64E
    forEachArgumentFPR(macro (offset, fpr1, fpr2)
        storepaird fpr2, fpr1, -offset - 16 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)
else
    forEachArgumentFPR(macro (offset, fpr)
        stored fpr, -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)
end

    loadp UnboxedWasmCalleeStackSlot[cfr], ws0
    checkSwitchToJITForPrologue(ws0)

    # Set up the PC.
    loadp Wasm::LLIntCallee::m_instructionsRawPointer[ws0], PB
    move 0, PC

    loadi Wasm::LLIntCallee::m_numVars[ws0], ws1
    subi NumberOfWasmArguments + CalleeSaveSpaceAsVirtualRegisters, ws1
    btiz ws1, .zeroInitializeLocalsDone

    lshifti 3, ws1
    negi ws1

if JSVALUE64
    sxi2q ws1, ws1
end
    leap (NumberOfWasmArguments + CalleeSaveSpaceAsVirtualRegisters + 1) * -8[cfr], ws0
.zeroInitializeLocalsLoop:
    addp PtrSize, ws1
    storep 0, [ws0, ws1]
    btpnz ws1, .zeroInitializeLocalsLoop
.zeroInitializeLocalsDone:
end

macro forEachVectorArgument(fn)
     const base = NumberOfWasmArgumentJSRs * 8
     fn(base + 0 * 16, -(base + 0 * 16 + 8), wfa0)
     fn(base + 1 * 16, -(base + 1 * 16 + 8), wfa1)
     fn(base + 2 * 16, -(base + 2 * 16 + 8), wfa2)
     fn(base + 3 * 16, -(base + 3 * 16 + 8), wfa3)
     fn(base + 4 * 16, -(base + 4 * 16 + 8), wfa4)
     fn(base + 5 * 16, -(base + 5 * 16 + 8), wfa5)
     fn(base + 6 * 16, -(base + 6 * 16 + 8), wfa6)
     fn(base + 7 * 16, -(base + 7 * 16 + 8), wfa7)
end

# Tier up immediately, while saving full vectors in argument FPRs
macro wasmPrologueSIMD(slow_path)
if (WEBASSEMBLY_BBQJIT or WEBASSEMBLY_OMGJIT) and not ARMv7
    preserveCallerPCAndCFR()
    preserveCalleeSavesUsedByWasm()
    reloadMemoryRegistersFromInstance(wasmInstance, ws0)

    storep wasmInstance, CodeBlock[cfr]
    loadp Callee[cfr], ws0
if JSVALUE64
    andp ~(constexpr JSValue::NativeCalleeTag), ws0
end
    leap WTFConfig + constexpr WTF::offsetOfWTFConfigLowestAccessibleAddress, ws1
    loadp [ws1], ws1
    addp ws1, ws0
    storep ws0, UnboxedWasmCalleeStackSlot[cfr]

    # Get new sp in ws1 and check stack height.
    # This should match the calculation of m_stackSize, but with double the size for fpr arg storage and no locals.
    move 8 + 8 * 2 + constexpr CallFrame::headerSizeInRegisters + 1, ws1
    lshiftp 3, ws1
    addp maxFrameExtentForSlowPathCall, ws1
    subp cfr, ws1, ws1

if not JSVALUE64
    subp 8, ws1 # align stack pointer
end

if not ADDRESS64
    bpa ws1, cfr, .stackOverflow
end
    bplteq JSWebAssemblyInstance::m_softStackLimit[wasmInstance], ws1, .stackHeightOK

.stackOverflow:
    throwException(StackOverflow)

.stackHeightOK:
    move ws1, sp

if ARM64 or ARM64E
    forEachArgumentJSR(macro (offset, gpr1, gpr2)
        storepairq gpr2, gpr1, -offset - 16 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)
elsif JSVALUE64
    forEachArgumentJSR(macro (offset, gpr)
        storeq gpr, -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)
else
    forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
        store2ia gpLsw, gprMsw, -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)
end

    forEachVectorArgument(macro (offset, negOffset, fpr)
        storev fpr, negOffset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)

    slow_path()
    move r0, ws0

if ARM64 or ARM64E
    forEachArgumentJSR(macro (offset, gpr1, gpr2)
        loadpairq -offset - 16 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpr2, gpr1
    end)
elsif JSVALUE64
    forEachArgumentJSR(macro (offset, gpr)
        loadq -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpr
    end)
else
    forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
        load2ia -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpLsw, gprMsw
    end)
end

    forEachVectorArgument(macro (offset, negOffset, fpr)
        loadv negOffset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], fpr
    end)

    restoreCalleeSavesUsedByWasm()
    restoreCallerPCAndCFR()
    if ARM64E
        leap _g_config, ws1
        jmp JSCConfigGateMapOffset + (constexpr Gate::wasmOSREntry) * PtrSize[ws1], NativeToJITGatePtrTag # WasmEntryPtrTag
    else
        jmp ws0, WasmEntryPtrTag
    end
end
    break
end

if ARMv7
macro branchIfWasmException(exceptionTarget)
    loadp CodeBlock[cfr], t3
    loadp JSWebAssemblyInstance::m_vm[t3], t3
    btpz VM::m_exception[t3], .noException
    jmp exceptionTarget
.noException:
end
end

macro zeroExtend32ToWord(r)
    if JSVALUE64
        andq 0xffffffff, r
    end
end

macro boxInt32(r, rTag)
    if JSVALUE64
        orq constexpr JSValue::NumberTag, r
    else
        move constexpr JSValue::Int32Tag, rTag
    end
end

// If you change this, make sure to modify JSToWasm.cpp:createJSToWasmJITShared
op(js_to_wasm_wrapper_entry, macro ()
    if not WEBASSEMBLY or C_LOOP
        error
    end

    macro clobberVolatileRegisters()
        if ARM64 or ARM64E
            emit "movz  x9, #0xBAD"
            emit "movz x10, #0xBAD"
            emit "movz x11, #0xBAD"
            emit "movz x12, #0xBAD"
            emit "movz x13, #0xBAD"
            emit "movz x14, #0xBAD"
            emit "movz x15, #0xBAD"
            emit "movz x16, #0xBAD"
            emit "movz x17, #0xBAD"
            emit "movz x18, #0xBAD"
        elsif ARMv7
            emit "mov r4, #0xBAD"
            emit "mov r5, #0xBAD"
            emit "mov r6, #0xBAD"
            emit "mov r8, #0xBAD"
            emit "mov r9, #0xBAD"
            emit "mov r12, #0xBAD"
        end
    end

    macro repeat(scratch, f)
        move 0xBEEF, scratch
        f(0)
        f(1)
        f(2)
        f(3)
        f(4)
        f(5)
        f(6)
        f(7)
        f(8)
        f(9)
        f(10)
        f(11)
        f(12)
        f(13)
        f(14)
        f(15)
        f(16)
        f(17)
        f(18)
        f(19)
        f(20)
        f(21)
        f(22)
        f(23)
        f(24)
        f(25)
        f(26)
        f(27)
        f(28)
        f(29)
    end

    macro saveJSEntrypointRegisters()
        subp constexpr Wasm::JSEntrypointCallee::SpillStackSpaceAligned, sp
        if ARM64 or ARM64E
            storepairq memoryBase, boundsCheckingSize, -2 * SlotSize[cfr]
            storep wasmInstance, -3 * SlotSize[cfr]
        elsif X86_64
            # These must match the wasmToJS thunk, since the unwinder won't be able to tell who made this frame.
            storep boundsCheckingSize, -1 * SlotSize[cfr]
            storep memoryBase, -2 * SlotSize[cfr]
            storep wasmInstance, -3 * SlotSize[cfr]
        else
            storei wasmInstance, -1 * SlotSize[cfr]
        end
    end

    macro restoreJSEntrypointRegisters()
        if ARM64 or ARM64E
            loadpairq -2 * SlotSize[cfr], memoryBase, boundsCheckingSize
            loadp -3 * SlotSize[cfr], wasmInstance
        elsif X86_64
            loadp -1 * SlotSize[cfr], boundsCheckingSize
            loadp -2 * SlotSize[cfr], memoryBase
            loadp -3 * SlotSize[cfr], wasmInstance
        else
            loadi -1 * SlotSize[cfr], wasmInstance
        end
        addp constexpr Wasm::JSEntrypointCallee::SpillStackSpaceAligned, sp
    end

    macro getWebAssemblyFunctionAndsetNativeCalleeAndInstance(webAssemblyFunctionOut, scratch)
        # Re-load WebAssemblyFunction Callee
        loadp Callee[cfr], webAssemblyFunctionOut

        # Replace the WebAssemblyFunction Callee with our JSToWasm NativeCallee
        loadp WebAssemblyFunction::m_boxedJSToWasmCallee[webAssemblyFunctionOut], scratch
        storep scratch, Callee[cfr] # JSToWasmCallee
        if not JSVALUE64
            move constexpr JSValue::NativeCalleeTag, scratch
            storep scratch, TagOffset + Callee[cfr]
        end
        storep wasmInstance, CodeBlock[cfr]
    end

if ASSERT_ENABLED
    clobberVolatileRegisters()
end

    tagReturnAddress sp
    preserveCallerPCAndCFR()
    saveJSEntrypointRegisters()

    # Load data from the entry callee
    # This was written by doVMEntry
    loadp Callee[cfr], ws0 # WebAssemblyFunction*
    loadp WebAssemblyFunction::m_instance[ws0], wasmInstance

    # Memory
    if ARM64 or ARM64E
        loadpairq JSWebAssemblyInstance::m_cachedMemory[wasmInstance], memoryBase, boundsCheckingSize
    elsif X86_64
        loadp JSWebAssemblyInstance::m_cachedMemory[wasmInstance], memoryBase
        loadp JSWebAssemblyInstance::m_cachedBoundsCheckingSize[wasmInstance], boundsCheckingSize
    end
    if not ARMv7
        cagedPrimitiveMayBeNull(memoryBase, wa0)
    end

    # Allocate stack space
    loadi WebAssemblyFunction::m_frameSize[ws0], wa0
    subp sp, wa0, wa0

if not ADDRESS64
    bpa wa0, cfr, .stackOverflow
end
    bplteq wa0, JSWebAssemblyInstance::m_softStackLimit[wasmInstance], .stackOverflow

    move wa0, sp

if ASSERT_ENABLED
    repeat(wa0, macro (i)
        storep wa0, -i * SlotSize + constexpr Wasm::JSEntrypointCallee::RegisterStackSpaceAligned[sp]
    end)
end

    # Prepare frame
    move ws0, a2
    move cfr, a1
    move sp, a0
    cCall3(_operationJSToWasmEntryWrapperBuildFrame)

    btpnz r1, .buildEntryFrameThrew
    move r0, ws0

    # Arguments

if ARM64 or ARM64E
    forEachArgumentJSR(macro (offset, gpr1, gpr2)
        loadpairq offset[sp], gpr1, gpr2
    end)
elsif JSVALUE64
    forEachArgumentJSR(macro (offset, gpr)
        loadq offset[sp], gpr
    end)
else
    forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
        load2ia offset[sp], gpLsw, gprMsw
    end)
end

if ARM64 or ARM64E
    forEachArgumentFPR(macro (offset, fpr1, fpr2)
        loadpaird offset[sp], fpr1, fpr2
    end)
else
    forEachArgumentFPR(macro (offset, fpr)
        loadd offset[sp], fpr
    end)
end

    # Pop argument space values
    addp constexpr Wasm::JSEntrypointCallee::RegisterStackSpaceAligned, sp

if ASSERT_ENABLED
    repeat(ws1, macro (i)
        storep ws1, -i * SlotSize[sp]
    end)
end

    getWebAssemblyFunctionAndsetNativeCalleeAndInstance(ws1, ws0)

    # Load callee entrypoint
    loadp WebAssemblyFunction::m_importableFunction + Wasm::WasmOrJSImportableFunction::entrypointLoadLocation[ws1], ws0
    loadp [ws0], ws0

    # Set the callee's interpreter Wasm::Callee
if JSVALUE64
    transferp WebAssemblyFunction::m_boxedWasmCallee[ws1], constexpr (CallFrameSlot::callee - CallerFrameAndPC::sizeInRegisters) * 8[sp]
else
    transferp WebAssemblyFunction::m_boxedWasmCallee + PayloadOffset[ws1], constexpr (CallFrameSlot::callee - CallerFrameAndPC::sizeInRegisters) * 8 + PayloadOffset[sp]
    transferp WebAssemblyFunction::m_boxedWasmCallee + TagOffset[ws1], constexpr (CallFrameSlot::callee - CallerFrameAndPC::sizeInRegisters) * 8 + TagOffset[sp]
end

    call ws0, WasmEntryPtrTag

if ASSERT_ENABLED
    clobberVolatileRegisters()
end

    # Restore SP
    loadp Callee[cfr], ws0 # CalleeBits(JSEntrypointCallee*)
if JSVALUE64
    andp ~(constexpr JSValue::NativeCalleeTag), ws0
end
    leap WTFConfig + constexpr WTF::offsetOfWTFConfigLowestAccessibleAddress, ws1
    loadp [ws1], ws1
    addp ws1, ws0
    loadi Wasm::JSEntrypointCallee::m_frameSize[ws0], ws1
    subp cfr, ws1, ws1
    move ws1, sp
    subp constexpr Wasm::JSEntrypointCallee::SpillStackSpaceAligned, sp

if ASSERT_ENABLED
    repeat(ws0, macro (i)
        storep ws0, -i * SlotSize + constexpr Wasm::JSEntrypointCallee::RegisterStackSpaceAligned[sp]
    end)
end

    # Save return registers
    macro forEachReturnWasmJSR(fn)
        if ARM64 or ARM64E
            fn(0 * 8, wa0, wa1)
            fn(2 * 8, wa2, wa3)
            fn(4 * 8, wa4, wa5)
            fn(6 * 8, wa6, wa7)
        elsif X86_64
            fn(0 * 8, wa0)
            fn(1 * 8, wa1)
            fn(2 * 8, wa2)
            fn(3 * 8, wa3)
            fn(4 * 8, wa4)
            fn(5 * 8, wa5)
        elsif JSVALUE64
            fn(0 * 8, wa0)
            fn(1 * 8, wa1)
            fn(2 * 8, wa2)
            fn(3 * 8, wa3)
            fn(4 * 8, wa4)
            fn(5 * 8, wa5)
        else
            fn(0 * 8, wa1, wa0)
            fn(1 * 8, wa3, wa2)
        end
    end
    macro forEachReturnJSJSR(fn)
        if ARM64 or ARM64E
            fn(0 * 8, r0, r1)
        elsif X86_64
            fn(0 * 8, r0)
            fn(1 * 8, r1)
        elsif JSVALUE64
            fn(0 * 8, r0)
            fn(1 * 8, r1)
        else
            fn(0 * 8, r1, r0)
        end
    end

if ARM64 or ARM64E
    forEachReturnWasmJSR(macro (offset, gpr1, gpr2)
        storepairq gpr1, gpr2, offset[sp]
    end)
elsif JSVALUE64
    forEachReturnWasmJSR(macro (offset, gpr)
        storeq gpr, offset[sp]
    end)
else
    forEachReturnWasmJSR(macro (offset, gprMsw, gpLsw)
        store2ia gpLsw, gprMsw, offset[sp]
    end)
end

if ARM64 or ARM64E
    forEachArgumentFPR(macro (offset, fpr1, fpr2)
        storepaird fpr1, fpr2, offset[sp]
    end)
else
    forEachArgumentFPR(macro (offset, fpr)
        stored fpr, offset[sp]
    end)
end

    # Prepare frame
    move sp, a0
    move cfr, a1
    cCall2(_operationJSToWasmEntryWrapperBuildReturnFrame)

if ARMv7
    branchIfWasmException(.unwind)
else
    btpnz r1, .unwind
end

    # Clean up and return
    restoreJSEntrypointRegisters()
if ASSERT_ENABLED
    clobberVolatileRegisters()
end
    restoreCallerPCAndCFR()
    ret

    # We need to set our NativeCallee/instance here since haven't done it already and wasm_throw_from_slow_path_trampoline expects them.
.stackOverflow:
    getWebAssemblyFunctionAndsetNativeCalleeAndInstance(ws1, ws0)
    throwException(StackOverflow)

.buildEntryFrameThrew:
    getWebAssemblyFunctionAndsetNativeCalleeAndInstance(ws1, ws0)

.unwind:
    loadp JSWebAssemblyInstance::m_vm[wasmInstance], a0
    copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(a0, a1)

# Should be (not USE_BUILTIN_FRAME_ADDRESS) but need to keep down the size of LLIntAssembly.h
if ASSERT_ENABLED or ARMv7
    storep cfr, JSWebAssemblyInstance::m_temporaryCallFrame[wasmInstance]
end

    move wasmInstance, a0
    call _operationWasmUnwind
    jumpToException()
end)

# This is the interpreted analogue to WasmBinding.cpp:wasmToWasm
op(wasm_to_wasm_wrapper_entry, macro()
    # We have only pushed PC (intel) or pushed nothing(others), and we
    # are still in the caller frame.
if X86_64
    loadp (Callee - CallerFrameAndPCSize + 8)[sp], ws0
else
    loadp (Callee - CallerFrameAndPCSize)[sp], ws0
end

if JSVALUE64
    andp ~(constexpr JSValue::NativeCalleeTag), ws0
end
    leap WTFConfig + constexpr WTF::offsetOfWTFConfigLowestAccessibleAddress, ws1
    loadp [ws1], ws1
    addp ws1, ws0

    loadp JSC::Wasm::LLIntCallee::m_entrypoint[ws0], ws0

    # Load the instance
if X86_64
    loadp (CodeBlock - CallerFrameAndPCSize + 8)[sp], wasmInstance
else
    loadp (CodeBlock - CallerFrameAndPCSize)[sp], wasmInstance
end

    # Memory
    if ARM64 or ARM64E
        loadpairq JSWebAssemblyInstance::m_cachedMemory[wasmInstance], memoryBase, boundsCheckingSize
    elsif X86_64
        loadp JSWebAssemblyInstance::m_cachedMemory[wasmInstance], memoryBase
        loadp JSWebAssemblyInstance::m_cachedBoundsCheckingSize[wasmInstance], boundsCheckingSize
    end
    if not ARMv7
        cagedPrimitiveMayBeNull(memoryBase, ws1)
    end

    jmp ws0, WasmEntryPtrTag
end)

op(wasm_to_wasm_ipint_wrapper_entry, macro()
    # We have only pushed PC (intel) or pushed nothing(others), and we
    # are still in the caller frame.
if X86_64
    loadp (Callee - CallerFrameAndPCSize + 8)[sp], ws0
else
    loadp (Callee - CallerFrameAndPCSize)[sp], ws0
end

if JSVALUE64
    andp ~(constexpr JSValue::NativeCalleeTag), ws0
end
    leap WTFConfig + constexpr WTF::offsetOfWTFConfigLowestAccessibleAddress, ws1
    loadp [ws1], ws1
    addp ws1, ws0

    loadp JSC::Wasm::IPIntCallee::m_entrypoint[ws0], ws0

    # Load the instance
if X86_64
    loadp (CodeBlock - CallerFrameAndPCSize + 8)[sp], wasmInstance
else
    loadp (CodeBlock - CallerFrameAndPCSize)[sp], wasmInstance
end

    # Memory
    if ARM64 or ARM64E
        loadpairq JSWebAssemblyInstance::m_cachedMemory[wasmInstance], memoryBase, boundsCheckingSize
    elsif X86_64
        loadp JSWebAssemblyInstance::m_cachedMemory[wasmInstance], memoryBase
        loadp JSWebAssemblyInstance::m_cachedBoundsCheckingSize[wasmInstance], boundsCheckingSize
    end
    if not ARMv7
        cagedPrimitiveMayBeNull(memoryBase, ws1)
    end

    jmp ws0, WasmEntryPtrTag
end)

# This is the interpreted analogue to WasmToJS.cpp:wasmToJS
op(wasm_to_js_wrapper_entry, macro()
    # We have only pushed PC (intel) or pushed nothing(others), and we
    # are still in the caller frame.
    # Load this before we create the stack frame, since we lose old cfr, which we wrote Callee to

    # We repurpose this slot temporarily for a WasmCallableFunction* from resolveWasmCall and friends.
    tagReturnAddress sp
    preserveCallerPCAndCFR()

    const RegisterSpaceScratchSize = 0x80
    subp (WasmToJSScratchSpaceSize + RegisterSpaceScratchSize), sp

    loadp CodeBlock[cfr], ws0
    storep ws0, WasmToJSCallableFunctionSlot[cfr]

    # Store all the registers here

if ARM64 or ARM64E
    forEachArgumentJSR(macro (offset, gpr1, gpr2)
        storepairq gpr1, gpr2, offset[sp]
    end)
elsif JSVALUE64
    forEachArgumentJSR(macro (offset, gpr)
        storeq gpr, offset[sp]
    end)
else
    forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
        store2ia gpLsw, gprMsw, offset[sp]
    end)
end

if ARM64 or ARM64E
    forEachArgumentFPR(macro (offset, fpr1, fpr2)
        storepaird fpr1, fpr2, offset[sp]
    end)
else
    forEachArgumentFPR(macro (offset, fpr)
        stored fpr, offset[sp]
    end)
end

if ASSERT_ENABLED or ARMv7
    storep cfr, JSWebAssemblyInstance::m_temporaryCallFrame[wasmInstance]
end

    move wasmInstance, a0
    move ws0, a1
    cCall2(_operationGetWasmCalleeStackSize)

    move sp, a2
    subp r0, sp
    move sp, a0
    move cfr, a1
    move wasmInstance, a3
    cCall4(_operationWasmToJSExitMarshalArguments)
    btpnz r1, .oom

    bineq r0, 0, .safe
    move wasmInstance, r0
    move (constexpr Wasm::ExceptionType::TypeErrorInvalidValueUse), r1
    cCall2(_operationWasmToJSException)
    jumpToException()
    break

.safe:
    loadp WasmToJSCallableFunctionSlot[cfr], t2
    loadp JSC::Wasm::WasmOrJSImportableFunctionCallLinkInfo::importFunction[t2], t0
if not JSVALUE64
    move (constexpr JSValue::CellTag), t1
end
    loadp JSC::Wasm::WasmOrJSImportableFunctionCallLinkInfo::callLinkInfo[t2], t2

    # calleeGPR = t0
    # callLinkInfoGPR = t2
    # callTargetGPR = t5
    loadp CallLinkInfo::m_monomorphicCallDestination[t2], t5

    # scratch = t3
    loadp CallLinkInfo::m_callee[t2], t3
    bpeq t3, t0, .found
    btpnz t3, (constexpr CallLinkInfo::polymorphicCalleeMask), .found

.notfound:
if ARM64 or ARM64E
    pcrtoaddr _llint_default_call_trampoline, t5
else
    leap (_llint_default_call_trampoline), t5
end
    loadp CallLinkInfo::m_codeBlock[t2], t3
    storep t3, (CodeBlock - CallerFrameAndPCSize)[sp]
    call _llint_default_call_trampoline
    jmp .postcall
.found:
    # jit.transferPtr CallLinkInfo::codeBlock[t2], CodeBlock[cfr]
    loadp CallLinkInfo::m_codeBlock[t2], t3
    storep t3, (CodeBlock - CallerFrameAndPCSize)[sp]
    call t5, JSEntryPtrTag

.postcall:
    storep r0, [sp]
if not JSVALUE64
    storep r1, TagOffset[sp]
end

    loadp WasmToJSCallableFunctionSlot[cfr], a0
    call _operationWasmToJSExitNeedToUnpack
    btpnz r0, .unpack

    move sp, a0
    move cfr, a1
    move wasmInstance, a2
    cCall3(_operationWasmToJSExitMarshalReturnValues)
    btpnz r0, .handleException
    jmp .end

.unpack:

    move r0, a1
    move wasmInstance, a0
    move sp, a2
    move cfr, a3
    cCall4(_operationWasmToJSExitIterateResults)
    btpnz r0, .handleException

.end:
    macro forEachReturnWasmJSR(fn)
        if ARM64 or ARM64E
            fn(0 * 8, wa0, wa1)
            fn(2 * 8, wa2, wa3)
            fn(4 * 8, wa4, wa5)
            fn(6 * 8, wa6, wa7)
        elsif X86_64
            fn(0 * 8, wa0)
            fn(1 * 8, wa1)
            fn(2 * 8, wa2)
            fn(3 * 8, wa3)
            fn(4 * 8, wa4)
            fn(5 * 8, wa5)
        elsif JSVALUE64
            fn(0 * 8, wa0)
            fn(1 * 8, wa1)
            fn(2 * 8, wa2)
            fn(3 * 8, wa3)
            fn(4 * 8, wa4)
            fn(5 * 8, wa5)
        else
            fn(0 * 8, wa1, wa0)
            fn(1 * 8, wa3, wa2)
        end
    end

if ARM64 or ARM64E
    forEachReturnWasmJSR(macro (offset, gpr1, gpr2)
        loadpairq offset[sp], gpr1, gpr2
    end)
elsif JSVALUE64
    forEachReturnWasmJSR(macro (offset, gpr)
        loadq offset[sp], gpr
    end)
else
    forEachReturnWasmJSR(macro (offset, gprMsw, gprLsw)
        load2ia offset[sp], gprLsw, gprMsw
    end)
end

if ARM64 or ARM64E
    forEachArgumentFPR(macro (offset, fpr1, fpr2)
        loadpaird offset[sp], fpr1, fpr2
    end)
else
    forEachArgumentFPR(macro (offset, fpr)
        loadd offset[sp], fpr
    end)
end

    loadp CodeBlock[cfr], wasmInstance
    restoreCallerPCAndCFR()
    ret

.handleException:
    loadp JSWebAssemblyInstance::m_vm[wasmInstance], a0
    copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(a0, a1)

if ASSERT_ENABLED or ARMv7
    storep cfr, JSWebAssemblyInstance::m_temporaryCallFrame[wasmInstance]
end

    move wasmInstance, a0
    call _operationWasmUnwind
    jumpToException()

.oom:
    throwException(OutOfMemory)

end)

macro traceExecution()
    if TRACING
        callWasmSlowPath(_slow_path_wasm_trace)
    end
end

macro commonWasmOp(opcodeName, opcodeStruct, prologue, fn)
    commonOp(opcodeName, prologue, macro(size)
        fn(macro(fn2)
            fn2(opcodeName, opcodeStruct, size)
        end)
    end)
end

# Less convenient, but required for opcodes that collide with reserved instructions (e.g. wasm_nop)
macro unprefixedWasmOp(opcodeName, opcodeStruct, fn)
    commonWasmOp(opcodeName, opcodeStruct, traceExecution, fn)
end

macro wasmOp(opcodeName, opcodeStruct, fn)
    unprefixedWasmOp(wasm_%opcodeName%, opcodeStruct, fn)
end

# Same as unprefixedWasmOp, necessary for e.g. wasm_call
macro unprefixedSlowWasmOp(opcodeName)
    unprefixedWasmOp(opcodeName, unusedOpcodeStruct, macro(ctx)
        callWasmSlowPath(_slow_path_%opcodeName%)
        dispatch(ctx)
    end)
end

macro slowWasmOp(opcodeName)
    unprefixedSlowWasmOp(wasm_%opcodeName%)
end

# Float to float rounding ops
macro wasmRoundingOp(opcodeName, opcodeStruct, fn)
if JSVALUE64 # All current 64-bit platforms have instructions for these
    wasmOp(opcodeName, opcodeStruct, fn)
else
    slowWasmOp(opcodeName)
end
end

# i64 (signed/unsigned) to f32 or f64
macro wasmI64ToFOp(opcodeName, opcodeStruct, fn)
if JSVALUE64 # All current 64-bit platforms have instructions for these
    wasmOp(opcodeName, opcodeStruct, fn)
else
    slowWasmOp(opcodeName)
end
end

# Macro version of load operations: mload[suffix]
# loads field from the instruction stream and performs load[suffix] to dst
macro firstConstantRegisterIndex(ctx, fn)
    ctx(macro(opcodeName, opcodeStruct, size)
        size(FirstConstantRegisterIndexNarrow, FirstConstantRegisterIndexWide16, FirstConstantRegisterIndexWide32, fn)
    end)
end

macro loadConstantOrVariable(ctx, index, loader)
    firstConstantRegisterIndex(ctx, macro (firstConstantIndex)
        bpgteq index, firstConstantIndex, .constant
        loader([cfr, index, 8])
        jmp .done
    .constant:
        loadp UnboxedWasmCalleeStackSlot[cfr], t6
        loadp Wasm::LLIntCallee::m_constants[t6], t6
        subp firstConstantIndex, index
        loader((constexpr (Int64FixedVector::Storage::offsetOfData()))[t6, index, 8])
    .done:
    end)
end

if JSVALUE64
macro mloadq(ctx, field, dst)
    wgets(ctx, field, dst)
    loadConstantOrVariable(ctx, dst, macro (from)
        loadq from, dst
    end)
end
else
macro mload2i(ctx, field, dstMsw, dstLsw)
    wgets(ctx, field, dstLsw)
    loadConstantOrVariable(ctx, dstLsw, macro (from)
        load2ia from, dstLsw, dstMsw
    end)
end
end

macro mloadi(ctx, field, dst)
    wgets(ctx, field, dst)
    loadConstantOrVariable(ctx, dst, macro (from)
        loadi from, dst
    end)
end

macro mloadp(ctx, field, dst)
    wgets(ctx, field, dst)
    loadConstantOrVariable(ctx, dst, macro (from)
        loadp from, dst
    end)
end

macro mloadf(ctx, field, dst)
    wgets(ctx, field, t5)
    loadConstantOrVariable(ctx, t5, macro (from)
        loadf from, dst
    end)
end

macro mloadd(ctx, field, dst)
    wgets(ctx, field, t5)
    loadConstantOrVariable(ctx, t5, macro (from)
        loadd from, dst
    end)
end

# Typed returns

if JSVALUE64
macro returnq(ctx, value)
    wgets(ctx, m_dst, t5)
    storeq value, [cfr, t5, 8]
    dispatch(ctx)
end
else
macro return2i(ctx, msw, lsw)
    wgets(ctx, m_dst, t5)
    store2ia lsw, msw, [cfr, t5, 8]
    dispatch(ctx)
end
end

macro returni(ctx, value)
    wgets(ctx, m_dst, t5)
    storei value, [cfr, t5, 8]
    dispatch(ctx)
end

macro returnf(ctx, value)
    wgets(ctx, m_dst, t5)
    storef value, [cfr, t5, 8]
    dispatch(ctx)
end

macro returnd(ctx, value)
    wgets(ctx, m_dst, t5)
    stored value, [cfr, t5, 8]
    dispatch(ctx)
end

# Wasm wrapper of get/getu that operate on ctx
macro wgets(ctx, field, dst)
    ctx(macro(opcodeName, opcodeStruct, size)
        size(getOperandNarrow, getOperandWide16Wasm, getOperandWide32Wasm, macro (get)
            get(opcodeStruct, field, dst)
        end)
    end)
end

macro wgetu(ctx, field, dst)
    ctx(macro(opcodeName, opcodeStruct, size)
        size(getuOperandNarrow, getuOperandWide16Wasm, getuOperandWide32Wasm, macro (getu)
            getu(opcodeStruct, field, dst)
        end)
    end)
end

# Control flow helpers

macro dispatch(ctx)
    ctx(macro(opcodeName, opcodeStruct, size)
        genericDispatchOpWasm(wasmDispatch, size, opcodeName)
    end)
end

macro jump(ctx, target)
    wgets(ctx, target, t0)
    btiz t0, .outOfLineJumpTarget
    wasmDispatchIndirect(t0)
.outOfLineJumpTarget:
    callWasmSlowPath(_slow_path_wasm_out_of_line_jump_target)
    wasmNextInstruction()
end

macro doReturn()
    restoreCalleeSavesUsedByWasm()
    restoreCallerPCAndCFR()
    if ARM64E
        leap _g_config, ws0
        jmp JSCConfigGateMapOffset + (constexpr Gate::returnFromLLInt) * PtrSize[ws0], NativeToJITGatePtrTag
    else
        ret
    end
end

# Entry point

op(wasm_function_prologue_trampoline, macro ()
    tagReturnAddress sp
    jmp _wasm_function_prologue
end)

op(wasm_function_prologue, macro ()
    if not WEBASSEMBLY or C_LOOP
        error
    end

    wasmPrologue()
    wasmNextInstruction()
end)

op(wasm_function_prologue_simd_trampoline, macro ()
    tagReturnAddress sp
    jmp _wasm_function_prologue_simd
end)

op(wasm_function_prologue_simd, macro ()
    if not WEBASSEMBLY or C_LOOP
        error
    end

    wasmPrologueSIMD(macro()
        move cfr, a0
        move PC, a1
        move wasmInstance, a2
        cCall4(_slow_path_wasm_simd_go_straight_to_bbq_osr)
    end)
    break
end)

op(ipint_function_prologue_simd_trampoline, macro ()
    tagReturnAddress sp
    jmp _ipint_function_prologue_simd
end)

op(ipint_function_prologue_simd, macro ()
    if not WEBASSEMBLY or C_LOOP
        error
    end

    wasmPrologueSIMD(macro()
        move wasmInstance, a0
        move cfr, a1
        cCall2(_ipint_extern_simd_go_straight_to_bbq)
    end)
    break
end)

macro jumpToException()
    if ARM64E
        move r0, a0
        leap _g_config, a1
        jmp JSCConfigGateMapOffset + (constexpr Gate::exceptionHandler) * PtrSize[a1], NativeToJITGatePtrTag # ExceptionHandlerPtrTag
    else
        jmp r0, ExceptionHandlerPtrTag
    end
end

op(wasm_throw_from_slow_path_trampoline, macro ()
    loadp JSWebAssemblyInstance::m_vm[wasmInstance], t5
    loadp VM::topEntryFrame[t5], t5
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(t5)

    move cfr, a0
    move wasmInstance, a1
    # Slow paths and the throwException macro store the exception code in the ArgumentCountIncludingThis slot
    loadi ArgumentCountIncludingThis + PayloadOffset[cfr], a2
    storei 0, CallSiteIndex[cfr]
    cCall3(_slow_path_wasm_throw_exception)
    jumpToException()
end)

macro wasm_throw_from_fault_handler(instance)
    # instance should be in a2 when we get here
    loadp JSWebAssemblyInstance::m_vm[instance], a0
    loadp VM::topEntryFrame[a0], a0
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(a0)

    move cfr, a0
    move a2, a1
    move constexpr Wasm::ExceptionType::OutOfBoundsMemoryAccess, a2

    storei 0, CallSiteIndex[cfr]
    cCall3(_slow_path_wasm_throw_exception)
    jumpToException()
end

op(wasm_throw_from_fault_handler_trampoline_reg_instance, macro ()
    move wasmInstance, a2
    wasm_throw_from_fault_handler(a2)
end)

# Disable wide version of narrow-only opcodes
noWide(wasm_enter)
noWide(wasm_wide16)
noWide(wasm_wide32)

# Opcodes that always invoke the slow path

slowWasmOp(ref_func)
slowWasmOp(table_get)
slowWasmOp(table_set)
slowWasmOp(table_init)
slowWasmOp(table_fill)
slowWasmOp(table_grow)
slowWasmOp(set_global_ref)
slowWasmOp(set_global_ref_portable_binding)
slowWasmOp(memory_atomic_wait32)
slowWasmOp(memory_atomic_wait64)
slowWasmOp(memory_atomic_notify)
slowWasmOp(array_new)
slowWasmOp(array_get)
slowWasmOp(array_set)
slowWasmOp(array_fill)
slowWasmOp(struct_new)
slowWasmOp(struct_get)
slowWasmOp(struct_set)

wasmOp(grow_memory, WasmGrowMemory, macro(ctx)
    callWasmSlowPath(_slow_path_wasm_grow_memory)
    reloadMemoryRegistersFromInstance(wasmInstance, ws0)
    dispatch(ctx)
end)

# Opcodes that should eventually be shared with JS llint

_wasm_wide16:
    wasmNextInstructionWide16()

_wasm_wide32:
    wasmNextInstructionWide32()

_wasm_enter:
    traceExecution()
    checkStackPointerAlignment(t2, 0xdead00e1)
    loadp UnboxedWasmCalleeStackSlot[cfr], t2 // t2<UnboxedWasmCalleeStackSlot> = cfr.UnboxedWasmCalleeStackSlot
    loadi Wasm::LLIntCallee::m_numVars[t2], t2 // t2<size_t> = t2<UnboxedWasmCalleeStackSlot>.m_numVars
    subi CalleeSaveSpaceAsVirtualRegisters + NumberOfWasmArguments, t2
    btiz t2, .opEnterDone
    subp cfr, (CalleeSaveSpaceAsVirtualRegisters + NumberOfWasmArguments) * SlotSize, t1
    lshifti 3, t2
    negi t2
if JSVALUE64
    sxi2q t2, t2
end
    move 0, t6
.opEnterLoop:
if JSVALUE64
    storeq t6, [t1, t2]
else
    store2ia t6, t6, [t1, t2]
end
    addp 8, t2
    btpnz t2, .opEnterLoop
.opEnterDone:
    wasmDispatchIndirect(1)

unprefixedWasmOp(wasm_nop, WasmNop, macro(ctx)
    dispatch(ctx)
end)

wasmOp(loop_hint, WasmLoopHint, macro(ctx)
    checkSwitchToJITForLoop()
    dispatch(ctx)
end)

wasmOp(jtrue, WasmJtrue, macro(ctx)
    mloadi(ctx, m_condition, t0)
    btiz t0, .continue
    jump(ctx, m_targetLabel)
.continue:
    dispatch(ctx)
end)

wasmOp(jfalse, WasmJfalse, macro(ctx)
    mloadi(ctx, m_condition, t0)
    btinz t0, .continue
    jump(ctx, m_targetLabel)
.continue:
    dispatch(ctx)
end)

wasmOp(switch, WasmSwitch, macro(ctx)
    mloadi(ctx, m_scrutinee, t0)
    wgetu(ctx, m_tableIndex, t1)

    loadp UnboxedWasmCalleeStackSlot[cfr], t2
    loadp Wasm::LLIntCallee::m_jumpTables[t2], t2
    muli sizeof Wasm::JumpTable, t1
    addp t1, t2

    loadp (constexpr (WasmJumpTableFixedVector::Storage::offsetOfData()))[t2], t2
    loadi Wasm::JumpTable::Storage::m_size[t2], t3
    bib t0, t3, .inBounds

.outOfBounds:
    subi t3, 1, t0

.inBounds:
    muli sizeof Wasm::JumpTableEntry, t0

    loadi (constexpr (Wasm::JumpTable::Storage::offsetOfData())) + Wasm::JumpTableEntry::startOffset[t2, t0], t1
    loadi (constexpr (Wasm::JumpTable::Storage::offsetOfData())) + Wasm::JumpTableEntry::dropCount[t2, t0], t3
    loadi (constexpr (Wasm::JumpTable::Storage::offsetOfData())) + Wasm::JumpTableEntry::keepCount[t2, t0], t5
    dropKeep(t1, t3, t5)

    loadis (constexpr (Wasm::JumpTable::Storage::offsetOfData())) + Wasm::JumpTableEntry::target[t2, t0], t3
    assert(macro(ok) btinz t3, .ok end)
    wasmDispatchIndirect(t3)
end)

unprefixedWasmOp(wasm_jmp, WasmJmp, macro(ctx)
    jump(ctx, m_targetLabel)
end)

unprefixedWasmOp(wasm_ret, WasmRet, macro(ctx)
    checkSwitchToJITForEpilogue()
if ARM64 or ARM64E
    forEachArgumentJSR(macro (offset, gpr1, gpr2)
        loadpairq -offset - 16 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpr2, gpr1
    end)
elsif JSVALUE64
    forEachArgumentJSR(macro (offset, gpr)
        loadq -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpr
    end)
else
    forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
        load2ia -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpLsw, gprMsw
    end)
end
if ARM64 or ARM64E
    forEachArgumentFPR(macro (offset, fpr1, fpr2)
        loadpaird -offset - 16 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], fpr2, fpr1
    end)
else
    forEachArgumentFPR(macro (offset, fpr)
        loadd -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], fpr
    end)
end
    doReturn()
end)

# Wasm specific bytecodes

wasmOp(unreachable, WasmUnreachable, macro(ctx)
    throwException(Unreachable)
end)

wasmOp(ret_void, WasmRetVoid, macro(ctx)
    checkSwitchToJITForEpilogue()
    doReturn()
end)

macro slowPathForWasmCall(ctx, slowPath, storeWasmInstance)
    # First, prepare the new stack frame
    # We do this before the CCall so it can write stuff into the new frame from CPP
    wgetu(ctx, m_stackOffset, ws1)
    muli SlotSize, ws1
if ARMv7
    subp cfr, ws1, ws1
    move ws1, sp
else
    subp cfr, ws1, sp
end

if ASSERT_ENABLED
    if ARMv7
        move 0xBEEF, a0
        storep a0, PayloadOffset + Callee[sp]
        move 0, a0
        storep a0, TagOffset + Callee[sp]
    else
        move 0xBEEF, a0
        storep a0, Callee[sp]
    end
end

    callWasmCallSlowPath(
        slowPath,
        # callee is r0 and targetWasmInstance is r1
        macro (calleeEntryPoint, targetWasmInstance)
            loadi CallSiteIndex[cfr], PC

            move calleeEntryPoint, ws0

            # the call might throw (e.g. indirect call with bad signature)
            btpz targetWasmInstance, .throw

            wgetu(ctx, m_numberOfStackArgs, ws1)

            # Preserve the current instance
            move wasmInstance, PB

            storeWasmInstance(targetWasmInstance)
            reloadMemoryRegistersFromInstance(targetWasmInstance, wa0)

            # Load registers from stack
if ARM64 or ARM64E
            leap [sp, ws1, 8], ws1
            forEachArgumentJSR(macro (offset, gpr1, gpr2)
                loadpairq CallFrameHeaderSize + 8 + offset[ws1], gpr1, gpr2
            end)
elsif JSVALUE64
            forEachArgumentJSR(macro (offset, gpr)
                loadq CallFrameHeaderSize + 8 + offset[sp, ws1, 8], gpr
            end)
else
            forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
                load2ia CallFrameHeaderSize + 8 + offset[sp, ws1, 8], gpLsw, gprMsw
            end)
end
if ARM64 or ARM64E
            forEachArgumentFPR(macro (offset, fpr1, fpr2)
                loadpaird CallFrameHeaderSize + 8 + offset[ws1], fpr1, fpr2
            end)
else
            forEachArgumentFPR(macro (offset, fpr)
                loadd CallFrameHeaderSize + 8 + offset[sp, ws1, 8], fpr
            end)
end
            addp CallerFrameAndPCSize, sp

            ctx(macro(opcodeName, opcodeStruct, size)
                macro callNarrow()
                    if ARM64E
                        leap _g_config, ws1
                        jmp JSCConfigGateMapOffset + (constexpr Gate::%opcodeName%) * PtrSize[ws1], NativeToJITGatePtrTag # WasmEntryPtrTag
                    end
                    _wasm_trampoline_%opcodeName%:
                    call ws0, WasmEntryPtrTag
                end

                macro callWide16()
                    if ARM64E
                        leap _g_config, ws1
                        jmp JSCConfigGateMapOffset + (constexpr Gate::%opcodeName%_wide16) * PtrSize[ws1], NativeToJITGatePtrTag # WasmEntryPtrTag
                    end
                    _wasm_trampoline_%opcodeName%_wide16:
                    call ws0, WasmEntryPtrTag
                end

                macro callWide32()
                    if ARM64E
                        leap _g_config, ws1
                        jmp JSCConfigGateMapOffset + (constexpr Gate::%opcodeName%_wide32) * PtrSize[ws1], NativeToJITGatePtrTag # WasmEntryPtrTag
                    end
                    _wasm_trampoline_%opcodeName%_wide32:
                    call ws0, WasmEntryPtrTag
                end

                size(callNarrow, callWide16, callWide32, macro (gen) gen() end)
                defineReturnLabel(opcodeName, size)
            end)

            restoreStackPointerAfterCall()

            # We need to set PC to load information from the instruction stream, but we
            # need to preserve its current value since it might contain a return value
if ARMv7
            push PC
else
            move PC, memoryBase
end
            move PB, wasmInstance
            loadi CallSiteIndex[cfr], PC
            loadp UnboxedWasmCalleeStackSlot[cfr], PB
            loadp Wasm::LLIntCallee::m_instructionsRawPointer[PB], PB

            wgetu(ctx, m_stackOffset, ws1)
            lshifti 3, ws1
            negi ws1
if JSVALUE64
            sxi2q ws1, ws1
end
            addp cfr, ws1

            # Argument registers are also return registers, so they must be stored to the stack
            # in case they contain return values.
            wgetu(ctx, m_numberOfStackArgs, ws0)
if ARMv7
            pop PC
else
            move memoryBase, PC
end
if ARM64 or ARM64E
            leap [ws1, ws0, 8], ws1
            forEachArgumentJSR(macro (offset, gpr1, gpr2)
                storepairq gpr1, gpr2, CallFrameHeaderSize + 8 + offset[ws1]
            end)
elsif JSVALUE64
            forEachArgumentJSR(macro (offset, gpr)
                storeq gpr, CallFrameHeaderSize + 8 + offset[ws1, ws0, 8]
            end)
else
            forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
                store2ia gpLsw, gprMsw, CallFrameHeaderSize + 8 + offset[ws1, ws0, 8]
            end)
end
if ARM64 or ARM64E
            forEachArgumentFPR(macro (offset, fpr1, fpr2)
                storepaird fpr1, fpr2, CallFrameHeaderSize + 8 + offset[ws1]
            end)
else
            forEachArgumentFPR(macro (offset, fpr)
                stored fpr, CallFrameHeaderSize + 8 + offset[ws1, ws0, 8]
            end)
end

            loadi CallSiteIndex[cfr], PC

            storeWasmInstance(wasmInstance)
            reloadMemoryRegistersFromInstance(wasmInstance, ws0)

            dispatch(ctx)

        .throw:
            restoreStateAfterCCall()
            dispatch(ctx)
        end)
end

macro slowPathForWasmTailCall(ctx, slowPath)
    # First, prepare the new stack frame like a regular call
    # We do this before the CCall so it can write stuff into the new frame from CPP
    # Later, we will move the frame up.
    wgetu(ctx, m_stackOffset, ws1)
    muli SlotSize, ws1
    if ARMv7
        subp cfr, ws1, ws1
        move ws1, sp
    else
        subp cfr, ws1, sp
    end

    if ASSERT_ENABLED
        if ARMv7
            move 0xBEEF, a0
            storep a0, PayloadOffset + Callee[sp]
            move 0, a0
            storep a0, TagOffset + Callee[sp]
        else
            move 0xBEEF, a0
            storep a0, Callee[sp]
        end
    end

    callWasmCallSlowPath(
        slowPath,
        # callee is r0 and targetWasmInstance is r1
        macro (calleeEntryPoint, targetWasmInstance)
            move calleeEntryPoint, ws0

            loadi CallSiteIndex[cfr], PC

            # the call might throw (e.g. indirect call with bad signature)
            btpz targetWasmInstance, .throw

            move targetWasmInstance, wasmInstance
            reloadMemoryRegistersFromInstance(targetWasmInstance, wa0)

            wgetu(ctx, m_numberOfCalleeStackArgs, ws1)

            # We make sure arguments registers are loaded from the stack before shuffling the frame.
if ARM64 or ARM64E
            leap [sp, ws1, 8], ws1
            forEachArgumentJSR(macro (offset, gpr1, gpr2)
                loadpairq CallFrameHeaderSize + 8 + offset[ws1], gpr1, gpr2
            end)
elsif JSVALUE64
            forEachArgumentJSR(macro (offset, gpr)
                loadq CallFrameHeaderSize + 8 + offset[sp, ws1, 8], gpr
            end)
else
            forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
                load2ia CallFrameHeaderSize + 8 + offset[sp, ws1, 8], gpLsw, gprMsw
            end)
end
if ARM64 or ARM64E
            forEachArgumentFPR(macro (offset, fpr1, fpr2)
                loadpaird CallFrameHeaderSize + 8 + offset[ws1], fpr1, fpr2
            end)
else
            forEachArgumentFPR(macro (offset, fpr)
                loadd CallFrameHeaderSize + 8 + offset[sp, ws1, 8], fpr
            end)
end

            # We need PC to load information from the context
            # and two scratch registers to perform the copy.

            preserveGPRsUsedByTailCall(wa0, wa1)

            # Preserve PC
            move PC, ws1

            # Reload PC
            loadi CallSiteIndex[cfr], PC

            # Compute old stack size
            wgetu(ctx, m_numberOfCallerStackArgs, wa0)
            muli SlotSize, wa0
            addi CallFrameHeaderSize + StackAlignment - 1, wa0
            andi ~StackAlignmentMask, wa0

            # Compute new stack size
            wgetu(ctx, m_numberOfCalleeStackArgs, wa1)
            muli SlotSize, wa1
            addi CallFrameHeaderSize + StackAlignment - 1, wa1
            andi ~StackAlignmentMask, wa1

            # Compute new stack pointer
            addp cfr, wa0
            subp wa1, wa0

            # Restore PC
            move ws1, PC

if ARM64E
            addp CallerFrameAndPCSize, cfr, ws2
end
            preserveReturnAddress(ws1)
            usePreviousFrame()

if ARMv7
            addi SlotSize, wa1
end
            .copyLoop:
if JSVALUE64
            subi MachineRegisterSize, wa1
            loadq [sp, wa1, 1], ws1
            storeq ws1, [wa0, wa1, 1]
else
            subi PtrSize, wa1
            loadp [sp, wa1, 1], ws1
            storep ws1, [wa0, wa1, 1]
end
            btinz wa1, .copyLoop

            # Store the wasm callee, set by the c slow path above.
            loadp Callee[sp], ws1
            storep ws1, Callee[wa0]

            # Store WasmCallableFunction*
            loadp CodeBlock[sp], ws1
            storep ws1, CodeBlock[wa0]

            move wa0, ws1

            restoreGPRsUsedByTailCall(wa0, wa1)

            # We are done copying the callee's stack arguments to their final position.
            # ws1 is the new stack pointer.
            # cfr is the caller's caller's frame pointer.

if X86_64
            addp PtrSize, ws1, sp
elsif ARMv7
            addp CallerFrameAndPCSize, ws1
            move ws1, sp
elsif ARM64 or ARM64E or RISCV64
            addp CallerFrameAndPCSize, ws1, sp
else
            error
end

            ctx(macro(opcodeName, opcodeStruct, size)
                macro jumpNarrow()
                    if ARM64E
                        leap _g_config, ws1
                        jmp JSCConfigGateMapOffset + (constexpr Gate::wasmTailCallWasmEntryPtrTag) * PtrSize[ws1], NativeToJITGatePtrTag # WasmEntryPtrTag
                    end
                    _wasm_trampoline_%opcodeName%:
                    jmp ws0, WasmEntryPtrTag
                end

                macro jumpWide16()
                    if ARM64E
                        leap _g_config, ws1
                        jmp JSCConfigGateMapOffset + (constexpr Gate::wasmTailCallWasmEntryPtrTag) * PtrSize[ws1], NativeToJITGatePtrTag # WasmEntryPtrTag
                    end
                    _wasm_trampoline_%opcodeName%_wide16:
                    jmp ws0, WasmEntryPtrTag
                end

                macro jumpWide32()
                    if ARM64E
                        leap _g_config, ws1
                        jmp JSCConfigGateMapOffset + (constexpr Gate::wasmTailCallWasmEntryPtrTag) * PtrSize[ws1], NativeToJITGatePtrTag # WasmEntryPtrTag
                    end
                    _wasm_trampoline_%opcodeName%_wide32:
                    jmp ws0, WasmEntryPtrTag
                end

                size(jumpNarrow, jumpWide16, jumpWide32, macro (gen) gen() end)
            end)
    .throw:
    restoreStateAfterCCall()
    dispatch(ctx)
    end)
end

unprefixedWasmOp(wasm_call, WasmCall, macro(ctx)
    slowPathForWasmCall(ctx, _slow_path_wasm_call, macro(targetInstance) move targetInstance, wasmInstance end)
end)

wasmOp(call_indirect, WasmCallIndirect, macro(ctx)
    slowPathForWasmCall(ctx, _slow_path_wasm_call_indirect, macro(targetInstance) move targetInstance, wasmInstance end)
end)

wasmOp(call_ref, WasmCallRef, macro(ctx)
    slowPathForWasmCall(ctx, _slow_path_wasm_call_ref, macro(targetInstance) move targetInstance, wasmInstance end)
end)

wasmOp(tail_call, WasmTailCall, macro(ctx)
    slowPathForWasmTailCall(ctx, _slow_path_wasm_tail_call)
end)

wasmOp(tail_call_indirect, WasmTailCallIndirect, macro(ctx)
    slowPathForWasmTailCall(ctx, _slow_path_wasm_tail_call_indirect)
end)

wasmOp(tail_call_ref, WasmTailCallRef, macro(ctx)
    slowPathForWasmTailCall(ctx, _slow_path_wasm_tail_call_ref)
end)

slowWasmOp(call_builtin)

wasmOp(select, WasmSelect, macro(ctx)
    mloadi(ctx, m_condition, t0)
    btiz t0, .isZero
if JSVALUE64
    mloadq(ctx, m_nonZero, t0)
    returnq(ctx, t0)
.isZero:
    mloadq(ctx, m_zero, t0)
    returnq(ctx, t0)
else
    mload2i(ctx, m_nonZero, t1, t0)
    return2i(ctx, t1, t0)
.isZero:
    mload2i(ctx, m_zero, t1, t0)
    return2i(ctx, t1, t0)
end
end)

# Opcodes that don't have the `b3op` entry in wasm.json. This should be kept in sync

wasmOp(i32_ctz, WasmI32Ctz, macro (ctx)
    mloadi(ctx, m_operand, t0)
    tzcnti t0, t0
    returni(ctx, t0)
end)

wasmOp(i32_popcnt, WasmI32Popcnt, macro (ctx)
    mloadi(ctx, m_operand, a1)
    prepareStateForCCall()
    move PC, a0
    cCall2(_slow_path_wasm_popcount)
    restoreStateAfterCCall()
    returni(ctx, r1)
end)

wasmRoundingOp(f32_trunc, WasmF32Trunc, macro (ctx)
    mloadf(ctx, m_operand, ft0)
    truncatef ft0, ft0
    returnf(ctx, ft0)
end)

wasmRoundingOp(f32_nearest, WasmF32Nearest, macro (ctx)
    mloadf(ctx, m_operand, ft0)
    roundf ft0, ft0
    returnf(ctx, ft0)
end)

wasmRoundingOp(f64_trunc, WasmF64Trunc, macro (ctx)
    mloadd(ctx, m_operand, ft0)
    truncated ft0, ft0
    returnd(ctx, ft0)
end)

wasmRoundingOp(f64_nearest, WasmF64Nearest, macro (ctx)
    mloadd(ctx, m_operand, ft0)
    roundd ft0, ft0
    returnd(ctx, ft0)
end)

wasmOp(i32_trunc_s_f32, WasmI32TruncSF32, macro (ctx)
    mloadf(ctx, m_operand, ft0)

    move 0xcf000000, t0 # INT32_MIN (Note that INT32_MIN - 1.0 in float is the same as INT32_MIN in float).
    fi2f t0, ft1
    bfltun ft0, ft1, .outOfBoundsTrunc

    move 0x4f000000, t0 # -INT32_MIN
    fi2f t0, ft1
    bfgtequn ft0, ft1, .outOfBoundsTrunc

    truncatef2is ft0, t0
    returni(ctx, t0)

.outOfBoundsTrunc:
    throwException(OutOfBoundsTrunc)
end)

wasmOp(i32_trunc_u_f32, WasmI32TruncUF32, macro (ctx)
    mloadf(ctx, m_operand, ft0)

    move 0xbf800000, t0 # -1.0
    fi2f t0, ft1
    bfltequn ft0, ft1, .outOfBoundsTrunc

    move 0x4f800000, t0 # INT32_MIN * -2.0
    fi2f t0, ft1
    bfgtequn ft0, ft1, .outOfBoundsTrunc

    truncatef2i ft0, t0
    returni(ctx, t0)

.outOfBoundsTrunc:
    throwException(OutOfBoundsTrunc)
end)

wasmOp(i32_trunc_sat_f32_s, WasmI32TruncSatF32S, macro (ctx)
    mloadf(ctx, m_operand, ft0)

    move 0xcf000000, t0 # INT32_MIN (Note that INT32_MIN - 1.0 in float is the same as INT32_MIN in float).
    fi2f t0, ft1
    bfltun ft0, ft1, .outOfBoundsTruncSatMinOrNaN

    move 0x4f000000, t0 # -INT32_MIN
    fi2f t0, ft1
    bfgtequn ft0, ft1, .outOfBoundsTruncSatMax

    truncatef2is ft0, t0
    returni(ctx, t0)

.outOfBoundsTruncSatMinOrNaN:
    bfeq ft0, ft0, .outOfBoundsTruncSatMin
    move 0, t0
    returni(ctx, t0)

.outOfBoundsTruncSatMax:
    move (constexpr INT32_MAX), t0
    returni(ctx, t0)

.outOfBoundsTruncSatMin:
    move (constexpr INT32_MIN), t0
    returni(ctx, t0)
end)

wasmOp(i32_trunc_sat_f32_u, WasmI32TruncSatF32U, macro (ctx)
    mloadf(ctx, m_operand, ft0)

    move 0xbf800000, t0 # -1.0
    fi2f t0, ft1
    bfltequn ft0, ft1, .outOfBoundsTruncSatMin

    move 0x4f800000, t0 # INT32_MIN * -2.0
    fi2f t0, ft1
    bfgtequn ft0, ft1, .outOfBoundsTruncSatMax

    truncatef2i ft0, t0
    returni(ctx, t0)

.outOfBoundsTruncSatMin:
    move 0, t0
    returni(ctx, t0)

.outOfBoundsTruncSatMax:
    move (constexpr UINT32_MAX), t0
    returni(ctx, t0)
end)

wasmI64ToFOp(f32_convert_u_i64, WasmF32ConvertUI64, macro (ctx)
    mloadq(ctx, m_operand, t0)
    if X86_64
        cq2f t0, t1, ft0
    else
        cq2f t0, ft0
    end
    returnf(ctx, ft0)
end)

wasmI64ToFOp(f64_convert_u_i64, WasmF64ConvertUI64, macro (ctx)
    mloadq(ctx, m_operand, t0)
    if X86_64
        cq2d t0, t1, ft0
    else
        cq2d t0, ft0
    end
    returnd(ctx, ft0)
end)

wasmOp(i32_eqz, WasmI32Eqz, macro(ctx)
    mloadi(ctx, m_operand, t0)
    cieq t0, 0, t0
    returni(ctx, t0)
end)

wasmOp(f32_min, WasmF32Min, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)

    bfeq ft0, ft1, .equal
    bflt ft0, ft1, .lt
    bfgt ft0, ft1, .return

.NaN:
    addf ft0, ft1
    jmp .return

.equal:
    orf ft0, ft1
    jmp .return

.lt:
    moved ft0, ft1

.return:
    returnf(ctx, ft1)
end)

wasmOp(f32_max, WasmF32Max, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)

    bfeq ft1, ft0, .equal
    bflt ft1, ft0, .lt
    bfgt ft1, ft0, .return

.NaN:
    addf ft0, ft1
    jmp .return

.equal:
    andf ft0, ft1
    jmp .return

.lt:
    moved ft0, ft1

.return:
    returnf(ctx, ft1)
end)

wasmOp(f32_copysign, WasmF32Copysign, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)

    ff2i ft1, t1
    move 0x80000000, t2
    andi t2, t1

    ff2i ft0, t0
    move 0x7fffffff, t2
    andi t2, t0

    ori t1, t0
    fi2f t0, ft0
    returnf(ctx, ft0)
end)

wasmOp(f64_min, WasmF64Min, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)

    bdeq ft0, ft1, .equal
    bdlt ft0, ft1, .lt
    bdgt ft0, ft1, .return

.NaN:
    addd ft0, ft1
    jmp .return

.equal:
    ord ft0, ft1
    jmp .return

.lt:
    moved ft0, ft1

.return:
    returnd(ctx, ft1)
end)

wasmOp(f64_max, WasmF64Max, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)

    bdeq ft1, ft0, .equal
    bdlt ft1, ft0, .lt
    bdgt ft1, ft0, .return

.NaN:
    addd ft0, ft1
    jmp .return

.equal:
    andd ft0, ft1
    jmp .return

.lt:
    moved ft0, ft1

.return:
    returnd(ctx, ft1)
end)

wasmOp(f32_convert_u_i32, WasmF32ConvertUI32, macro(ctx)
    mloadi(ctx, m_operand, t0)
    ci2f t0, ft0
    returnf(ctx, ft0)
end)

wasmOp(f64_convert_u_i32, WasmF64ConvertUI32, macro(ctx)
    mloadi(ctx, m_operand, t0)
    ci2d t0, ft0
    returnd(ctx, ft0)
end)

wasmOp(i32_add, WasmI32Add, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    addi t0, t1, t2
    returni(ctx, t2)
end)

wasmOp(i32_sub, WasmI32Sub, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    subi t1, t0
    returni(ctx, t0)
end)

wasmOp(i32_mul, WasmI32Mul, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    muli t0, t1
    returni(ctx, t1)
end)

wasmOp(i32_and, WasmI32And, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    andi t0, t1
    returni(ctx, t1)
end)

wasmOp(i32_or, WasmI32Or, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    ori t0, t1
    returni(ctx, t1)
end)

wasmOp(i32_xor, WasmI32Xor, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    xori t0, t1
    returni(ctx, t1)
end)

wasmOp(i32_shl, WasmI32Shl, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    lshifti t1, t0
    returni(ctx, t0)
end)

wasmOp(i32_shr_u, WasmI32ShrU, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    urshifti t1, t0
    returni(ctx, t0)
end)

wasmOp(i32_shr_s, WasmI32ShrS, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    rshifti t1, t0
    returni(ctx, t0)
end)

wasmOp(i32_rotr, WasmI32Rotr, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    rrotatei t1, t0
    returni(ctx, t0)
end)

wasmOp(i32_rotl, WasmI32Rotl, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    lrotatei t1, t0
    returni(ctx, t0)
end)

wasmOp(i32_eq, WasmI32Eq, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    cieq t0, t1, t2
    returni(ctx, t2)
end)

wasmOp(i32_ne, WasmI32Ne, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    cineq t0, t1, t2
    returni(ctx, t2)
end)

wasmOp(i32_lt_s, WasmI32LtS, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    cilt t0, t1, t2
    returni(ctx, t2)
end)

wasmOp(i32_le_s, WasmI32LeS, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    cilteq t0, t1, t2
    returni(ctx, t2)
end)

wasmOp(i32_lt_u, WasmI32LtU, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    cib t0, t1, t2
    returni(ctx, t2)
end)

wasmOp(i32_le_u, WasmI32LeU, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    cibeq t0, t1, t2
    returni(ctx, t2)
end)

wasmOp(i32_gt_s, WasmI32GtS, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    cigt t0, t1, t2
    returni(ctx, t2)
end)

wasmOp(i32_ge_s, WasmI32GeS, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    cigteq t0, t1, t2
    returni(ctx, t2)
end)

wasmOp(i32_gt_u, WasmI32GtU, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    cia t0, t1, t2
    returni(ctx, t2)
end)

wasmOp(i32_ge_u, WasmI32GeU, macro(ctx)
    mloadi(ctx, m_lhs, t0)
    mloadi(ctx, m_rhs, t1)
    ciaeq t0, t1, t2
    returni(ctx, t2)
end)

wasmOp(i32_clz, WasmI32Clz, macro(ctx)
    mloadi(ctx, m_operand, t0)
    lzcnti t0, t1
    returni(ctx, t1)
end)

wasmOp(f32_add, WasmF32Add, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)
    addf ft0, ft1
    returnf(ctx, ft1)
end)

wasmOp(f32_sub, WasmF32Sub, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)
    subf ft1, ft0
    returnf(ctx, ft0)
end)

wasmOp(f32_mul, WasmF32Mul, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)
    mulf ft0, ft1
    returnf(ctx, ft1)
end)

wasmOp(f32_div, WasmF32Div, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)
    divf ft1, ft0
    returnf(ctx, ft0)
end)

wasmOp(f32_abs, WasmF32Abs, macro(ctx)
    mloadf(ctx, m_operand, ft0)
    absf ft0, ft1
    returnf(ctx, ft1)
end)

wasmOp(f32_neg, WasmF32Neg, macro(ctx)
    mloadf(ctx, m_operand, ft0)
    negf ft0, ft1
    returnf(ctx, ft1)
end)

wasmRoundingOp(f32_ceil, WasmF32Ceil, macro(ctx)
    mloadf(ctx, m_operand, ft0)
    ceilf ft0, ft1
    returnf(ctx, ft1)
end)

wasmRoundingOp(f32_floor, WasmF32Floor, macro(ctx)
    mloadf(ctx, m_operand, ft0)
    floorf ft0, ft1
    returnf(ctx, ft1)
end)

wasmOp(f32_sqrt, WasmF32Sqrt, macro(ctx)
    mloadf(ctx, m_operand, ft0)
    sqrtf ft0, ft1
    returnf(ctx, ft1)
end)

wasmOp(f32_eq, WasmF32Eq, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)
    cfeq ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(f32_ne, WasmF32Ne, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)
    cfnequn ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(f32_lt, WasmF32Lt, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)
    cflt ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(f32_le, WasmF32Le, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)
    cflteq ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(f32_gt, WasmF32Gt, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)
    cfgt ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(f32_ge, WasmF32Ge, macro(ctx)
    mloadf(ctx, m_lhs, ft0)
    mloadf(ctx, m_rhs, ft1)
    cfgteq ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(f64_add, WasmF64Add, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)
    addd ft0, ft1
    returnd(ctx, ft1)
end)

wasmOp(f64_sub, WasmF64Sub, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)
    subd ft1, ft0
    returnd(ctx, ft0)
end)

wasmOp(f64_mul, WasmF64Mul, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)
    muld ft0, ft1
    returnd(ctx, ft1)
end)

wasmOp(f64_div, WasmF64Div, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)
    divd ft1, ft0
    returnd(ctx, ft0)
end)

wasmOp(f64_abs, WasmF64Abs, macro(ctx)
    mloadd(ctx, m_operand, ft0)
    absd ft0, ft1
    returnd(ctx, ft1)
end)

wasmOp(f64_neg, WasmF64Neg, macro(ctx)
    mloadd(ctx, m_operand, ft0)
    negd ft0, ft1
    returnd(ctx, ft1)
end)

wasmRoundingOp(f64_ceil, WasmF64Ceil, macro(ctx)
    mloadd(ctx, m_operand, ft0)
    ceild ft0, ft1
    returnd(ctx, ft1)
end)

wasmRoundingOp(f64_floor, WasmF64Floor, macro(ctx)
    mloadd(ctx, m_operand, ft0)
    floord ft0, ft1
    returnd(ctx, ft1)
end)

wasmOp(f64_sqrt, WasmF64Sqrt, macro(ctx)
    mloadd(ctx, m_operand, ft0)
    sqrtd ft0, ft1
    returnd(ctx, ft1)
end)

wasmOp(f64_eq, WasmF64Eq, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)
    cdeq ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(f64_ne, WasmF64Ne, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)
    cdnequn ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(f64_lt, WasmF64Lt, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)
    cdlt ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(f64_le, WasmF64Le, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)
    cdlteq ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(f64_gt, WasmF64Gt, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)
    cdgt ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(f64_ge, WasmF64Ge, macro(ctx)
    mloadd(ctx, m_lhs, ft0)
    mloadd(ctx, m_rhs, ft1)
    cdgteq ft0, ft1, t0
    returni(ctx, t0)
end)

wasmOp(i32_wrap_i64, WasmI32WrapI64, macro(ctx)
    mloadi(ctx, m_operand, t0)
    returni(ctx, t0)
end)

wasmOp(i32_extend8_s, WasmI32Extend8S, macro(ctx)
    mloadi(ctx, m_operand, t0)
    sxb2i t0, t1
    returni(ctx, t1)
end)

wasmOp(i32_extend16_s, WasmI32Extend16S, macro(ctx)
    mloadi(ctx, m_operand, t0)
    sxh2i t0, t1
    returni(ctx, t1)
end)

wasmOp(f32_convert_s_i32, WasmF32ConvertSI32, macro(ctx)
    mloadi(ctx, m_operand, t0)
    ci2fs t0, ft0
    returnf(ctx, ft0)
end)

wasmI64ToFOp(f32_convert_s_i64, WasmF32ConvertSI64, macro(ctx)
    mloadq(ctx, m_operand, t0)
    cq2fs t0, ft0
    returnf(ctx, ft0)
end)

wasmOp(f32_demote_f64, WasmF32DemoteF64, macro(ctx)
    mloadd(ctx, m_operand, ft0)
    cd2f ft0, ft1
    returnf(ctx, ft1)
end)

wasmOp(f32_reinterpret_i32, WasmF32ReinterpretI32, macro(ctx)
    mloadi(ctx, m_operand, t0)
    fi2f t0, ft0
    returnf(ctx, ft0)
end)

wasmOp(f64_convert_s_i32, WasmF64ConvertSI32, macro(ctx)
    mloadi(ctx, m_operand, t0)
    ci2ds t0, ft0
    returnd(ctx, ft0)
end)

wasmI64ToFOp(f64_convert_s_i64, WasmF64ConvertSI64, macro(ctx)
    mloadq(ctx, m_operand, t0)
    cq2ds t0, ft0
    returnd(ctx, ft0)
end)

wasmOp(f64_promote_f32, WasmF64PromoteF32, macro(ctx)
    mloadf(ctx, m_operand, ft0)
    cf2d ft0, ft1
    returnd(ctx, ft1)
end)

wasmOp(i32_reinterpret_f32, WasmI32ReinterpretF32, macro(ctx)
    mloadf(ctx, m_operand, ft0)
    ff2i ft0, t0
    returni(ctx, t0)
end)

macro dropKeep(startOffset, drop, keep)
    lshifti 3, startOffset
    subp cfr, startOffset, startOffset
    negi drop
if JSVALUE64
    sxi2q drop, drop
end

.copyLoop:
    btiz keep, .done
if JSVALUE64
    loadq [startOffset, drop, 8], t6
    storeq t6, [startOffset]
else
    load2ia [startOffset, drop, 8], t5, t6
    store2ia t5, t6, [startOffset]
end
    subi 1, keep
    subp 8, startOffset
    jmp .copyLoop

.done:
end

wasmOp(drop_keep, WasmDropKeep, macro(ctx)
    wgetu(ctx, m_startOffset, t0)
    wgetu(ctx, m_dropCount, t1)
    wgetu(ctx, m_keepCount, t2)

    dropKeep(t0, t1, t2)

    dispatch(ctx)
end)

wasmOp(atomic_fence, WasmDropKeep, macro(ctx)
    fence
    dispatch(ctx)
end)

wasmOp(throw, WasmThrow, macro(ctx)
    loadp JSWebAssemblyInstance::m_vm[wasmInstance], t5
    loadp VM::topEntryFrame[t5], t5
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(t5)

    callWasmSlowPath(_slow_path_wasm_throw)
    jumpToException()
end)

wasmOp(rethrow, WasmRethrow, macro(ctx)
    loadp JSWebAssemblyInstance::m_vm[wasmInstance], t5
    loadp VM::topEntryFrame[t5], t5
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(t5)

    callWasmSlowPath(_slow_path_wasm_rethrow)
    jumpToException()
end)

wasmOp(throw_ref, WasmThrowRef, macro(ctx)
    loadp JSWebAssemblyInstance::m_vm[wasmInstance], t5
    loadp VM::topEntryFrame[t5], t5
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(t5)

    callWasmSlowPath(_slow_path_wasm_throw_ref)
    btpz r1, .throw
    jumpToException()
.throw:
    restoreStateAfterCCall()
    dispatch(ctx)
end)

macro commonCatchImpl(ctx)
    getVMFromCallFrame(t3, t0)
    restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer(t3, t0)

    loadp VM::callFrameForCatch[t3], cfr
    storep 0, VM::callFrameForCatch[t3]

    restoreStackPointerAfterCall()

    loadp CodeBlock[cfr], wasmInstance
    reloadMemoryRegistersFromInstance(wasmInstance, ws0)

    loadp UnboxedWasmCalleeStackSlot[cfr], PB
    loadp Wasm::LLIntCallee::m_instructionsRawPointer[PB], PB
    loadp VM::targetInterpreterPCForThrow[t3], PC
    subp PB, PC

    callWasmSlowPath(_slow_path_wasm_retrieve_and_clear_exception)
end

commonWasmOp(wasm_catch, WasmCatch, macro() end, macro(ctx)
    commonCatchImpl(ctx)

    move r1, t1

    wgetu(ctx, m_startOffset, t2)
    wgetu(ctx, m_argumentCount, t3)

    lshifti 3, t2
    subp cfr, t2, t2

.copyLoop:
    btiz t3, .done
if JSVALUE64
    loadq [t1], t6
    storeq t6, [t2]
else
    load2ia [t1], t5, t6
    store2ia t5, t6, [t2]
end
    subi 1, t3
    # FIXME: Use arm store-add/sub instructions in wasm LLInt catch
    # https://bugs.webkit.org/show_bug.cgi?id=231210
    subp 8, t2
    addp 8, t1
    jmp .copyLoop

.done:
    traceExecution()
    dispatch(ctx)
end)

commonWasmOp(wasm_catch_all, WasmCatchAll, macro() end, macro(ctx)
    commonCatchImpl(ctx)
    traceExecution()
    dispatch(ctx)
end)

commonWasmOp(wasm_try_table_catch, WasmTryTableCatch, macro() end, macro(ctx)
    # push the arguments to the stack
    commonCatchImpl(ctx)

    move r1, t1

    wgetu(ctx, m_startOffset, t2)
    wgetu(ctx, m_argumentCount, t3)

    lshifti 3, t2
    subp cfr, t2, t2

.copyLoop:
    btiz t3, .done
if JSVALUE64
    loadq [t1], t6
    storeq t6, [t2]
else
    load2ia [t1], t5, t6
    store2ia t5, t6, [t2]
end
    subi 1, t3
    # FIXME: Use arm store-add/sub instructions in wasm LLInt catch
    # https://bugs.webkit.org/show_bug.cgi?id=231210
    subp 8, t2
    addp 8, t1
    jmp .copyLoop

.done:
    dispatch(ctx)
end)

commonWasmOp(wasm_try_table_catchref, WasmTryTableCatch, macro() end, macro(ctx)
    # push the arguments, exnref handled by the slow path
    commonCatchImpl(ctx)

    move r1, t1

    wgetu(ctx, m_startOffset, t2)
    wgetu(ctx, m_argumentCount, t3)

    lshifti 3, t2
    subp cfr, t2, t2

.copyLoop:
    btiz t3, .done
if JSVALUE64
    loadq [t1], t6
    storeq t6, [t2]
else
    load2ia [t1], t5, t6
    store2ia t5, t6, [t2]
end
    subi 1, t3
    # FIXME: Use arm store-add/sub instructions in wasm LLInt catch
    # https://bugs.webkit.org/show_bug.cgi?id=231210
    subp 8, t2
    addp 8, t1
    jmp .copyLoop

.done:
    dispatch(ctx)
end)

commonWasmOp(wasm_try_table_catchall, WasmTryTableCatch, macro() end, macro(ctx)
    # don't do anything, we don't push at all just jump
    commonCatchImpl(ctx)
    dispatch(ctx)
end)

commonWasmOp(wasm_try_table_catchallref, WasmTryTableCatch, macro() end, macro(ctx)
    # exnref handled by the slow path
    commonCatchImpl(ctx)
    dispatch(ctx)
end)

# Value-representation-specific code.
if JSVALUE64
    include WebAssembly64
else
    include WebAssembly32_64
end
