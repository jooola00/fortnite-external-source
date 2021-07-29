#include <iostream>
#include <Windows.h>
#include "Utils.h"
#include "d3d.h"
#include <dwmapi.h>
#include <vector>
#include <sstream>
#include <string>
#include <algorithm>
#include <list>
#include <filesystem>
#include "offsets.h"
#include "settings.h"


// source code of adzara public external ( used old pasto source and updated and changed a lot)


static HWND Window = NULL;
IDirect3D9Ex* pObject = NULL;
static LPDIRECT3DDEVICE9 D3DDevice = NULL;
static LPDIRECT3DVERTEXBUFFER9 VertBuff = NULL;
int is3D;

#define OFFSET_UWORLD 0x9865730

int localplayerID;

float Aim_Speed = 3.0f;
const int Max_Aim_Speed = 100;
const int Min_Aim_Speed = 35;


ImFont* m_pFont;

DWORD_PTR Uworld;
DWORD_PTR LocalPawn;
DWORD_PTR Localplayer;
DWORD_PTR Rootcomp;
DWORD_PTR PlayerController;
DWORD_PTR Ulevel;
DWORD_PTR entityx;
bool isaimbotting;

Vector3 localactorpos;
Vector3 Localcam;

static void WindowMain();
static void InitializeD3D();
static void Loop();
static void ShutDown();

FTransform GetBoneIndex(DWORD_PTR mesh, int index)
{
	DWORD_PTR bonearray = read<DWORD_PTR>(DrverInit, FNProcID, mesh + 0x4A8);  

	if (bonearray == NULL) 
	{
		bonearray = read<DWORD_PTR>(DrverInit, FNProcID, mesh + 0x4A8 + 0x10); 
	}

	return read<FTransform>(DrverInit, FNProcID, bonearray + (index * 0x30));  
}

Vector3 GetBoneWithRotation(DWORD_PTR mesh, int id)
{
	FTransform bone = GetBoneIndex(mesh, id);
	FTransform ComponentToWorld = read<FTransform>(DrverInit, FNProcID, mesh + 0x1C0); 

	D3DMATRIX Matrix;
	Matrix = MatrixMultiplication(bone.ToMatrixWithScale(), ComponentToWorld.ToMatrixWithScale());

	return Vector3(Matrix._41, Matrix._42, Matrix._43);
}

D3DMATRIX Matrix(Vector3 rot, Vector3 origin = Vector3(0, 0, 0))
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


extern Vector3 CameraEXT(0, 0, 0);
float FovAngle;
Vector3 ProjectWorldToScreen(Vector3 WorldLocation, Vector3 camrot)
{
	Vector3 Screenlocation = Vector3(0, 0, 0);
	Vector3 Camera;

	auto chain69 = read<uintptr_t>(DrverInit, FNProcID, Localplayer + 0xa8);
	uint64_t chain699 = read<uintptr_t>(DrverInit, FNProcID, chain69 + 8);

	Camera.x = read<float>(DrverInit, FNProcID, chain699 + 0x7F8);  
	Camera.y = read<float>(DrverInit, FNProcID, Rootcomp + 0x12C);  

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

	uint64_t chain = read<uint64_t>(DrverInit, FNProcID, Localplayer + 0x70);
	uint64_t chain1 = read<uint64_t>(DrverInit, FNProcID, chain + 0x98);
	uint64_t chain2 = read<uint64_t>(DrverInit, FNProcID, chain1 + 0x130);

	Vector3 vDelta = WorldLocation - read<Vector3>(DrverInit, FNProcID, chain2 + 0x10); 
	Vector3 vTransformed = Vector3(vDelta.Dot(vAxisY), vDelta.Dot(vAxisZ), vDelta.Dot(vAxisX));

	if (vTransformed.z < 1.f)
		vTransformed.z = 1.f;

	float zoom = read<float>(DrverInit, FNProcID, chain699 + 0x590);

	FovAngle = 80.0f / (zoom / 1.19f);
	float ScreenCenterX = Width / 2.0f;
	float ScreenCenterY = Height / 2.0f;

	Screenlocation.x = ScreenCenterX + vTransformed.x * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.z;
	Screenlocation.y = ScreenCenterY - vTransformed.y * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.z;
	CameraEXT = Camera;

	return Screenlocation;
}


DWORD GUI(LPVOID in)
{
	while (1)
	{

		static bool pressed = false;

		if (GetKeyState(VK_INSERT) & 0x8000)
			pressed = true;

		else if (!(GetKeyState(VK_INSERT) & 0x8000) && pressed) {
			Settings::ShowMenu = !Settings::ShowMenu;
			pressed = false;
		}
	}
}


typedef struct
{
	DWORD R;
	DWORD G;
	DWORD B;
	DWORD A;
}RGBA;

class Color
{
public:
	RGBA red = { 255,0,0,255 };
	RGBA Magenta = { 255,0,255,255 };
	RGBA yellow = { 255,255,0,255 };
	RGBA grayblue = { 128,128,255,255 };
	RGBA green = { 128,224,0,255 };
	RGBA darkgreen = { 0,224,128,255 };
	RGBA brown = { 192,96,0,255 };
	RGBA pink = { 255,168,255,255 };
	RGBA DarkYellow = { 216,216,0,255 };
	RGBA SilverWhite = { 236,236,236,255 };
	RGBA purple = { 144,0,255,255 };
	RGBA Navy = { 88,48,224,255 };
	RGBA skyblue = { 0,136,255,255 };
	RGBA graygreen = { 128,160,128,255 };
	RGBA blue = { 0,96,192,255 };
	RGBA orange = { 255,128,0,255 };
	RGBA peachred = { 255,80,128,255 };
	RGBA reds = { 255,128,192,255 };
	RGBA darkgray = { 96,96,96,255 };
	RGBA Navys = { 0,0,128,255 };
	RGBA darkgreens = { 0,128,0,255 };
	RGBA darkblue = { 0,128,128,255 };
	RGBA redbrown = { 128,0,0,255 };
	RGBA purplered = { 128,0,128,255 };
	RGBA greens = { 0,255,0,255 };
	RGBA envy = { 0,255,255,255 };
	RGBA black = { 0,0,0,255 };
	RGBA gray = { 128,128,128,255 };
	RGBA white = { 255,255,255,255 };
	RGBA blues = { 30,144,255,255 };
	RGBA lightblue = { 135,206,250,160 };
	RGBA Scarlet = { 220, 20, 60, 160 };
	RGBA white_ = { 255,255,255,200 };
	RGBA gray_ = { 128,128,128,200 };
	RGBA black_ = { 0,0,0,200 };
	RGBA red_ = { 255,0,0,200 };
	RGBA Magenta_ = { 255,0,255,200 };
	RGBA yellow_ = { 255,255,0,200 };
	RGBA grayblue_ = { 128,128,255,200 };
	RGBA green_ = { 128,224,0,200 };
	RGBA darkgreen_ = { 0,224,128,200 };
	RGBA brown_ = { 192,96,0,200 };
	RGBA pink_ = { 255,168,255,200 };
	RGBA darkyellow_ = { 216,216,0,200 };
	RGBA silverwhite_ = { 236,236,236,200 };
	RGBA purple_ = { 144,0,255,200 };
	RGBA Blue_ = { 88,48,224,200 };
	RGBA skyblue_ = { 0,136,255,200 };
	RGBA graygreen_ = { 128,160,128,200 };
	RGBA blue_ = { 0,96,192,200 };
	RGBA orange_ = { 255,128,0,200 };
	RGBA pinks_ = { 255,80,128,200 };
	RGBA Fuhong_ = { 255,128,192,200 };
	RGBA darkgray_ = { 96,96,96,200 };
	RGBA Navy_ = { 0,0,128,200 };
	RGBA darkgreens_ = { 0,128,0,200 };
	RGBA darkblue_ = { 0,128,128,200 };
	RGBA redbrown_ = { 128,0,0,200 };
	RGBA purplered_ = { 128,0,128,200 };
	RGBA greens_ = { 0,255,0,200 };
	RGBA envy_ = { 0,255,255,200 };

	RGBA glassblack = { 0, 0, 0, 160 };
	RGBA GlassBlue = { 65,105,225,80 };
	RGBA glassyellow = { 255,255,0,160 };
	RGBA glass = { 200,200,200,60 };


	RGBA Plum = { 221,160,221,160 };

};
Color Col;


std::string GetGNamesByObjID(int32_t ObjectID)
{
	return 0;
}


typedef struct _FNlEntity
{
	uint64_t Actor;
	int ID;
	uint64_t mesh;
}FNlEntity;

std::vector<FNlEntity> entityList;

#define DEBUG



HWND GameWnd = NULL;
void Window2Target()
{
	while (true)
	{
		if (hWnd)
		{
			ZeroMemory(&ProcessWH, sizeof(ProcessWH));
			GetWindowRect(hWnd, &ProcessWH);
			Width = ProcessWH.right - ProcessWH.left;
			Height = ProcessWH.bottom - ProcessWH.top;
			DWORD dwStyle = GetWindowLong(hWnd, GWL_STYLE);

			if (dwStyle & WS_BORDER)
			{
				ProcessWH.top += 32;
				Height -= 39;
			}
			ScreenCenterX = Width / 2;
			ScreenCenterY = Height / 2;
			MoveWindow(Window, ProcessWH.left, ProcessWH.top, Width, Height, true);
		}
		else
		{
			exit(0);
		}
	}
}


const MARGINS Margin = { -1 };


void WindowMain()
{

	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Window2Target, 0, 0, 0);
	WNDCLASSEX wClass =
	{
		sizeof(WNDCLASSEX),
		0,
		WindowProc,
		0,
		0,
		nullptr,
		LoadIcon(nullptr, IDI_APPLICATION),
		LoadCursor(nullptr, IDC_ARROW),
		nullptr,
		nullptr,
		TEXT("Test1"),
		LoadIcon(nullptr, IDI_APPLICATION)
	};

	if (!RegisterClassEx(&wClass))
		exit(1);

	hWnd = FindWindowW(NULL, TEXT("Fortnite  "));

	

	if (hWnd)
	{
		GetClientRect(hWnd, &ProcessWH);
		POINT xy;
		ClientToScreen(hWnd, &xy);
		ProcessWH.left = xy.x;
		ProcessWH.top = xy.y;


		Width = ProcessWH.right;
		Height = ProcessWH.bottom;
	}
	

	Window = CreateWindowExA(NULL, "Test1", "Test1", WS_POPUP | WS_VISIBLE, ProcessWH.left, ProcessWH.top, Width, Height, NULL, NULL, 0, NULL);
	DwmExtendFrameIntoClientArea(Window, &Margin);
	SetWindowLong(Window, GWL_EXSTYLE, WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW);
	ShowWindow(Window, SW_SHOW);
	UpdateWindow(Window);

}

void InitializeD3D()
{
	if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &pObject)))
		exit(3);

	ZeroMemory(&d3d, sizeof(d3d));
	d3d.BackBufferWidth = Width;
	d3d.BackBufferHeight = Height;
	d3d.BackBufferFormat = D3DFMT_A8R8G8B8;
	d3d.MultiSampleQuality = D3DMULTISAMPLE_NONE;
	d3d.AutoDepthStencilFormat = D3DFMT_D16;
	d3d.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3d.EnableAutoDepthStencil = TRUE;
	d3d.hDeviceWindow = Window;
	d3d.Windowed = TRUE;

	pObject->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, Window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3d, &D3DDevice);

	IMGUI_CHECKVERSION();

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;

	ImGui_ImplWin32_Init(Window);
	ImGui_ImplDX9_Init(D3DDevice);

	ImGui::StyleColorsClassic();
	ImGuiStyle* style = &ImGui::GetStyle();

	ImVec4* colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.83f);
	colors[ImGuiCol_ChildBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
	colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.29f, 0.48f, 0.54f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.29f, 0.48f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.61f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.16f, 0.29f, 0.48f, 0.54f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.46f, 0.4f, 0.98f, 0.40f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);

	pObject->Release();
}

void RadarRange(float* x, float* y, float range)
{
	if (fabs((*x)) > range || fabs((*y)) > range)
	{
		if ((*y) > (*x))
		{
			if ((*y) > -(*x))
			{
				(*x) = range * (*x) / (*y);
				(*y) = range;
			}
			else
			{
				(*y) = -range * (*y) / (*x);
				(*x) = -range;
			}
		}
		else
		{
			if ((*y) > -(*x))
			{
				(*y) = range * (*y) / (*x);
				(*x) = range;
			}
			else
			{
				(*x) = -range * (*x) / (*y);
				(*y) = -range;
			}
		}
	}
}




void renderRadar() {
	

	int screenx = 0;
	int screeny = 0;

	float Color_R = 255 / 255.f;
	float Color_G = 128 / 255.f;
	float Color_B = 0 / 255.f;

	

	ImDrawList* Draw = ImGui::GetOverlayDrawList();

	

	Draw->AddRectFilled(ImVec2((float)screenx, (float)screeny),
		ImVec2((float)screenx + 5, (float)screeny + 5),
		ImColor(Color_R, Color_G, Color_B));
}

bool firstS = false;
void RadarLoop()
{

}

Vector3 Camera(unsigned __int64 RootComponent)
{
	unsigned __int64 PtrPitch;
	Vector3 Camera;

	auto pitch = read<uintptr_t>(DrverInit, FNProcID, Offsets::LocalPlayer + 0xb0);
	Camera.x = read<float>(DrverInit, FNProcID, RootComponent + 0x12C);
	Camera.y = read<float>(DrverInit, FNProcID, pitch + 0x678);

	float test = asin(Camera.y);
	float degrees = test * (180.0 / M_PI);

	Camera.y = degrees;

	if (Camera.x < 0)
		Camera.x = 360 + Camera.x;

	return Camera;
}

bool GetAimKey()
{
	return (GetAsyncKeyState(VK_RBUTTON));
}

void aimbot(float x, float y)
{
	float ScreenCenterX = (Width / 2);
	float ScreenCenterY = (Height / 2);
	int AimSpeed = Aim_Speed;
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
			float Dist = GetCrossDistance(Pos.x, Pos.y, Width / 2, Height / 2);

			if (Dist < max)
			{
				max = Dist;
				entityx = entity;
				Settings::AimbotFOV = aimfov;
			}
		}
	}
	return false;
}



void AimAt(DWORD_PTR entity, Vector3 Localcam)
{

	
	{
		uint64_t currentactormesh = read<uint64_t>(DrverInit, FNProcID, entity + 0x280);
		auto rootHead = GetBoneWithRotation(currentactormesh, 66);
		Vector3 rootHeadOut = ProjectWorldToScreen(rootHead, Vector3(Localcam.y, Localcam.x, Localcam.z));
		Vector3 Headpos = GetBoneWithRotation(currentactormesh, 66);
		Vector3 HeadposW2s = ProjectWorldToScreen(Headpos, Vector3(Localcam.y, Localcam.x, Localcam.z));

		if (rootHeadOut.x != 0 || rootHeadOut.y != 0)
		{
			if ((GetCrossDistance(rootHeadOut.x, rootHeadOut.y, Width / 2, Height / 2) <= Settings::AimbotFOV * 8) || isaimbotting)
			{
				aimbot(rootHeadOut.x, rootHeadOut.y);
				
				

			}
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

	uint64_t currentactormesh = read<uint64_t>(DrverInit, FNProcID, entity + 0x280); 

	Vector3 rootHead = GetBoneWithRotation(currentactormesh, 66);
	Vector3 rootHeadOut = ProjectWorldToScreen(rootHead, Vector3(Localcam.y, Localcam.x, Localcam.z));

	if (GetClosestPlayerToCrossHair(rootHeadOut, max, Settings::AimbotFOV, entity))
		entityx = entity;
}


float TextShadowNigg(ImFont* pFont, const std::string& text, const ImVec2& pos, float size, ImU32 color, bool center)
{
	

	std::stringstream stream(text);
	std::string line;

	float y = 0.0f;
	int i = 0;

	while (std::getline(stream, line))
	{
		ImVec2 textSize = pFont->CalcTextSizeA(size, FLT_MAX, 0.0f, line.c_str());

		if (center)
		{
			ImGui::GetOverlayDrawList()->AddText(pFont, size, ImVec2((pos.x - textSize.x / 2.0f) + 1, (pos.y + textSize.y * i) + 1), ImGui::GetColorU32(ImVec4(0, 0, 0, 255)), line.c_str());
			ImGui::GetOverlayDrawList()->AddText(pFont, size, ImVec2((pos.x - textSize.x / 2.0f) - 1, (pos.y + textSize.y * i) - 1), ImGui::GetColorU32(ImVec4(0, 0, 0, 255)), line.c_str());
			

			ImGui::GetOverlayDrawList()->AddText(pFont, size, ImVec2(pos.x - textSize.x / 2.0f, pos.y + textSize.y * i), ImGui::GetColorU32(color), line.c_str());
		}
		else
		{
			ImGui::GetOverlayDrawList()->AddText(pFont, size, ImVec2((pos.x) + 1, (pos.y + textSize.y * i) + 1), ImGui::GetColorU32(ImVec4(0, 0, 0, 255)), line.c_str());
			ImGui::GetOverlayDrawList()->AddText(pFont, size, ImVec2((pos.x) - 1, (pos.y + textSize.y * i) - 1), ImGui::GetColorU32(ImVec4(0, 0, 0, 255)), line.c_str());
			
			

			ImGui::GetOverlayDrawList()->AddText(pFont, size, ImVec2(pos.x, pos.y + textSize.y * i), ImGui::GetColorU32(color), line.c_str());
		}

		y = pos.y + textSize.y * (i + 1);
		i++;
	}
	return y;
}

void DrawLine(int x1, int y1, int x2, int y2, RGBA* color, int thickness)
{
	ImGui::GetOverlayDrawList()->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), ImGui::ColorConvertFloat4ToU32(ImVec4(color->R / 255.0, color->G / 255.0, color->B / 255.0, color->A / 255.0)), thickness);
}

void DrawFilledRect(int x, int y, int w, int h, RGBA* color)
{
	ImGui::GetOverlayDrawList()->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), ImGui::ColorConvertFloat4ToU32(ImVec4(color->R / 255.0, color->G / 255.0, color->B / 255.0, color->A / 255.0)), 0, 0);
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

	//DrawCircle(vHeadBone.x , vHeadBone.y , vHeadBoneOut.x , vHeadBoneOut.y, 1.f , 255.f, 0.f, 0.f, 200.f),


	DrawLine(vHipOut.x, vHipOut.y, vNeckOut.x, vNeckOut.y, &Col.red, 1.f);

	DrawLine(vUpperArmLeftOut.x, vUpperArmLeftOut.y, vNeckOut.x, vNeckOut.y, &Col.red, 1.f);
	DrawLine(vUpperArmRightOut.x, vUpperArmRightOut.y, vNeckOut.x, vNeckOut.y, &Col.red, 1.f);

	DrawLine(vLeftHandOut.x, vLeftHandOut.y, vUpperArmLeftOut.x, vUpperArmLeftOut.y, &Col.red, 1.f);
	DrawLine(vRightHandOut.x, vRightHandOut.y, vUpperArmRightOut.x, vUpperArmRightOut.y, &Col.red, 1.f);

	DrawLine(vLeftHandOut.x, vLeftHandOut.y, vLeftHandOut1.x, vLeftHandOut1.y, &Col.red, 1.f);
	DrawLine(vRightHandOut.x, vRightHandOut.y, vRightHandOut1.x, vRightHandOut1.y, &Col.red, 1.f);

	DrawLine(vLeftThighOut.x, vLeftThighOut.y, vHipOut.x, vHipOut.y, &Col.red, 1.f);
	DrawLine(vRightThighOut.x, vRightThighOut.y, vHipOut.x, vHipOut.y, &Col.red, 1.f);

	DrawLine(vLeftCalfOut.x, vLeftCalfOut.y, vLeftThighOut.x, vLeftThighOut.y, &Col.red, 1.f);
	DrawLine(vRightCalfOut.x, vRightCalfOut.y, vRightThighOut.x, vRightThighOut.y, &Col.red, 1.f);

	DrawLine(vLeftFootOut.x, vLeftFootOut.y, vLeftCalfOut.x, vLeftCalfOut.y, &Col.red, 1.f);
	DrawLine(vRightFootOut.x, vRightFootOut.y, vRightCalfOut.x, vRightCalfOut.y, &Col.red, 1.f);
}
std::string GetObjectNames(int32_t ObjectID) /* maybe working.. */
{
	uint64_t gname = read<uint64_t>(DrverInit, FNProcID, base_address + 0x9643080);

	int64_t fNamePtr = read<uint64_t>(DrverInit, FNProcID, gname + int(ObjectID / 0x4000) * 0x8);
	int64_t fName = read<uint64_t>(DrverInit, FNProcID, fNamePtr + int(ObjectID % 0x4000) * 0x8);

	char pBuffer[64] = { NULL };

	info_t blyat;
	blyat.pid = FNProcID;
	blyat.address = fName + 0x10;
	blyat.value = &pBuffer;
	blyat.size = sizeof(pBuffer);

	unsigned long int asd;
	DeviceIoControl(DrverInit, ctl_read, &blyat, sizeof blyat, &blyat, sizeof blyat, &asd, nullptr);

	return std::string(pBuffer);
}
void DrawCornerBox(int x, int y, int w, int h, int borderPx, RGBA* color)
{
	DrawFilledRect(x + borderPx, y, w / 3, borderPx, color); 
	DrawFilledRect(x + w - w / 3 + borderPx, y, w / 3, borderPx, color); 
	DrawFilledRect(x, y, borderPx, h / 3, color); 
	DrawFilledRect(x, y + h - h / 3 + borderPx * 2, borderPx, h / 3, color); 
	DrawFilledRect(x + borderPx, y + h + borderPx, w / 3, borderPx, color); 
	DrawFilledRect(x + w - w / 3 + borderPx, y + h + borderPx, w / 3, borderPx, color); 
	DrawFilledRect(x + w + borderPx, y, borderPx, h / 3, color); 
	DrawFilledRect(x + w + borderPx, y + h - h / 3 + borderPx * 2, borderPx, h / 3, color);
}

void drawLoop() {


	Uworld = read<DWORD_PTR>(DrverInit, FNProcID, base_address + OFFSET_UWORLD);
	

	DWORD_PTR Gameinstance = read<DWORD_PTR>(DrverInit, FNProcID, Uworld + 0x180); 

	if (Gameinstance == (DWORD_PTR)nullptr)
		return;

	

	DWORD_PTR LocalPlayers = read<DWORD_PTR>(DrverInit, FNProcID, Gameinstance + 0x38);

	if (LocalPlayers == (DWORD_PTR)nullptr)
		return;

	

	Localplayer = read<DWORD_PTR>(DrverInit, FNProcID, LocalPlayers);

	if (Localplayer == (DWORD_PTR)nullptr)
		return;

	

	PlayerController = read<DWORD_PTR>(DrverInit, FNProcID, Localplayer + 0x30);

	if (PlayerController == (DWORD_PTR)nullptr)
		return;

	

	LocalPawn = read<uint64_t>(DrverInit, FNProcID, PlayerController + 0x2A0);  

	if (LocalPawn == (DWORD_PTR)nullptr)
		return;

	

	Rootcomp = read<uint64_t>(DrverInit, FNProcID, LocalPawn + 0x130);

	if (Rootcomp == (DWORD_PTR)nullptr)
		return;



	if (LocalPawn != 0) {
		localplayerID = read<int>(DrverInit, FNProcID, LocalPawn + 0x18);
	}

	

	if (Ulevel == (DWORD_PTR)nullptr)
		return;

	DWORD64 PlayerState = read<DWORD64>(DrverInit, FNProcID, LocalPawn + 0x240);  

	if (PlayerState == (DWORD_PTR)nullptr)
		return;

	DWORD ActorCount = read<DWORD>(DrverInit, FNProcID, Ulevel + 0xA0);

	DWORD_PTR AActors = read<DWORD_PTR>(DrverInit, FNProcID, Ulevel + 0x98);
	

	if (AActors == (DWORD_PTR)nullptr)
		return;

	for (int i = 0; i < ActorCount; i++)
	{
		uint64_t CurrentActor = read<uint64_t>(DrverInit, FNProcID, AActors + i * 0x8);

		auto name = GetObjectNames(CurrentActor);

		int curactorid = read<int>(DrverInit, FNProcID, CurrentActor + 0x18);

		if (curactorid == localplayerID || curactorid == 9907819 || curactorid == 9875145 || curactorid == 9873134 || curactorid == 9876800 || curactorid == 9874439) 
		{
			if (CurrentActor == (uint64_t)nullptr || CurrentActor == -1 || CurrentActor == NULL)
				continue;

			uint64_t CurrentActorRootComponent = read<uint64_t>(DrverInit, FNProcID, CurrentActor + 0x130);

			if (CurrentActorRootComponent == (uint64_t)nullptr || CurrentActorRootComponent == -1 || CurrentActorRootComponent == NULL)
				continue;

			uint64_t currentactormesh = read<uint64_t>(DrverInit, FNProcID, CurrentActor + 0x280);

			if (currentactormesh == (uint64_t)nullptr || currentactormesh == -1 || currentactormesh == NULL)
				continue;

			int MyTeamId = read<int>(DrverInit, FNProcID, PlayerState + 0xED0);  

			DWORD64 otherPlayerState = read<uint64_t>(DrverInit, FNProcID, CurrentActor + 0x240); 

			if (otherPlayerState == (uint64_t)nullptr || otherPlayerState == -1 || otherPlayerState == NULL)
				continue;

			int ActorTeamId = read<int>(DrverInit, FNProcID, otherPlayerState + 0xED0); 

			Vector3 Headpos = GetBoneWithRotation(currentactormesh, 66);
			Localcam = CameraEXT;
			localactorpos = read<Vector3>(DrverInit, FNProcID, Rootcomp + 0x11C);

			float distance = localactorpos.Distance(Headpos) / 100.f;

			if (distance < 0.5)
				continue;
			Vector3 CirclePOS = GetBoneWithRotation(currentactormesh, 2);
			

			Vector3 rootOut = GetBoneWithRotation(currentactormesh, 0);

			Vector3 Out = ProjectWorldToScreen(rootOut, Vector3(Localcam.y, Localcam.x, Localcam.z));

			Vector3 HeadposW2s = ProjectWorldToScreen(Headpos, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 bone0 = GetBoneWithRotation(currentactormesh, 0);
			Vector3 bottom = ProjectWorldToScreen(bone0, Vector3(Localcam.y, Localcam.x, Localcam.z));
			Vector3 Headbox = ProjectWorldToScreen(Vector3(Headpos.x, Headpos.y, Headpos.z + 15), Vector3(Localcam.y, Localcam.x, Localcam.z));	

			float boxsize = (float)(Out.y - HeadposW2s.y);
			float boxwidth = boxsize / 3.0f;
			float Height1 = abs(Headbox.y - bottom.y);
			float Width1 = Height1 * 0.65;
			float dwpleftx = (float)Out.x - boxwidth / 2.0f;
			float dwplefty = (float)Out.y;
			
			m_pFont = ImGui::GetIO().Fonts->ImFontAtlas::AddFontDefault();

			

			if (Settings::PlayerESP)
			{
				ImGui::GetOverlayDrawList()->AddRectFilled(ImVec2(dwpleftx, dwplefty), ImVec2(HeadposW2s.x + boxwidth, HeadposW2s.y + 5.0f), IM_COL32(0, 0, 0, 70));
				ImGui::GetOverlayDrawList()->AddRect(ImVec2(dwpleftx, dwplefty), ImVec2(HeadposW2s.x + boxwidth, HeadposW2s.y + 5.0f), IM_COL32(128, 0, 255, 255));
			}
			if (Settings::Skeleton)
			{
				DrawSkeleton(currentactormesh);
			}
			if (Settings::CornerESP)
			{
				DrawLine(Headbox.x - (Width1 / 2), Headbox.y, Headbox.x - (Width1 / 2) + (Width1 / 4), Headbox.y, &Col.red, 1.5f);
				DrawLine(Headbox.x - (Width1 / 2), Headbox.y, Headbox.x - (Width1 / 2), Headbox.y + (Width1 / 4), &Col.red, 1.5f);
				DrawLine(Headbox.x - (Width1 / 2) + Width1, Headbox.y, Headbox.x + (Width1 / 2) - (Width1 / 4), Headbox.y, &Col.red, 1.5f);
				DrawLine(Headbox.x - (Width1 / 2) + Width1, Headbox.y, Headbox.x - (Width1 / 2) + Width1, Headbox.y + (Width1 / 4), &Col.red, 1.5f);
				DrawLine(Headbox.x - (Width1 / 2), Headbox.y + Height1, Headbox.x - (Width1 / 2) + (Width1 / 4), Headbox.y + Height1, &Col.red, 1.5f);
				DrawLine(Headbox.x - (Width1 / 2), Headbox.y - (Width1 / 4) + Height1, Headbox.x - (Width1 / 2), Headbox.y + Height1, &Col.red, 1.5f);
				DrawLine(Headbox.x - (Width1 / 2) + Width1, Headbox.y + Height1, Headbox.x + (Width1 / 2) - (Width1 / 4), Headbox.y + Height1, &Col.red, 1.5f);
				DrawLine(Headbox.x - (Width1 / 2) + Width1, Headbox.y - (Width1 / 4) + Height1, Headbox.x - (Width1 / 2) + Width1, Headbox.y + Height1, &Col.red, 1.5f);
			}
			if (Settings::lineesp)
			{
				DrawLine(Width / 2, Height, bottom.x, bottom.y, &Col.purple, 1.5f);
			}


				
			if (Settings::Distance)
			{
				CHAR dist[50];
				sprintf_s(dist, ("%.fm"), distance);

				
				TextShadowNigg(m_pFont, dist, ImVec2(HeadposW2s.x, HeadposW2s.y - 35), 16.0f, IM_COL32(255, 0, 0, 255), true);
			}


			if (Settings::MouseAimbot)
			{
				AIms(CurrentActor, Localcam);
			}
		}
	}
	if (Settings::MouseAimbot)
	{
		aimbot(Localcam);
	}
}
int r, g, b;
int r1, g2, b2;

float color_red = 1.;
float color_green = 0;
float color_blue = 0;
float color_random = 0.0;
float color_speed = -10.0;
bool rainbowmode = false;

void ColorChange()
{
	if (rainbowmode)
	{
		static float Color[3];
		static DWORD Tickcount = 0;
		static DWORD Tickcheck = 0;
		ImGui::ColorConvertRGBtoHSV(color_red, color_green, color_blue, Color[0], Color[1], Color[2]);
		if (GetTickCount() - Tickcount >= 1)
		{
			if (Tickcheck != Tickcount)
			{
				Color[0] += 0.001f * color_speed;
				Tickcheck = Tickcount;
			}
			Tickcount = GetTickCount();
		}
		if (Color[0] < 0.0f) Color[0] += 1.0f;
		ImGui::ColorConvertHSVtoRGB(Color[0], Color[1], Color[2], color_red, color_green, color_blue);
	}
}

void Render() {

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	int X;
	int Y;
	float size1 = 3.0f;
	float size2 = 2.0f;

	if (Settings::AimbotCircle)
	{
		
		ImGui::GetOverlayDrawList()->AddCircle(ImVec2(ScreenCenterX, ScreenCenterY), Settings::AimbotFOV, ImColor(0, 0, 0, 230), Settings::Roughness);
		
	}
	

	
	if (Settings::Crosshair)
	{
		ImGui::GetOverlayDrawList()->AddLine(ImVec2(ScreenCenterX - 12, ScreenCenterY), ImVec2((ScreenCenterX - 12) + (12 * 2), ScreenCenterY), ImGui::GetColorU32({ 227, 252, 3, 0.65f }), 1.9);
		ImGui::GetOverlayDrawList()->AddLine(ImVec2(ScreenCenterX, ScreenCenterY - 12), ImVec2(ScreenCenterX, (ScreenCenterY - 12) + (12 * 2)), ImGui::GetColorU32({ 227, 252, 3, 0.65f }), 1.9);
	}
	if (Settings::Radar)
	{
		RadarLoop();
	}

	ColorChange();

	bool circleedit = false;

	ImGuiStyle* style = &ImGui::GetStyle();
	ImVec4* colors = style->Colors;


	
	style->PopupRounding = 3;

	style->WindowPadding = ImVec2(4, 4);
	style->FramePadding = ImVec2(6, 4);
	style->ItemSpacing = ImVec2(6, 2);

	style->ScrollbarSize = 18;

	style->WindowBorderSize = 1;
	style->ChildBorderSize = 1;
	style->PopupBorderSize = 1;
	style->FrameBorderSize = is3D;

	style->WindowRounding = 3;
	style->ChildRounding = 3;
	style->FrameRounding = 3;
	style->ScrollbarRounding = 2;
	style->GrabRounding = 3;

	style->WindowTitleAlign = ImVec2(1.0f, 0.5f);
	

	style->DisplaySafeAreaPadding = ImVec2(4, 4);

	style->DisplaySafeAreaPadding = ImVec2(4, 4);
	ImGui::SetNextWindowSize({ 500, 320 });
	if (Settings::ShowMenu)
	{


		ImGui::Begin("Adzara Private | External", 0, ImGuiWindowFlags_::ImGuiWindowFlags_NoResize);

		static int fortnitetab;

		
		
			
			ImVec4* colors = style->Colors;

			style->WindowRounding = 2.0f;             
			style->ScrollbarRounding = 3.0f;             
			style->GrabRounding = 2.0f;             
			style->AntiAliasedLines = true;
			style->AntiAliasedFill = true;
			style->WindowRounding = 2;
			style->ChildRounding = 2;
			style->ScrollbarSize = 16;
			style->ScrollbarRounding = 3;
			style->GrabRounding = 2;
			style->ItemSpacing.x = 10;
			style->ItemSpacing.y = 4;
			style->IndentSpacing = 22;
			style->FramePadding.x = 6;
			style->FramePadding.y = 4;
			style->Alpha = 1.0f;
			style->FrameRounding = 3.0f;

			colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
			colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
			colors[ImGuiCol_WindowBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
			colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
			colors[ImGuiCol_PopupBg] = ImVec4(0.93f, 0.93f, 0.93f, 0.98f);
			colors[ImGuiCol_Border] = ImVec4(0.71f, 0.71f, 0.71f, 0.08f);
			colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.04f);
			colors[ImGuiCol_FrameBg] = ImVec4(0.71f, 0.71f, 0.71f, 0.55f);
			colors[ImGuiCol_FrameBgHovered] = ImVec4(0.94f, 0.94f, 0.94f, 0.55f);
			colors[ImGuiCol_FrameBgActive] = ImVec4(0.71f, 0.78f, 0.69f, 0.98f);
			colors[ImGuiCol_TitleBg] = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
			colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.82f, 0.78f, 0.78f, 0.51f);
			colors[ImGuiCol_TitleBgActive] = ImVec4(0.78f, 0.78f, 0.78f, 1.00f);
			colors[ImGuiCol_MenuBarBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
			colors[ImGuiCol_ScrollbarBg] = ImVec4(0.20f, 0.25f, 0.30f, 0.61f);
			colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.90f, 0.90f, 0.90f, 0.30f);
			colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.92f, 0.92f, 0.92f, 0.78f);
			colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
			colors[ImGuiCol_CheckMark] = ImVec4(0.184f, 0.407f, 0.193f, 1.00f);
			colors[ImGuiCol_SliderGrab] = ImVec4(0.26f, 0.59f, 0.98f, 0.78f);
			colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
			colors[ImGuiCol_Button] = ImVec4(0.71f, 0.78f, 0.69f, 0.40f);
			colors[ImGuiCol_ButtonHovered] = ImVec4(0.725f, 0.805f, 0.702f, 1.00f);
			colors[ImGuiCol_ButtonActive] = ImVec4(0.793f, 0.900f, 0.836f, 1.00f);
			colors[ImGuiCol_Header] = ImVec4(0.71f, 0.78f, 0.69f, 0.31f);
			colors[ImGuiCol_HeaderHovered] = ImVec4(0.71f, 0.78f, 0.69f, 0.80f);
			colors[ImGuiCol_HeaderActive] = ImVec4(0.71f, 0.78f, 0.69f, 1.00f);
			colors[ImGuiCol_Column] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
			colors[ImGuiCol_ColumnHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.78f);
			colors[ImGuiCol_ColumnActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
			colors[ImGuiCol_Separator] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
			colors[ImGuiCol_SeparatorHovered] = ImVec4(0.14f, 0.44f, 0.80f, 0.78f);
			colors[ImGuiCol_SeparatorActive] = ImVec4(0.14f, 0.44f, 0.80f, 1.00f);
			colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
			colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.45f);
			colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.78f);
			colors[ImGuiCol_PlotLines] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
			colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
			colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
			colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
			colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
			colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
			colors[ImGuiCol_DragDropTarget] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
			colors[ImGuiCol_NavHighlight] = colors[ImGuiCol_HeaderHovered];
			colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.70f, 0.70f, 0.70f, 0.70f);
		

		if (ImGui::CollapsingHeader("Softaim")) {
			ImGui::Text(" Join The Discord https://discord.gg/D8xwnEEqUB");
			ImGui::Checkbox("Softaim ", &Settings::MouseAimbot);
			
			ImGui::Checkbox("Enable FOV", &Settings::AimbotCircle);
			ImGui::SliderFloat("Aim Smoothness", &Aim_Speed, 2, 50);
			ImGui::SliderFloat("Fov Size", &Settings::AimbotFOV, 15, 500);
		}
		if (ImGui::CollapsingHeader("ESP Settings")) {
			ImGui::Checkbox("Player Snaplines", &Settings::lineesp);
			ImGui::NewLine();
			ImGui::Checkbox("Box ESP", &Settings::CornerESP);
		}
		if (ImGui::CollapsingHeader("Other")) {
			ImGui::Checkbox("Crosshair", &Settings::Crosshair);
		}
		

		

		

		

		
		
		ImGui::End();
	}
	
	drawLoop();

	ImGui::EndFrame();
	D3DDevice->SetRenderState(D3DRS_ZENABLE, false);
	D3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
	D3DDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
	D3DDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);

	if (D3DDevice->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		D3DDevice->EndScene();
	}
	HRESULT Results = D3DDevice->Present(NULL, NULL, NULL, NULL);

	if (Results == D3DERR_DEVICELOST && D3DDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
	{
		ImGui_ImplDX9_InvalidateDeviceObjects();
		D3DDevice->Reset(&d3d);
		ImGui_ImplDX9_CreateDeviceObjects();
	}
}

DWORD Menuthread(LPVOID in)
{
	while (1)
	{
		HWND test = FindWindowA(0, ("Fortnite  "));

		if (test == NULL)
		{
			ExitProcess(0);
		}

		
		


		if (Settings::ShowMenu)
		{



			if (GetAsyncKeyState(VK_F1) & 1) {
				Settings::MouseAimbot = !Settings::MouseAimbot;
			}

			if (GetAsyncKeyState(VK_F2) & 1) {
				Settings::AimbotCircle = !Settings::AimbotCircle;
			}

			if (GetAsyncKeyState(VK_F3) & 1) {
				Settings::Crosshair = !Settings::Crosshair;
			}

			if (GetAsyncKeyState(VK_F4) & 1) {
				Settings::lineesp = !Settings::lineesp;
			}

			if (GetAsyncKeyState(VK_F5) & 1) {
				Settings::Skeleton = !Settings::Skeleton;
			}

			if (GetAsyncKeyState(VK_F6) & 1) {
				Settings::PlayerESP = !Settings::PlayerESP;
			}

			if (GetAsyncKeyState(VK_F7) & 1) {
				Settings::CornerESP = !Settings::CornerESP;
			}

			
		}
	}
}


MSG Message_Loop = { NULL };

void Loop()
{
	static RECT old_rc;
	ZeroMemory(&Message_Loop, sizeof(MSG));

	while (Message_Loop.message != WM_QUIT)
	{
		if (PeekMessage(&Message_Loop, Window, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&Message_Loop);
			DispatchMessage(&Message_Loop);
		}

		HWND hwnd_active = GetForegroundWindow();

		if (hwnd_active == hWnd) {
			HWND hwndtest = GetWindow(hwnd_active, GW_HWNDPREV);
			SetWindowPos(Window, hwndtest, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}

		if (GetAsyncKeyState(0x23) & 1)
			exit(8);

		RECT rc;
		POINT xy;

		ZeroMemory(&rc, sizeof(RECT));
		ZeroMemory(&xy, sizeof(POINT));
		GetClientRect(hWnd, &rc);
		ClientToScreen(hWnd, &xy);
		rc.left = xy.x;
		rc.top = xy.y;

		ImGuiIO& io = ImGui::GetIO();
		io.ImeWindowHandle = hWnd;
		io.DeltaTime = 1.0f / 60.0f;

		POINT p;
		GetCursorPos(&p);
		io.MousePos.x = p.x - xy.x;
		io.MousePos.y = p.y - xy.y;

		if (GetAsyncKeyState(VK_LBUTTON)) {
			io.MouseDown[0] = true;
			io.MouseClicked[0] = true;
			io.MouseClickedPos[0].x = io.MousePos.x;
			io.MouseClickedPos[0].x = io.MousePos.y;
		}
		else
			io.MouseDown[0] = false;

		if (rc.left != old_rc.left || rc.right != old_rc.right || rc.top != old_rc.top || rc.bottom != old_rc.bottom)
		{
			old_rc = rc;

			Width = rc.right;
			Height = rc.bottom;

			d3d.BackBufferWidth = Width;
			d3d.BackBufferHeight = Height;
			SetWindowPos(Window, (HWND)0, xy.x, xy.y, Width, Height, SWP_NOREDRAW);
			D3DDevice->Reset(&d3d);
		}
		Render();
	}
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	DestroyWindow(Window);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, Message, wParam, lParam))
		return true;

	switch (Message)
	{
	case WM_DESTROY:
		ShutDown();
		PostQuitMessage(0);
		exit(4);
		break;
	case WM_SIZE:
		if (D3DDevice != NULL && wParam != SIZE_MINIMIZED)
		{
			ImGui_ImplDX9_InvalidateDeviceObjects();
			d3d.BackBufferWidth = LOWORD(lParam);
			d3d.BackBufferHeight = HIWORD(lParam);
			HRESULT hr = D3DDevice->Reset(&d3d);
			if (hr == D3DERR_INVALIDCALL)
				IM_ASSERT(0);
			ImGui_ImplDX9_CreateDeviceObjects();
		}
		break;
	default:
		return DefWindowProc(hWnd, Message, wParam, lParam);
		break;
	}
	return 0;
}



void ShutDown()
{
	VertBuff->Release();
	D3DDevice->Release();
	pObject->Release();

	DestroyWindow(Window);
	UnregisterClass(L"fgers", NULL);
}

int main(int argc, const char* argv[])
{
	Sleep(300);  
	SetConsoleTitleA("Adzara Private");



	Sleep(3000);

	CreateThread(NULL, NULL, GUI, NULL, NULL, NULL);
	DrverInit = CreateFileW((L"\\\\.\\ronindrv"), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr); // replace external with your own handler Flow#7662

	if (DrverInit == INVALID_HANDLE_VALUE)
	{
		printf("\n Driver Failure");
		Sleep(2000);
		exit(0);
	}

	info_t Input_Output_Data1;
	unsigned long int Readed_Bytes_Amount1;
	DeviceIoControl(DrverInit, ctl_clear, &Input_Output_Data1, sizeof Input_Output_Data1, &Input_Output_Data1, sizeof Input_Output_Data1, &Readed_Bytes_Amount1, nullptr);
	std::string name = ("Hfh98HDF89hf89dHF98H9h98HF98");

	while (hWnd == NULL)
	{
		hWnd = FindWindowA(0, ("Fortnite  "));
		system("cls");
		printf("\n Looking for Fortnite Process!");
		Sleep(1000);

	}
	GetWindowThreadProcessId(hWnd, &FNProcID);

	
	info_t Input_Output_Data;
	Input_Output_Data.pid = FNProcID;
	unsigned long int Readed_Bytes_Amount;
	CreateThread(NULL, NULL, Menuthread, NULL, NULL, NULL);
	DeviceIoControl(DrverInit, ctl_base, &Input_Output_Data, sizeof Input_Output_Data, &Input_Output_Data, sizeof Input_Output_Data, &Readed_Bytes_Amount, nullptr);
	base_address = (unsigned long long int)Input_Output_Data.data;
	printf("\n Cheat Loaded!!");
	std::printf(("ID: %p.\n"), (void*)base_address);
	printf("\n Dont close This");
	Sleep(1000);
	printf("\n Have fun!");




	

	WindowMain();
	InitializeD3D();

	Loop();
	ShutDown();

	return 0;

}