#include "Fixes.h"

//Bandaid fix for SetAlpha not working properly for first person
namespace Fixes::FirstPersonAlpha
{
	struct FirstPersonAlpha
	{
		static inline float currentAlpha = 0.f;

		static void UpdateFPRootAlpha(RE::NiAVObject* a_object, float a_alpha)
		{
			RE::BSVisit::TraverseScenegraphGeometries(a_object, [&](RE::BSGeometry* a_geometry) -> RE::BSVisit::BSVisitControl {
				using State = RE::BSGeometry::States;
				using Flag = RE::BSShaderProperty::EShaderPropertyFlag;
	
				auto effect = a_geometry->properties[State::kEffect].get();
				if (effect)
				{
					auto lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(effect);
					if (lightingShader && (lightingShader->flags.any(Flag::kSkinned) || lightingShader->flags.any(Flag::kZBufferWrite))) // sceneroot has zbufferwrite flag
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
			if (RE::PlayerCamera::GetSingleton()->IsInFirstPerson()) {
				UpdateFPRootAlpha(a_player->Get3D(true), a_alpha);
			}
			currentAlpha = a_alpha;
			//if (a_alpha == 1.f)
				//UpdateFPRootAlpha(a_player->Get3D(false), a_alpha);
			//a_player->Get3D(true)->UpdateMaterialAlpha(a_alpha, false);
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
			if (a_event && a_event->newState && a_event->oldState) {
				auto a_player = RE::PlayerCharacter::GetSingleton();
				if (a_event->newState->id == RE::CameraState::kFirstPerson && a_event->oldState->id != RE::CameraState::kFirstPerson) {
					FirstPersonAlpha::UpdateFPRootAlpha(a_player->Get3D(false), FirstPersonAlpha::currentAlpha);
				}
				else if (a_event->newState->id != RE::CameraState::kFirstPerson && a_event->oldState->id == RE::CameraState::kFirstPerson) {
					FirstPersonAlpha::UpdateFPRootAlpha(a_player->Get3D(false), 1.0);
				}
				logger::info("switched camera to 1st person: {}", a_event->newState->id == RE::CameraState::kFirstPerson);
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
