//! Where things live. The mod is a self-contained folder (gdlaunch.exe, grimdark.dll, prism.dll, assets\)
//! installed per user under %LOCALAPPDATA%\Programs\Grimdark; its log and settings stay in
//! %LOCALAPPDATA%\Grimdark, which an install or uninstall never touches. Nothing is written to the game folder.
use std::path::PathBuf;

/// The build's identity, from the environment at compile time (CI's workflow env, or .cargo/config.toml in a
/// fork): GRIMDARK_REPO = whose releases are offered, GRIMDARK_APP_NAME = the installed name, GRIMDARK_INSTALLER_EXE =
/// the name the installer copies itself under. Unset = this repo's own values.
macro_rules! build_env {
    ($name:literal, $default:literal) => {
        match option_env!($name) {
            Some(v) if !v.is_empty() => v,
            _ => $default,
        }
    };
}
/// "owner/name" on GitHub.
pub const GITHUB_REPO: &str = build_env!("GRIMDARK_REPO", "ahicks92/grimdark");
/// The rolling pre-release CI republishes on every push to main (the "latest successful CI build" choice).
pub const CI_TAG: &str = "ci-latest";
/// The top-level folder inside the release zip (tools/package.py).
pub const ZIP_ROOT: &str = "grimdark";
/// The spoken name AND the on-disk one: the install folder, the shortcuts and the Add/Remove entry.
pub const APP_NAME: &str = build_env!("GRIMDARK_APP_NAME", "Grimdark");
pub const LAUNCHER_EXE: &str = "gdlaunch.exe";
pub const MOD_DLL: &str = "grimdark.dll";
pub const INSTALLER_EXE: &str = build_env!("GRIMDARK_INSTALLER_EXE", "GrimdarkInstaller.exe");
pub const VERSION_FILE: &str = "version.txt";
/// HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\<this> = the Add/Remove Programs entry.
pub const UNINSTALL_KEY: &str = APP_NAME;

fn local_app_data() -> PathBuf {
    dirs::data_local_dir().unwrap_or_else(|| PathBuf::from("C:\\Users\\Default\\AppData\\Local"))
}

/// %LOCALAPPDATA%\Programs\Grimdark -- the per-user programs folder (no admin, survives anything Steam does).
pub fn install_dir() -> PathBuf {
    local_app_data().join("Programs").join(APP_NAME)
}

/// The API listing of the repo's releases.
pub fn github_releases_url() -> String {
    format!("https://api.github.com/repos/{}/releases", GITHUB_REPO)
}

/// The repo's web page (the Add/Remove Programs "about" link).
pub fn github_page_url() -> String {
    format!("https://github.com/{}", GITHUB_REPO)
}

/// %LOCALAPPDATA%\Grimdark -- the mod's own log + settings.txt (kept across install/uninstall). Always this name: the
/// DLL and gdlaunch hardcode it, so a fork installed beside upstream shares it and both keep the player's settings.
pub fn data_dir() -> PathBuf {
    local_app_data().join("Grimdark")
}

pub fn launcher_path() -> PathBuf {
    install_dir().join(LAUNCHER_EXE)
}

pub fn desktop_shortcut() -> Option<PathBuf> {
    dirs::desktop_dir().map(|d| d.join(format!("{}.lnk", APP_NAME)))
}

pub fn start_menu_shortcut() -> Option<PathBuf> {
    dirs::config_dir().map(|appdata| {
        appdata.join("Microsoft").join("Windows").join("Start Menu").join("Programs").join(format!("{}.lnk", APP_NAME))
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn install_dir_is_under_programs() {
        assert!(install_dir().ends_with(format!("Programs\\{}", APP_NAME)));
        assert!(!install_dir().starts_with(data_dir()));
    }
}
