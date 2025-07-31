#include "pch.h"

#include "MyMenu.h"

#include "Features.h"

bool MyMenu::Render()
{
	ImGui::Begin("CyNickal's GTA Menu");

	ImGui::SeparatorText("Window Toggles");

	ImGui::Checkbox("Weapon Inspector", &WeaponInspector::bEnable);

	ImGui::Checkbox("Teleport", &Teleport::bEnable);

	ImGui::SeparatorText("Feature Toggles");

	ImGui::Checkbox("Refresh Health", &RefreshHealth::bEnable);

	ImGui::Checkbox("Never Wanted", &NoWanted::bEnable);

	ImGui::SeparatorText("God Mode");
	ImGui::Checkbox("Player", &GodMode::bPlayerGodMode);
	ImGui::SameLine();
	ImGui::Checkbox("Vehicle", &GodMode::bVehicleGodMode);
	ImGui::Indent();
	if (ImGui::Button("Update"))
		GodMode::bRequestedGodmode = 1;
	ImGui::Unindent();

	ImGui::End();

	return 1;
}