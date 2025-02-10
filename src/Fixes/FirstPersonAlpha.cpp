#include "Fixes.h"

//Bandaid fix for SetAlpha not working properly for first person
namespace Fixes::FirstPersonAlpha
{
	struct FirstPersonAlpha
	{
		static inline float fCurrentAlpha = 1.0f;
		static inline bool bIsValidTween = false;

		static void UpdateFPRootAlpha(RE::NiAVObject* a_object, float a_alpha)
		{
			RE::BSVisit::TraverseScenegraphGeometries(a_object, [&](RE::BSGeometry* a_geometry) -> RE::BSVisit::BSVisitControl {
				using State = RE::BSGeometry::States;
				using Flag = RE::BSShaderProperty::EShaderPropertyFlag;
				
				auto effect = a_geometry->properties[State::kEffect].get();
				if (effect)
				{
					auto lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(effect);
					if (lightingShader && lightingShader->flags.any(Flag::kSkinned, Flag::kSpecular))
					{
						auto material = static_cast<RE::BSLightingShaderMaterialBase*>(lightingShader->material);
						if (material) {
							material->materialAlpha = a_alpha;
						}
					}
				}
				return RE::BSVisit::BSVisitControl::kContinue;
			});
		}

		static RE::NiAVObject* thunk(RE::PlayerCharacter* a_player, float a_alpha)
		{
			// fade == false -> alpha_value is 2 to 3
			// fade == true -> alpha_value is 0 to 1
			if (a_alpha >= 2.f) {
				a_alpha -= 2.f;
			}
			if (RE::PlayerCamera::GetSingleton()->IsInFirstPerson() || bIsValidTween) {
				UpdateFPRootAlpha(a_player->Get3D(true), a_alpha);
			}
			fCurrentAlpha = a_alpha;
			return nullptr;  // Return null so that the original fade function for 1st person doesn't execute
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	class FirstPersonCameraHandler final :
		public ISingleton<FirstPersonCameraHandler>,
		public RE::BSTEventSink<SKSE::CameraEvent>
	{
	public:
		using EventResult = RE::BSEventNotifyControl;

		EventResult ProcessEvent(const SKSE::CameraEvent* a_event, RE::BSTEventSource<SKSE::CameraEvent>*) override
		{
			if (a_event && a_event->newState && a_event->oldState)
			{
				if (FirstPersonAlpha::bIsValidTween && a_event->newState->id != RE::CameraState::kTween)
					FirstPersonAlpha::bIsValidTween = false;

				if (FirstPersonAlpha::fCurrentAlpha < 1.0f)
				{
					auto a_player = RE::PlayerCharacter::GetSingleton();
					if (a_event->newState->id == RE::CameraState::kFirstPerson) {
						FirstPersonAlpha::UpdateFPRootAlpha(a_player->Get3D(true), FirstPersonAlpha::fCurrentAlpha);
					}
					else if (a_event->newState->id == RE::CameraState::kTween && a_event->oldState->id == RE::CameraState::kFirstPerson) {
						FirstPersonAlpha::bIsValidTween = true;
					}
					else if (a_event->newState->id != RE::CameraState::kFirstPerson && a_event->oldState->id == RE::CameraState::kFirstPerson) {
						FirstPersonAlpha::UpdateFPRootAlpha(a_player->Get3D(true), 1.0);
					}
				}
			}
			return EventResult::kContinue;
		}
	};
	
	void Install()
	{
		REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(37777, 38722), 0x55 };
		stl::write_thunk_call<FirstPersonAlpha, 6>(target.address());

		if (auto eventSource = SKSE::GetCameraEventSource())
		{
			eventSource->AddEventSink(FirstPersonCameraHandler::GetSingleton());
			logger::info("\t\tInstalled first person alpha fix"sv);
		}
	}
}
