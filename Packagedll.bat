"C:\tools\MRE_SDK_V3\tools\DllPackage.exe" "C:\Users\anon\Desktop\GUI\MRE_API_Tests.vcproj"
if %errorlevel% == 0 (
 echo postbuild OK.
  copy MRE_API_Tests.vpp ..\..\..\MoDIS_VC9\WIN32FS\DRIVE_E\MRE_API_Tests.vpp /y
exit 0
)else (
echo postbuild error
  echo error code: %errorlevel%
  exit 1
)

