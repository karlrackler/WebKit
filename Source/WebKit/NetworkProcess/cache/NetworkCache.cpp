/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "NetworkCache.h"

#include "AsyncRevalidation.h"
#include "Logging.h"
#include "NetworkCacheSpeculativeLoad.h"
#include "NetworkCacheSpeculativeLoadManager.h"
#include "NetworkCacheStorage.h"
#include "NetworkProcess.h"
#include "NetworkSession.h"
#include "WebsiteDataType.h"
#include <WebCore/CacheValidation.h>
#include <WebCore/HTTPHeaderNames.h>
#include <WebCore/HTTPStatusCodes.h>
#include <WebCore/LowPowerModeNotifier.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/ThermalMitigationNotifier.h>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(COCOA)
#include <notify.h>
#endif

namespace WebKit {
namespace NetworkCache {

using namespace FileSystem;

WTF_MAKE_TZONE_ALLOCATED_IMPL(Cache::RetrieveInfo);

static const AtomString& resourceType()
{
    ASSERT(WTF::RunLoop::isMain());
    static NeverDestroyed<const AtomString> resource("Resource"_s);
    return resource;
}

static size_t computeCapacity(CacheModel cacheModel, const String& cachePath)
{
    if (auto diskFreeSize = FileSystem::volumeFreeSpace(cachePath)) {
        // As a fudge factor, use 1000 instead of 1024, in case the reported byte
        // count doesn't align exactly to a megabyte boundary.
        *diskFreeSize /= KB * 1000;
        return calculateURLCacheDiskCapacity(cacheModel, *diskFreeSize);
    }
    return 0;
}

RefPtr<Cache> Cache::open(NetworkProcess& networkProcess, const String& cachePath, OptionSet<CacheOption> options, PAL::SessionID sessionID)
{
    if (!FileSystem::makeAllDirectories(cachePath))
        return nullptr;

    auto capacity = computeCapacity(networkProcess.cacheModel(), cachePath);
    auto storage = Storage::open(cachePath, options.contains(CacheOption::TestingMode) ? Storage::Mode::AvoidRandomness : Storage::Mode::Normal, capacity);

    LOG(NetworkCache, "(NetworkProcess) opened cache storage, success %d", !!storage);

    if (!storage)
        return nullptr;

    return adoptRef(*new Cache(networkProcess, cachePath, storage.releaseNonNull(), options, sessionID));
}

#if PLATFORM(GTK) || PLATFORM(WPE)
static void dumpFileChanged(Cache* cache)
{
    cache->dumpContentsToFile();
}
#endif

Cache::Cache(NetworkProcess& networkProcess, const String& storageDirectory, Ref<Storage>&& storage, OptionSet<CacheOption> options, PAL::SessionID sessionID)
    : m_storage(WTFMove(storage))
    , m_networkProcess(networkProcess)
    , m_sessionID(sessionID)
    , m_storageDirectory(storageDirectory)
{
    if (options.contains(CacheOption::SpeculativeRevalidation)) {
        m_lowPowerModeNotifier = makeUnique<WebCore::LowPowerModeNotifier>([this, weakThis = WeakPtr { *this }](bool) {
            if (RefPtr protectedThis = weakThis.get())
                updateSpeculativeLoadManagerEnabledState();
        });
        m_thermalMitigationNotifier = makeUnique<WebCore::ThermalMitigationNotifier>([this, weakThis = WeakPtr { *this }](bool) {
            if (RefPtr protectedThis = weakThis.get())
                updateSpeculativeLoadManagerEnabledState();
        });
        if (shouldUseSpeculativeLoadManager())
            m_speculativeLoadManager = makeUnique<SpeculativeLoadManager>(*this, protectedStorage());
    }

    if (options.contains(CacheOption::RegisterNotify)) {
#if PLATFORM(COCOA)
        // Triggers with "notifyutil -p com.apple.WebKit.Cache.dump".
        int token;
        notify_register_dispatch("com.apple.WebKit.Cache.dump", &token, dispatch_get_main_queue(), ^(int) {
            dumpContentsToFile();
        });
#endif
#if PLATFORM(GTK) || PLATFORM(WPE)
        // Triggers with "touch $cachePath/dump".
        CString dumpFilePath = fileSystemRepresentation(pathByAppendingComponent(m_storage->basePathIsolatedCopy(), "dump"_s));
        GRefPtr<GFile> dumpFile = adoptGRef(g_file_new_for_path(dumpFilePath.data()));
        GFileMonitor* monitor = g_file_monitor_file(dumpFile.get(), G_FILE_MONITOR_NONE, nullptr, nullptr);
        g_signal_connect_swapped(monitor, "changed", G_CALLBACK(dumpFileChanged), this);
#endif
    }
}

Cache::~Cache()
{
}

size_t Cache::capacity() const
{
    return m_storage->capacity();
}

void Cache::updateCapacity()
{
    auto newCapacity = computeCapacity(m_networkProcess->cacheModel(), m_storage->basePathIsolatedCopy());
    m_storage->setCapacity(newCapacity);
}

Key Cache::makeCacheKey(const WebCore::ResourceRequest& request)
{
    // FIXME: This implements minimal Range header disk cache support. We don't parse
    // ranges so only the same exact range request will be served from the cache.
    String range = request.httpHeaderField(WebCore::HTTPHeaderName::Range);
    return { request.cachePartition(), resourceType(), range, request.url().stringWithoutFragmentIdentifier(), m_storage->salt() };
}

static bool cachePolicyAllowsExpired(WebCore::ResourceRequestCachePolicy policy)
{
    switch (policy) {
    case WebCore::ResourceRequestCachePolicy::ReturnCacheDataElseLoad:
    case WebCore::ResourceRequestCachePolicy::ReturnCacheDataDontLoad:
        return true;
    case WebCore::ResourceRequestCachePolicy::UseProtocolCachePolicy:
    case WebCore::ResourceRequestCachePolicy::ReloadIgnoringCacheData:
    case WebCore::ResourceRequestCachePolicy::RefreshAnyCacheData:
        return false;
    case WebCore::ResourceRequestCachePolicy::DoNotUseAnyCache:
        ASSERT_NOT_REACHED();
        return false;
    }
    return false;
}

static UseDecision responseNeedsRevalidation(NetworkSession& networkSession, const WebCore::ResourceResponse& response, WallTime timestamp, std::optional<Seconds> maxStale)
{
    if (response.cacheControlContainsNoCache())
        return UseDecision::Validate;

    auto age = WebCore::computeCurrentAge(response, timestamp);
    auto lifetime = WebCore::computeFreshnessLifetimeForHTTPFamily(response, timestamp);

    auto maximumStaleness = maxStale ? maxStale.value() : 0_ms;
    bool hasExpired = age - lifetime > maximumStaleness;
    if (hasExpired && !maxStale && networkSession.isStaleWhileRevalidateEnabled()) {
        auto responseMaxStaleness = response.cacheControlStaleWhileRevalidate();
        maximumStaleness += responseMaxStaleness ? responseMaxStaleness.value() : 0_ms;
        bool inResponseStaleness = age - lifetime < maximumStaleness;
        if (inResponseStaleness)
            return UseDecision::AsyncRevalidate;
    }

    if (hasExpired) {
#ifndef LOG_DISABLED
        LOG(NetworkCache, "(NetworkProcess) needsRevalidation hasExpired age=%f lifetime=%f max-staleness=%f", age, lifetime, maximumStaleness);
#endif
        return UseDecision::Validate;
    }

    return UseDecision::Use;
}

static UseDecision responseNeedsRevalidation(NetworkSession& networkSession, const WebCore::ResourceResponse& response, const WebCore::ResourceRequest& request, WallTime timestamp)
{
    auto requestDirectives = WebCore::parseCacheControlDirectives(request.httpHeaderFields());
    if (requestDirectives.noCache)
        return UseDecision::Validate;
    // For requests we ignore max-age values other than zero.
    if (requestDirectives.maxAge && requestDirectives.maxAge.value() == 0_ms)
        return UseDecision::Validate;

    return responseNeedsRevalidation(networkSession, response, timestamp, requestDirectives.maxStale);
}

static UseDecision makeUseDecision(NetworkProcess& networkProcess, PAL::SessionID sessionID, const Entry& entry, const WebCore::ResourceRequest& request)
{
    // The request is conditional so we force revalidation from the network. We merely check the disk cache
    // so we can update the cache entry.
    if (request.isConditional() && !entry.redirectRequest())
        return UseDecision::Validate;

    if (!WebCore::verifyVaryingRequestHeaders(networkProcess.checkedStorageSession(sessionID).get(), entry.varyingRequestHeaders(), request))
        return UseDecision::NoDueToVaryingHeaderMismatch;

    // We never revalidate in the case of a history navigation.
    if (cachePolicyAllowsExpired(request.cachePolicy()))
        return UseDecision::Use;

    // We could have cached a redirect without a fragment and now may have
    // a fragment in the URL.
    if (request.url().hasFragmentIdentifier() && entry.redirectRequest())
        return UseDecision::NoDueToRequestContainingFragments;

    auto decision = responseNeedsRevalidation(*networkProcess.checkedNetworkSession(sessionID), entry.response(), request, entry.timeStamp());
    if (decision != UseDecision::Validate)
        return decision;

    if (!entry.response().hasCacheValidatorFields())
        return UseDecision::NoDueToMissingValidatorFields;

    return entry.redirectRequest() ? UseDecision::NoDueToExpiredRedirect : UseDecision::Validate;
}

static RetrieveDecision makeRetrieveDecision(const WebCore::ResourceRequest& request)
{
    ASSERT(request.cachePolicy() != WebCore::ResourceRequestCachePolicy::DoNotUseAnyCache);

    // FIXME: Support HEAD requests.
    if (request.httpMethod() != "GET"_s)
        return RetrieveDecision::NoDueToHTTPMethod;
    if (request.cachePolicy() == WebCore::ResourceRequestCachePolicy::ReloadIgnoringCacheData && !request.isConditional())
        return RetrieveDecision::NoDueToReloadIgnoringCache;

    return RetrieveDecision::Yes;
}

static bool isMediaMIMEType(const String& type)
{
    return startsWithLettersIgnoringASCIICase(type, "video/"_s) || startsWithLettersIgnoringASCIICase(type, "audio/"_s);
}

static StoreDecision makeStoreDecision(const WebCore::ResourceRequest& originalRequest, const WebCore::ResourceResponse& response, size_t bodySize)
{
    if (!originalRequest.url().protocolIsInHTTPFamily() || !response.isInHTTPFamily())
        return StoreDecision::NoDueToProtocol;

    if (originalRequest.httpMethod() != "GET"_s)
        return StoreDecision::NoDueToHTTPMethod;

    auto requestDirectives = WebCore::parseCacheControlDirectives(originalRequest.httpHeaderFields());
    if (requestDirectives.noStore)
        return StoreDecision::NoDueToNoStoreRequest;

    if (response.cacheControlContainsNoStore())
        return StoreDecision::NoDueToNoStoreResponse;

    if (!WebCore::isStatusCodeCacheableByDefault(response.httpStatusCode())) {
        // http://tools.ietf.org/html/rfc7234#section-4.3.2
        bool hasExpirationHeaders = response.expires() || response.cacheControlMaxAge();
        bool expirationHeadersAllowCaching = WebCore::isStatusCodePotentiallyCacheable(response.httpStatusCode()) && hasExpirationHeaders;
        if (!expirationHeadersAllowCaching)
            return StoreDecision::NoDueToHTTPStatusCode;
    }

    // FIXME: We are not correctly computing the redirected request URL in case original request
    // has a fragment identifier and response location URL does not have one. Let's not store it for now.
    if ((response.isRedirection() || response.isRedirected()) && originalRequest.url().hasFragmentIdentifier())
        return StoreDecision::NoDueToRequestContainingFragments;

    bool isMainResource = originalRequest.requester() == WebCore::ResourceRequestRequester::Main;
    bool storeUnconditionallyForHistoryNavigation = isMainResource || originalRequest.priority() == WebCore::ResourceLoadPriority::VeryHigh;
    if (!storeUnconditionallyForHistoryNavigation) {
        auto now = WallTime::now();
        Seconds allowedStale { 0_ms };
        if (auto value = response.cacheControlStaleWhileRevalidate())
            allowedStale = value.value();
        bool hasNonZeroLifetime = !response.cacheControlContainsNoCache() && (WebCore::computeFreshnessLifetimeForHTTPFamily(response, now) > 0_ms || allowedStale > 0_ms);
        bool possiblyReusable = response.hasCacheValidatorFields() || hasNonZeroLifetime;
        if (!possiblyReusable)
            return StoreDecision::NoDueToUnlikelyToReuse;
    }

    // Media loaded via XHR is likely being used for MSE streaming (YouTube and Netflix for example).
    // Streaming media fills the cache quickly and is unlikely to be reused.
    // FIXME: We should introduce a separate media cache partition that doesn't affect other resources.
    // FIXME: We should also make sure make the MSE paths are copy-free so we can use mapped buffers from disk effectively.
    auto requester = originalRequest.requester();
    bool isDefinitelyStreamingMedia = requester == WebCore::ResourceRequestRequester::Media;
    bool isLikelyStreamingMedia = requester == WebCore::ResourceRequestRequester::XHR && isMediaMIMEType(response.mimeType());
    if (isLikelyStreamingMedia || isDefinitelyStreamingMedia)
        return StoreDecision::NoDueToStreamingMedia;

    return StoreDecision::Yes;
}

bool Cache::shouldUseSpeculativeLoadManager() const
{
    CheckedPtr lowPowerModeNotifier = m_lowPowerModeNotifier.get();
    bool isLowPowerModeEnabled = lowPowerModeNotifier && lowPowerModeNotifier->isLowPowerModeEnabled();
    bool isThermalMitigationEnabled = m_thermalMitigationNotifier && m_thermalMitigationNotifier->isThermalMitigationEnabled();
    return !isLowPowerModeEnabled && !isThermalMitigationEnabled;
}

void Cache::updateSpeculativeLoadManagerEnabledState()
{
    ASSERT(WTF::RunLoop::isMain());

    bool shouldEnable = shouldUseSpeculativeLoadManager();
    if (!shouldEnable && m_speculativeLoadManager) {
        m_speculativeLoadManager = nullptr;
        RELEASE_LOG(NetworkCacheSpeculativePreloading, "%p - Cache::updateSpeculativeLoadManagerEnabledState: disabling speculative loads due to low power mode or thermal change", this);
    } else if (shouldEnable && !m_speculativeLoadManager) {
        m_speculativeLoadManager = makeUnique<SpeculativeLoadManager>(*this, protectedStorage());
        RELEASE_LOG(NetworkCacheSpeculativePreloading, "%p - Cache::updateSpeculativeLoadManagerEnabledState: enabling speculative loads due to low power mode or thermal change", this);
    }
}

static bool inline canRequestUseSpeculativeRevalidation(const WebCore::ResourceRequest& request)
{
    if (request.isConditional())
        return false;

    if (request.requester() == WebCore::ResourceRequestRequester::XHR || request.requester() == WebCore::ResourceRequestRequester::Fetch)
        return false;

    switch (request.cachePolicy()) {
    case WebCore::ResourceRequestCachePolicy::ReturnCacheDataElseLoad:
    case WebCore::ResourceRequestCachePolicy::ReturnCacheDataDontLoad:
    case WebCore::ResourceRequestCachePolicy::ReloadIgnoringCacheData:
        return false;
    case WebCore::ResourceRequestCachePolicy::UseProtocolCachePolicy:
    case WebCore::ResourceRequestCachePolicy::RefreshAnyCacheData:
        return true;
    case WebCore::ResourceRequestCachePolicy::DoNotUseAnyCache:
        ASSERT_NOT_REACHED();
        return false;
    }
    return false;
}

void Cache::startAsyncRevalidationIfNeeded(const WebCore::ResourceRequest& request, const NetworkCache::Key& key, std::unique_ptr<Entry>&& entry, const GlobalFrameID& frameID, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, bool allowPrivacyProxy, OptionSet<WebCore::AdvancedPrivacyProtections> advancedPrivacyProtections)
{
    m_pendingAsyncRevalidations.ensure(key, [&] {
        auto addResult = m_pendingAsyncRevalidationByPage.ensure(frameID, [] {
            return WeakHashSet<AsyncRevalidation>();
        });
        Ref revalidation = AsyncRevalidation::create(*this, frameID, request, WTFMove(entry), isNavigatingToAppBoundDomain, allowPrivacyProxy, advancedPrivacyProtections, [weakThis = WeakPtr { *this }, key](auto result) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            ASSERT(protectedThis->m_pendingAsyncRevalidations.contains(key));
            protectedThis->m_pendingAsyncRevalidations.remove(key);
            LOG(NetworkCache, "(NetworkProcess) revalidation completed for '%s' with result %d", key.identifier().utf8().data(), static_cast<int>(result));
        });
        addResult.iterator->value.add(revalidation.get());
        return revalidation;
    });
}

void Cache::browsingContextRemoved(WebPageProxyIdentifier webPageProxyID, WebCore::PageIdentifier webPageID, WebCore::FrameIdentifier webFrameID)
{
    auto loaders = m_pendingAsyncRevalidationByPage.take({ webPageProxyID, webPageID, webFrameID });
    for (Ref loader : loaders)
        loader->cancel();
}

void Cache::retrieve(const WebCore::ResourceRequest& request, std::optional<GlobalFrameID> frameID, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, bool allowPrivacyProxy, OptionSet<WebCore::AdvancedPrivacyProtections> advancedPrivacyProtections, RetrieveCompletionHandler&& completionHandler)
{
    ASSERT(request.url().protocolIsInHTTPFamily());

    LOG(NetworkCache, "(NetworkProcess) retrieving %s priority %d", request.url().stringWithoutFragmentIdentifier().ascii().data(), static_cast<int>(request.priority()));

    Key storageKey = makeCacheKey(request);
    auto priority = static_cast<unsigned>(request.priority());

    RetrieveInfo info;
    info.startTime = MonotonicTime::now();
    info.priority = priority;

    CheckedPtr speculativeLoadManager = m_speculativeLoadManager.get();
    bool canUseSpeculativeRevalidation = frameID && speculativeLoadManager && canRequestUseSpeculativeRevalidation(request);
    if (canUseSpeculativeRevalidation)
        speculativeLoadManager->registerLoad(*frameID, request, storageKey, isNavigatingToAppBoundDomain, allowPrivacyProxy, advancedPrivacyProtections);

    auto retrieveDecision = makeRetrieveDecision(request);
    if (retrieveDecision != RetrieveDecision::Yes) {
        completeRetrieve(WTFMove(completionHandler), nullptr, info);
        return;
    }

    if (canUseSpeculativeRevalidation && speculativeLoadManager->canRetrieve(storageKey, request, *frameID)) {
        speculativeLoadManager->retrieve(storageKey, [networkProcess = Ref { networkProcess() }, request, completionHandler = WTFMove(completionHandler), info = WTFMove(info), sessionID = m_sessionID](std::unique_ptr<Entry> entry) mutable {
            info.wasSpeculativeLoad = true;
            if (entry && WebCore::verifyVaryingRequestHeaders(networkProcess->checkedStorageSession(sessionID).get(), entry->varyingRequestHeaders(), request))
                completeRetrieve(WTFMove(completionHandler), WTFMove(entry), info);
            else
                completeRetrieve(WTFMove(completionHandler), nullptr, info);
        });
        return;
    }

    m_storage->retrieve(storageKey, priority, [this, protectedThis = Ref { *this }, request, completionHandler = WTFMove(completionHandler), info = WTFMove(info), storageKey, networkProcess = Ref { networkProcess() }, sessionID = m_sessionID, frameID, isNavigatingToAppBoundDomain, allowPrivacyProxy, advancedPrivacyProtections](auto record, auto timings) mutable {
        info.storageTimings = timings;

        if (record.isNull()) {
            LOG(NetworkCache, "(NetworkProcess) not found in storage");
            completeRetrieve(WTFMove(completionHandler), nullptr, info);
            return false;
        }

        ASSERT(record.key == storageKey);

        auto entry = Entry::decodeStorageRecord(record);

        auto useDecision = entry ? makeUseDecision(networkProcess, sessionID, *entry, request) : UseDecision::NoDueToDecodeFailure;
        switch (useDecision) {
        case UseDecision::AsyncRevalidate: {
            auto entryCopy = makeUnique<Entry>(*entry);
            entryCopy->setNeedsValidation(true);
            startAsyncRevalidationIfNeeded(request, storageKey, WTFMove(entryCopy), *frameID, isNavigatingToAppBoundDomain, allowPrivacyProxy, advancedPrivacyProtections);
            [[fallthrough]];
        }
        case UseDecision::Use:
            break;
        case UseDecision::Validate:
            entry->setNeedsValidation(true);
            break;
        default:
            entry = nullptr;
        };

#if !LOG_DISABLED
        auto elapsed = MonotonicTime::now() - info.startTime;
        LOG(NetworkCache, "(NetworkProcess) retrieve complete useDecision=%d priority=%d time=%" PRIi64 "ms", static_cast<int>(useDecision), static_cast<int>(request.priority()), elapsed.millisecondsAs<int64_t>());
#endif
        completeRetrieve(WTFMove(completionHandler), WTFMove(entry), info);

        return useDecision != UseDecision::NoDueToDecodeFailure;
    });
}

void Cache::completeRetrieve(RetrieveCompletionHandler&& handler, std::unique_ptr<Entry> entry, RetrieveInfo& info)
{
    info.completionTime = MonotonicTime::now();
    handler(WTFMove(entry), info);
}
    
std::unique_ptr<Entry> Cache::makeEntry(const WebCore::ResourceRequest& request, const WebCore::ResourceResponse& response, PrivateRelayed privateRelayed, RefPtr<WebCore::FragmentedSharedBuffer>&& responseData)
{
    return makeUnique<Entry>(makeCacheKey(request), response, privateRelayed, WTFMove(responseData), WebCore::collectVaryingRequestHeaders(m_networkProcess->checkedStorageSession(m_sessionID).get(), request, response));
}

std::unique_ptr<Entry> Cache::makeRedirectEntry(const WebCore::ResourceRequest& request, const WebCore::ResourceResponse& response, const WebCore::ResourceRequest& redirectRequest)
{
    auto cachedRedirectRequest = redirectRequest;
    cachedRedirectRequest.clearHTTPAuthorization();
    return makeUnique<Entry>(makeCacheKey(request), response, WTFMove(cachedRedirectRequest), WebCore::collectVaryingRequestHeaders(m_networkProcess->checkedStorageSession(m_sessionID).get(), request, response));
}

std::unique_ptr<Entry> Cache::store(const WebCore::ResourceRequest& request, const WebCore::ResourceResponse& response, PrivateRelayed privateRelayed, RefPtr<WebCore::FragmentedSharedBuffer>&& responseData, Function<void(MappedBody&&)>&& completionHandler)
{
    ASSERT(responseData);

    LOG(NetworkCache, "(NetworkProcess) storing %s, partition %s", request.url().stringWithoutFragmentIdentifier().latin1().data(), makeCacheKey(request).partition().latin1().data());

    StoreDecision storeDecision = makeStoreDecision(request, response, responseData ? responseData->size() : 0);
    if (storeDecision != StoreDecision::Yes) {
        LOG(NetworkCache, "(NetworkProcess) didn't store, storeDecision=%d", static_cast<int>(storeDecision));
        auto key = makeCacheKey(request);

        auto isSuccessfulRevalidation = response.httpStatusCode() == httpStatus304NotModified;
        if (!isSuccessfulRevalidation) {
            // Make sure we don't keep a stale entry in the cache.
            remove(key);
        }

        return nullptr;
    }

    auto cacheEntry = makeEntry(request, response, privateRelayed, WTFMove(responseData));
    auto record = cacheEntry->encodeAsStorageRecord();

    m_storage->store(record, [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](const Data& bodyData) mutable {
        MappedBody mappedBody;
#if ENABLE(SHAREABLE_RESOURCE)
        if (auto sharedMemory = bodyData.tryCreateSharedMemory()) {
            mappedBody.shareableResource = WebCore::ShareableResource::create(sharedMemory.releaseNonNull(), 0, bodyData.size());
            if (!mappedBody.shareableResource) {
                if (completionHandler)
                    completionHandler(WTFMove(mappedBody));
                return;
            }
            if (auto handle = Ref { *mappedBody.shareableResource }->createHandle())
                mappedBody.shareableResourceHandle = WTFMove(*handle);
        }
#endif
        if (completionHandler)
            completionHandler(WTFMove(mappedBody));
        LOG(NetworkCache, "(NetworkProcess) stored");
    });

    return cacheEntry;
}

std::unique_ptr<Entry> Cache::storeRedirect(const WebCore::ResourceRequest& request, const WebCore::ResourceResponse& response, const WebCore::ResourceRequest& redirectRequest, std::optional<Seconds> maxAgeCap)
{
    LOG(NetworkCache, "(NetworkProcess) storing redirect %s -> %s", request.url().string().latin1().data(), redirectRequest.url().string().latin1().data());

    StoreDecision storeDecision = makeStoreDecision(request, response, 0);
    if (storeDecision != StoreDecision::Yes) {
        LOG(NetworkCache, "(NetworkProcess) didn't store redirect, storeDecision=%d", static_cast<int>(storeDecision));
        return nullptr;
    }

    auto cacheEntry = makeRedirectEntry(request, response, redirectRequest);

    if (maxAgeCap) {
        LOG(NetworkCache, "(NetworkProcess) capping max age for redirect %s -> %s", request.url().string().latin1().data(), redirectRequest.url().string().latin1().data());
        cacheEntry->capMaxAge(maxAgeCap.value());
    }

    auto record = cacheEntry->encodeAsStorageRecord();

    m_storage->store(record, nullptr);
    
    return cacheEntry;
}

std::unique_ptr<Entry> Cache::update(const WebCore::ResourceRequest& originalRequest, const Entry& existingEntry, const WebCore::ResourceResponse& validatingResponse, PrivateRelayed privateRelayed)
{
    LOG(NetworkCache, "(NetworkProcess) updating %s", originalRequest.url().string().latin1().data());

    WebCore::ResourceResponse response = existingEntry.response();
    WebCore::updateResponseHeadersAfterRevalidation(response, validatingResponse);

    auto updateEntry = makeUnique<Entry>(existingEntry.key(), response, privateRelayed, existingEntry.buffer(), WebCore::collectVaryingRequestHeaders(m_networkProcess->checkedStorageSession(m_sessionID).get(), originalRequest, response));
    auto updateRecord = updateEntry->encodeAsStorageRecord();

    m_storage->store(updateRecord, { });

    return updateEntry;
}

void Cache::remove(const Key& key)
{
    m_storage->remove(key);
}

void Cache::remove(const WebCore::ResourceRequest& request)
{
    remove(makeCacheKey(request));
}

void Cache::remove(const Vector<Key>& keys, Function<void()>&& completionHandler)
{
    m_storage->remove(keys, WTFMove(completionHandler));
}

void Cache::traverse(Function<void(const TraversalEntry*)>&& traverseHandler)
{
    // Protect against clients making excessive traversal requests.
    const unsigned maximumTraverseCount = 3;
    if (m_traverseCount >= maximumTraverseCount) {
        WTFLogAlways("Maximum parallel cache traverse count exceeded. Ignoring traversal request.");

        RunLoop::mainSingleton().dispatch([traverseHandler = WTFMove(traverseHandler)] () mutable {
            traverseHandler(nullptr);
        });
        return;
    }

    ++m_traverseCount;

    m_storage->traverse(resourceType(), { }, [this, protectedThis = Ref { *this }, traverseHandler = WTFMove(traverseHandler)] (const Storage::Record* record, const Storage::RecordInfo& recordInfo) mutable {
        if (!record) {
            --m_traverseCount;
            traverseHandler(nullptr);
            return;
        }

        auto entry = Entry::decodeStorageRecord(*record);
        if (!entry)
            return;

        TraversalEntry traversalEntry { *entry, recordInfo };
        traverseHandler(&traversalEntry);
    });
}

void Cache::traverse(const String& partition, Function<void(const TraversalEntry*)>&& traverseHandler)
{
    m_storage->traverse(resourceType(), partition, { }, [traverseHandler = WTFMove(traverseHandler)] (const Storage::Record* record, const Storage::RecordInfo& recordInfo) mutable {
        if (!record) {
            traverseHandler(nullptr);
            return;
        }

        auto entry = Entry::decodeStorageRecord(*record);
        if (!entry)
            return;

        TraversalEntry traversalEntry { *entry, recordInfo };
        traverseHandler(&traversalEntry);
    });
}

String Cache::dumpFilePath() const
{
    return pathByAppendingComponent(m_storage->versionPath(), "dump.json"_s);
}

void Cache::dumpContentsToFile()
{
    auto fileHandle = openFile(dumpFilePath(), FileOpenMode::Truncate);
    if (!fileHandle)
        return;

    constexpr auto prologue = "{\n\"entries\": [\n"_s;
    fileHandle.write(prologue.span8());

    struct Totals {
        unsigned count { 0 };
        double worth { 0 };
        size_t bodySize { 0 };
    };
    Totals totals;
    auto flags = { Storage::TraverseFlag::ComputeWorth, Storage::TraverseFlag::ShareCount };
    size_t capacity = m_storage->capacity();
    m_storage->traverse(resourceType(), flags, [fileHandle = WTFMove(fileHandle), totals, capacity](const Storage::Record* record, const Storage::RecordInfo& info) mutable {
        if (!record) {
            CString writeData = makeString(
                "{}\n"
                "],\n"
                "\"totals\": {\n"
                "\"capacity\": "_s, capacity, ",\n"
                "\"count\": "_s, totals.count, ",\n"
                "\"bodySize\": "_s, totals.bodySize, ",\n"
                "\"averageWorth\": "_s, totals.count ? totals.worth / totals.count : 0, "\n"
                "}\n}\n"_s
            ).utf8();
            fileHandle.write(byteCast<uint8_t>(writeData.span()));
            fileHandle = { };
            return;
        }
        auto entry = Entry::decodeStorageRecord(*record);
        if (!entry)
            return;
        ++totals.count;
        totals.worth += info.worth;
        totals.bodySize += info.bodySize;

        StringBuilder json;
        entry->asJSON(json, info);
        json.append(",\n"_s);
        fileHandle.write(byteCast<uint8_t>(json.toString().utf8().span()));
    });
}

void Cache::deleteDumpFile()
{
    WorkQueue::create("com.apple.WebKit.Cache.delete"_s)->dispatch([path = dumpFilePath().isolatedCopy()] {
        deleteFile(path);
    });
}

void Cache::clear(WallTime modifiedSince, Function<void()>&& completionHandler)
{
    LOG(NetworkCache, "(NetworkProcess) clearing cache");

    String anyType;
    m_storage->clear(WTFMove(anyType), modifiedSince, WTFMove(completionHandler));

    deleteDumpFile();
}

void Cache::clear()
{
    clear(-WallTime::infinity(), nullptr);
}

String Cache::recordsPathIsolatedCopy() const
{
    return m_storage->recordsPathIsolatedCopy();
}

void Cache::fetchData(bool shouldComputeSize, CompletionHandler<void(Vector<WebsiteData::Entry>&&)>&& completionHandler)
{
    HashMap<WebCore::SecurityOriginData, uint64_t> originsAndSizes;
    traverse([protectedThis = Ref { *this }, shouldComputeSize, completionHandler = WTFMove(completionHandler), originsAndSizes = WTFMove(originsAndSizes)](auto* traversalEntry) mutable {
        if (traversalEntry) {
            auto url = traversalEntry->entry.response().url();
            auto result = originsAndSizes.add({ url.protocol().toString(), url.host().toString(), url.port() }, 0);
            if (shouldComputeSize)
                result.iterator->value += traversalEntry->entry.sourceStorageRecord().header.size() + traversalEntry->recordInfo.bodySize;
            return;
        }

        auto entries = WTF::map(originsAndSizes, [](auto& originAndSize) {
            return WebsiteData::Entry { originAndSize.key, WebsiteDataType::DiskCache, originAndSize.value };
        });
        completionHandler(WTFMove(entries));
    });
}

void Cache::deleteData(const Vector<WebCore::SecurityOriginData>& origins, CompletionHandler<void()>&& completionHandler)
{
    HashSet<WebCore::SecurityOriginData> originSet;
    for (auto& origin : origins)
        originSet.add(origin);

    Vector<NetworkCache::Key> keysToDelete;
    traverse([this, protectedThis = Ref { *this }, originSet = WTFMove(originSet), completionHandler = WTFMove(completionHandler), keysToDelete = WTFMove(keysToDelete)](auto* traversalEntry) mutable {
        if (traversalEntry) {
            auto origin = WebCore::SecurityOriginData::fromURLWithoutStrictOpaqueness(traversalEntry->entry.response().url());
            if (originSet.contains(origin))
                keysToDelete.append(traversalEntry->entry.key());
            return;
        }

        remove(keysToDelete, WTFMove(completionHandler));
    });
}

void Cache::deleteDataForRegistrableDomains(const Vector<WebCore::RegistrableDomain>& domains, CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&& completionHandler)
{
    HashSet<WebCore::RegistrableDomain> domainSet;
    for (auto& domain : domains)
        domainSet.add(domain);

    Vector<NetworkCache::Key> keysToDelete;
    HashSet<WebCore::RegistrableDomain> domainsDeleted;
    traverse([this, protectedThis = Ref { *this }, domainSet = WTFMove(domainSet), completionHandler = WTFMove(completionHandler), keysToDelete = WTFMove(keysToDelete), domainsDeleted = WTFMove(domainsDeleted)](auto* traversalEntry) mutable {
        if (traversalEntry) {
            auto domain = WebCore::RegistrableDomain { traversalEntry->entry.response().url() };
            if (domainSet.contains(domain)) {
                keysToDelete.append(traversalEntry->entry.key());
                domainsDeleted.add(domain);
            }
            return;
        }

        remove(keysToDelete, [completionHandler = WTFMove(completionHandler), domainsDeleted = WTFMove(domainsDeleted)]() mutable {
            completionHandler(WTFMove(domainsDeleted));
        });
    });
}

} // namespace NetworkCache
} // namespace WebKit
