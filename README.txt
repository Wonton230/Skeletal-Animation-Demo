FbxLoader - An app to test FBX loading and rendering. Requires FBX SDK project setup.

**If running in Visual Studio, remember to set the debugging configuration:
	- Command = $(TargetFilePath)
	- Active Directory = $(SolutionDir)/Run

------------------------------------------Current State of the Project----------------------------------------------------------------------
Right now, nothing fbx related will render because there has not been support for translating Mesh information into data for our engine's DX11 rendering pipeline.
Things that were accomplished:
	- The FbxLoader project is dynamically linked with the Fbx Sdk dynamically.
	- The data structures designed to hold the Fbx node data are created (most not used yet)
	- The project compiles with no missing pdb errors and all warnings about the linker have been addressed
	- Eliminated all memory leak errors from fbx sdk.
	- Right now, an Fbx manager is created and initialized when leaving attract mode, then shutdown and cleaned up when entering attract mode again.
	- A Fbx file can be taken in as a path and loaded, then properly destroyed when leaving scene mode.

Attract Screen Controls:
	~	- DevConsole
	SPACE 	- Enter Scene View Mode
	ESC 	- Quit
Scene Mode Controls:
	~	- DevConsole
	WASD 	- Move
	Mouse	- Look
	Q	- Move Down
	E	- Move up
	Z	- Roll left
	C	- Roll Right
	F1	- Scene A (unlit cube)
	F2	- Scene B (empty)
	F3	- Scene C (empty)
	ESC	- Quit to Attract Mode


------------------------------------------DLL Link Configuration Notes (All of this should be automated)------------------------------------
If in DEBUG mode:

	Engine Project Properties:
		Property -> C/C++ -> General -> Additional Include Directories -> [Add Path to fbx sdk include directory]
			"$(SolutionDir)../Engine/Code/ThirdParty/fbx_sdk/2020.3.7/include/"

	FbxLoader Project Properties:
		Property -> C/C++ -> General -> Additional Include Directories -> [Add Path to fbx sdk include directory]
			"$(SolutionDir)../Engine/Code/ThirdParty/fbx_sdk/2020.3.7/include/"
		Property -> C/C++ -> Code Generation -> Runtime Library -> Multi-Threaded Debug DLL
		Property -> C/C++ -> Preprocessor -> Preprocessor Definitions -> Add [FBXSDK_SHARED]
		Property -> Linker -> Additional Library Directories -> [Add Path to fbx sdk debug lib directory]
			"$(SolutionDir)../Engine/Code/ThirdParty/fbx_sdk/2020.3.7/lib/x64/debug/"
		Post Build Event:
		xcopy /Y /D "$(SolutionDir)../Engine/Code/ThirdParty/fbx_sdk/2020.3.7/lib/x64/debug/*.dll" "$(SolutionDir)Run"

If in RELEASE mode:

	Engine Project Properties:
		Property -> C/C++ -> General -> Additional Include Directories -> [Add Path to fbx sdk include directory]
			"$(SolutionDir)../Engine/Code/ThirdParty/fbx_sdk/2020.3.7/include/"

	FbxLoader Project Properties:
		Property -> C/C++ -> General -> Additional Include Directories -> [Add Path to fbx sdk include directory]
			"$(SolutionDir)../Engine/Code/ThirdParty/fbx_sdk/2020.3.7/include/"
		Property -> C/C++ -> Code Generation -> Runtime Library -> Multi-Threaded DLL
		Property -> C/C++ -> Preprocessor -> Preprocessor Definitions -> Add [FBXSDK_SHARED]
		Property -> Linker -> Additional Library Directories -> [Add Path to fbx sdk debug lib directory]
			"$(SolutionDir)../Engine/Code/ThirdParty/fbx_sdk/2020.3.7/lib/x64/release/"
		Post Build Event:
		xcopy /Y /D "$(SolutionDir)../Engine/Code/ThirdParty/fbx_sdk/2020.3.7/lib/x64/release/*.dll" "$(SolutionDir)Run"


	
