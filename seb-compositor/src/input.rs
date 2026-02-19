//! Keyboard input: block VT switch, Super, Alt+Tab, etc.

use smithay::backend::input::{Event, InputEvent, KeyState, KeyboardKeyEvent};
use smithay::input::keyboard::FilterResult;
use smithay::utils::SERIAL_COUNTER;
use xkbcommon::xkb::Keysym;

use crate::state::KioskState;

/// Process input events; only keyboard is handled for now.
pub fn process_input_event<I: smithay::backend::input::InputBackend>(
    state: &mut KioskState,
    event: InputEvent<I>,
) where
    I::KeyboardKeyEvent: KeyboardKeyEvent<I>,
{
    match event {
        InputEvent::Keyboard { event } => {
            state.on_keyboard_key::<I>(event);
        }
        _ => {}
    }
}

impl KioskState {
    pub fn on_keyboard_key<I: smithay::backend::input::InputBackend>(
        &mut self,
        event: I::KeyboardKeyEvent,
    ) where
        I::KeyboardKeyEvent: KeyboardKeyEvent<I>,
    {
        let keyboard = match self.keyboard.as_ref() {
            Some(k) => k.clone(),
            None => return,
        };
        let suppressed_keys = self.suppressed_keys.clone();
        let serial = SERIAL_COUNTER.next_serial();
        let time = event.time() as u32;
        let surface = self
            .single_surface
            .as_ref()
            .map(|t| t.wl_surface().clone());
        let key_state = event.state();
        keyboard.set_focus(self, surface, serial);
        let _ = keyboard.input(
            self,
            event.key_code(),
            key_state,
            serial,
            time,
            move |_, modifiers, keysym_handle| {
                let keysym = keysym_handle
                    .raw_syms()
                    .into_iter()
                    .next()
                    .unwrap_or_else(|| xkbcommon::xkb::Keysym::from(xkbcommon::xkb::keysyms::KEY_NoSymbol));
                if key_state == KeyState::Pressed {
                    if is_blocked(modifiers, keysym) {
                        tracing::warn!(keysym = ?keysym, "Blocked key");
                        suppressed_keys.borrow_mut().push(keysym);
                        return FilterResult::Intercept(());
                    }
                } else {
                    let mut suppressed = suppressed_keys.borrow_mut();
                    if suppressed.iter().any(|&k| k == keysym) {
                        suppressed.retain(|&k| k != keysym);
                        return FilterResult::Intercept(());
                    }
                }
                FilterResult::Forward
            },
        );
    }
}

fn is_blocked(modifiers: &smithay::input::keyboard::ModifiersState, keysym: Keysym) -> bool {
    use xkbcommon::xkb::keysyms;
    let k: u32 = keysym.into();

    // VT switch keys (XF86Switch_VT_1 .. XF86Switch_VT_12)
    if (keysyms::KEY_XF86Switch_VT_1..=keysyms::KEY_XF86Switch_VT_12).contains(&k) {
        return true;
    }
    // Alt + F1..F12
    if modifiers.alt {
        if (keysyms::KEY_F1..=keysyms::KEY_F12).contains(&k) {
            return true;
        }
        if k == keysyms::KEY_Tab {
            return true;
        }
    }
    // Ctrl+Alt + F1..F12
    if modifiers.ctrl && modifiers.alt {
        if (keysyms::KEY_F1..=keysyms::KEY_F12).contains(&k) {
            return true;
        }
        if k == keysyms::KEY_Delete {
            return true;
        }
    }
    // Super (logo)
    if modifiers.logo {
        return true;
    }
    // Print / SysRq
    if k == keysyms::KEY_Print || k == keysyms::KEY_Sys_Req {
        return true;
    }
    false
}
