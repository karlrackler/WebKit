/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "CoreIPCStringSet.h"

#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(COCOA)

OBJC_CLASS NSSet;

namespace WebKit {

CoreIPCStringSet::CoreIPCStringSet(NSSet *stringSet)
{
    if (![stringSet isKindOfClass:[NSSet class]])
        return;
    for (id value in stringSet) {
        if (![value isKindOfClass:[NSString class]])
            continue;
        m_stringSet.append(value);
    }
}

CoreIPCStringSet::CoreIPCStringSet(Vector<String>&& strings)
    : m_stringSet(WTFMove(strings)) { }

CoreIPCStringSet::~CoreIPCStringSet() = default;

RetainPtr<id> CoreIPCStringSet::toID() const
{
    RetainPtr result = adoptNS([[NSMutableArray alloc] init]);
    for (auto& s : m_stringSet)
        [result addObject:s.createNSString().get()];

    RetainPtr<NSSet> set = [NSSet setWithArray:result.get()];
    return set;
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
