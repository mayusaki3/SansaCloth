#include "SansaClothBackendProbeValidationSystemComponent.h"

#include <AzCore/Interface/Interface.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Physics/Common/PhysicsSimulatedBody.h>
#include <AzFramework/Physics/Configuration/RigidBodyConfiguration.h>
#include <AzFramework/Physics/PhysicsScene.h>

namespace SansaClothBackendProbeValidation
{
    namespace
    {
        constexpr float Tolerance = 1.0e-5f;

        bool Near(const AZ::Vector3& lhs, const AZ::Vector3& rhs)
        {
            return lhs.IsClose(rhs, Tolerance);
        }

        void LogVector(const char* key, const AZ::Vector3& value)
        {
            AZ_Printf(
                "SansaClothBackendProbe",
                "SANSA_O3DE|OBF-006.%s|%.9g,%.9g,%.9g\n",
                key,
                value.GetX(),
                value.GetY(),
                value.GetZ());
        }
    }

    void SystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SystemComponent, AZ::Component>()
                ->Version(1);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext
                ->Method("RunObf006", &SystemComponent::RunObf006)
                ->Attribute(AZ::Script::Attributes::Module, "sansacloth_probe")
                ->Attribute(
                    AZ::Script::Attributes::Scope,
                    AZ::Script::Attributes::ScopeFlags::Common);
        }
    }

    bool SystemComponent::RunObf006()
    {
        auto* sceneInterface = AZ::Interface<AzPhysics::SceneInterface>::Get();
        if (sceneInterface == nullptr)
        {
            AZ_Printf(
                "SansaClothBackendProbe",
                "SANSA_O3DE|OBF-006.RESULT|FAIL|SceneInterface unavailable\n");
            return false;
        }

        const AzPhysics::SceneHandle sceneHandle =
            sceneInterface->GetSceneHandle(AzPhysics::EditorPhysicsSceneName);
        if (sceneHandle == AzPhysics::InvalidSceneHandle)
        {
            AZ_Printf(
                "SansaClothBackendProbe",
                "SANSA_O3DE|OBF-006.RESULT|FAIL|Editor physics scene unavailable\n");
            return false;
        }

        const AZ::Vector3 originalGravity = sceneInterface->GetGravity(sceneHandle);

        // Canonical test gravity (1.25, -3.75, 2.5) maps to O3DE
        // (1.25, 2.5, -3.75) under canonical (x,y,z) -> O3DE (x,z,y).
        const AZ::Vector3 probeGravity(1.25f, 2.5f, -3.75f);
        sceneInterface->SetGravity(sceneHandle, probeGravity);
        const AZ::Vector3 gravityBeforeBody = sceneInterface->GetGravity(sceneHandle);

        AzPhysics::RigidBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_startSimulationEnabled = false;
        bodyConfiguration.m_orientation = AZ::Quaternion::CreateIdentity();

        AzPhysics::SimulatedBodyHandle bodyHandle =
            sceneInterface->AddSimulatedBody(sceneHandle, &bodyConfiguration);

        bool result = bodyHandle != AzPhysics::InvalidSimulatedBodyHandle;
        AZ::Vector3 gravityAtIdentity = AZ::Vector3::CreateZero();
        AZ::Vector3 gravityAfterRotation = AZ::Vector3::CreateZero();

        if (result)
        {
            AzPhysics::SimulatedBody* body =
                sceneInterface->GetSimulatedBodyFromHandle(sceneHandle, bodyHandle);
            result = body != nullptr;

            if (body != nullptr)
            {
                gravityAtIdentity = sceneInterface->GetGravity(sceneHandle);

                const AZ::Quaternion rotated =
                    AZ::Quaternion::CreateRotationY(AZ::DegToRad(90.0f));
                body->SetTransform(
                    AZ::Transform::CreateFromQuaternionAndTranslation(
                        rotated,
                        AZ::Vector3::CreateZero()));

                gravityAfterRotation = sceneInterface->GetGravity(sceneHandle);
                const AZ::Vector3 bodyBasisX = body->GetTransform().GetBasisX();
                LogVector("BODY_ROTATED_BASIS_X", bodyBasisX);
            }

            sceneInterface->RemoveSimulatedBody(sceneHandle, bodyHandle);
        }

        sceneInterface->SetGravity(sceneHandle, originalGravity);
        const AZ::Vector3 restoredGravity = sceneInterface->GetGravity(sceneHandle);

        LogVector("ORIGINAL_GRAVITY", originalGravity);
        LogVector("PROBE_GRAVITY", probeGravity);
        LogVector("GRAVITY_BEFORE_BODY", gravityBeforeBody);
        LogVector("GRAVITY_AT_IDENTITY", gravityAtIdentity);
        LogVector("GRAVITY_AFTER_BODY_ROTATION", gravityAfterRotation);
        LogVector("RESTORED_GRAVITY", restoredGravity);

        result =
            result
            && Near(gravityBeforeBody, probeGravity)
            && Near(gravityAtIdentity, probeGravity)
            && Near(gravityAfterRotation, probeGravity)
            && Near(restoredGravity, originalGravity);

        AZ_Printf(
            "SansaClothBackendProbe",
            "SANSA_O3DE|OBF-006.RESULT|%s\n",
            result ? "PASS" : "FAIL");

        return result;
    }
} // namespace SansaClothBackendProbeValidation
