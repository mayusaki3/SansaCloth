#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/std/string/string.h>

namespace SansaClothBackendProbeValidation
{
    class SystemComponent final
        : public AZ::Component
    {
    public:
        AZ_COMPONENT(
            SystemComponent,
            "{A0B79F3B-1D54-4D20-86A7-5BDB9F87F8A1}");

        static void Reflect(AZ::ReflectContext* context);

        void Activate() override {}
        void Deactivate() override {}

        static bool RunObf006();
        static bool RunBf003();
        static bool RunBf004();
        static bool RunBf005006();
        static bool RunBf007();
        static AZStd::string RunBf008Capture();
        static AZStd::string ProbeHandoffString(const AZStd::string& payload);
    };
} // namespace SansaClothBackendProbeValidation
