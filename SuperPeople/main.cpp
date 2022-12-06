#include <Windows.h>
#include <iostream>
#include "overlay.h"
#include "memory.h"
#include "struct.h"
#include "auth.hpp"
#include "protect/protectmain.h"
#include "xorstr.hpp"
#include <iostream>
#include <Windows.h>
#define COLOUR(x) x/255
bool showmenu = true;
uintptr_t LocalPawn, ULocalPlayer, LocalController, World, CameraManager;
#define GWorld 0x8700480
#define UGameInstance 0x230
#define PersistentLevel 0x50
#define Actors 0xA0
#define LocalPlayers 0xC0
#define PlayerController 0x38
#define Pawn 0x320
#define Mesh 0x3B0
#define RootComponent 0x98
#define RelativeLocation 0x1a0
#define RelativeRotation 0x228
#define PlayerCameraManager 0x638
#define CameraCachePrivate 0x1dd0
TArray<uintptr_t> Characters;
FMinimalViewInfo Kamera;

Vector3 GetBone(DWORD_PTR mesh, int id)
{
	DWORD_PTR array = read<uintptr_t>(mesh + 0x510); // BONE ARRAY
	if (array == NULL)
		array = read<uintptr_t>(mesh + 0x510 + 0X10); // BONE ARRAY

	//printf("array %d\n", array);

	FTransform bone = read<FTransform>(array + (id * 0x30));

	FTransform ComponentToWorld = read<FTransform>(mesh + 0x1D0); // COMPONENT
	D3DMATRIX Matrix;

	Matrix = MatrixMultiplication(bone.ToMatrixWithScale(), ComponentToWorld.ToMatrixWithScale());

	return Vector3(Matrix._41, Matrix._42, Matrix._43);
}
Vector3 project_world_to_screen(Vector3 WorldLocation)
{
	Vector3 Screenlocation = Vector3(0, 0, 0);
	FMinimalViewInfo camera = read<FMinimalViewInfo>(CameraManager + CameraCachePrivate + 0x10);

	D3DMATRIX tempMatrix = Matrix(camera.Rotation, Vector3(0, 0, 0));

	Vector3 vAxisX = Vector3(tempMatrix.m[0][0], tempMatrix.m[0][1], tempMatrix.m[0][2]),
		vAxisY = Vector3(tempMatrix.m[1][0], tempMatrix.m[1][1], tempMatrix.m[1][2]),
		vAxisZ = Vector3(tempMatrix.m[2][0], tempMatrix.m[2][1], tempMatrix.m[2][2]);

	Vector3 vDelta = WorldLocation - camera.Location;
	Vector3 vTransformed = Vector3(vDelta.Dot(vAxisY), vDelta.Dot(vAxisZ), vDelta.Dot(vAxisX));

	if (vTransformed.z < 1.f) vTransformed.z = 1.f;

	float FovAngle = camera.FOV;

	float ScreenCenterX = Width / 2.0f;
	float ScreenCenterY = Height / 2.0f;

	Screenlocation.x = ScreenCenterX + vTransformed.x * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.z;
	Screenlocation.y = ScreenCenterY - vTransformed.y * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.z;

	return Screenlocation;
}

int realkey;
int keystatus;
int aimkey;
bool GetKey(int key)
{
	realkey = key;
	return true;
}
void ChangeKey(void* blank)
{
	keystatus = 1;
	while (true)
	{
		for (int i = 0; i < 0x87; i++)
		{
			if (GetKeyState(i) & 0x8000)
			{
				aimkey = i;
				keystatus = 0;
				return;
			}
		}
	}
}
static const char* keyNames[] = {
	"",
	"Left Mouse",
	"Right Mouse",
	"Cancel",
	"Middle Mouse",
	"Mouse 5",
	"Mouse 4",
	"",
	"Backspace",
	"Tab",
	"",
	"",
	"Clear",
	"Enter",
	"",
	"",
	"Shift",
	"Control",
	"Alt",
	"Pause",
	"Caps",
	"",
	"",
	"",
	"",
	"",
	"",
	"Escape",
	"",
	"",
	"",
	"",
	"Space",
	"Page Up",
	"Page Down",
	"End",
	"Home",
	"Left",
	"Up",
	"Right",
	"Down",
	"",
	"",
	"",
	"Print",
	"Insert",
	"Delete",
	"",
	"0",
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	"9",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"A",
	"B",
	"C",
	"D",
	"E",
	"F",
	"G",
	"H",
	"I",
	"J",
	"K",
	"L",
	"M",
	"N",
	"O",
	"P",
	"Q",
	"R",
	"S",
	"T",
	"U",
	"V",
	"W",
	"X",
	"Y",
	"Z",
	"",
	"",
	"",
	"",
	"",
	"Numpad 0",
	"Numpad 1",
	"Numpad 2",
	"Numpad 3",
	"Numpad 4",
	"Numpad 5",
	"Numpad 6",
	"Numpad 7",
	"Numpad 8",
	"Numpad 9",
	"Multiply",
	"Add",
	"",
	"Subtract",
	"Decimal",
	"Divide",
	"F1",
	"F2",
	"F3",
	"F4",
	"F5",
	"F6",
	"F7",
	"F8",
	"F9",
	"F10",
	"F11",
	"F12",
};
static bool Items_ArrayGetter(void* data, int idx, const char** out_text)
{
	const char* const* items = (const char* const*)data;
	if (out_text)
		*out_text = items[idx];
	return true;
}
void HotkeyButton(int aimkey, void* changekey, int status)
{
	const char* preview_value = NULL;
	if (aimkey >= 0 && aimkey < IM_ARRAYSIZE(keyNames))
		Items_ArrayGetter(keyNames, aimkey, &preview_value);

	std::string aimkeys;
	if (preview_value == NULL)
		aimkeys = skCrypt("Select Key");
	else
		aimkeys = preview_value;

	if (status == 1)
	{
		aimkeys = skCrypt("Press key to bind");
	}
	if (ImGui::Button(aimkeys.c_str(), ImVec2(100, 20)))
	{
		if (status == 0)
		{
			CreateThread(0, 0, (LPTHREAD_START_ROUTINE)changekey, nullptr, 0, nullptr);
		}
	}
}

float distanceee = 300;
bool ESP = true;
bool SNAPLINE;
bool FILLBOX = true;
bool DISTANCEESP = true;
bool healthbar = true;
bool BoneESP = true;
bool viewrotation = false;
bool radar = true;

bool aimbot;
float aimfovv = 90;
float aSmoothAmount = 1;


ImColor cRainbow = ImColor(255, 255, 182);
static int widthh = 576;
static int heightt = 326;
static int MenuSayfasi = 1;
static int tab = 1;
void draw_menu()
{
	auto s = ImVec2{}, p = ImVec2{}, gs = ImVec2{ 620, 485 };
	ImGui::SetNextWindowSize(ImVec2(gs));
	ImGui::SetNextWindowBgAlpha(1.0f);
	ImGui::Begin(skCrypt("Alien Cheats - (ver. 0.0.2)"), NULL, ImGuiWindowFlags_::ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_::ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);
	{

		ImGui::SetCursorPosX(121);
		s = ImVec2(ImGui::GetWindowSize().x - ImGui::GetStyle().WindowPadding.x * 2, ImGui::GetWindowSize().y - ImGui::GetStyle().WindowPadding.y * 2); p = ImVec2(ImGui::GetWindowPos().x + ImGui::GetStyle().WindowPadding.x, ImGui::GetWindowPos().y + ImGui::GetStyle().WindowPadding.y); auto draw = ImGui::GetWindowDrawList();
		draw->AddRectFilled(p, ImVec2(p.x + s.x, p.y + s.y), ImColor(15, 17, 19), 5);
		draw->AddRectFilled(ImVec2(p.x, p.y + 40), ImVec2(p.x + s.x, p.y + s.y), ImColor(18, 20, 21), 5, ImDrawCornerFlags_Bot);

		//draw->AddImage(lg, ImVec2(p.x - 60, p.y - 30), ImVec2(p.x + 110, p.y + 70), ImVec2(0, 0), ImVec2(1, 1), ImColor(255, 255, 102));

		ImGui::PushFont(icons);
		auto logo_size = ImGui::CalcTextSize(skCrypt("A"));
		draw->AddText(ImVec2(p.x + 28 - logo_size.x / 2, p.y + 29 - (logo_size.y / 2) + 4), cRainbow, skCrypt("A"));
		ImGui::PopFont();

		ImGui::PushFont(main_font2);
		auto logo_size2 = ImGui::CalcTextSize(skCrypt("A"));
		draw->AddText(ImVec2(p.x + 42 - logo_size2.x / 2, p.y + 29 - logo_size2.y), cRainbow, skCrypt("Alien Cheats"));
		ImGui::PopFont();

		ImGui::PushFont(main_font);
		ImGui::BeginGroup();
		ImGui::SameLine(110);
		if (ImGui::tab(skCrypt("AIMBOT"), tab == 1))tab = 1; ImGui::SameLine();
		if (ImGui::tab(skCrypt("Visuals"), tab == 2))tab = 2;
		ImGui::EndGroup();
		ImGui::PopFont();

		if (tab == 1)
		{
			ImGui::PushFont(main_font);
			{//left upper
				ImGui::SetCursorPosY(50);
				ImGui::BeginGroup();
				ImGui::SetCursorPosX(15);
				ImGui::MenuChild(skCrypt("Aimbot"), ImVec2(285, 415), false);
				{
					ImGui::Checkbox("AIMBOT", &aimbot);
					ImGui::Text(skCrypt("")); ImGui::SameLine(8); HotkeyButton(aimkey, ChangeKey, keystatus); ImGui::SameLine(); ImGui::Text(skCrypt("Aimbot Key"));
					ImGui::SliderFloat(skCrypt("AimFov"), &aimfovv, 30, 180);
					ImGui::SliderFloat(skCrypt("AimSmooth"), &aSmoothAmount, 1, 20);
				}
				ImGui::EndChild();
				ImGui::EndGroup();
			}
			{//right
				ImGui::SetCursorPosY(50);
				ImGui::BeginGroup();
				ImGui::SetCursorPosX(320);
				ImGui::MenuChild(skCrypt(" "), ImVec2(285, 415), false);
				{

				}
				ImGui::EndChild();
				ImGui::EndGroup();
				ImGui::PopFont();
			}
		}
		if (tab == 2)
		{
			ImGui::PushFont(main_font);
			{//left upper
				ImGui::SetCursorPosY(50);
				ImGui::BeginGroup();
				ImGui::SetCursorPosX(15);
				ImGui::MenuChild(skCrypt("Visuals"), ImVec2(285, 415), false);
				{
					ImGui::Checkbox("Radar", &radar);
					ImGui::Checkbox("ESP", &ESP);
					ImGui::Checkbox("View Rotation", &viewrotation);
					ImGui::Checkbox("Health Bar", &healthbar);
					ImGui::Checkbox("Skeleton ESP", &BoneESP);
					ImGui::Checkbox("SNAPLINE", &SNAPLINE);
					ImGui::Checkbox("FILL BOX", &FILLBOX);
					ImGui::Checkbox("DISTANCE ESP", &DISTANCEESP);
					ImGui::SliderFloat(skCrypt("Distance Float"), &distanceee, 300, 1500);
				}
				ImGui::EndChild();
				ImGui::EndGroup();
			}
			{//right
				ImGui::SetCursorPosY(50);
				ImGui::BeginGroup();
				ImGui::SetCursorPosX(320);
				ImGui::MenuChild(skCrypt(" "), ImVec2(285, 415), false);
				{

				}
				ImGui::EndChild();
				ImGui::EndGroup();
				ImGui::PopFont();
			}
		}
	}
}
RGBA col = {255,0,0,255};

ImVec2 GetWindowSize() {
	return { (float)GetSystemMetrics(SM_CXSCREEN), (float)GetSystemMetrics(SM_CYSCREEN) };
}

float CalculateDistance(int p1x, int p1y, int p2x, int p2y)
{
	float diffY = p1y - p2y;
	float diffX = p1x - p2x;
	return sqrt((diffY * diffY) + (diffX * diffX));
}


enum BoneList : uint32_t
{
	Root = 0,
	pelvis = 1,
	spine_01 = 2,
	spine_02 = 3,
	spine_03 = 4,
	clavicle_r = 5,
	upperarm_r = 6,
	lowerarm_r = 7,
	hand_r = 8,
	index_01_r = 9,
	index_02_r = 10,
	index_03_r = 11,
	middle_01_r = 12,
	middle_02_r = 13,
	middle_03_r = 14,
	pinky_01_r = 15,
	pinky_02_r = 16,
	pinky_03_r = 17,
	ring_01_r = 18,
	ring_02_r = 19,
	ring_03_r = 20,
	thumb_01_r = 21,
	thumb_02_r = 22,
	thumb_03_r = 23,
	Dummy_Weapon = 24,
	Dummy_EnergyBar = 25,
	Dummy_Drink = 26,
	Dummy_Bandage = 27,
	Dummy_Book = 28,
	Dummy_Painkiller = 29,
	lowerarm_twist_01_r = 30,
	upperarm_twist_01_r = 31,
	Ghilliesuit_06_R = 32,
	Ghilliesuit_06_R01 = 33,
	Ghilliesuit_06_R02 = 34,
	Ghilliesuit_05_R = 35,
	Ghilliesuit_05_R01 = 36,
	Ghilliesuit_05_R02 = 37,
	clavicle_l = 38,
	upperarm_l = 39,
	upperarm_twist_01_l = 40,
	Ghilliesuit_06_L = 41,
	Ghilliesuit_06_L01 = 42,
	Ghilliesuit_06_L02 = 43,
	lowerarm_l = 44,
	lowerarm_twist_01_l = 45,
	hand_l = 46,
	index_01_l = 47,
	index_02_l = 48,
	index_03_l = 49,
	middle_01_l = 50,
	middle_02_l = 51,
	middle_03_l = 52,
	pinky_01_l = 53,
	pinky_02_l = 54,
	pinky_03_l = 55,
	ring_01_l = 56,
	ring_02_l = 57,
	ring_03_l = 58,
	thumb_01_l = 59,
	thumb_02_l = 60,
	thumb_03_l = 61,
	Weapon_Equip_Dummy = 62,
	Dummy_Kits = 63,
	Ghilliesuit_05_L = 64,
	Ghilliesuit_05_L01 = 65,
	Ghilliesuit_05_L02 = 66,
	neck_01 = 67,
	head = 68,
	Hair_Pony = 69,
	Hair_Pony_B01 = 70,
	Hair_Pony_B02 = 71,
	Hair_Pony_B03 = 72,
	Hair_Top = 73,
	Hair_Top_B01 = 74,
	chin = 75,
	lip_down_l = 76,
	lip_down_r = 77,
	lip_down_c = 78,
	lip_down = 79,
	Hoodcap_R = 80,
	Hoodcap_R1 = 81,
	L_Hair00 = 82,
	R_Hair00 = 83,
	C_Hair00 = 84,
	LC_Hair00 = 85,
	RC_Hair00 = 86,
	head_eye_r = 87,
	head_eye_l = 88,
	Ghilliesuit_04_R = 89,
	Ghilliesuit_04_R01 = 90,
	Ghilliesuit_04_R02 = 91,
	Ghilliesuit_04_L = 92,
	Ghilliesuit_04_L01 = 93,
	Ghilliesuit_04_L02 = 94,
	lip_up_l = 95,
	lip_up_r = 96,
	cheek_l = 97,
	cheek_r = 98,
	cheekbone_l1 = 99,
	cheekbone_r1 = 100,
	nose_m = 101,
	nose_r = 102,
	nose_l = 103,
	head_eye_c_l3 = 104,
	head_eye_c_r3 = 105,
	eyeBrow_m = 106,
	eyeBrow_l3 = 107,
	eyeBrow_l2 = 108,
	eyeBrow_l1 = 109,
	eyeBrow_r1 = 110,
	eyeBrow_r2 = 111,
	eyeBrow_r3 = 112,
	lip_up_c = 113,
	lip_left = 114,
	lip_right = 115,
	lip_r = 116,
	lip_l = 117,
	lip_up = 118,
	head_eye_c_l = 119,
	head_eye_c_r = 120,
	Hoodcap_L = 121,
	Hoodcap_L1 = 122,
	HoodCap_C = 123,
	HoodCap_C1 = 124,
	HoodCap_C2 = 125,
	backpack = 126,
	Backpack_B01 = 127,
	Backpack_B02 = 128,
	Backpack_B03 = 129,
	Weapon_Slot_Back_L_03 = 130,
	Weapon_Slot_Back_L_02 = 131,
	Weapon_Slot_Back_R_01 = 132,
	Weapon_Slot_Back_R_03 = 133,
	Weapon_Slot_Back_R_02 = 134,
	Weapon_Slot_Back_L_01 = 135,
	Weapon_Slot_Back_R_04 = 136,
	Weapon_Slot_Back_R_05 = 137,
	Breast_L = 138,
	Breast_L01 = 139,
	Breast_L02 = 140,
	Breast_R = 141,
	Breast_R01 = 142,
	Breast_R02 = 143,
	Ghilliesuit_03_R = 144,
	Ghilliesuit_03_R01 = 145,
	Ghilliesuit_03_R02 = 146,
	Ghilliesuit_03_L = 147,
	Ghilliesuit_03_L01 = 148,
	Ghilliesuit_03_L02 = 149,
	Ghilliesuit_02_R = 150,
	Ghilliesuit_02_R01 = 151,
	Ghilliesuit_02_R02 = 152,
	Ghilliesuit_02_L = 153,
	Ghilliesuit_02_L01 = 154,
	Ghilliesuit_02_L02 = 155,
	Ghilliesuit_01_L = 156,
	Ghilliesuit_01_L01 = 157,
	Ghilliesuit_01_L02 = 158,
	Ghilliesuit_01_R = 159,
	Ghilliesuit_01_R01 = 160,
	Ghilliesuit_01_R02 = 161,
	HoodString_R = 162,
	HoodString_R1 = 163,
	HoodString_R2 = 164,
	HoodString_R3 = 165,
	HoodString_R4 = 166,
	HoodString_L = 167,
	HoodString_L1 = 168,
	HoodString_L2 = 169,
	HoodString_L3 = 170,
	HoodString_L4 = 171,
	HoodString_N = 172,
	HoodString_N1 = 173,
	HoodString_N2 = 174,
	HoodString_N3 = 175,
	HoodString_N4 = 176,
	Dummy_Parachute = 177,
	Ghilliesuit_00_L = 178,
	Ghilliesuit_00_L01 = 179,
	Ghilliesuit_00_L02 = 180,
	Ghilliesuit_00_R = 181,
	Ghilliesuit_00_R01 = 182,
	Ghilliesuit_00_R02 = 183,
	thigh_l = 184,
	calf_l = 185,
	calf_twist_01_l = 186,
	foot_l = 187,
	ball_l = 188,
	thigh_twist_01_l = 189,
	Skirt_FL_Bone02 = 190,
	Skirt_FL_Bone03 = 191,
	Skirt_L_Bone02 = 192,
	Skirt_L_Bone03 = 193,
	thigh_r = 194,
	calf_r = 195,
	calf_twist_01_r = 196,
	foot_r = 197,
	ball_r = 198,
	thigh_twist_01_r = 199,
	Skirt_FR_Bone02 = 200,
	Skirt_FR_Bone03 = 201,
	Skirt_R_Bone02 = 202,
	Skirt_R_Bone03 = 203,
	Dummy_parkour = 204,
	C_Bone_01 = 205,
	C_Bone_02 = 206,
	C_Bone_03 = 207,
	C_Bone_04 = 208,
	Skirt_BL_Bone02 = 209,
	Skirt_BL_Bone03 = 210,
	Skirt_BR_Bone02 = 211,
	Skirt_BR_Bone03 = 212,
	Coat_FL_Bone_01 = 213,
	Coat_FL_Bone_02 = 214,
	Coat_FL_Bone_03 = 215,
	Coat_FL_Bone_04 = 216,
	Coat_FL_Bone_05 = 217,
	Coat_L_Bone_01 = 218,
	Coat_L_Bone_02 = 219,
	Coat_L_Bone_03 = 220,
	Coat_L_Bone_04 = 221,
	Coat_L_Bone_05 = 222,
	Coat_FR_Bone_01 = 223,
	Coat_FR_Bone_02 = 224,
	Coat_FR_Bone_03 = 225,
	Coat_FR_Bone_04 = 226,
	Coat_FR_Bone_05 = 227,
	Coat_R_Bone_01 = 228,
	Coat_R_Bone_02 = 229,
	Coat_R_Bone_03 = 230,
	Coat_R_Bone_04 = 231,
	Coat_R_Bone_05 = 232,
	Coat_BL_Bone_01 = 233,
	Coat_BL_Bone_02 = 234,
	Coat_BL_Bone_03 = 235,
	Coat_BL_Bone_04 = 236,
	Coat_BC_Bone_05 = 237,
	Coat_BR_Bone_01 = 238,
	Coat_BR_Bone_02 = 239,
	Coat_BR_Bone_03 = 240,
	Coat_BR_Bone_04 = 241,
	Coat_BR_Bone_05 = 242,
	ik_foot_root = 243,
	ik_foot_l = 244,
	ik_foot_r = 245,
	ik_hand_root = 246,
	ik_hand_gun = 247,
	ik_hand_l = 248,
	ik_hand_r = 249,
	head_eye_c = 250,
	Hair_Pony_B04 = 251,
	VBthigh_l_calf_l = 252,
	VBthigh_r_calf_r = 253,
	VBhead = 254,
};
void skeletonesp(int X, int Y, int W, int H, int thickness) {
	auto RGB = ImGui::GetColorU32({ 255, 255, 255, 255 });
	ImDrawList* Renderer = ImGui::GetOverlayDrawList();
	Renderer->AddLine(ImVec2(X, Y), ImVec2(W, H), RGB, thickness);

}


float DegreeToRadian(float degree)
{
	return degree * (M_PI / 180);

}

Vector3 RadianToDegree(Vector3 radians)
{
	Vector3 degrees;
	degrees.x = radians.x * (180 / M_PI);
	degrees.y = radians.y * (180 / M_PI);
	degrees.z = radians.z * (180 / M_PI);
	return degrees;
}

Vector3 DegreeToRadian(Vector3 degrees)
{
	Vector3 radians;
	radians.x = degrees.x * (M_PI / 180);
	radians.y = degrees.y * (M_PI / 180);
	radians.z = degrees.z * (M_PI / 180);
	return radians;
}
Vector2 WorldRadar(Vector3 srcPos, Vector3 distPos, float yaw, float radarX, float radarY, float size)
{
	auto cosYaw = cos(DegreeToRadian(yaw));
	auto sinYaw = sin(DegreeToRadian(yaw));

	auto deltaX = srcPos.x - distPos.x;
	auto deltaY = srcPos.y - distPos.y;

	auto locationX = (float)(deltaY * cosYaw - deltaX * sinYaw) / 45.f;
	auto locationY = (float)(deltaX * cosYaw + deltaY * sinYaw) / 45.f;

	if (locationX > (size - 2.f))
		locationX = (size - 2.f);
	else if (locationX < -(size - 2.f))
		locationX = -(size - 2.f);

	if (locationY > (size - 6.f))
		locationY = (size - 6.f);
	else if (locationY < -(size - 6.f))
		locationY = -(size - 6.f);

	return Vector2((int)(-locationX + radarX), (int)(locationY + radarY));
}
static Vector3 pRadar;
void DrawLine3(int x1, int y1, int x2, int y2, RGBA* color, int thickness)
{
	ImGui::GetOverlayDrawList()->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), ImGui::ColorConvertFloat4ToU32(ImVec4(color->R / 255.0, color->G / 255.0, color->B / 255.0, color->A / 255.0)), thickness);
}
void DrawBorder(float x, float y, float w, float h, float px, RGBA* BorderColor)
{
	DrawRect(x, (y + h - px), w, px, BorderColor, 1 / 2);
	DrawRect(x, y, px, h, BorderColor, 1 / 2);
	DrawRect(x, y, w, px, BorderColor, 1 / 2);
	DrawRect((x + w - px), y, px, h, BorderColor, 1 / 2);
}

void DrawCircleFilled2(int x, int y, int radius, RGBA* color)
{
	ImGui::GetOverlayDrawList()->AddCircleFilled(ImVec2(x, y), radius, ImGui::ColorConvertFloat4ToU32(ImVec4(color->R / 255.0, color->G / 255.0, color->B / 255.0, color->A / 255.0)));
}

void DrawRadar(Vector3 EntityPos)
{
	auto radar_posX = pRadar.x + 135;
	auto radar_posY = pRadar.y + 135;
	uint64_t LocalRootComp = read<uint64_t>(LocalPawn + RootComponent);
	FMinimalViewInfo camera = read<FMinimalViewInfo>(CameraManager + CameraCachePrivate + 0x10);
	Vector3 LocalPos = read<Vector3>(LocalRootComp + RelativeLocation);
	auto Radar2D = WorldRadar(LocalPos, EntityPos, camera.Rotation.y, radar_posX, radar_posY, 135.f);
	DrawCircleFilled2(Radar2D.x, Radar2D.y, 2, &Col.red);
}

void cheat()
{
	if (Characters.Count == 0)
		return;
	for (int i = 0; i < Characters.Count; i++)
	{
		uintptr_t actor = Characters[i];
		if (!actor)
			continue;

		if (actor == LocalPawn)
			continue;
	
		uintptr_t pawn = read<uintptr_t>(actor + 0x2a8);	
		uintptr_t playerstate = read<uintptr_t>(pawn + 0x338);
		if (!playerstate)
			continue;

	
		uintptr_t rootcomponent = read<uintptr_t>(LocalPawn + RootComponent);

		Vector3 rootposition = read<Vector3>(rootcomponent + RelativeLocation);
		Vector3 rootpositionout = project_world_to_screen(rootposition);

		float health = read<float>(actor + 0x3cdc);

		uintptr_t mesh = read<uintptr_t>(actor + 0x398);
		FMinimalViewInfo camerainfo = read<FMinimalViewInfo>(CameraManager + CameraCachePrivate + 0x10);

		Vector3 vBaseBone = GetBone(mesh, 0);
		Vector3 vHeadBone = GetBone(mesh, 68);

		Vector3 vBaseBoneOut = project_world_to_screen(vBaseBone);
		Vector3 vHeadBoneOut = project_world_to_screen(vHeadBone);

		Vector3 vBaseBoneOut2 = project_world_to_screen(Vector3(vBaseBone.x, vBaseBone.y, vBaseBone.z - 15));
		Vector3 vHeadBoneOut2 = project_world_to_screen(Vector3(vHeadBone.x, vHeadBone.y, vHeadBone.z));

		float BoxHeight = abs(vHeadBoneOut.y - vBaseBoneOut.y);
		float BoxWidth = BoxHeight * 0.55;


		float distance = camerainfo.Location.distancee(vHeadBone) * 0.001F;


		std::string DistanceSt = std::to_string((int)(distance * 5));

		std::string Can = std::to_string((int)(health));

		uintptr_t rootcomponenttt = read<uintptr_t>(actor + RootComponent);
		Vector3 pos = read<Vector3>(rootcomponenttt + RelativeLocation);

		if (radar)
		{
			pRadar.x = (GetWindowSize().x / GetWindowSize().x);
			pRadar.y = GetWindowSize().x / 2 - GetWindowSize().y / 2 - 410;
			DrawRect(pRadar.x, pRadar.y, 270, 270, &Col.black, 1);
			DrawBorder(pRadar.x, pRadar.y, 270, 270, 1, &Col.SilverWhite);
			auto radar_posX = pRadar.x + 135;
			auto radar_posY = pRadar.y + 135;
			DrawLine3(radar_posX, radar_posY, radar_posX, radar_posY + 135, &Col.SilverWhite, 1);
			DrawLine3(radar_posX, radar_posY, radar_posX, radar_posY - 135, &Col.SilverWhite, 1);
			DrawLine3(radar_posX, radar_posY, radar_posX + 135, radar_posY, &Col.SilverWhite, 1);
			DrawLine3(radar_posX, radar_posY, radar_posX - 135, radar_posY, &Col.SilverWhite, 1);
			DrawCircleFilled2(radar_posX - 0.5f, radar_posY - 0.5f, 3, &Col.yellow);
			DrawRadar(pos);
		}
		
		if ((distance * 5) < distanceee)
		{
			if (viewrotation)
			{
				Vector3 conv_angle = pos;

				Vector3 head_bone = GetBone(mesh, 68);

				Vector3 output_angle;
				angle_vectors(conv_angle, &output_angle);

				output_angle.x *= 125;
				output_angle.y *= 125;
				output_angle.z *= 125;

				Vector3 end_angle = head_bone + output_angle;

				Vector3 head_out = project_world_to_screen(head_bone);
				Vector3 end_out = project_world_to_screen(end_angle);


				DrawLine(ImVec2(head_out.x, head_out.y), ImVec2(end_out.x, end_out.y), ImGui::ColorConvertFloat4ToU32(ImVec4(255, 255, 255, 255)), 1.5f);
			}
			if (BoneESP)
			{	
				Vector3 BoneHead    =   project_world_to_screen(GetBone(mesh, BoneList::head));
				Vector3 BoneNeck    =   project_world_to_screen(GetBone(mesh, BoneList::neck_01));
				Vector3 BoneStomach =   project_world_to_screen(GetBone(mesh, BoneList::spine_01));
				Vector3 BoneLArm3   =   project_world_to_screen(GetBone(mesh, BoneList::hand_l));
				Vector3 BoneLArm2   =   project_world_to_screen(GetBone(mesh, BoneList::lowerarm_l));
				Vector3 BoneLArm1   =   project_world_to_screen(GetBone(mesh, BoneList::upperarm_l));
				Vector3 BoneRArm3   =   project_world_to_screen(GetBone(mesh, BoneList::hand_r));
				Vector3 BoneRArm2   =   project_world_to_screen(GetBone(mesh, BoneList::lowerarm_r));
				Vector3 BoneRArm1   =   project_world_to_screen(GetBone(mesh, BoneList::upperarm_r));
				Vector3 BoneLLeg3   =   project_world_to_screen(GetBone(mesh, BoneList::foot_r));
				Vector3 BoneLLeg2   =   project_world_to_screen(GetBone(mesh, BoneList::ball_r));
				Vector3 BoneLLeg1   =   project_world_to_screen(GetBone(mesh, BoneList::thigh_r));
				Vector3 BoneRLeg3   =   project_world_to_screen(GetBone(mesh, BoneList::foot_l));
				Vector3 BoneRLeg2   =   project_world_to_screen(GetBone(mesh, BoneList::ball_l));
				Vector3 BoneRLeg1   =   project_world_to_screen(GetBone(mesh, BoneList::thigh_l));
				skeletonesp(BoneHead.x, BoneHead.y, BoneNeck.x, BoneNeck.y, 1.5f);
				skeletonesp(BoneNeck.x, BoneNeck.y, BoneLArm1.x, BoneLArm1.y, 1.5f);
				skeletonesp(BoneLArm1.x, BoneLArm1.y, BoneLArm2.x, BoneLArm2.y, 1.5f);
				skeletonesp(BoneLArm2.x, BoneLArm2.y, BoneLArm3.x, BoneLArm3.y, 1.5f);
				skeletonesp(BoneNeck.x, BoneNeck.y, BoneRArm1.x, BoneRArm1.y, 1.5f);
				skeletonesp(BoneRArm1.x, BoneRArm1.y, BoneRArm2.x, BoneRArm2.y, 1.5f);
				skeletonesp(BoneRArm2.x, BoneRArm2.y, BoneRArm3.x, BoneRArm3.y, 1.5f);
				skeletonesp(BoneNeck.x, BoneNeck.y, BoneStomach.x, BoneStomach.y, 1.5f);
				skeletonesp(BoneStomach.x, BoneStomach.y, BoneLLeg1.x, BoneLLeg1.y, 1.5f);
				skeletonesp(BoneLLeg1.x, BoneLLeg1.y, BoneLLeg2.x, BoneLLeg2.y, 1.5f);
				skeletonesp(BoneLLeg2.x, BoneLLeg2.y, BoneLLeg3.x, BoneLLeg3.y, 1.5f);
				skeletonesp(BoneStomach.x, BoneStomach.y, BoneRLeg1.x, BoneRLeg1.y, 1.5f);
				skeletonesp(BoneRLeg1.x, BoneRLeg1.y, BoneRLeg2.x, BoneRLeg2.y, 1.5f);
				skeletonesp(BoneRLeg2.x, BoneRLeg2.y, BoneRLeg3.x, BoneRLeg3.y, 1.5f);
			}
			if (healthbar)
			{
				if (health <= 0) continue;

				float entHp = health;
				if (entHp > 101) entHp = 100;
				float HealthHeightCalc = ((float)entHp / 100) * (float)BoxHeight;
				DrawFilledRectImGui(vBaseBoneOut2.x - (BoxWidth / 2), vHeadBoneOut2.y, 2, BoxHeight, ImVec4(COLOUR(30.0f), COLOUR(30.0f), COLOUR(30.0f), COLOUR(220.0f)));
				DrawFilledRectImGui(vBaseBoneOut2.x - (BoxWidth / 2), vHeadBoneOut2.y, 2, HealthHeightCalc, ImVec4(COLOUR(80.0f), COLOUR(220.0f), COLOUR(100.0f), COLOUR(255.0f)));
			}

			if (DISTANCEESP)	
			{
				DrawString(12, vBaseBoneOut2.x, vBaseBoneOut2.y, &Col.white, true, true, DistanceSt.c_str());
			}
			if (ESP)
			{
				DrawRectImGui((vBaseBoneOut2.x - BoxWidth / 2) - (BoxWidth / 50) - 1, vHeadBoneOut2.y - 1, BoxWidth + 2, BoxHeight + 2, ImVec4(0, 0, 0, 255.f), 1.3f);
				DrawRectImGui((vBaseBoneOut2.x - BoxWidth / 2) - (BoxWidth / 50), vHeadBoneOut2.y, BoxWidth, BoxHeight, ImVec4(255, 255, 255, 255.f), 1.f);
				DrawRectImGui((vBaseBoneOut2.x - BoxWidth / 2) - (BoxWidth / 50) + 1, vHeadBoneOut2.y + 1, BoxWidth - 2, BoxHeight - 2, ImVec4(0, 0, 0, 255.f), 1.3f);
			
			
			/*	{
					Vector3 bottom1 = project_world_to_screen(Vector3(vBaseBone.x + 40, vBaseBone.y - 40, vBaseBone.z));
					Vector3 bottom2 = project_world_to_screen(Vector3(vBaseBone.x - 40, vBaseBone.y - 40, vBaseBone.z));
					Vector3 bottom3 = project_world_to_screen(Vector3(vBaseBone.x - 40, vBaseBone.y + 40, vBaseBone.z));
					Vector3 bottom4 = project_world_to_screen(Vector3(vBaseBone.x + 40, vBaseBone.y + 40, vBaseBone.z));

					Vector3 top1 = project_world_to_screen(Vector3(vHeadBone.x + 40, vHeadBone.y - 40, vHeadBone.z + 15));
					Vector3 top2 = project_world_to_screen(Vector3(vHeadBone.x - 40, vHeadBone.y - 40, vHeadBone.z + 15));
					Vector3 top3 = project_world_to_screen(Vector3(vHeadBone.x - 40, vHeadBone.y + 40, vHeadBone.z + 15));
					Vector3 top4 = project_world_to_screen(Vector3(vHeadBone.x + 40, vHeadBone.y + 40, vHeadBone.z + 15));

					ImGui::GetOverlayDrawList()->AddLine(ImVec2(bottom1.x, bottom1.y), ImVec2(top1.x, top1.y), ImGui::ColorConvertFloat4ToU32(ImVec4(255, 255, 255, 255)), 0.1f);
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(bottom2.x, bottom2.y), ImVec2(top2.x, top2.y), ImGui::ColorConvertFloat4ToU32(ImVec4(255, 255, 255, 255)), 0.1f);
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(bottom3.x, bottom3.y), ImVec2(top3.x, top3.y), ImGui::ColorConvertFloat4ToU32(ImVec4(255, 255, 255, 255)), 0.1f);
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(bottom4.x, bottom4.y), ImVec2(top4.x, top4.y), ImGui::ColorConvertFloat4ToU32(ImVec4(255, 255, 255, 255)), 0.1f);

					ImGui::GetOverlayDrawList()->AddLine(ImVec2(bottom1.x, bottom1.y), ImVec2(bottom2.x, bottom2.y), ImGui::ColorConvertFloat4ToU32(ImVec4(255, 255, 255, 255)), 0.1f);
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(bottom2.x, bottom2.y), ImVec2(bottom3.x, bottom3.y), ImGui::ColorConvertFloat4ToU32(ImVec4(255, 255, 255, 255)), 0.1f);
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(bottom3.x, bottom3.y), ImVec2(bottom4.x, bottom4.y), ImGui::ColorConvertFloat4ToU32(ImVec4(255, 255, 255, 255)), 0.1f);
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(bottom4.x, bottom4.y), ImVec2(bottom1.x, bottom1.y), ImGui::ColorConvertFloat4ToU32(ImVec4(255, 255, 255, 255)), 0.1f);

					ImGui::GetOverlayDrawList()->AddLine(ImVec2(top1.x, top1.y), ImVec2(top2.x, top2.y), ImGui::ColorConvertFloat4ToU32(ImVec4(255, 255, 255, 255)), 0.1f);
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(top2.x, top2.y), ImVec2(top3.x, top3.y), ImGui::ColorConvertFloat4ToU32(ImVec4(255, 255, 255, 255)), 0.1f);
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(top3.x, top3.y), ImVec2(top4.x, top4.y), ImGui::ColorConvertFloat4ToU32(ImVec4(255, 255, 255, 255)), 0.1f);
					ImGui::GetOverlayDrawList()->AddLine(ImVec2(top4.x, top4.y), ImVec2(top1.x, top1.y), ImGui::ColorConvertFloat4ToU32(ImVec4(255, 255, 255, 255)), 0.1f);
				}*/


			}
			if (FILLBOX)
			{
				DrawFilledRectImGui(vBaseBoneOut2.x - (BoxWidth / 2), vHeadBoneOut2.y, BoxWidth, BoxHeight, ImVec4(COLOUR(0.0f), COLOUR(0.0f), COLOUR(0.0f), COLOUR(100.0f)));
			}
			if (SNAPLINE)
			{
				DrawLine(ImVec2(GetWindowSize().x / 2, GetWindowSize().y), ImVec2(vBaseBoneOut.x, vBaseBoneOut.y), ImGui::ColorConvertFloat4ToU32(ImColor(255, 255, 255)), 1.f); // ASAGIDAN
			}

			Vector3 AimbotHead = GetBone(mesh, 68);
			Vector3 vAimbotHead = project_world_to_screen(AimbotHead);



			
			//Vector3 localView = read<Vector3>(LocalController + 0x380);
			//
			//Vector3 neck_position = GetBone(mesh, 68);
			//uintptr_t rootcomponentttt = read<uintptr_t>(actor + RootComponent);
			//Vector3 root_position = read<Vector3>(rootcomponentttt + RelativeLocation);
			//if (neck_position.z <= root_position.z)
			//{
			//	return;
			//}
			//Vector3 vecCaclculatedAngles = fhgfsdhkfshdghfsd205(camerainfo.Location, vHeadBone);
			//Vector3 angleEx = CaadadalcAngle(camerainfo.Location, vHeadBone);
			//
			//Vector3 fin = Vector3(vecCaclculatedAngles.y, angleEx.y, 0);
			//Vector3 delta = fin - localView;
			//Vector3 TargetAngle = localView + (delta / aSmoothAmount);

			float distance5 = 999999;
			float dist = CalculateDistance(GetWindowSize().x / 2, GetWindowSize().y / 2, vHeadBoneOut.x, vHeadBoneOut.y);
			if (dist < distance5 && dist < aimfovv)
			{
				if (GetAsyncKeyState(aimkey))
				{
					if (aimbot)
					{
						CURSORINFO ci = { sizeof(CURSORINFO) };
						if (GetCursorInfo(&ci))
						{
							if (ci.flags == 0)
								mouse_event(MOUSEEVENTF_MOVE, (vAimbotHead.x - GetWindowSize().x / 2) / aSmoothAmount, (vAimbotHead.y - GetWindowSize().y / 2) / aSmoothAmount, 0, 0);
						}
						//driver.write<Vector3>(LocalController + 0x380, TargetAngle);
					}
				}
			}
		}
	}
}
	
	
void render()
{
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::PushFont(main_font);
	if (GetAsyncKeyState(VK_INSERT) & 1) { showmenu = !showmenu; }
	cheat();

	if (aimbot)
	{
		ImGui::GetOverlayDrawList()->AddCircle(ImVec2(GetWindowSize().x / 2, GetWindowSize().y / 2), aimfovv, IM_COL32_WHITE, 10000, 1);
	}

	if (showmenu) { draw_menu(); }
	ImGui::PopFont();
	ImGui::EndFrame();
	p_Device->SetRenderState(D3DRS_ZENABLE, false);
	p_Device->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
	p_Device->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
	p_Device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);

	if (p_Device->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		p_Device->EndScene();
	}

	HRESULT result = p_Device->Present(NULL, NULL, NULL, NULL);

	if (result == D3DERR_DEVICELOST && p_Device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
	{
		ImGui_ImplDX9_InvalidateDeviceObjects();
		p_Device->Reset(&d3dpp);
		ImGui_ImplDX9_CreateDeviceObjects();
	}
}

		
void Loop()
{
	while (true)
	{
		World = read<uintptr_t>(g_base + GWorld);
		//printf("[>] GWorld 0x%p\n", World);
		uintptr_t GameInstance = read<uintptr_t>(World + UGameInstance);
		//printf("[>] UGameInstance 0x%p\n", GameInstance);
		uintptr_t Level = read<uintptr_t>(World + PersistentLevel);
		//printf("[>] Level 0x%p\n", Level);
		uintptr_t ULocalPlayers = read<uintptr_t>(GameInstance + LocalPlayers);
		//printf("[>] ULocalPlayers 0x%p\n", ULocalPlayers);
		ULocalPlayer = read<uintptr_t>(ULocalPlayers);
		//printf("[>] ULocalPlayer 0x%p\n", ULocalPlayer);
		LocalController = read<uintptr_t>(ULocalPlayer + PlayerController);
		//printf("[>] LocalController 0x%p\n", LocalController);
		LocalPawn = read<uintptr_t>(LocalController + Pawn);
		//printf("[>] LocalPawn 0x%p\n", LocalPawn);
		CameraManager = read<uintptr_t>(LocalController + PlayerCameraManager);
		//printf("[>] CameraManager 0x%p\n", CameraManager);
		Characters = read<TArray<uintptr_t>>(Level + Actors);

		std::this_thread::sleep_for(std::chrono::milliseconds(750));
	}
}

using namespace KeyAuth;

std::string namee = "Super People"; // application name. right above the blurred text aka the secret on the licenses tab among other tabs
std::string ownerid = "jHHXUBClti"; // ownerid, found in account settings. click your profile picture on top right of dashboard and then account settings.
std::string secret = "ef71c43cbc4fdfaedd38941b6d240cb1b2b417b66897ea67359780734731ecca"; // app secret, the blurred text on licenses tab and other tabs
std::string version = "1.1"; // leave alone unless you've changed version on website
std::string url = "https://auth.aliencheats.com/api/1.1/"; // change if you're self-hosting
std::string sslPin = "ssl pin key (optional)"; // don't change unless you intend to pin public certificate key. you can get here in the "Pin SHA256" field https://www.ssllabs.com/ssltest/analyze.html?d=keyauth.win&latest. If you do this you need to be aware of when SSL key expires so you can update it
api KeyAuthApp(namee, ownerid, secret, version, url, sslPin);

int main()
{	
	SetConsoleTitleA(skCrypt("SUPER PEOPLE V1"));
	int horizontal = 0, vertical = 0;
	int x = 350, y = 250; //// alta doðru
	HWND consoleWindow = GetConsoleWindow();
	SetWindowLong(consoleWindow, GWL_STYLE, GetWindowLong(consoleWindow, GWL_STYLE) & ~WS_MAXIMIZEBOX & ~WS_SIZEBOX);
	HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_CURSOR_INFO     cursorInfo;
	GetConsoleCursorInfo(out, &cursorInfo);
	SetConsoleCursorInfo(out, &cursorInfo);
	CONSOLE_FONT_INFOEX cfi;
	cfi.cbSize = sizeof(cfi);
	cfi.nFont = 0;
	cfi.dwFontSize.X = 0;
	cfi.dwFontSize.Y = 15;
	cfi.FontFamily = FF_DONTCARE;
	cfi.FontWeight = FW_NORMAL;
	system("color 4");
	wcscpy_s(cfi.FaceName, L"Consolas");
	SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &cfi);
	HWND hwnd = GetConsoleWindow();
	MoveWindow(hwnd, 0, 0, x, y, TRUE);
	LONG lStyle = GetWindowLong(hwnd, GWL_STYLE);
	lStyle &= ~(WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
	SetWindowLong(hwnd, GWL_STYLE, lStyle);
	HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(console, &csbi);
	COORD scrollbar = {
		csbi.srWindow.Right - csbi.srWindow.Left + 1,
		csbi.srWindow.Bottom - csbi.srWindow.Top + 1
	};
	if (console == 0)
		throw 0;
	else
		SetConsoleScreenBufferSize(console, scrollbar);
	SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
	SetLayeredWindowAttributes(hwnd, 0, 225, LWA_ALPHA);
	SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);

	SetConsoleTitle("SUPER PEOPLE V1");
	mainprotect();

	std::string DosyaKonumu = (_xor_("C:\\Windows\\System\\SuperPeoplePrivLicense.txt"));

	KeyAuthApp.init();
EnBasaDon:
	std::string License;
	FILE* Dosya;
	if (Dosya = fopen((DosyaKonumu.c_str()), skCrypt("r"))) {
		std::ifstream DosyaOku(DosyaKonumu.c_str());
		std::string Anahtar;
		std::getline(DosyaOku, Anahtar);
		DosyaOku.close();
		fclose(Dosya);
		License = Anahtar;
		goto LisansaGit;
	}
	else
	{
		SetConsoleTitleA(skCrypt("  "));

		system(skCrypt("cls"));
		std::cout << skCrypt("\n\n  [+] Connecting..");
		std::cout << skCrypt("\n\n  [+] Enter License: ");
		std::cin >> License;
	LisansaGit:
		std::ofstream key; key.open(DosyaKonumu.c_str());
		key << License;
		key.close();
		KeyAuthApp.license(License);
		if (!KeyAuthApp.data.success)
		{
			std::cout << _xor_("\n Status: ").c_str() + KeyAuthApp.data.message;
			Sleep(1500);
			remove(DosyaKonumu.c_str());
			goto EnBasaDon;
			//exit(0);
		}
		system(skCrypt("cls"));
		Sleep(300);
		std::cout << skCrypt("\n\n  [+] License Activated.") << std::endl;;
	}

r8:
	if (!driver.init())
	{
	r7:
		if (FindWindowA(skCrypt("BravoHotelClient-Win64-Shipping.protected.exe"), NULL))
		{
			printf(skCrypt("[>] close game please...\n"));
			Sleep(1000);
			goto r7;
		}
		if (LoadDriver())
		{
			printf(skCrypt("[>] driver loaded!\n"));
			Sleep(1000);
			system("cls");
			goto r8;
		}
	}
	HWND Entryhwnd = NULL;
	while (Entryhwnd == NULL)
	{
		printf(skCrypt("[>] waiting for game\n"));
		Sleep(1);
		g_pid = get_pid("BravoHotelClient-Win64-Shipping.protected.exe");
		Entryhwnd = get_process_wnd(g_pid);
		Sleep(1);
	}
	printf(skCrypt("[>] pid: %d\n"), g_pid);
	driver.attach(g_pid);
	setup_window();
	init_wndparams(MyWnd);
	g_base = driver.get_process_base(g_pid);
	if (!g_base) { printf(skCrypt("[>] g_base can't found!\n")); return 0; }
	printf("[>] g_base: 0x%p\n", g_base);
	static RECT old_rc;
	ZeroMemory(&Message, sizeof(MSG));
	Style();
	SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
	std::thread(Loop).detach();
	while (Message.message != WM_QUIT)
	{
		if (PeekMessage(&Message, MyWnd, 0, 0, PM_REMOVE)) {
			TranslateMessage(&Message);
			DispatchMessage(&Message);
		}

		HWND hwnd_active = GetForegroundWindow();


		if (hwnd_active == GameWnd) {
			HWND hwndtest = GetWindow(hwnd_active, GW_HWNDPREV);
			SetWindowPos(MyWnd, hwndtest, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}
		RECT rc;
		POINT xy;

		ZeroMemory(&rc, sizeof(RECT));
		ZeroMemory(&xy, sizeof(POINT));
		GetClientRect(GameWnd, &rc);
		ClientToScreen(GameWnd, &xy);
		rc.left = xy.x;
		rc.top = xy.y;

		ImGuiIO& io = ImGui::GetIO();
		io.ImeWindowHandle = GameWnd;
		io.DeltaTime = 1.0f / 60.0f;

		POINT p;
		GetCursorPos(&p);
		io.MousePos.x = p.x - xy.x;
		io.MousePos.y = p.y - xy.y;
		if (GetAsyncKeyState(0x1)) {
			io.MouseDown[0] = true;
			io.MouseClicked[0] = true;
			io.MouseClickedPos[0].x = io.MousePos.x;
			io.MouseClickedPos[0].x = io.MousePos.y;
		}
		else io.MouseDown[0] = false;

		if (rc.left != old_rc.left || rc.right != old_rc.right || rc.top != old_rc.top || rc.bottom != old_rc.bottom) {

			old_rc = rc;

			Width = rc.right;
			Height = rc.bottom;

			p_Params.BackBufferWidth = Width;
			p_Params.BackBufferHeight = Height;
			SetWindowPos(MyWnd, (HWND)0, xy.x, xy.y, Width, Height, SWP_NOREDRAW);
			p_Device->Reset(&p_Params);
		}
		render();
		Sleep(2);
	}

	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	cleanup_d3d();
	DestroyWindow(MyWnd);
	return Message.wParam;
}