//! Desktop + Start Menu shortcuts to gdlaunch.exe (IShellLink, the shell's own .lnk writer) and the
//! per-user Add/Remove Programs entry (HKCU\...\Uninstall\Grimdark, which runs our copy with --uninstall).
use std::path::Path;

use super::paths::{desktop_shortcut, github_page_url, start_menu_shortcut, APP_NAME, INSTALLER_EXE, LAUNCHER_EXE, UNINSTALL_KEY};

#[cfg(windows)]
fn write_lnk(lnk: &Path, target: &Path, workdir: &Path, description: &str) -> Result<(), String> {
    use windows::core::{Interface, HSTRING, PCWSTR};
    use windows::Win32::System::Com::{
        CoCreateInstance, CoInitializeEx, CoUninitialize, IPersistFile, CLSCTX_INPROC_SERVER, COINIT_APARTMENTTHREADED,
    };
    use windows::Win32::UI::Shell::{IShellLinkW, ShellLink};

    unsafe {
        let hr = CoInitializeEx(None, COINIT_APARTMENTTHREADED);
        let inited = hr.is_ok();
        let result = (|| -> windows::core::Result<()> {
            let link: IShellLinkW = CoCreateInstance(&ShellLink, None, CLSCTX_INPROC_SERVER)?;
            let target_w = HSTRING::from(target.as_os_str());
            let workdir_w = HSTRING::from(workdir.as_os_str());
            let desc_w = HSTRING::from(description);
            link.SetPath(PCWSTR(target_w.as_ptr()))?;
            link.SetWorkingDirectory(PCWSTR(workdir_w.as_ptr()))?;
            link.SetDescription(PCWSTR(desc_w.as_ptr()))?;
            link.SetIconLocation(PCWSTR(target_w.as_ptr()), 0)?;
            let file: IPersistFile = link.cast()?;
            let lnk_w = HSTRING::from(lnk.as_os_str());
            file.Save(PCWSTR(lnk_w.as_ptr()), true)?;
            Ok(())
        })();
        if inited {
            CoUninitialize();
        }
        result.map_err(|e| format!("Failed to write {}: {}", lnk.display(), e))
    }
}

#[cfg(not(windows))]
fn write_lnk(_lnk: &Path, _target: &Path, _workdir: &Path, _description: &str) -> Result<(), String> {
    Err("shortcuts are Windows-only".into())
}

fn register_uninstall(install_dir: &Path, version: &str) -> Result<(), String> {
    use winreg::enums::HKEY_CURRENT_USER;
    use winreg::RegKey;
    let root = RegKey::predef(HKEY_CURRENT_USER);
    let (key, _) = root
        .create_subkey(format!("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{}", UNINSTALL_KEY))
        .map_err(|e| format!("Failed to create the Add/Remove Programs entry: {}", e))?;
    let installer = install_dir.join(INSTALLER_EXE);
    let launcher = install_dir.join(LAUNCHER_EXE);
    let set = |name: &str, value: &str| key.set_value(name, &value.to_string()).map_err(|e| format!("Registry write {} failed: {}", name, e));
    set("DisplayName", APP_NAME)?;
    set("DisplayVersion", version)?;
    set("Publisher", "Austin Hicks")?;
    set("InstallLocation", &install_dir.to_string_lossy())?;
    set("DisplayIcon", &launcher.to_string_lossy())?;
    set("UninstallString", &format!("\"{}\" --uninstall", installer.to_string_lossy()))?;
    set("URLInfoAbout", &github_page_url())?;
    key.set_value("NoModify", &1u32).map_err(|e| e.to_string())?;
    key.set_value("NoRepair", &1u32).map_err(|e| e.to_string())?;
    Ok(())
}

pub fn create_all(install_dir: &Path) -> Result<(), String> {
    let target = install_dir.join(LAUNCHER_EXE);
    let desc = &format!("Start Grim Dawn with the {} screen-reader mod", APP_NAME);
    if let Some(lnk) = desktop_shortcut() {
        write_lnk(&lnk, &target, install_dir, desc)?;
    }
    if let Some(lnk) = start_menu_shortcut() {
        if let Some(parent) = lnk.parent() {
            let _ = std::fs::create_dir_all(parent);
        }
        write_lnk(&lnk, &target, install_dir, desc)?;
    }
    let version = std::fs::read_to_string(install_dir.join(super::paths::VERSION_FILE))
        .ok()
        .and_then(|t| t.lines().next().map(|l| l.trim().to_string()))
        .unwrap_or_else(|| "unknown".into());
    register_uninstall(install_dir, &version)
}

pub fn remove_all() {
    for lnk in [desktop_shortcut(), start_menu_shortcut()].into_iter().flatten() {
        let _ = std::fs::remove_file(lnk);
    }
    use winreg::enums::HKEY_CURRENT_USER;
    use winreg::RegKey;
    let _ = RegKey::predef(HKEY_CURRENT_USER)
        .delete_subkey_all(format!("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{}", UNINSTALL_KEY));
}
