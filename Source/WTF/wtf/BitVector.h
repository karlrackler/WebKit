/*
 * Copyright (C) 2011-2023 Apple Inc. All rights reserved.
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

#include <stdio.h>
#include <wtf/Assertions.h>
#include <wtf/DataLog.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>
#include <wtf/PrintStream.h>
#include <wtf/StdLibExtras.h>

#if USE(CF)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace JSC {
class CachedBitVector;
}

namespace WTF {

class FixedBitVector;

// This is a space-efficient, resizeable bitvector class. In the common case it
// occupies one word, but if necessary, it will inflate this one word to point
// to a single chunk of out-of-line allocated storage to store an arbitrary number
// of bits.
//
// - The bitvector remembers the bound of how many bits can be stored, but this
//   may be slightly greater (by as much as some platform-specific constant)
//   than the last argument passed to ensureSize().
//
// - The bitvector can resize itself automatically (set, clear, get) or can be used
//   in a manual mode, which is faster (quickSet, quickClear, quickGet, ensureSize).
//
// - Accesses ASSERT that you are within bounds.
//
// - Bits are automatically initialized to zero.
//
// On the other hand, this BitVector class may not be the fastest around, since
// it does conditionals on every get/set/clear. But it is great if you need to
// juggle a lot of variable-length BitVectors and you're worried about wasting
// space.

// If you know the length of the vector at compile-time,
// please consider using WTF::BitSet instead.
class BitVector final {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(BitVector);
public: 
    BitVector()
        : m_bitsOrPointer(makeInlineBits(0))
    {
    }
    
    explicit BitVector(size_t numBits)
        : m_bitsOrPointer(makeInlineBits(0))
    {
        ensureSize(numBits);
    }
    
    BitVector(const BitVector& other)
        : m_bitsOrPointer(makeInlineBits(0))
    {
        (*this) = other;
    }

#if USE(CF)
    BitVector(CFBitVectorRef bitVector)
        : BitVector(CFBitVectorGetCount(bitVector))
    {
        auto count = CFBitVectorGetCount(bitVector);
        for (CFIndex i = 0; i < count; ++i) {
            if (CFBitVectorGetBitAtIndex(bitVector, i))
                quickSet(i);
        }
    }
#endif
    
    ~BitVector()
    {
        if (isInline())
            return;
        OutOfLineBits::destroy(outOfLineBits());
    }
    
    BitVector& operator=(const BitVector& other)
    {
        if (isInline() && other.isInline())
            m_bitsOrPointer = other.m_bitsOrPointer;
        else
            setSlow(other);
        return *this;
    }

    size_t size() const
    {
        if (isInline())
            return maxInlineBits();
        return outOfLineBits()->numBits();
    }

    void ensureSize(size_t numBits)
    {
        if (numBits <= size())
            return;
        resizeOutOfLine(numBits);
    }
    
    // Like ensureSize(), but supports reducing the size of the bitvector.
    WTF_EXPORT_PRIVATE void resize(size_t numBits);
    
    WTF_EXPORT_PRIVATE void clearAll();

    bool quickGet(size_t bit) const
    {
        ASSERT_WITH_SECURITY_IMPLICATION(bit < size());
        return !!(words()[bit / bitsInPointer()] & (static_cast<uintptr_t>(1) << (bit & (bitsInPointer() - 1))));
    }
    
    bool quickSet(size_t bit)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(bit < size());
        uintptr_t& word = words()[bit / bitsInPointer()];
        uintptr_t mask = static_cast<uintptr_t>(1) << (bit & (bitsInPointer() - 1));
        bool result = !!(word & mask);
        word |= mask;
        return result;
    }
    
    bool quickClear(size_t bit)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(bit < size());
        uintptr_t& word = words()[bit / bitsInPointer()];
        uintptr_t mask = static_cast<uintptr_t>(1) << (bit & (bitsInPointer() - 1));
        bool result = !!(word & mask);
        word &= ~mask;
        return result;
    }
    
    bool quickSet(size_t bit, bool value)
    {
        if (value)
            return quickSet(bit);
        return quickClear(bit);
    }
    
    bool get(size_t bit) const
    {
        if (bit >= size())
            return false;
        return quickGet(bit);
    }

    bool contains(size_t bit) const
    {
        return get(bit);
    }
    
    bool set(size_t bit)
    {
        ensureSize(bit + 1);
        return quickSet(bit);
    }

    // This works like the add methods of sets. Instead of returning the previous value, like set(),
    // it returns whether the bit transitioned from false to true.
    bool add(size_t bit)
    {
        return !set(bit);
    }

    bool ensureSizeAndSet(size_t bit, size_t size)
    {
        ensureSize(size);
        return quickSet(bit);
    }

    bool clear(size_t bit)
    {
        if (bit >= size())
            return false;
        return quickClear(bit);
    }

    bool remove(size_t bit)
    {
        return clear(bit);
    }
    
    bool set(size_t bit, bool value)
    {
        if (value)
            return set(bit);
        return clear(bit);
    }
    
    void merge(const BitVector& other)
    {
        if (!isInline() || !other.isInline()) {
            mergeSlow(other);
            return;
        }
        m_bitsOrPointer |= other.m_bitsOrPointer;
        ASSERT(isInline());
    }
    
    void filter(const BitVector& other)
    {
        if (!isInline() || !other.isInline()) {
            filterSlow(other);
            return;
        }
        m_bitsOrPointer &= other.m_bitsOrPointer;
        ASSERT(isInline());
    }
    
    void exclude(const BitVector& other)
    {
        if (!isInline() || !other.isInline()) {
            excludeSlow(other);
            return;
        }
        m_bitsOrPointer &= ~other.m_bitsOrPointer;
        m_bitsOrPointer |= (static_cast<uintptr_t>(1) << maxInlineBits());
        ASSERT(isInline());
    }
    
    size_t bitCount() const
    {
        if (isInline())
            return bitCount(cleanseInlineBits(m_bitsOrPointer));
        return bitCountSlow();
    }

    bool isEmpty() const
    {
        if (isInline())
            return !cleanseInlineBits(m_bitsOrPointer);
        return isEmptySlow();
    }
    
    size_t findBit(size_t index, bool value) const
    {
        size_t result = findBitFast(index, value);
        if (ASSERT_ENABLED) {
            size_t expectedResult = findBitSimple(index, value);
            if (result != expectedResult) {
                dataLog("findBit(", index, ", ", value, ") on ", *this, " should have gotten ", expectedResult, " but got ", result, "\n");
                ASSERT_NOT_REACHED();
            }
        }
        return result;
    }

    // If the lambda returns an IterationStatus, we use it. The lambda can also return
    // void, in which case, we'll iterate every set bit.
    template<typename Func>
    constexpr ALWAYS_INLINE void forEachSetBit(const Func&) const;

    template<typename Func>
    constexpr ALWAYS_INLINE void forEachSetBit(size_t startIndex, const Func&) const;
    
    WTF_EXPORT_PRIVATE void dump(PrintStream& out) const;
    
    enum EmptyValueTag { EmptyValue };
    enum DeletedValueTag { DeletedValue };
    
    BitVector(EmptyValueTag)
        : m_bitsOrPointer(0)
    {
    }
    
    BitVector(DeletedValueTag)
        : m_bitsOrPointer(1)
    {
    }
    
    bool isEmptyValue() const { return !m_bitsOrPointer; }
    bool isDeletedValue() const { return m_bitsOrPointer == 1; }
    
    bool isEmptyOrDeletedValue() const { return m_bitsOrPointer <= 1; }
    
    bool operator==(const BitVector& other) const
    {
        if (isInline() && other.isInline())
            return m_bitsOrPointer == other.m_bitsOrPointer;
        return equalsSlowCase(other);
    }
    
    unsigned hash() const
    {
        // This is a very simple hash. Just xor together the words that hold the various
        // bits and then compute the hash. This makes it very easy to deal with bitvectors
        // that have a lot of trailing zero's.
        uintptr_t value;
        if (isInline())
            value = cleanseInlineBits(m_bitsOrPointer);
        else
            value = hashSlowCase();
        return IntHash<uintptr_t>::hash(value);
    }
    
    class iterator {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(iterator);
    public:
        iterator() = default;
        
        iterator(const BitVector& bitVector, size_t index)
            : m_bitVector(&bitVector)
            , m_index(index)
        {
        }
        
        size_t operator*() const { return m_index; }
        
        iterator& operator++()
        {
            m_index = m_bitVector->findBit(m_index + 1, true);
            return *this;
        }

        iterator operator++(int)
        {
            iterator result = *this;
            ++(*this);
            return result;
        }

        bool isAtEnd() const
        {
            return m_index >= m_bitVector->size();
        }
        
        bool operator==(const iterator& other) const
        {
            return m_index == other.m_index;
        }
        
    private:
        const BitVector* m_bitVector { nullptr };
        size_t m_index { 0 };
    };

    // Use this to iterate over set bits.
    iterator begin() const LIFETIME_BOUND { return iterator(*this, findBit(0, true)); }
    iterator end() const LIFETIME_BOUND { return iterator(*this, size()); }

    static unsigned outOfLineMemoryUse(size_t bitCount)
    {
        if (bitCount <= maxInlineBits())
            return 0;
        return byteCount(bitCount);
    }
    unsigned outOfLineMemoryUse() const { return outOfLineMemoryUse(size()); }
        
    WTF_EXPORT_PRIVATE void shiftRightByMultipleOf64(size_t);

private:
    friend class JSC::CachedBitVector;
    friend class FixedBitVector;

    static unsigned bitsInPointer()
    {
        return sizeof(void*) << 3;
    }

    static unsigned maxInlineBits()
    {
        return bitsInPointer() - 1;
    }

    static size_t byteCount(size_t bitCount)
    {
        return (bitCount + 7) >> 3;
    }

    static uintptr_t makeInlineBits(uintptr_t bits)
    {
        ASSERT(!(bits & (static_cast<uintptr_t>(1) << maxInlineBits())));
        return bits | (static_cast<uintptr_t>(1) << maxInlineBits());
    }
    
    static uintptr_t cleanseInlineBits(uintptr_t bits)
    {
        return bits & ~(static_cast<uintptr_t>(1) << maxInlineBits());
    }
    
    static size_t bitCount(uintptr_t bits)
    {
        if (sizeof(uintptr_t) == 4)
            return WTF::bitCount(static_cast<unsigned>(bits));
        return WTF::bitCount(static_cast<uint64_t>(bits));
    }
    
    size_t findBitFast(size_t startIndex, bool value) const
    {
        if (isInline()) {
            size_t index = startIndex;
            findBitInWord(m_bitsOrPointer, index, maxInlineBits(), value);
            return index;
        }
        
        const OutOfLineBits* bits = outOfLineBits();
        
        // value = true: casts to 1, then xors to 0, then negates to 0.
        // value = false: casts to 0, then xors to 1, then negates to -1 (i.e. all one bits).
        uintptr_t skipValue = -(static_cast<uintptr_t>(value) ^ 1);
        
        size_t wordIndex = startIndex / bitsInPointer();
        size_t startIndexInWord = startIndex - wordIndex * bitsInPointer();
        
        auto words = bits->wordsSpan();
        while (wordIndex < words.size()) {
            uintptr_t word = words[wordIndex];
            if (word != skipValue) {
                size_t index = startIndexInWord;
                if (findBitInWord(word, index, bitsInPointer(), value))
                    return wordIndex * bitsInPointer() + index;
            }
            
            ++wordIndex;
            startIndexInWord = 0;
        }
        
        return bits->numBits();
    }
    
    size_t findBitSimple(size_t index, bool value) const
    {
        while (index < size()) {
            if (get(index) == value)
                return index;
            ++index;
        }
        return size();
    }
    
    class OutOfLineBits {
    public:
        size_t numBits() const { return m_numBits; }
        size_t numWords() const { return (m_numBits + bitsInPointer() - 1) / bitsInPointer(); }

        std::span<const uint8_t> byteSpan() const LIFETIME_BOUND { return unsafeMakeSpan(reinterpret_cast<const uint8_t*>(m_words), byteCount(numBits())); }
        std::span<uint8_t> byteSpan() LIFETIME_BOUND { return unsafeMakeSpan(reinterpret_cast<uint8_t*>(m_words), byteCount(numBits())); }
        std::span<const uintptr_t> wordsSpan() const LIFETIME_BOUND { return unsafeMakeSpan(m_words, numWords()); }
        std::span<uintptr_t> wordsSpan() LIFETIME_BOUND { return unsafeMakeSpan(m_words, numWords()); }

        static WTF_EXPORT_PRIVATE OutOfLineBits* create(size_t numBits);
        
        static WTF_EXPORT_PRIVATE void destroy(OutOfLineBits*);

    private:
        OutOfLineBits(size_t numBits)
            : m_numBits(numBits)
        {
        }

        size_t m_numBits;
        uintptr_t m_words[0];
    };
    
    bool isInline() const { return m_bitsOrPointer >> maxInlineBits(); }
    
    const OutOfLineBits* outOfLineBits() const { return std::bit_cast<const OutOfLineBits*>(m_bitsOrPointer << 1); }
    OutOfLineBits* outOfLineBits() { return std::bit_cast<OutOfLineBits*>(m_bitsOrPointer << 1); }
    
    WTF_EXPORT_PRIVATE void resizeOutOfLine(size_t numBits, size_t shiftInWords = 0);
    WTF_EXPORT_PRIVATE void setSlow(const BitVector& other);
    
    WTF_EXPORT_PRIVATE void mergeSlow(const BitVector& other);
    WTF_EXPORT_PRIVATE void filterSlow(const BitVector& other);
    WTF_EXPORT_PRIVATE void excludeSlow(const BitVector& other);
    
    WTF_EXPORT_PRIVATE size_t bitCountSlow() const;
    WTF_EXPORT_PRIVATE bool isEmptySlow() const;
    
    WTF_EXPORT_PRIVATE bool equalsSlowCase(const BitVector& other) const;
    bool equalsSlowCaseFast(const BitVector& other) const;
    bool equalsSlowCaseSimple(const BitVector& other) const;
    WTF_EXPORT_PRIVATE uintptr_t hashSlowCase() const;
    
    std::span<uintptr_t> words()
    {
        if (isInline())
            return singleElementSpan(m_bitsOrPointer);
        return outOfLineBits()->wordsSpan();
    }

    std::span<const uintptr_t> words() const
    {
        if (isInline())
            return singleElementSpan(m_bitsOrPointer);
        return outOfLineBits()->wordsSpan();
    }

    std::span<uint8_t> byteSpan() LIFETIME_BOUND { return asMutableByteSpan(words()).first(byteCount(size())); }
    std::span<const uint8_t> byteSpan() const LIFETIME_BOUND { return asByteSpan(words()).first(byteCount(size())); }

    uintptr_t m_bitsOrPointer;
};

template<typename Func>
ALWAYS_INLINE constexpr void BitVector::forEachSetBit(const Func& func) const
{
    const uintptr_t copiedInline = cleanseInlineBits(m_bitsOrPointer);
    auto words = isInline() ? singleElementSpan(copiedInline) : outOfLineBits()->wordsSpan();
    WTF::forEachSetBit(words, func);
}

template<typename Func>
ALWAYS_INLINE constexpr void BitVector::forEachSetBit(size_t startIndex, const Func& func) const
{
    const uintptr_t copiedInline = cleanseInlineBits(m_bitsOrPointer);
    auto words = isInline() ? singleElementSpan(copiedInline) : outOfLineBits()->wordsSpan();
    WTF::forEachSetBit(words, startIndex, func);
}

struct BitVectorHash {
    static unsigned hash(const BitVector& vector) { return vector.hash(); }
    static bool equal(const BitVector& a, const BitVector& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

template<typename T> struct DefaultHash;
template<> struct DefaultHash<BitVector> : BitVectorHash { };

template<> struct HashTraits<BitVector> : public CustomHashTraits<BitVector> { };

} // namespace WTF

using WTF::BitVector;
