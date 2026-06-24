#include "pch.h"
#include "trace.h"

#include <common/Telemetry/TraceBase.h>

// Telemetry strings should not be localized.
#define LoggingProviderKey "Microsoft.PowerToys"

#define EventEnableAltTabGroupedKey "AltTabGrouped_EnableAltTabGrouped"
#define EventInvokedKey "AltTabGrouped_Invoked"
#define EventEnabledKey "Enabled"
#define EventGroupCountKey "GroupCount"

TRACELOGGING_DEFINE_PROVIDER(
    g_hProvider,
    LoggingProviderKey,
    // {38e8889b-9731-53f5-e901-e8a7c1753074}
    (0x38e8889b, 0x9731, 0x53f5, 0xe9, 0x01, 0xe8, 0xa7, 0xc1, 0x75, 0x30, 0x74),
    TraceLoggingOptionProjectTelemetry());

void Trace::AltTabGrouped::Enable(bool enabled) noexcept
{
    TraceLoggingWriteWrapper(
        g_hProvider,
        EventEnableAltTabGroupedKey,
        ProjectTelemetryPrivacyDataTag(ProjectTelemetryTag_ProductAndServicePerformance),
        TraceLoggingKeyword(PROJECT_KEYWORD_MEASURE),
        TraceLoggingBoolean(enabled, EventEnabledKey));
}

void Trace::AltTabGrouped::Invoked(size_t groupCount) noexcept
{
    TraceLoggingWriteWrapper(
        g_hProvider,
        EventInvokedKey,
        ProjectTelemetryPrivacyDataTag(ProjectTelemetryTag_ProductAndServicePerformance),
        TraceLoggingKeyword(PROJECT_KEYWORD_MEASURE),
        TraceLoggingUInt64(static_cast<UINT64>(groupCount), EventGroupCountKey));
}
