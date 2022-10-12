"C:\tools\MRE_SDK_V3\tools\DllPackage.exe" "C:\Users\anon\Desktop\BitcoinMRE\BitcoinMRE\BitcoinMRE.vcproj"
if %errorlevel% == 0 (
 echo postbuild OK.
  copy BitcoinMRE.vpp ..\..\..\MoDIS_VC9\WIN32FS\DRIVE_E\BitcoinMRE.vpp /y
exit 0
)else (
echo postbuild error
  echo error code: %errorlevel%
  exit 1
)

