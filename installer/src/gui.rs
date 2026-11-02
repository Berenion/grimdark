//! The window: a status line, a game line, a log box that narrates every step, and the buttons
//! (Install / Update with a version picker, Install from file, Uninstall, Controller controls). Native wx controls, so a
//! screen reader reads it as an ordinary dialog. Downloads run on a worker thread; a timer drains its
//! progress messages into the log so the window never goes "not responding".
use std::cell::RefCell;
use std::path::PathBuf;
use std::rc::Rc;
use std::sync::mpsc;

use wxdragon::prelude::*;

use crate::core::install::Progress;
use crate::core::paths::{data_dir, install_dir, APP_NAME};
use crate::core::{detect, github, install, uninstall};

/// The controller controls list: the same file the mod embeds and shows from its quick menu.
const CONTROLLER_CONTROLS: &str = include_str!("../../data/controller_controls.txt");

/// What the worker thread sends back.
enum Msg {
    Progress(Progress),
    Done(Result<String, String>), // Ok(what was installed) / Err(message)
}

struct Ui {
    frame: Frame,
    status: StaticText,
    log: TextCtrl,
    install_btn: Button,
    file_btn: Button,
    uninstall_btn: Button,
}

impl Ui {
    fn log(&self, msg: &str) {
        let cur = self.log.get_value();
        if cur.is_empty() {
            self.log.set_value(msg);
        } else {
            self.log.set_value(&format!("{}\n{}", cur, msg));
        }
    }

    fn busy(&self, busy: bool) {
        self.install_btn.enable(!busy);
        self.file_btn.enable(!busy);
        self.uninstall_btn.enable(!busy && detect::is_installed());
    }

    /// Status line + button labels from what is installed and what GitHub offers.
    fn refresh(&self, catalog: &Option<github::Catalog>) {
        let installed = detect::is_installed();
        let version = detect::installed_version();
        self.uninstall_btn.enable(installed);
        self.file_btn.enable(true);
        let latest = catalog.as_ref().and_then(|c| c.latest().map(|r| r.tag_name.clone()));
        let has_any = catalog.as_ref().map(|c| !c.versions.is_empty() || c.ci.is_some()).unwrap_or(false);
        self.install_btn.enable(has_any);
        let line = match (installed, &latest) {
            (false, Some(l)) => {
                self.install_btn.set_label("Install");
                format!("Ready to install {} {}.", APP_NAME, l)
            }
            (false, None) => {
                self.install_btn.set_label("Install");
                format!("{} is not installed.", APP_NAME)
            }
            (true, Some(l)) if github::is_up_to_date(version.as_deref(), l) => {
                self.install_btn.set_label("Install");
                format!("{} {} is installed and up to date.", APP_NAME, version.as_deref().unwrap_or("?"))
            }
            (true, Some(l)) => {
                self.install_btn.set_label("Update");
                format!("Update available: {} -> {}.", version.as_deref().unwrap_or("unknown"), l)
            }
            (true, None) => {
                self.install_btn.set_label("Install");
                format!("{} {} is installed.", APP_NAME, version.as_deref().unwrap_or("?"))
            }
        };
        self.status.set_label(&line);
    }

    fn finish(&self, result: Result<String, String>, verb: &str) {
        match result {
            Ok(what) => {
                self.log(&format!("{} {} ({}).", APP_NAME, verb, what));
                let text = if verb == "installed" {
                    format!("{} {} ({}). A \"{}\" shortcut is on your desktop and in the Start Menu; run it to play.", APP_NAME, verb, what, APP_NAME)
                } else {
                    format!("{} {}.", APP_NAME, verb)
                };
                MessageDialog::builder(&self.frame, &text, APP_NAME)
                    .with_style(MessageDialogStyle::OK | MessageDialogStyle::IconInformation)
                    .build()
                    .show_modal();
            }
            Err(e) => {
                self.log(&format!("Error: {}", e));
                MessageDialog::builder(&self.frame, &e, APP_NAME)
                    .with_style(MessageDialogStyle::OK | MessageDialogStyle::IconError)
                    .build()
                    .show_modal();
            }
        }
    }
}

pub fn run() {
    wxdragon::main(|_app| {
        let frame = Frame::builder().with_title(&format!("{} installer", APP_NAME)).with_size(Size::new(680, 520)).build();
        let panel = Panel::builder(&frame).build();
        let sizer = BoxSizer::builder(Orientation::Vertical).build();

        let status = StaticText::builder(&panel).with_label("Checking...").build();
        let game_line = StaticText::builder(&panel).with_label("").build();
        let log = TextCtrl::builder(&panel)
            .with_style(TextCtrlStyle::MultiLine | TextCtrlStyle::ReadOnly | TextCtrlStyle::WordWrap)
            .build();
        let buttons = BoxSizer::builder(Orientation::Horizontal).build();
        let install_btn = Button::builder(&panel).with_label("Install").build();
        let file_btn = Button::builder(&panel).with_label("Install from file...").build();
        let uninstall_btn = Button::builder(&panel).with_label("Uninstall").build();
        let controls_btn = Button::builder(&panel).with_label("Controller controls").build();
        let close_btn = Button::builder(&panel).with_label("Close").build();
        for b in [&install_btn, &file_btn, &uninstall_btn, &controls_btn, &close_btn] {
            buttons.add(b, 0, SizerFlag::All, 4);
        }
        sizer.add(&status, 0, SizerFlag::Expand | SizerFlag::All, 8);
        sizer.add(&game_line, 0, SizerFlag::Expand | SizerFlag::Left | SizerFlag::Right, 8);
        sizer.add(&log, 1, SizerFlag::Expand | SizerFlag::All, 8);
        sizer.add_sizer(&buttons, 0, SizerFlag::Expand | SizerFlag::All, 4);
        panel.set_sizer(sizer, true);

        let ui = Rc::new(Ui { frame: frame.clone(), status, log, install_btn: install_btn.clone(), file_btn: file_btn.clone(), uninstall_btn: uninstall_btn.clone() });
        ui.busy(true);

        ui.log(&format!("Install folder: {}", install_dir().display()));
        ui.log(&format!("Settings and log folder (kept across updates): {}", data_dir().display()));
        match detect::detect_game_exe() {
            Some(exe) => {
                game_line.set_label(&format!("Grim Dawn: {}", exe.display()));
                ui.log(&format!("Grim Dawn found: {}", exe.display()));
            }
            None => {
                game_line.set_label("Grim Dawn was not found through Steam.");
                ui.log("Grim Dawn was not found through Steam. You can still install; the launcher will ask for a game_path.txt (see the README).");
            }
        }
        if let Some(v) = detect::installed_version() {
            ui.log(&format!("Installed version: {}", v));
        }

        // The release list, fetched before the window shows (a few hundred ms).
        let catalog: Rc<RefCell<Option<github::Catalog>>> = Rc::new(RefCell::new(None));
        match github::fetch_catalog() {
            Ok(c) => {
                match c.latest() {
                    Some(r) => ui.log(&format!("Newest release: {}", r.tag_name)),
                    None => ui.log("No tagged release is published yet."),
                }
                if c.ci.is_some() {
                    ui.log("A CI build is also available (last in the version list).");
                }
                *catalog.borrow_mut() = Some(c);
            }
            Err(e) => {
                ui.log(&format!("Could not check GitHub for releases: {}", e));
                ui.log("Online install is unavailable; \"Install from file\" still works.");
            }
        }
        ui.refresh(&catalog.borrow());

        // Worker -> UI channel, drained by a timer.
        let (tx, rx) = mpsc::channel::<Msg>();
        let rx = Rc::new(RefCell::new(rx));
        let pending_verb: Rc<RefCell<&'static str>> = Rc::new(RefCell::new("installed"));
        {
            let ui = ui.clone();
            let rx = rx.clone();
            let catalog = catalog.clone();
            let pending_verb = pending_verb.clone();
            let timer = Timer::new(&frame);
            timer.on_tick(move |_| {
                while let Ok(msg) = rx.borrow().try_recv() {
                    match msg {
                        Msg::Progress(Progress::Status(s)) => {
                            ui.status.set_label(&s);
                            ui.log(&s);
                        }
                        Msg::Progress(Progress::Percent(p)) => {
                            ui.status.set_label(&format!("Downloading... {}%", p));
                        }
                        Msg::Done(result) => {
                            ui.busy(false);
                            ui.finish(result, &pending_verb.borrow());
                            ui.refresh(&catalog.borrow());
                        }
                    }
                }
            });
            timer.start(100, false);
            std::mem::forget(timer); // lives as long as the window
        }

        // Install / Update: pick a version (tagged releases newest first; the CI build last, on purpose).
        {
            let ui = ui.clone();
            let catalog = catalog.clone();
            let tx = tx.clone();
            let pending_verb = pending_verb.clone();
            install_btn.on_click(move |_| {
                let Some(cat) = catalog.borrow().clone() else { return };
                let mut labels: Vec<String> = cat
                    .versions
                    .iter()
                    .map(|r| if r.prerelease { format!("{} (pre-release)", r.tag_name) } else { r.tag_name.clone() })
                    .collect();
                if let Some(ci) = &cat.ci {
                    labels.push(format!("latest successful CI build ({}) -- untested, may be broken", github::ci_build_label(ci)));
                }
                if labels.is_empty() {
                    return;
                }
                let refs: Vec<&str> = labels.iter().map(|s| s.as_str()).collect();
                let dialog = SingleChoiceDialog::builder(&ui.frame, "Select a version to install:", "Choose version", &refs).build();
                if dialog.show_modal() != ID_OK {
                    return;
                }
                let sel = dialog.get_selection();
                if sel < 0 {
                    return;
                }
                let sel = sel as usize;
                let rel = if sel < cat.versions.len() { &cat.versions[sel] } else { cat.ci.as_ref().unwrap() };
                if !rel.body.is_empty() && sel < cat.versions.len() {
                    let notes = MessageDialog::builder(
                        &ui.frame,
                        &format!("Release notes for {}:\n\n{}\n\nInstall it?", rel.tag_name, rel.body),
                        &format!("Install {}", rel.tag_name),
                    )
                    .with_style(MessageDialogStyle::YesNo | MessageDialogStyle::IconQuestion)
                    .build();
                    if notes.show_modal() != ID_YES {
                        return;
                    }
                }
                let Some(asset) = github::zip_asset(rel) else {
                    ui.log("Error: that release has no zip asset.");
                    return;
                };
                let url = asset.browser_download_url.clone();
                let label = if sel < cat.versions.len() { rel.tag_name.clone() } else { format!("latest CI build ({})", github::ci_build_label(rel)) };
                *pending_verb.borrow_mut() = "installed";
                ui.busy(true);
                ui.log(&format!("Installing {}...", label));
                let tx = tx.clone();
                std::thread::spawn(move || {
                    let tx2 = tx.clone();
                    let result = install::download_and_install(&url, &move |p| { let _ = tx2.send(Msg::Progress(p)); });
                    let _ = tx.send(Msg::Done(result.map(|_| label)));
                });
            });
        }

        // Install from a local zip (testers, offline).
        {
            let ui = ui.clone();
            let tx = tx.clone();
            let pending_verb = pending_verb.clone();
            file_btn.on_click(move |_| {
                let dialog = FileDialog::builder(&ui.frame)
                    .with_message(&format!("Select the {} zip", APP_NAME))
                    .with_wildcard("Zip files (*.zip)|*.zip")
                    .with_style(FileDialogStyle::Open | FileDialogStyle::FileMustExist)
                    .build();
                if dialog.show_modal() != ID_OK {
                    return;
                }
                let Some(path) = dialog.get_path() else { return };
                *pending_verb.borrow_mut() = "installed";
                ui.busy(true);
                ui.log(&format!("Installing from {}...", path));
                let tx = tx.clone();
                std::thread::spawn(move || {
                    let tx2 = tx.clone();
                    let result = install::install_from_file(&PathBuf::from(&path), &move |p| { let _ = tx2.send(Msg::Progress(p)); });
                    let _ = tx.send(Msg::Done(result.map(|_| "from file".to_string())));
                });
            });
        }

        // Uninstall (keeps the settings).
        {
            let ui = ui.clone();
            let catalog = catalog.clone();
            uninstall_btn.on_click(move |_| {
                let confirm = MessageDialog::builder(
                    &ui.frame,
                    &format!("Remove {}? Your settings are kept for a future reinstall.", APP_NAME),
                    &format!("Uninstall {}", APP_NAME),
                )
                .with_style(MessageDialogStyle::YesNo | MessageDialogStyle::IconQuestion)
                .build();
                if confirm.show_modal() != ID_YES {
                    return;
                }
                let result = uninstall::uninstall().map(|_| "removed".to_string());
                ui.finish(result, "uninstalled");
                ui.refresh(&catalog.borrow());
            });
        }

        // Controller controls: the list in a read-only text box (a screen reader reads it line by line), Escape or Close.
        {
            let frame = frame.clone();
            controls_btn.on_click(move |_| show_controller_controls(&frame));
        }

        {
            let frame = frame.clone();
            close_btn.on_click(move |_| {
                frame.close(true);
            });
        }

        frame.show(true);
    })
    .expect("Failed to start the installer window");
}

fn show_controller_controls(parent: &Frame) {
    let dialog = Dialog::builder(parent, "Controller controls")
        .with_style(DialogStyle::DefaultDialogStyle | DialogStyle::ResizeBorder)
        .with_size(760, 600)
        .build();
    let sizer = BoxSizer::builder(Orientation::Vertical).build();
    let text = TextCtrl::builder(&dialog)
        .with_style(TextCtrlStyle::MultiLine | TextCtrlStyle::ReadOnly | TextCtrlStyle::WordWrap)
        .build();
    text.set_value(&CONTROLLER_CONTROLS.replace("\r\n", "\n"));
    text.set_insertion_point(0);
    let close = Button::builder(&dialog).with_id(ID_CANCEL).with_label("Close").build();
    sizer.add(&text, 1, SizerFlag::Expand | SizerFlag::All, 8);
    sizer.add(&close, 0, SizerFlag::AlignRight | SizerFlag::All, 8);
    dialog.set_sizer(sizer, true);
    dialog.set_escape_id(ID_CANCEL);
    text.set_focus();
    dialog.show_modal();
    dialog.destroy();
}
