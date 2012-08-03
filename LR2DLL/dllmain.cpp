// dllmain.cpp : DLL 응용 프로그램의 진입점을 정의합니다.
#include "stdafx.h"
#include "d3d9.h"
#include "d3dx9.h"
#pragma comment(lib, "d3dx9.lib")
#include "stdio.h"

#define ENDSCENE 42	// vTable의 42번째에 위치함
#define SCRWIDTH 640
#define SCRHEIGHT 480
#define MSGTIME 2000

static HRESULT WINAPI h_EndScene(LPDIRECT3DDEVICE9 pDevice); 
typedef HRESULT(__stdcall* EndScene_t)(LPDIRECT3DDEVICE9);
EndScene_t org_EndScene;

HMODULE m_hmodule;
void setPath();

char filePath[256];
HANDLE hmm_LR2PicPath;
char *p_LR2PicPath;
HANDLE hmm_LR2Message;
char *p_LR2Message;
HANDLE hmm_LR2ScreenCapture;
int *p_LR2ScreenCapture;
void setSharedMemory();
void releaseSharedMemory();

BOOL isFontCreated = FALSE;
LPD3DXFONT g_pFont = NULL;
int fontTransparency = 0;
char g_Message[256];
int g_Message_key = 0;
DWORD g_Message_time = 0;
BOOL FontCreate(LPDIRECT3DDEVICE9 pDevice) {
	// create font object
	D3DXCreateFont(pDevice, 36, 0, FW_BOLD, 0, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
		TEXT("Arial"), &g_pFont);
	return TRUE;
}

void createErrorLog(char *msg) {
	FILE *fp = fopen(".\\LR2DLL_errorlog.txt", "a");
	if (!fp) return;
	fprintf(fp, "%s\n", msg);
	fclose(fp);
}

LPDIRECT3DDEVICE9 g_pDevice;
void LR2_CaptureScreen() {
	if (!g_pDevice) {
		createErrorLog("error : No Object Pointer");
		return;
	}
	
	int LR2ScrStatus = 0;
	LR2ScrStatus = *p_LR2ScreenCapture;

	if (LR2ScrStatus == 1) {
		// file will be saved to %appdata%\\LR2Twit.jpg
		IDirect3DSurface9* pSurface;

		HRESULT r;
		//r = g_pDevice->CreateOffscreenPlainSurface(SCRWIDTH, SCRHEIGHT, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &pSurface, NULL);
		r = g_pDevice->CreateOffscreenPlainSurface(640,480,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&pSurface,NULL);
		if (r != D3D_OK) {
			createErrorLog("error : CreateOffscreenPlainSurface");
			LR2ScrStatus = 3;
		}
		//r = g_pDevice->GetFrontBufferData(0, pSurface);
		r = g_pDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&pSurface);
		if (r != D3D_OK) {
			createErrorLog("error : GetFrontBufferData");
			LR2ScrStatus = 3;
		}
		r = D3DXSaveSurfaceToFile( L".\\LR2Twit.jpg", D3DXIFF_JPG, pSurface, NULL, NULL );
		if (r != D3D_OK) {
			createErrorLog("error : D3DXSaveSurfaceToFile");
			LR2ScrStatus = 3;
		}

		// change status to finished
		if (LR2ScrStatus != 3) LR2ScrStatus = 2;
	}

	*p_LR2ScreenCapture = LR2ScrStatus;
}

void LR2_CheckMessage() {
	if (g_Message_key != p_LR2Message[0]) {
		// difference in Message!
		strcpy(g_Message,p_LR2Message+1);

		g_Message_key = p_LR2Message[0];
		g_Message_time = GetTickCount() + MSGTIME;
	}
}

HRESULT WINAPI h_EndScene(LPDIRECT3DDEVICE9 pDevice)
{ 
	//get device pointer
	g_pDevice = pDevice;

	if (!isFontCreated) {
		FontCreate(pDevice);
		isFontCreated = TRUE;
	}

	// draw text
	RECT rectTemp = {10, 10, 620, 420};

	int m_trans = 0;
	int fontTransparency = g_Message_time - GetTickCount();
	if (fontTransparency > 0xCC) m_trans = 0xCC;
	else if (fontTransparency < 0) m_trans = 0;
	else m_trans = fontTransparency;
	g_pFont->DrawTextA(NULL, g_Message, -1, &rectTemp, 0, 0x00FFFF00 | (m_trans<<24));
	fontTransparency--;
	if (fontTransparency < 0) fontTransparency = 0;

	// check if screen capture is necessary
	LR2_CaptureScreen();
	// check message
	LR2_CheckMessage();

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
			m_hmodule = hModule;
			
			// initalize
			setSharedMemory();

			// setpath for Screenshot
			setPath();


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
			releaseSharedMemory();
			RestoreD3DHooks();
			break;
		}
	}
	return TRUE;
}

//////////////////////////
// shared memory part ////
//////////////////////////

void setPath() {
	// set path
	char filePath[256];
	::GetModuleFileNameA(0, filePath, sizeof(filePath));
	filePath[strrchr(filePath, '\\') - filePath] = 0;
	strcat(filePath, "\\LR2Twit.jpg");
	
	strcpy(p_LR2PicPath, filePath);
}

void setSharedMemory() {
	hmm_LR2PicPath = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(filePath), L"LR2PicPath");
	if (!hmm_LR2PicPath) {
		return;
	}
	p_LR2PicPath = (char*)MapViewOfFile(hmm_LR2PicPath, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (!p_LR2PicPath) {
		return;
	}

	hmm_LR2ScreenCapture = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(int), L"LR2ScreenCapture");
	if (!hmm_LR2ScreenCapture) {
		return;
	}
	p_LR2ScreenCapture = (int*)MapViewOfFile(hmm_LR2ScreenCapture, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (!p_LR2ScreenCapture) {
		return;
	}
	
	hmm_LR2Message = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 256, L"LR2Message");
	if (!hmm_LR2Message) {
		return;
	}
	p_LR2Message = (char*)MapViewOfFile(hmm_LR2Message, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (!p_LR2Message) {
		return;
	}
}

void releaseSharedMemory() {
	UnmapViewOfFile(p_LR2PicPath);
	CloseHandle(hmm_LR2PicPath);
	UnmapViewOfFile(p_LR2ScreenCapture);
	CloseHandle(hmm_LR2ScreenCapture);
	UnmapViewOfFile(p_LR2Message);
	CloseHandle(hmm_LR2Message);
}