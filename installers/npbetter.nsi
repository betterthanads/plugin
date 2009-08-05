; npbetter.nsi
;
; This script installs the betterthanads plugin using the information found at:
;   https://developer.mozilla.org/en/Plugins/The_First_Install_Problem

;--------------------------------

Name "BetterThanAds Plugin"
Caption "BetterThanAds Plugin Installer"
Icon "${NSISDIR}\Contrib\Graphics\Icons\nsis1-install.ico"
OutFile "betterplugin.exe"

SetDateSave on
SetDatablockOptimize on
CRCCheck on

InstallDir "$PROGRAMFILES\BetterThanAdsPlugin"
InstallDirRegKey HKCU "Software\MozillaPlugins\@betterthanads.com/BetterThanAdsPlugin" "Install_Dir"

LicenseText "This program will install the BetterThanAds plugin for any browser that supports Mozilla Plugins."
LicenseData "license.txt"

RequestExecutionLevel user

;--------------------------------

Page license
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

;--------------------------------

AutoCloseWindow false
ShowInstDetails show

;--------------------------------

Section "" ; empty string makes it hidden, so would starting with -

  ; write reg info
  WriteRegStr HKCU "Software\MozillaPlugins\@betterthanads.com/BetterThanAdsPlugin" "Install_Dir" "$INSTDIR"

  ; write uninstall strings
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BetterThanAdsPlugin" "DisplayName" "BetterThanAds Plugin (remove only)"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BetterThanAdsPlugin" "UninstallString" '"$INSTDIR\btap-uninst.exe"'

  WriteRegStr HKCU "Software\MozillaPlugins\@betterthanads.com/BetterThanAdsPlugin" "Path" "$INSTDIR\npbetter.dll"
  WriteRegStr HKCU "Software\MozillaPlugins\@betterthanads.com/BetterThanAdsPlugin" "ProductName" "BetterThanAds Plugin"
  WriteRegStr HKCU "Software\MozillaPlugins\@betterthanads.com/BetterThanAdsPlugin" "Description" "BetterThanAds.com site-support tracking, subscriptions, and payments plugin"
  WriteRegStr HKCU "Software\MozillaPlugins\@betterthanads.com/BetterThanAdsPlugin" "Vendor" "BetterThanAds.com"
  WriteRegStr HKCU "Software\MozillaPlugins\@betterthanads.com/BetterThanAdsPlugin" "Version" "0.85"
  WriteRegStr HKCU "Software\MozillaPlugins\@betterthanads.com/BetterThanAdsPlugin\MimeTypes\application/x-betterthanads-bta" "Description" "BetterThanAds.com site-support tracking, subscriptions, and payments plugin"
  WriteRegStr HKCU "Software\MozillaPlugins\@betterthanads.com/BetterThanAdsPlugin\MimeTypes\application/x-betterthanads-bta" "Suffixes" "bta"

  SetOutPath $INSTDIR
  WriteUninstaller "btap-uninst.exe"

  File "npbetter.dll"

  ; For ActiveX control
  ;File "iebetter.ocx"
  ;RegDLL "$INSTDIR\iebetter.ocx"
  ;Sleep 1000

  ExecShell "open" '"http://betterthanads.com/activate"'
  
SectionEnd

;--------------------------------

; Uninstaller

UninstallText "This will completely uninstall the BetterThanAds Plugin. Click Uninstall to continue..."

UninstallIcon "${NSISDIR}\Contrib\Graphics\Icons\nsis1-uninstall.ico"

Section "Uninstall"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BetterThanAdsPlugin"
  DeleteRegKey HKCU "Software\MozillaPlugins\@betterthanads.com/BetterThanAdsPlugin"

  ; For ActiveX control
  ;UnRegDLL "$INSTDIR\iebetter.ocx"
  ;Sleep 1000
  ;Delete "$INSTDIR\iebetter.ocx"

  Delete "$INSTDIR\npbetter.dll"
  Delete "$INSTDIR\btap-uninst.exe"
  RMDir "$INSTDIR"

SectionEnd
