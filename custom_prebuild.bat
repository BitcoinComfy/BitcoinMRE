REM custom_prebuild.bat 
REM this is for call the ResEditor.exe to save the project resource, to generate ResID.h
REM  for avoid ResID.h is modified after build, this will effect debug

"C:\tools\MRE_SDK_V3\tools\ResEditor\CmdShell.exe" SAVE "C:\Users\anon\Desktop\GUI\MRE_API_Tests.vcproj"
if %errorlevel% == 0 (
 echo prebuild OK.
 exit 0
) else (
 echo prebuild error
 echo error code: %errorlevel%
 exit 1
)

