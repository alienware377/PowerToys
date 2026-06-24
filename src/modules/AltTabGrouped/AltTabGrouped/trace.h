#pragma once

#include <common/Telemetry/TraceBase.h>

#include <cstddef>

class Trace
{
public:
    class AltTabGrouped : public telemetry::TraceBase
    {
    public:
        static void Enable(bool enabled) noexcept;
        static void Invoked(size_t groupCount) noexcept;
    };
};
