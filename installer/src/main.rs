//! GrimdarkInstaller.exe: installs a release of the Grimdark mod per user, makes the shortcuts, uninstalls.
//!
//!   GrimdarkInstaller.exe                the window
//!   GrimdarkInstaller.exe --cli          the same on the console
//!   GrimdarkInstaller.exe --uninstall    what Add/Remove Programs runs (confirm box, then remove)
#![windows_subsystem = "windows"]

mod cli;
mod core;
mod gui;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.iter().any(|a| a == "--uninstall") {
        uninstall_entry(args.iter().any(|a| a == "--from-temp"));
    } else if args.iter().any(|a| a == "--cli") {
        attach_console();
        cli::run();
    } else {
        gui::run();
    }
}

/// Add/Remove Programs runs the copy of this exe that lives INSIDE the install folder, which cannot delete
/// itself while running: re-run from a copy in %TEMP% first.
fn uninstall_entry(from_temp: bool) {
    use core::paths::{install_dir, APP_NAME};
    if !from_temp {
        if let Ok(me) = std::env::current_exe() {
            if me.starts_with(install_dir()) {
                let temp = std::env::temp_dir().join(format!("{}-uninstall.exe", APP_NAME));
                if std::fs::copy(&me, &temp).is_ok()
                    && std::process::Command::new(&temp).args(["--uninstall", "--from-temp"]).spawn().is_ok()
                {
                    return;
                }
            }
        }
    }
    let text = format!("Remove {}? Your settings are kept for a future reinstall.", APP_NAME);
    if !message_box(&text, APP_NAME, true) {
        return;
    }
    match core::uninstall::uninstall() {
        Ok(()) => { message_box(&format!("{} was removed.", APP_NAME), APP_NAME, false); }
        Err(e) => { message_box(&e, APP_NAME, false); }
    }
}

/// A plain Win32 message box (no wx needed for the two-line uninstall path). Returns true for Yes/OK.
fn message_box(text: &str, title: &str, yes_no: bool) -> bool {
    #[cfg(windows)]
    unsafe {
        use windows::core::HSTRING;
        use windows::Win32::UI::WindowsAndMessaging::{MessageBoxW, IDYES, MB_ICONQUESTION, MB_OK, MB_SETFOREGROUND, MB_YESNO};
        let style = if yes_no { MB_YESNO | MB_ICONQUESTION } else { MB_OK } | MB_SETFOREGROUND;
        let r = MessageBoxW(None, &HSTRING::from(text), &HSTRING::from(title), style);
        return !yes_no || r == IDYES;
    }
    #[cfg(not(windows))]
    {
        let _ = (text, title, yes_no);
        true
    }
}

/// Attach to the parent console so --cli can use stdin/stdout from a windows-subsystem exe.
fn attach_console() {
    #[cfg(windows)]
    unsafe {
        use windows::Win32::System::Console::{AttachConsole, ATTACH_PARENT_PROCESS};
        let _ = AttachConsole(ATTACH_PARENT_PROCESS);
    }
}
