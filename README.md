## Skeletal Animation Demo
An app to test FBX loading and rendering of objects and skeletal meshes. Uses animation data and physics from the engine reading FBX to power smart IK movement

**If running in Visual Studio, remember to set the debugging configuration:
	- Command = $(TargetFilePath)
	- Active Directory = $(SolutionDir)/Run

# Current State of the Project:
	Scene 1: Animation playback testing ground: A place to play raw animations and test functionalities like using raw world transformations, local joint rebuilding for anatomical consistency, and mesh rendering.

	Scene 2: IK testing ground: A scene showing a joint and a player controlled end effector target. Made to test different joint constraints in the Engine's FABRIK solver.

	Scene 3: Third Person playground: Gives the player a controllable third-person character that is fully animated with IK enhancements as well as objects to test against.

# Attract Screen Controls:
	~	- DevConsole
	SPACE 	- Enter Scene View Mode
	ESC 	- Quit
# Scene Mode Controls:
	~	- DevConsole
	WASD 	- Move
	Mouse	- Look
	Q	- Move Down
	E	- Move up
	Z	- Roll left
	C	- Roll Right
	F7	- Cycle Scenes
	ESC	- Quit to Attract Mode


# DLL Link Configuration Notes (All of this should be automated)
	If in DEBUG mode:
		# Engine Project Properties:
			Property -> C/C++ -> General -> Additional Include Directories -> [Add Path to fbx sdk include directory]
				"$(SolutionDir)../Engine/Code/ThirdParty/fbx_sdk/2020.3.7/include/"
	
		# FbxLoader Project Properties:
			Property -> C/C++ -> General -> Additional Include Directories -> [Add Path to fbx sdk include directory]
				"$(SolutionDir)../Engine/Code/ThirdParty/fbx_sdk/2020.3.7/include/"
			Property -> C/C++ -> Code Generation -> Runtime Library -> Multi-Threaded Debug DLL
			Property -> C/C++ -> Preprocessor -> Preprocessor Definitions -> Add [FBXSDK_SHARED]
			Property -> Linker -> Additional Library Directories -> [Add Path to fbx sdk debug lib directory]
				"$(SolutionDir)../Engine/Code/ThirdParty/fbx_sdk/2020.3.7/lib/x64/debug/"
			Post Build Event:
			xcopy /Y /D "$(SolutionDir)../Engine/Code/ThirdParty/fbx_sdk/2020.3.7/lib/x64/debug/*.dll" "$(SolutionDir)Run"

	If in RELEASE mode:
		# Engine Project Properties:
			Property -> C/C++ -> General -> Additional Include Directories -> [Add Path to fbx sdk include directory]
				"$(SolutionDir)../Engine/Code/ThirdParty/fbx_sdk/2020.3.7/include/"
	
		# FbxLoader Project Properties:
			Property -> C/C++ -> General -> Additional Include Directories -> [Add Path to fbx sdk include directory]
				"$(SolutionDir)../Engine/Code/ThirdParty/fbx_sdk/2020.3.7/include/"
			Property -> C/C++ -> Code Generation -> Runtime Library -> Multi-Threaded DLL
			Property -> C/C++ -> Preprocessor -> Preprocessor Definitions -> Add [FBXSDK_SHARED]
			Property -> Linker -> Additional Library Directories -> [Add Path to fbx sdk debug lib directory]
				"$(SolutionDir)../Engine/Code/ThirdParty/fbx_sdk/2020.3.7/lib/x64/release/"
			Post Build Event:
			xcopy /Y /D "$(SolutionDir)../Engine/Code/ThirdParty/fbx_sdk/2020.3.7/lib/x64/release/*.dll" "$(SolutionDir)Run"


	
