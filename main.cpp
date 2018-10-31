#include "nvse/nvse/PluginAPI.h"
#include "common/IDebugLog.h"
#include "nvse/nvse/nvse_version.h"
#include "nvse/nvse/Hooks_DirectInput8Create.h"
#include "nvse/nvse/SafeWrite.h"
#include <string>
#include <windows.h>

#ifdef NOGORE
IDebugLog		gLog("nvse_plugin_console_clipboard.log");
#else
IDebugLog		gLog("nvse_plugin_console_clipboard.log");
#endif

NVSEInterface * SaveNVSE;
DIHookControl *g_DIHookCtrl = NULL;

// function prototypes
void patchOnConsoleInput();
void __fastcall CheckCTRLV();
void GetClipboardText(char** buffer);
void __fastcall PrintToConsoleInput(UInt32 character);


static const UInt32 getConsoleStringLocation = 0x71B160;
static const UInt32 sendCharToInput = 0x71B210;



extern "C" {

bool NVSEPlugin_Query(const NVSEInterface * nvse, PluginInfo * info)
{
	// fill out the info structure
	info->infoVersion = PluginInfo::kInfoVersion;
	info->name = "nvse_plugin_console_clipboard";
	info->version = 2;

	// version checks
	if(nvse->nvseVersion < NVSE_VERSION_INTEGER)
	{
		_ERROR("NVSE version too old (got %08X expected at least %08X)", nvse->nvseVersion, NVSE_VERSION_INTEGER);
		return false;
	}

	if(!nvse->isEditor)
	{
		if(nvse->runtimeVersion < RUNTIME_VERSION_1_4_0_525)
		{
			_ERROR("incorrect runtime version (got %08X need at least %08X)", nvse->runtimeVersion, RUNTIME_VERSION_1_4_0_525);
			return false;
		}

#ifdef NOGORE
		if(!nvse->isNogore)
		{
			_ERROR("incorrect runtime edition (got %08X need %08X (nogore))", nvse->isNogore, 1);
			return false;
		}
#else
		if(nvse->isNogore)
		{
			_ERROR("incorrect runtime edition (got %08X need %08X (standard))", nvse->isNogore, 0);
			return false;
		}
#endif
	}
	else
	{
		if(nvse->editorVersion < CS_VERSION_1_4_0_518)
		{
			_ERROR("incorrect editor version (got %08X need at least %08X)", nvse->editorVersion, CS_VERSION_1_4_0_518);
			return false;
		}
#ifdef NOGORE
		_ERROR("Editor only uses standard edition, closing.");
		return false;
#endif
	}

	// version checks pass

	return true;
}


bool NVSEPlugin_Load(const NVSEInterface * nvse)
{

    NVSEDataInterface *nvseData = (NVSEDataInterface*)nvse->QueryInterface(kInterface_Data);
    g_DIHookCtrl = (DIHookControl*)nvseData->GetSingleton(NVSEDataInterface::kNVSEData_DIHookControl);

	patchOnConsoleInput();

    return true;
}

};


void patchOnConsoleInput() {
    UInt32 onConsoleInputAddress = 0x70E09E;
    WriteRelJump(onConsoleInputAddress, (UInt32) CheckCTRLV);
}


void PrintClipBoardToConsoleInput() {
    char* clipboardText = "";
    GetClipboardText(&clipboardText);

    for(int i=0,c=0; clipboardText[i] != '\0'; i++) {
        PrintToConsoleInput(clipboardText[i]);
	}
}


void GetClipboardText(char** buffer)
{
  // Try opening the clipboard
	if (! OpenClipboard(NULL)) {
      return;
	}

  // Get handle of clipboard object for ANSI text
  HANDLE hData = GetClipboardData(CF_TEXT);
  if (hData == NULL) {
    return;
  }

  // Lock the handle to get the actual text pointer
  char * pszText = static_cast<char*>( GlobalLock(hData) );
  if (pszText == NULL) {
    return;
  }
  
  // Save text in a string class instance
  std::string text( pszText );

  // Release the lock
  GlobalUnlock( hData );

  // Release the clipboard
  CloseClipboard();
  *buffer = strdup(text.c_str());
}

void __fastcall PrintToConsoleInput(UInt32 characterToPrint)
{
	__asm
	{
      getConsoleInputStringLocation:
        push  00
        call  getConsoleStringLocation // sets eax to location of console input String
        push  eax
      sendCharToConsoleInput:
        mov   eax, characterToPrint
        pop   ecx  // eax = location of console input String
        push  eax
        call sendCharToInput
	}
}


/*
  EAX will contain the character pressed, v's code is 0x76
  ECX contains the location of the input buffer to write to
*/
__declspec(naked) void __fastcall CheckCTRLV()
{
	static const UInt32 retnAddr = 0x70E0A3;
	__asm
	{
		push ecx
	  checkV:
        cmp eax, 0x76 // compare input to 'v'
        jne handleNormalInput
	  checkControl:	
        mov		ecx, g_DIHookCtrl
		cmp		byte ptr [ecx+0xCF], 0 // check left control
		jne		handlePrint
		cmp		byte ptr [ecx+0x44F], 0 // check right control
		je		handleNormalInput
      handlePrint:
        call PrintClipBoardToConsoleInput
		pop ecx
        jmp done
      handleNormalInput:
		pop ecx
        call sendCharToInput
      done:
		jmp retnAddr
  }
}
