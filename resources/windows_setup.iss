; EasyKiConverter Inno Setup Script

#define MyAppName "EasyKiConverter"
#define MyAppPublisher "tangsangsimida"
#define MyAppURL "https://github.com/tangsangsimida/EasyKiConverter"
#define MyAppExeName "easykiconverter.exe"
; 用户数据目录名称（需与 ConfigService 中一致）
#define UserDataDirName "EasyKiConverter_Cpp_Version"

; 这些变量将通过命令行定义传入
;#define MyAppVersion "3.0.0"
;#define SourceDir "..\build\bin\Release"
;#define AppIconPath "..\resources\icons\app_icon.ico"
;#define LicensePath "..\LICENSE"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{8C6E9E12-9A44-4F8A-8B3C-9B8D7E6F5A4B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DisableProgramGroupPage=yes
; the "ArchitecturesAllowed=x64" directive specifies that this setup is for 64-bit Windows only
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
; 要求管理员权限
PrivilegesRequired=admin
; 最低 Windows 10 版本 (Windows 10 1809+)
MinimumVersion=10.0.17763
; 卸载时显示程序图标
UninstallDisplayIcon={app}\{#MyAppExeName}
#ifdef LicensePath
LicenseFile={#LicensePath}
#else
LicenseFile=..\LICENSE
#endif
#ifdef AppIconPath
SetupIconFile={#AppIconPath}
#else
SetupIconFile=..\resources\icons\app_icon.ico
#endif
OutputBaseFilename=EasyKiConverter_{#MyAppVersion}_Setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; 主执行文件
Source: "{#SourceDir}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
; 所有依赖文件 (递归)
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; 注意：SourceDir 应该是已经包含所有 DLL 的目录

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
var
  DeleteUserDataCheckBox: TNewCheckBox;
  UninstallPage: TWizardPage;
  AppMutex: String;

// 创建卸载选项页面
procedure CreateUninstallPage();
begin
  UninstallPage := CreateCustomPage(wpWelcome, '卸载选项 (Uninstall Options)', '选择是否删除用户数据');
  DeleteUserDataCheckBox := TNewCheckBox.Create(UninstallPage);
  DeleteUserDataCheckBox.Parent := UninstallPage.Surface;
  DeleteUserDataCheckBox.Caption := '删除用户配置和数据 (Delete user data)';
  DeleteUserDataCheckBox.Left := ScaleX(20);
  DeleteUserDataCheckBox.Top := ScaleY(10);
  DeleteUserDataCheckBox.Width := ScaleX(350);
  DeleteUserDataCheckBox.Checked := False;
end;

procedure InitializeWizard();
begin
  AppMutex := 'EasyKiConverter_SingleInstance';
  // 仅在卸载时创建自定义页面
  if Uninstall then
    CreateUninstallPage();
end;

// 安装前检查程序是否在运行（静默和非静默都支持）
function InitializeSetup(): Boolean;
begin
  Result := True;
  if CheckForMutexes(AppMutex, 0) then
  begin
    if WizardSilent() then
    begin
      // 静默安装时直接退出，不弹框
      Result := False;
    end
    else
    begin
      MsgBox('请先关闭 EasyKiConverter，然后再继续安装。'#13#10#13#10'Please close EasyKiConverter before continuing.',
            mbError, MB_OK);
      Result := False;
    end;
  end;
end;

// 安装页面点击"下一步"前检查程序是否在运行
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpWelcome then
  begin
    if CheckForMutexes(AppMutex, 0) then
    begin
      MsgBox('请先关闭 EasyKiConverter，然后再继续安装。'#13#10#13#10'Please close EasyKiConverter before continuing.',
            mbError, MB_OK);
      Result := False;
    end;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  UserDataDir: String;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    if DeleteUserDataCheckBox.Checked then
    begin
      UserDataDir := ExpandConstant('{localappdata}') + '\' + '{#UserDataDirName}';
      if DirExists(UserDataDir) then
        DelTree(UserDataDir, True, True, True);
    end;
  end;
end;

// 支持静默安装/卸载参数
procedure CurPageChanged(CurPageID: Integer);
begin
  // 静默卸载模式下自动勾选删除用户数据选项
  if Uninstall and (CurPageID = UninstallPage.ID) then
  begin
    if WizardSilent() then
      DeleteUserDataCheckBox.Checked := True;
  end;
end;


[UninstallDelete]
Type: filesandordirectories; Name: "{localappdata}\{#UserDataDirName}"
