#include "SansaClothBackendProbeValidationSystemComponent.h"

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Module/Module.h>

namespace SansaClothBackendProbeValidation
{
    class Module final
        : public AZ::Module
    {
    public:
        AZ_RTTI(
            Module,
            "{13A8FBC6-0F9A-44A0-92A8-88E2A4ED76A2}",
            AZ::Module);
        AZ_CLASS_ALLOCATOR(Module, AZ::SystemAllocator);

        Module()
        {
            m_descriptors.insert(
                m_descriptors.end(),
                {
                    SystemComponent::CreateDescriptor(),
                });
        }

        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return {
                azrtti_typeid<SystemComponent>(),
            };
        }
    };
} // namespace SansaClothBackendProbeValidation

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(
    AZ_JOIN(Gem_, O3DE_GEM_NAME),
    SansaClothBackendProbeValidation::Module)
#else
AZ_DECLARE_MODULE_CLASS(
    Gem_SansaClothBackendProbeValidation,
    SansaClothBackendProbeValidation::Module)
#endif
