#include "pch.h"

#include "Teleport.h"

#include "Locations.h"

#include "Offsets.h"

bool Teleport::UpdatePlayerStartingLocation()
{
	puts(__FUNCTION__);

	uintptr_t LocationAddress = DMA::NavigationAddress + offsetof(CNavigation, Position);

	DWORD BytesRead = 0;

	VMMDLL_MemReadEx(DMA::vmh, DMA::PID, LocationAddress, (BYTE*)&StartingLocation, sizeof(Vec3), &BytesRead, VMMDLL_FLAG_NOCACHE);

	if (BytesRead != sizeof(Vec3))
	{
		ZeroMemory(&StartingLocation, sizeof(Vec3));
		return 0;
	}

	return 1;
}

void Teleport::OverwriteLocation(Vec3 Location)
{
	/* save starting location for later comparison */
	UpdatePlayerStartingLocation();

	BYTE InVehicleBits = 0x0;
	uintptr_t InVehicleAddress = DMA::LocalPlayerAddress + offsetof(PED, InVehicleBits);
	VMMDLL_MemRead(DMA::vmh, DMA::PID, InVehicleAddress, (BYTE*)&InVehicleBits, sizeof(BYTE));

	VMMDLL_SCATTER_HANDLE vmsh = VMMDLL_Scatter_Initialize(DMA::vmh, DMA::PID, VMMDLL_FLAG_NOCACHE);

	PreparePlayerWrites(vmsh);

	if (InVehicleBits & 0x1)
		PrepareVehicleWrites(vmsh);

	bool bSuccessfullyTeleported = false;
	for (int i = 0; i < 200; i++)
	{
		VMMDLL_Scatter_Execute(vmsh);

		Sleep(10);

		if (!DMA::UpdatePlayerCurrentLocation())
			continue;

		// Player cannot be exactly at desired location because that does not indicate success.
		// We should only stop when the player has moved off center from the desired location; indicating a successful TP
		float Distance = DMA::LocalPlayerLocation.Distance(Location);

		if (Distance > 1.0f
			&& Distance < 100.0f
			&& DMA::LocalPlayerLocation.Distance(StartingLocation) > 100.0f)
		{
			puts("Successfully teleported.");
			bSuccessfullyTeleported = true;
			break;
		}
	}

	VMMDLL_Scatter_CloseHandle(vmsh);

	if (!bSuccessfullyTeleported)
		puts("Teleport failed.");
}

bool Teleport::OnDMAFrame()
{
	if (bRequestedTeleport)
	{
		OverwriteLocation(DesiredLocation);
		bRequestedTeleport = false;
	}

	return 1;
}

static const std::vector<std::string>CayoSecondaryLocationStrings = { "Coke Field","NorthDock1","NorthDock2","NorthDock3","Middle","Main1","Main2","Main3","Hangar1","Hangar2","Spawn" };
static const std::vector<std::string>GeneralLocationStrings = { "Arcade","Military Base","Sandy Shores","Bunker","Nightclub","Casino" };

bool Teleport::Render()
{

	ImGui::Begin("Teleport");

	ImGui::Text("Current Location: %.2f, %.2f, %.2f", DMA::LocalPlayerLocation.x, DMA::LocalPlayerLocation.y, DMA::LocalPlayerLocation.z);

	ImGui::InputFloat("X", &Teleport::DesiredLocation.x);
	ImGui::InputFloat("Y", &Teleport::DesiredLocation.y);
	ImGui::InputFloat("Z", &Teleport::DesiredLocation.z);

	if (ImGui::Button("Teleport Player"))
	{
		bRequestedTeleport = true;
	}

	if (ImGui::Button("Teleport To Waypoint"))
	{
		Vec3 WaypointCoords = GetWaypointCoords();
		if (WaypointCoords.x != 0.0f)
		{
			DesiredLocation = WaypointCoords;
			bRequestedTeleport = true;
		}
	}

	if (ImGui::CollapsingHeader("General TPs"))
	{
		for (auto Name : GeneralLocationStrings)
		{
			if (ImGui::Button(Name.c_str()))
			{
				DesiredLocation = LocationMap[Name];
				bRequestedTeleport = true;
			}
		}
	}

	if (ImGui::CollapsingHeader("Cayo TPs"))
	{
		ImGui::Indent();

		if (ImGui::CollapsingHeader("Out of Compound"))
		{

			if (ImGui::Button("Radio Tower"))
			{
				DesiredLocation = LocationMap["Radio Tower"];
				bRequestedTeleport = true;
			}

			if (ImGui::Button("Upper Radio Tower"))
			{
				DesiredLocation = LocationMap["Upper Radio Tower"];
				bRequestedTeleport = true;
			}
		}

		if (ImGui::CollapsingHeader("Inside Compound"))
		{


			if (ImGui::Button("First Room"))
			{
				DesiredLocation = LocationMap["First Room"];
				bRequestedTeleport = true;
			}

			if (ImGui::Button("Main Exit"))
			{
				DesiredLocation = LocationMap["Main Exit"];
				bRequestedTeleport = true;
			}
		}

		if (ImGui::CollapsingHeader("Secondary Loot"))
		{
			for (auto Name : CayoSecondaryLocationStrings)
			{
				if (ImGui::Button(Name.c_str()))
				{
					DesiredLocation = LocationMap[Name.c_str()];
					bRequestedTeleport = true;
				}
			}
		}

		ImGui::Unindent();
	}


	ImGui::End();

	return 1;
}

void Teleport::PreparePlayerWrites(VMMDLL_SCATTER_HANDLE vmsh)
{
	uintptr_t LocationAddress = DMA::NavigationAddress + offsetof(CNavigation, Position);
	VMMDLL_Scatter_PrepareWrite(vmsh, LocationAddress, (BYTE*)&DesiredLocation, sizeof(Vec3));
}

void Teleport::PrepareVehicleWrites(VMMDLL_SCATTER_HANDLE vmsh)
{
	uintptr_t LocationAddress = DMA::VehicleNavigationAddress + offsetof(CNavigation, Position);
	VMMDLL_Scatter_PrepareWrite(vmsh, LocationAddress, (BYTE*)&DesiredLocation, sizeof(Vec3));
}

#define BLIP_NUM 1000

Vec3 Teleport::GetWaypointCoords()
{
	std::println("Getting waypoint coords...");

	Vec3 WaypointCoordinates = { 0.0f,0.0f,0.0f };

	uintptr_t BlipsArrayAddress = DMA::BaseAddress + Offsets::BlipPtr;

	uintptr_t BlipAddresses[BLIP_NUM];

	DWORD BytesRead = 0x0;

	VMMDLL_MemReadEx(DMA::vmh, DMA::PID, BlipsArrayAddress, (BYTE*)&BlipAddresses, sizeof(uintptr_t) * BLIP_NUM, &BytesRead, VMMDLL_FLAG_NOCACHE);

	if (BytesRead != (sizeof(uintptr_t) * BLIP_NUM))
	{
		puts("Failed reading blips.");
		return WaypointCoordinates;
	}

	auto pBlips = std::make_unique<Blip[]>(BLIP_NUM);
	ZeroMemory(pBlips.get(), sizeof(Blip) * BLIP_NUM);

	VMMDLL_SCATTER_HANDLE vmsh = VMMDLL_Scatter_Initialize(DMA::vmh, DMA::PID, VMMDLL_FLAG_NOCACHE);

	for (int i = 0; i < BLIP_NUM; i++)
	{
		if (!BlipAddresses[i])
			continue;

		VMMDLL_Scatter_PrepareEx(vmsh, BlipAddresses[i], sizeof(Blip), (BYTE*)&pBlips[i], &BytesRead);

	}

	VMMDLL_Scatter_Execute(vmsh);
	VMMDLL_Scatter_CloseHandle(vmsh);
	vmsh = 0;

	bool bFound = false;

	for (int i = 0; i < BLIP_NUM; i++)
	{
		if (pBlips[i].ID == 8)
		{
			std::println("Found waypoint blip!\n");
			std::println("{0:.2f} {1:.2f} {2:.2f}\n", pBlips[i].Position.x, pBlips[i].Position.y, pBlips[i].Position.z);

			WaypointCoordinates = pBlips[i].Position;

			if (WaypointCoordinates.z == 20)
				WaypointCoordinates.z = -255;
			else
				WaypointCoordinates.z += 2;

			bFound = true;

			break;
		}
	}

	if (!bFound)
		std::println("Couldn't find waypoint coords.");

	return WaypointCoordinates;
}