#include "DirectOverlay.h"
#include <D3dx9math.h>
#include <sstream>
#include <string>
#include <algorithm>
#include <list>
#include <iostream>
#include<Windows.h>
#include<string>

#define M_PI 3.14159265358979323846264338327950288419716939937510

#define OFFSET_UWORLD 0x985C970

using namespace std;

bool Menu = true;
bool Aimbot = true;
bool EnemyESP = true;
bool skeleton = true;
bool BoxESP = true;
bool LineESP = true;
bool DistanceESP = true;
bool DrawFov = true;

DWORD processID;
HWND hwnd = NULL;

int width;
int height;
int localplayerID;
float FovAngle;

HANDLE DriverHandle;
uint64_t base_address;

DWORD_PTR Uworld;
DWORD_PTR LocalPawn;
DWORD_PTR Localplayer;
DWORD_PTR Rootcomp;
DWORD_PTR PlayerController;
DWORD_PTR Ulevel;

Vector3 localactorpos;
Vector3 Localcam;

float AimFOV = 25;

bool isaimbotting;
DWORD_PTR entityx;

FTransform GetBoneIndex(DWORD_PTR mesh, int index)
{
	DWORD_PTR bonearray = read<DWORD_PTR>(DriverHandle, processID, mesh + 0x4A8);

	if (bonearray == NULL)
	{
		bonearray = read<DWORD_PTR>(DriverHandle, processID, mesh + 0x4A8 + 0x10); // added 4u
	}

	return read<FTransform>(DriverHandle, processID, bonearray + (index * 0x30));  // doesn't change
}

Vector3 GetBoneWithRotation(DWORD_PTR mesh, int id)
{
	FTransform bone = GetBoneIndex(mesh, id);
	FTransform ComponentToWorld = read<FTransform>(DriverHandle, processID, mesh + 0x1C0);  // have never seen this change 4u

	D3DMATRIX Matrix;
	Matrix = MatrixMultiplication(bone.ToMatrixWithScale(), ComponentToWorld.ToMatrixWithScale());

	return Vector3(Matrix._41, Matrix._42, Matrix._43);
}

D3DXMATRIX Matrix(Vector3 rot, Vector3 origin = Vector3(0, 0, 0))
{
	float radPitch = (rot.x * float(M_PI) / 180.f);
	float radYaw = (rot.y * float(M_PI) / 180.f);
	float radRoll = (rot.z * float(M_PI) / 180.f);

	float SP = sinf(radPitch);
	float CP = cosf(radPitch);
	float SY = sinf(radYaw);
	float CY = cosf(radYaw);
	float SR = sinf(radRoll);
	float CR = cosf(radRoll);

	D3DMATRIX matrix;
	matrix.m[0][0] = CP * CY;
	matrix.m[0][1] = CP * SY;
	matrix.m[0][2] = SP;
	matrix.m[0][3] = 0.f;

	matrix.m[1][0] = SR * SP * CY - CR * SY;
	matrix.m[1][1] = SR * SP * SY + CR * CY;
	matrix.m[1][2] = -SR * CP;
	matrix.m[1][3] = 0.f;

	matrix.m[2][0] = -(CR * SP * CY + SR * SY);
	matrix.m[2][1] = CY * SR - CR * SP * SY;
	matrix.m[2][2] = CR * CP;
	matrix.m[2][3] = 0.f;

	matrix.m[3][0] = origin.x;
	matrix.m[3][1] = origin.y;
	matrix.m[3][2] = origin.z;
	matrix.m[3][3] = 1.f;

	return matrix;
}

//4u note:  changes to projectw2s and camera are the most diffucult changes to understand reworking old camloc, be careful blindly making edits

extern Vector3 CameraEXT(0, 0, 0);

Vector3 ProjectWorldToScreen(Vector3 WorldLocation, Vector3 camrot)
{
	Vector3 Screenlocation = Vector3(0, 0, 0);
	Vector3 Camera;

	auto chain69 = read<uintptr_t>(DriverHandle, processID, Localplayer + 0xa8);
	uint64_t chain699 = read<uintptr_t>(DriverHandle, processID, chain69 + 8);

	Camera.x = read<float>(DriverHandle, processID, chain699 + 0x7F8);  //camera pitch  watch out for x and y swapped 4u
	Camera.y = read<float>(DriverHandle, processID, Rootcomp + 0x12C);  //camera yaw

	float test = asin(Camera.x);
	float degrees = test * (180.0 / M_PI);
	Camera.x = degrees;

	if (Camera.y < 0)
		Camera.y = 360 + Camera.y;

	D3DMATRIX tempMatrix = Matrix(Camera);
	Vector3 vAxisX, vAxisY, vAxisZ;

	vAxisX = Vector3(tempMatrix.m[0][0], tempMatrix.m[0][1], tempMatrix.m[0][2]);
	vAxisY = Vector3(tempMatrix.m[1][0], tempMatrix.m[1][1], tempMatrix.m[1][2]);
	vAxisZ = Vector3(tempMatrix.m[2][0], tempMatrix.m[2][1], tempMatrix.m[2][2]);

	uint64_t chain = read<uint64_t>(DriverHandle, processID, Localplayer + 0x70);
	uint64_t chain1 = read<uint64_t>(DriverHandle, processID, chain + 0x98);
	uint64_t chain2 = read<uint64_t>(DriverHandle, processID, chain1 + 0x130);

	Vector3 vDelta = WorldLocation - read<Vector3>(DriverHandle, processID, chain2 + 0x10); //camera location credits for Object9999
	Vector3 vTransformed = Vector3(vDelta.Dot(vAxisY), vDelta.Dot(vAxisZ), vDelta.Dot(vAxisX));

	if (vTransformed.z < 1.f)
		vTransformed.z = 1.f;

	float zoom = read<float>(DriverHandle, processID, chain699 + 0x590);

	FovAngle = 80.0f / (zoom / 1.19f);
	float ScreenCenterX = width / 2.0f;
	float ScreenCenterY = height / 2.0f;

	Screenlocation.x = ScreenCenterX + vTransformed.x * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.z;
	Screenlocation.y = ScreenCenterY - vTransformed.y * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.z;
	CameraEXT = Camera;

	return Screenlocation;
}


void menu()
{
	if (Menu)
	{
		DrawBox(20, 300, 370, 246, 0.0f, 0.f, 0.f, 0.f, 80, true);                                  //zweites wo oben anfängt // dritte breite //viertens nach unten
		//DrawString(_xor_("VoidlessCheatz").c_str(), 17, 10, 8, 255.f, 255.f, 255.f, 255.f);
		//DrawString(_xor_("dsc.gg/Voidless").c_str(), 60, 1487, 8, 255.f, 255.f, 255.f, 255.f);
		//DrawString(_xor_("dsc.gg/Voidless").c_str(), 60, 1486, 8, 255.f, 255.f, 255.f, 255.f);
		//DrawString(_xor_("dsc gg/Voidless").c_str(), 60, 1485, 8, 255.f, 255.f, 255.f, 255.f);
		//DrawString(_xor_("dsc gg/Voidless").c_str(), 60, 1484, 8, 255.f, 255.f, 255.f, 255.f);
		//DrawString(_xor_("dsc gg/Voidless").c_str(), 60, 1483, 8, 255.f, 255.f, 255.f, 255.f);
		//DrawString(_xor_("dsc gg/Voidless").c_str(), 60, 1482, 8, 255.f, 255.f, 255.f, 255.f);
		//DrawString(_xor_("dsc gg/Voidless").c_str(), 60, 1481, 8, 255.f, 255.f, 255.f, 255.f);
		//DrawString(_xor_("dsc gg/Voidless").c_str(), 60, 1480, 8, 255.f, 255.f, 255.f, 255.f);
		//DrawString(_xor_("OG").c_str(), 43, 28, 300, 255.f, 151.f, 255.f, 1.f);          //erste größe //zweite rechts //dritte tiefe 
		DrawString(_xor_("VoidlessFN").c_str(), 43, 28, 300, 60.f, 120.f, 0.f, 110.f);
		DrawString(_xor_("---------------------------------------------").c_str(), 20, 28, 335, 25, 56, 255, 255);
		DrawString(_xor_("dsc.gg/Voidless").c_str(), 16, 274, 329, 255.f, 255.f, 255.f, 255.f);
		DrawString(_xor_("ESP :").c_str(), 23, 28, 400, 255.f, 255.f, 255.f, 255.f);
		if (EnemyESP)
			DrawString(_xor_("ON").c_str(), 15, 10 + 110, 10 + 430, 0.f, 255.f, 0.f, 255.f);
		else
			DrawString(_xor_("OFF").c_str(), 15, 10 + 110, 10 + 430, 255.f, 0.f, 0.f, 255.f);

		DrawString(_xor_("EnemyText =").c_str(), 14, 28, 440, 105.f, 255.f, 255.f, 255.f); //erste größe //zweite rechts //dritte tiefe 

		if (BoxESP)
			DrawString(_xor_("ON").c_str(), 15, 10 + 110, 10 + 450, 0.f, 255.f, 0.f, 255.f);
		else
			DrawString(_xor_("OFF").c_str(), 15, 10 + 110, 10 + 450, 255.f, 0.f, 0.f, 255.f);

		DrawString(_xor_("CornerBox =").c_str(), 14, 28, 460, 105.f, 255.f, 255.f, 255.f); //erste größe //zweite rechts //dritte tiefe 

		if (LineESP)
			DrawString(_xor_("ON").c_str(), 15, 10 + 110, 10 + 470, 0.f, 255.f, 0.f, 255.f);
		else
			DrawString(_xor_("OFF").c_str(), 15, 10 + 110, 10 + 470, 255.f, 0.f, 0.f, 255.f);

		DrawString(_xor_("TrackLines =").c_str(), 14, 28, 480, 105.f, 255.f, 255.f, 255.f); //erste größe //zweite rechts //dritte tiefe 

		if (DistanceESP)
			DrawString(_xor_("ON").c_str(), 15, 10 + 110, 10 + 490, 0.f, 255.f, 0.f, 255.f);
		else
			DrawString(_xor_("OFF").c_str(), 15, 10 + 110, 10 + 490, 255.f, 0.f, 0.f, 255.f);

		DrawString(_xor_("Distance   =").c_str(), 14, 28, 500, 105.f, 255.f, 255.f, 255.f); //erste größe //zweite rechts //dritte tiefe 

		if (skeleton)
			DrawString(_xor_("ON").c_str(), 15, 10 + 110, 10 + 510, 0.f, 255.f, 0.f, 255.f);
		else
			DrawString(_xor_("OFF").c_str(), 15, 10 + 110, 10 + 510, 255.f, 0.f, 0.f, 255.f);

		DrawString(_xor_("Skeleton   =").c_str(), 14, 28, 520, 255.f, 255.f, 255.f, 255.f);

		if (Aimbot)
			DrawString(_xor_("ON").c_str(), 21, 10 + 124, 10 + 355, 0.f, 255.f, 0.f, 255.f);
		else
			DrawString(_xor_("OFF").c_str(), 21, 10 + 124, 10 + 355, 255.f, 0.f, 0.f, 255.f); 

		DrawString(_xor_("Aimbot =").c_str(), 23, 28, 363, 255.f, 255.f, 255.f, 255.f);          //erste größe //zweite rechts //dritte tiefe 
		DrawString(_xor_("[F1]").c_str(), 20, 180, 365, 255.f, 255.f, 255.f, 255.f);     //erste größe //zweite rechts //dritte tiefe 
		DrawString(_xor_("[F2]").c_str(), 14, 154, 440, 255.f, 255.f, 255.f, 255.f);
		DrawString(_xor_("[F3]").c_str(), 14, 154, 461, 255.f, 255.f, 255.f, 255.f);
		DrawString(_xor_("[F4]").c_str(), 14, 154, 480, 255.f, 255.f, 255.f, 255.f);
		DrawString(_xor_("[F5]").c_str(), 14, 154, 500, 255.f, 255.f, 255.f, 255.f);
		DrawString(_xor_("[F6]").c_str(), 14, 154, 520, 255.f, 255.f, 255.f, 255.f);
	}
}

DWORD Menuthread(LPVOID in)
{
	while (1)
	{
		HWND test = FindWindowA(0, _xor_("Fortnite  ").c_str());

		if (test == NULL)
		{
			ExitProcess(0);
		}

		if (GetAsyncKeyState(VK_INSERT) & 1) {
			Menu = !Menu;
		}

		if (Menu)
		{
			if (GetAsyncKeyState(VK_F2) & 1) {
				EnemyESP = !EnemyESP;
			}

			if (GetAsyncKeyState(VK_F3) & 1) {
				BoxESP = !BoxESP;
			}

			if (GetAsyncKeyState(VK_F4) & 1) {
				LineESP = !LineESP;
			}

			if (GetAsyncKeyState(VK_F5) & 1) {
				DistanceESP = !DistanceESP;
			}

			if (GetAsyncKeyState(VK_F6) & 1) {
				skeleton = !skeleton;
			}

			if (GetAsyncKeyState(VK_F1) & 1) {
				Aimbot = !Aimbot;
			}
		}
	}
}

bool GetAimKey()
{
	return (GetAsyncKeyState(VK_RBUTTON));  // was "alt" key in original but didn't want to lose too many people 4u
}

void aimbot(float x, float y)
{
	float ScreenCenterX = (width / 2);
	float ScreenCenterY = (height / 2);
	int AimSpeed = 5.0f;
	float TargetX = 0;
	float TargetY = 0;

	if (x != 0)
	{
		if (x > ScreenCenterX)
		{
			TargetX = -(ScreenCenterX - x);
			TargetX /= AimSpeed;
			if (TargetX + ScreenCenterX > ScreenCenterX * 2) TargetX = 0;
		}

		if (x < ScreenCenterX)
		{
			TargetX = x - ScreenCenterX;
			TargetX /= AimSpeed;
			if (TargetX + ScreenCenterX < 0) TargetX = 0;
		}
	}

	if (y != 0)
	{
		if (y > ScreenCenterY)
		{
			TargetY = -(ScreenCenterY - y);
			TargetY /= AimSpeed;
			if (TargetY + ScreenCenterY > ScreenCenterY * 2) TargetY = 0;
		}

		if (y < ScreenCenterY)
		{
			TargetY = y - ScreenCenterY;
			TargetY /= AimSpeed;
			if (TargetY + ScreenCenterY < 0) TargetY = 0;
		}
	}

	mouse_event(MOUSEEVENTF_MOVE, static_cast<DWORD>(TargetX), static_cast<DWORD>(TargetY), NULL, NULL);

	return;
}

double GetCrossDistance(double x1, double y1, double x2, double y2)
{
	return sqrt(pow((x2 - x1), 2) + pow((y2 - y1), 2));
}

bool GetClosestPlayerToCrossHair(Vector3 Pos, float& max, float aimfov, DWORD_PTR entity)
{
	if (!GetAimKey() || !isaimbotting)
	{
		if (entity)
		{
			float Dist = GetCrossDistance(Pos.x, Pos.y, width / 2, height / 2);

			if (Dist < max)
			{
				max = Dist;
				entityx = entity;
				AimFOV = aimfov;
			}
		}
	}
	return false;
}

void AimAt(DWORD_PTR entity, Vector3 Localcam)
{
	uint64_t currentactormesh = read<uint64_t>(DriverHandle, processID, entity + 0x280); // changes often 4u
	auto rootHead = GetBoneWithRotation(currentactormesh, 66);
	Vector3 rootHeadOut = ProjectWorldToScreen(rootHead, Vector3(Localcam.y, Localcam.x, Localcam.z));

	if (rootHeadOut.x != 0 || rootHeadOut.y != 0)  // these are both y in all updates, fixed 4u, is old bug never fixed
	{
		if ((GetCrossDistance(rootHeadOut.x, rootHeadOut.y, width / 2, height / 2) <= AimFOV * 8) || isaimbotting)
		{
			aimbot(rootHeadOut.x, rootHeadOut.y);
		}
	}
}

void aimbot(Vector3 Localcam)
{
	if (entityx != 0)
	{
		if (GetAimKey())
		{
			AimAt(entityx, Localcam);
		}
		else
		{
			isaimbotting = false;
		}
	}
}



void AIms(DWORD_PTR entity, Vector3 Localcam)
{
	float max = 100.f;

	uint64_t currentactormesh = read<uint64_t>(DriverHandle, processID, entity + 0x280);  // changed often 4u

	Vector3 rootHead = GetBoneWithRotation(currentactormesh, 66);
	Vector3 rootHeadOut = ProjectWorldToScreen(rootHead, Vector3(Localcam.y, Localcam.x, Localcam.z));

	if (GetClosestPlayerToCrossHair(rootHeadOut, max, AimFOV, entity))
		entityx = entity;
}

void DrawSkeleton(DWORD_PTR mesh)
{


	Vector3 vHeadBone = GetBoneWithRotation(mesh, 96);
	Vector3 vHip = GetBoneWithRotation(mesh, 2);
	Vector3 vNeck = GetBoneWithRotation(mesh, 65);
	Vector3 vUpperArmLeft = GetBoneWithRotation(mesh, 34);
	Vector3 vUpperArmRight = GetBoneWithRotation(mesh, 91);
	Vector3 vLeftHand = GetBoneWithRotation(mesh, 35);
	Vector3 vRightHand = GetBoneWithRotation(mesh, 63);
	Vector3 vLeftHand1 = GetBoneWithRotation(mesh, 33);
	Vector3 vRightHand1 = GetBoneWithRotation(mesh, 60);
	Vector3 vRightThigh = GetBoneWithRotation(mesh, 74);
	Vector3 vLeftThigh = GetBoneWithRotation(mesh, 67);
	Vector3 vRightCalf = GetBoneWithRotation(mesh, 75);
	Vector3 vLeftCalf = GetBoneWithRotation(mesh, 68);
	Vector3 vLeftFoot = GetBoneWithRotation(mesh, 69);
	Vector3 vRightFoot = GetBoneWithRotation(mesh, 76);

	Vector3 vHeadBoneOut = ProjectWorldToScreen(vHeadBone, Vector3(Localcam.y, Localcam.x, Localcam.z));
	Vector3 vHipOut = ProjectWorldToScreen(vHip, Vector3(Localcam.y, Localcam.x, Localcam.z));
	Vector3 vNeckOut = ProjectWorldToScreen(vNeck, Vector3(Localcam.y, Localcam.x, Localcam.z));
	Vector3 vUpperArmLeftOut = ProjectWorldToScreen(vUpperArmLeft, Vector3(Localcam.y, Localcam.x, Localcam.z));
	Vector3 vUpperArmRightOut = ProjectWorldToScreen(vUpperArmRight, Vector3(Localcam.y, Localcam.x, Localcam.z));
	Vector3 vLeftHandOut = ProjectWorldToScreen(vLeftHand, Vector3(Localcam.y, Localcam.x, Localcam.z));
	Vector3 vRightHandOut = ProjectWorldToScreen(vRightHand, Vector3(Localcam.y, Localcam.x, Localcam.z));
	Vector3 vLeftHandOut1 = ProjectWorldToScreen(vLeftHand1, Vector3(Localcam.y, Localcam.x, Localcam.z));
	Vector3 vRightHandOut1 = ProjectWorldToScreen(vRightHand1, Vector3(Localcam.y, Localcam.x, Localcam.z));
	Vector3 vRightThighOut = ProjectWorldToScreen(vRightThigh, Vector3(Localcam.y, Localcam.x, Localcam.z));
	Vector3 vLeftThighOut = ProjectWorldToScreen(vLeftThigh, Vector3(Localcam.y, Localcam.x, Localcam.z));
	Vector3 vRightCalfOut = ProjectWorldToScreen(vRightCalf, Vector3(Localcam.y, Localcam.x, Localcam.z));
	Vector3 vLeftCalfOut = ProjectWorldToScreen(vLeftCalf, Vector3(Localcam.y, Localcam.x, Localcam.z));
	Vector3 vLeftFootOut = ProjectWorldToScreen(vLeftFoot, Vector3(Localcam.y, Localcam.x, Localcam.z));
	Vector3 vRightFootOut = ProjectWorldToScreen(vRightFoot, Vector3(Localcam.y, Localcam.x, Localcam.z));

	DrawLine(vHipOut.x, vHipOut.y, vNeckOut.x, vNeckOut.y, 0.7f, 255.f, 255.f, 255.f, 200.f);

	DrawLine(vUpperArmLeftOut.x, vUpperArmLeftOut.y, vNeckOut.x, vNeckOut.y, 0.7f, 255.f, 255.f, 255.f, 200.f);
	DrawLine(vUpperArmRightOut.x, vUpperArmRightOut.y, vNeckOut.x, vNeckOut.y, 0.7f, 255.f, 255.f, 255.f, 200.f);

	DrawLine(vLeftHandOut.x, vLeftHandOut.y, vUpperArmLeftOut.x, vUpperArmLeftOut.y, 0.7f, 255.f, 255.f, 255.f, 200.f);
	DrawLine(vRightHandOut.x, vRightHandOut.y, vUpperArmRightOut.x, vUpperArmRightOut.y, 0.7f, 255.f, 255.f, 255.f, 200.f);

	DrawLine(vLeftHandOut.x, vLeftHandOut.y, vLeftHandOut1.x, vLeftHandOut1.y, 0.7f, 255.f, 255.f, 255.f, 200.f);
	DrawLine(vRightHandOut.x, vRightHandOut.y, vRightHandOut1.x, vRightHandOut1.y, 0.7f, 255.f, 255.f, 255.f, 200.f);

	DrawLine(vLeftThighOut.x, vLeftThighOut.y, vHipOut.x, vHipOut.y, 0.7f, 255.f, 255.f, 255.f, 200.f);
	DrawLine(vRightThighOut.x, vRightThighOut.y, vHipOut.x, vHipOut.y, 0.7f, 255.f, 255.f, 255.f, 200.f);

	DrawLine(vLeftCalfOut.x, vLeftCalfOut.y, vLeftThighOut.x, vLeftThighOut.y, 0.7f, 255.f, 255.f, 255.f, 200.f);
	DrawLine(vRightCalfOut.x, vRightCalfOut.y, vRightThighOut.x, vRightThighOut.y, 0.7f, 255.f, 255.f, 255.f, 200.f);

	DrawLine(vLeftFootOut.x, vLeftFootOut.y, vLeftCalfOut.x, vLeftCalfOut.y, 0.7f, 255.f, 255.f, 255.f, 200.f);
	DrawLine(vRightFootOut.x, vRightFootOut.y, vRightCalfOut.x, vRightCalfOut.y, 0.7f, 255.f, 255.f, 255.f, 200.f);


}

void drawLoop(int width, int height) {
	menu();

	float radiusx = AimFOV * (width / 2 / 100);
	float radiusy = AimFOV * (height / 2 / 100);

	float calcradius = (radiusx + radiusy) / 2;

	DrawCircle(width / 2, height / 2, calcradius, 2.f, 255.f, 0.f, 0.f, 255.f, false);

	Uworld = read<DWORD_PTR>(DriverHandle, processID, base_address + OFFSET_UWORLD);
	//printf(_xor_("Uworld: %p.\n").c_str(), Uworld);

	DWORD_PTR Gameinstance = read<DWORD_PTR>(DriverHandle, processID, Uworld + 0x180); // changes sometimes 4u

	if (Gameinstance == (DWORD_PTR)nullptr)
		return;

	//printf(_xor_("Gameinstance: %p.\n").c_str(), Gameinstance);

	DWORD_PTR LocalPlayers = read<DWORD_PTR>(DriverHandle, processID, Gameinstance + 0x38);

	if (LocalPlayers == (DWORD_PTR)nullptr)
		return;

	//printf(_xor_("LocalPlayers: %p.\n").c_str(), LocalPlayers);

	Localplayer = read<DWORD_PTR>(DriverHandle, processID, LocalPlayers);

	if (Localplayer == (DWORD_PTR)nullptr)
		return;

	//printf(_xor_("LocalPlayer: %p.\n").c_str(), Localplayer);

	PlayerController = read<DWORD_PTR>(DriverHandle, processID, Localplayer + 0x30);

	if (PlayerController == (DWORD_PTR)nullptr)
		return;

	//printf(_xor_("playercontroller: %p.\n").c_str(), PlayerController);

	LocalPawn = read<uint64_t>(DriverHandle, processID, PlayerController + 0x2A0);  // changed often 4u sometimes called AcknowledgedPawn

	if (LocalPawn == (DWORD_PTR)nullptr)
		return;

	//printf(_xor_("Pawn: %p.\n").c_str(), LocalPawn);

	Rootcomp = read<uint64_t>(DriverHandle, processID, LocalPawn + 0x130);

	if (Rootcomp == (DWORD_PTR)nullptr)
		return;

	//printf(_xor_("Rootcomp: %p.\n").c_str(), Rootcomp);

	if (LocalPawn != 0) {
		localplayerID = read<int>(DriverHandle, processID, LocalPawn + 0x18);
	}

	Ulevel = read<DWORD_PTR>(DriverHandle, processID, Uworld + 0x30);
	//printf(_xor_("Ulevel: %p.\n").c_str(), Ulevel);

	if (Ulevel == (DWORD_PTR)nullptr)
		return;

	DWORD64 PlayerState = read<DWORD64>(DriverHandle, processID, LocalPawn + 0x240);  //changes often 4u

	if (PlayerState == (DWORD_PTR)nullptr)
		return;

	DWORD ActorCount = read<DWORD>(DriverHandle, processID, Ulevel + 0xA0);

	DWORD_PTR AActors = read<DWORD_PTR>(DriverHandle, processID, Ulevel + 0x98);
	//printf(_xor_("AActors: %p.\n").c_str(), AActors);

	if (AActors == (DWORD_PTR)nullptr)
		return;

	for (int i = 0; i < ActorCount; i++)
	{
		uint64_t CurrentActor = read<uint64_t>(DriverHandle, processID, AActors + i * 0x8);

		int curactorid = read<int>(DriverHandle, processID, CurrentActor + 0x18);

		if (curactorid == localplayerID || curactorid == 9882912 || curactorid == 1243298 || curactorid == 20926990 || curactorid == 21608177 || curactorid == 17397422 || curactorid == 9906460 || curactorid == 21643706 || curactorid == 20969079 || curactorid == 17420450) // this number changes for bot and NPC often, modified from original 4u		// you will need to print out the actorID on screen and find the new numbers, currently different numbers for bots, NPC(2), and bot in solo and 2 player games are different.
		//if (curactorid == localplayerID) //original changed 4u to target bots and NPC
		{
			if (CurrentActor == (uint64_t)nullptr || CurrentActor == -1 || CurrentActor == NULL)
				continue;

			uint64_t CurrentActorRootComponent = read<uint64_t>(DriverHandle, processID, CurrentActor + 0x130);

			if (CurrentActorRootComponent == (uint64_t)nullptr || CurrentActorRootComponent == -1 || CurrentActorRootComponent == NULL)
				continue;

			uint64_t currentactormesh = read<uint64_t>(DriverHandle, processID, CurrentActor + 0x280); // change as needed 4u

			if (currentactormesh == (uint64_t)nullptr || currentactormesh == -1 || currentactormesh == NULL)
				continue;

			int MyTeamId = read<int>(DriverHandle, processID, PlayerState + 0xED0);  //changes often 4u

			DWORD64 otherPlayerState = read<uint64_t>(DriverHandle, processID, CurrentActor + 0x240); //changes often 4u

			if (otherPlayerState == (uint64_t)nullptr || otherPlayerState == -1 || otherPlayerState == NULL)
				continue;

			int ActorTeamId = read<int>(DriverHandle, processID, otherPlayerState + 0xED0); //changes often 4u

			Vector3 Headpos = GetBoneWithRotation(currentactormesh, 66);
			Localcam = CameraEXT;
			localactorpos = read<Vector3>(DriverHandle, processID, Rootcomp + 0x11C);

			float distance = localactorpos.Distance(Headpos) / 100.f;

			if (distance < 1.5f)
				continue;

			//W2S
			Vector3 HeadposW2s = ProjectWorldToScreen(Headpos, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 bone0 = GetBoneWithRotation(currentactormesh, 0);
			Vector3 bottom = ProjectWorldToScreen(bone0, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 Headbox = ProjectWorldToScreen(Vector3(Headpos.x, Headpos.y, Headpos.z + 15), Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 Aimpos = ProjectWorldToScreen(Vector3(Headpos.x, Headpos.y, Headpos.z + 10), Vector3(Localcam.y, Localcam.x, Localcam.z));

			float Height1 = abs(Headbox.y - bottom.y);
			float Width1 = Height1 * 0.70;

			if (MyTeamId != ActorTeamId)
			{
				if (skeleton)
					DrawSkeleton(currentactormesh);

				if (BoxESP)
					DrawBox(Headbox.x - (Width1 / 2), Headbox.y, Width1, Height1, 0.7f, 255.f, 255.f, 255.f, 200.f, false);

				if (EnemyESP)
					DrawString(_xor_("Enemy").c_str(), 13, HeadposW2s.x, HeadposW2s.y - 25, 0, 1, 1);

				if (DistanceESP)
				{
					CHAR dist[50];
					sprintf_s(dist, _xor_("[%.f]").c_str(), distance);

					DrawString(dist, 13, HeadposW2s.x + 40, HeadposW2s.y - 25, 0, 1, 1);
				}

				if (LineESP)
					DrawLine(width / 2, height, bottom.x, bottom.y, 0.7f, 255.f, 0.f, 0.f, 200.f);

				if (Aimbot)
				{
					AIms(CurrentActor, Localcam);
				}
			}
		}
	}

	if (Aimbot)
	{
		aimbot(Localcam);
	}
}


void main()
{
	DriverHandle = CreateFileW(_xor_(L"\\\\.\\matchdriver123").c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);  // this needs to match your driver

	if (DriverHandle == INVALID_HANDLE_VALUE)
	{
		HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleTextAttribute(h, 4);
		printf("  ----------\n");
		SetConsoleTextAttribute(h, 3);
		printf("  VoidlessFN\n");
		SetConsoleTextAttribute(h, 4);
		printf("  ----------\n\n");
		SetConsoleTextAttribute(h, 7);
		printf("  INFO:\n  ");
		printf("This a free open source Fortnite Cheat.\n  (Download here: github.com/temal32/VoidlessFN)");
		SetConsoleTextAttribute(h, 1);
		printf("  \n\n  If you have any problems, please report to our discord server:\n  dsc.gg/Voidless");
		SetConsoleTextAttribute(h, 4);
		printf(_xor_("\n\n  Error: \n  Driver not loaded, renamed or loaded in wrong path.\n  Please reboot your pc, load the driver in the same location as the cheat,\n  make sure there are no errors if you run the driver, and dont rename any file.\n  If you still have the problem turn on Secure Boot and turn it back off").c_str());
		getchar();
		exit(0);
	}

	info_t Input_Output_Data1;
	unsigned long int Readed_Bytes_Amount1;
	DeviceIoControl(DriverHandle, ctl_clear, &Input_Output_Data1, sizeof Input_Output_Data1, &Input_Output_Data1, sizeof Input_Output_Data1, &Readed_Bytes_Amount1, nullptr);

	while (hwnd == NULL)
	{
		hwnd = FindWindowA(0, _xor_("Fortnite  ").c_str());
		HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleTextAttribute(h, 3);
		system("cls");
		SetConsoleTextAttribute(h, 4);
		printf("  ----------\n");
		SetConsoleTextAttribute(h, 3);
		printf("  VoidlessFN\n");
		SetConsoleTextAttribute(h, 4);
		printf("  ----------\n\n");
		SetConsoleTextAttribute(h, 7);
	    printf("  INFO:\n  ");
		printf("This a free open source Fortnite Cheat.\n  (Download here: github.com/temal32/VoidlessFN)");
		SetConsoleTextAttribute(h, 1);
		printf("  \n\n  If you have any problems, please report to our discord server:\n  dsc.gg/Voidless");
		SetConsoleTextAttribute(h, 6);
		printf("\n\n\n  PLEASE START FORNITE!");

		Sleep(300);  //slowed it down a bit 4u
	}
	GetWindowThreadProcessId(hwnd, &processID);

	RECT rect;
	if (GetWindowRect(hwnd, &rect))
	{
		width = rect.right - rect.left;
		height = rect.bottom - rect.top;
	}

	info_t Input_Output_Data;
	Input_Output_Data.pid = processID;
	unsigned long int Readed_Bytes_Amount;

	DeviceIoControl(DriverHandle, ctl_base, &Input_Output_Data, sizeof Input_Output_Data, &Input_Output_Data, sizeof Input_Output_Data, &Readed_Bytes_Amount, nullptr);
	base_address = (unsigned long long int)Input_Output_Data.data;
	//std::printf(_xor_("Process base address: %p.\n").c_str(), (void*)base_address);
	CreateThread(NULL, NULL, Menuthread, NULL, NULL, NULL);

	DirectOverlaySetOption(D2DOV_DRAW_FPS | D2DOV_FONT_COURIER);
	DirectOverlaySetup(drawLoop, hwnd);
	getchar();
}
