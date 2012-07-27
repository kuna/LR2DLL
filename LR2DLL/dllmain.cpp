// dllmain.cpp : DLL 응용 프로그램의 진입점을 정의합니다.
#include "stdafx.h"
#include "d3d9.h"
#include "d3dx9.h"
#pragma comment(lib, "d3dx9.lib")

#define ENDSCENE 42	// vTable의 42번째에 위치함

static HRESULT WINAPI h_EndScene(LPDIRECT3DDEVICE9 pDevice); 
typedef HRESULT(__stdcall* EndScene_t)(LPDIRECT3DDEVICE9);
EndScene_t org_EndScene;

HMODULE m_hmodule;
void LoadHandle(HWND *h);
void InputModule(HMODULE *hmodule);

BOOL isFontCreated = FALSE;
LPD3DXFONT g_pFont = NULL;
BOOL FontCreate(LPDIRECT3DDEVICE9 pDevice) {
	// create font object
	D3DXCreateFont(pDevice, 36, 0, FW_BOLD, 0, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
		TEXT("Arial"), &g_pFont);
	return TRUE;
}

HRESULT WINAPI h_EndScene(LPDIRECT3DDEVICE9 pDevice)
{ 
	if (!isFontCreated) {
		FontCreate(pDevice);
		isFontCreated = TRUE;
	}

	TCHAR str[255];
	wsprintf(str, L"this is test");

	// draw text
	RECT rectTemp = {10, 10, 800, 600};
	g_pFont->DrawText(NULL, str, -1, &rectTemp, 0, 0xCCFFFF00); //ID3DXSprite *g_pSprite = NULL;

	//DXGameHook.DrawRect(pDevice, 40, 110, 50, 50, txtPink);
	return org_EndScene(pDevice); 
}

void *DetourFunc(BYTE *src, const BYTE *dst, const int len) 
{
	BYTE *jmp = (BYTE*)malloc(len+5);
	DWORD dwback;
	VirtualProtect(src, len, PAGE_READWRITE, &dwback);
	memcpy(jmp, src, len); jmp += len;
	jmp[0] = 0xE9;
	*(DWORD*)(jmp+1) = (DWORD)(src+len - jmp) - 5;
	src[0] = 0xE9;
	*(DWORD*)(src+1) = (DWORD)(dst - src) - 5;
	VirtualProtect(src, len, dwback, &dwback);
	return (jmp-len);
}

bool bDataCompare(const BYTE* pData, const BYTE* bMask, const char* szMask)
{
	for(;*szMask;++szMask,++pData,++bMask)
		if(*szMask=='x'&& *pData!=*bMask ) 
			return false;
	return (*szMask) == NULL;
}

DWORD FindPattern(DWORD dwAddress,DWORD dwLen,BYTE *bMask,char* szMask)
{
	for(DWORD i=0; i < dwLen; i++)
		if( bDataCompare( (BYTE*)( dwAddress+i ),bMask,szMask) )
			return(DWORD)(dwAddress+i);
	return 0;
}

DWORD backup_Pattern = 0;
int StartD3DHooks()
{
	DWORD D3DPattern,*vTable, DXBase=NULL;
	DXBase = (DWORD)LoadLibraryA("d3d9.dll");
	while(!DXBase);
	{
		D3DPattern = FindPattern(DXBase, 0x128000, 
			(PBYTE)"\xC7\x06\x00\x00\x00\x00\x89\x86\x00\x00\x00\x00\x89\x86", "xx????xx????xx"); 
	}
	if(D3DPattern)
	{
		memcpy(&vTable,(void*)(D3DPattern+2),4);
		org_EndScene = (EndScene_t)DetourFunc((PBYTE)vTable[ENDSCENE],
			(PBYTE)h_EndScene,5);

		// backup
		backup_Pattern = D3DPattern;
	}
	
	return TRUE;
}

bool RetourFunc(BYTE *src, BYTE *restore, const int len)
{
  DWORD dwback;
  if(!VirtualProtect(src, len, PAGE_READWRITE, &dwback))  { return (false); }
  if(!memcpy(src, restore, len))              { return (false); }
  restore[0] = 0xE9;
  *(DWORD*)(restore+1) = (DWORD)(src - restore) - 5;
  if(!VirtualProtect(src, len, dwback, &dwback))      { return (false); }
  return (true);
}

int RestoreD3DHooks()
{
	if (backup_Pattern == 0) {
		MessageBox(NULL,L"wer", L"", NULL);
		return FALSE;
	}
	
	DWORD *vTable;
	memcpy(&vTable,(void*)(backup_Pattern+2),4);
	RetourFunc((PBYTE)vTable[ENDSCENE], (PBYTE)org_EndScene,5);

	return TRUE;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		{
			// backup
			//m_hmodule = hModule;
			//InputModule(&hModule);

			// shared memory로 hwnd 얻기
			TCHAR str[256];
			HWND hWnd = 0;
			LoadHandle(&hWnd);
			GetWindowText(hWnd, str, 256);
			lstrcat(str, L" (Hooked)");
			SetWindowText(hWnd, str);

			/* d3d hook */
			DisableThreadLibraryCalls(hModule);	// DLL THREAD ATTACH 이벤트 미연 방지, 디버거 검출 코드로부터 숨기.
			StartD3DHooks();
			break;
		}
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		{
			TCHAR str[256];
			HWND hWnd = 0;
			LoadHandle(&hWnd);
			GetWindowText(hWnd, str, 256);
			str[ lstrlen(str) - 9] = 0;
			SetWindowText(hWnd, str);

			RestoreD3DHooks();
			break;
		}
	}
	return TRUE;
}

//////////////////////////
// shared memory part ////
//////////////////////////

TCHAR *p;
void InputMessage(TCHAR *msg) {
	HANDLE hMemoryMap;
	hMemoryMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(TCHAR)*1024, L"testMap");
	if (!hMemoryMap) {
		lstrcpy(msg, L"* error! failed to CreateFileMapping");
		return;
	}

	p = (TCHAR*)MapViewOfFile(hMemoryMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (!p) {
		lstrcpy(msg, L"* error! failed to MapViewOfFile");
		return;
	}
	
	lstrcpy(p, msg);
}

void LoadMessage(TCHAR *msg) {
	HANDLE hMemoryMap;
	hMemoryMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(TCHAR)*1024, L"testMap");
	if (!hMemoryMap) {
		lstrcpy(msg, L"* error! failed to CreateFileMapping");
		return;
	}

	p = (TCHAR*)MapViewOfFile(hMemoryMap, FILE_MAP_READ, 0, 0, 0);
	if (!p) {
		lstrcpy(msg, L"* error! failed to MapViewOfFile");
		return;
	}
	
	lstrcpy(msg, p);
}

void LoadHandle(HWND *h) {
	HANDLE hMemoryMap;
	hMemoryMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(HWND), L"LR2_Handle");
	if (!hMemoryMap) {
		*h = (HWND)1;
		return;
	}

	HWND *m = (HWND*)MapViewOfFile(hMemoryMap, FILE_MAP_READ, 0, 0, 0);
	if (!m) {
		*h = (HWND)2;
		return;
	}
	
	*h = *m;
}

void InputModule(HMODULE *hmodule) {
	HANDLE hMemoryMap;
	hMemoryMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(HMODULE), L"LR2_DLLModule");
	if (!hMemoryMap) {
		return;
	}

	HMODULE *p = (HMODULE*)MapViewOfFile(hMemoryMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (!p) {
		return;
	}
	
	*p = *hmodule;
}
