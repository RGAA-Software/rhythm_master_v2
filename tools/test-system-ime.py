"""Exercise Microsoft Pinyin through real OS keys in the visible Studio test host.

Run through verify_windows.py. This requires an interactive Windows desktop and
an installed Simplified Chinese keyboard. No paste or Unicode injection is used.
"""

import argparse
import ctypes as ct
from ctypes import wintypes as wt
import json
from pathlib import Path
import subprocess
import time
import uuid


class MouseInput(ct.Structure):
    _fields_ = [('dx', wt.LONG), ('dy', wt.LONG), ('mouse_data', wt.DWORD),
                ('flags', wt.DWORD), ('time', wt.DWORD), ('extra', ct.c_size_t)]


class KeyboardInput(ct.Structure):
    _fields_ = [('vk', wt.WORD), ('scan', wt.WORD), ('flags', wt.DWORD),
                ('time', wt.DWORD), ('extra', ct.c_size_t)]


class HardwareInput(ct.Structure):
    _fields_ = [('message', wt.DWORD), ('low', wt.WORD), ('high', wt.WORD)]


class InputData(ct.Union):
    _fields_ = [('mouse', MouseInput), ('keyboard', KeyboardInput), ('hardware', HardwareInput)]


class Input(ct.Structure):
    _fields_ = [('type', wt.DWORD), ('data', InputData)]


class Keyboard:
    """Test-only Win32 boundary; borrowed window/IME handles stay in this adapter."""

    def __init__(self, pid):
        self.user = ct.WinDLL('user32', use_last_error=True)
        self.imm = ct.WinDLL('imm32', use_last_error=True)
        self.user.GetWindowThreadProcessId.argtypes = [wt.HWND, ct.POINTER(wt.DWORD)]
        self.user.GetWindowThreadProcessId.restype = wt.DWORD
        self.user.GetKeyboardLayout.argtypes = [wt.DWORD]
        self.user.GetKeyboardLayout.restype = wt.HANDLE
        self.user.GetKeyboardLayoutList.argtypes = [ct.c_int, ct.POINTER(wt.HANDLE)]
        self.user.SendMessageW.argtypes = [wt.HWND, wt.UINT, ct.c_size_t, ct.c_ssize_t]
        self.user.SendMessageW.restype = ct.c_ssize_t
        self.user.SetForegroundWindow.argtypes = [wt.HWND]
        self.user.GetForegroundWindow.restype = wt.HWND
        self.user.GetWindowRect.argtypes = [wt.HWND, ct.POINTER(wt.RECT)]
        self.user.IsWindowVisible.argtypes = [wt.HWND]
        self.user.GetWindowTextW.argtypes = [wt.HWND, wt.LPWSTR, ct.c_int]
        self.user.IsWindow.argtypes = [wt.HWND]
        self.user.SendInput.argtypes = [wt.UINT, ct.POINTER(Input), ct.c_int]
        self.imm.ImmGetContext.argtypes = [wt.HWND]
        self.imm.ImmGetContext.restype = wt.HANDLE
        self.imm.ImmReleaseContext.argtypes = [wt.HWND, wt.HANDLE]
        self.imm.ImmSetOpenStatus.argtypes = [wt.HANDLE, wt.BOOL]
        self.imm.ImmGetConversionStatus.argtypes = [wt.HANDLE, ct.POINTER(wt.DWORD),
                                                   ct.POINTER(wt.DWORD)]
        self.imm.ImmSetConversionStatus.argtypes = [wt.HANDLE, wt.DWORD, wt.DWORD]
        found = []
        callback_type = ct.WINFUNCTYPE(wt.BOOL, wt.HWND, ct.c_ssize_t)

        @callback_type
        def visit(window, _):
            owner = wt.DWORD()
            self.user.GetWindowThreadProcessId(window, ct.byref(owner))
            title = ct.create_unicode_buffer(256)
            self.user.GetWindowTextW(window, title, len(title))
            if (owner.value == pid and self.user.IsWindowVisible(window)
                    and title.value == 'Rhythm Master'):
                found.append(window)
            return True

        self.user.EnumWindows(visit, 0)
        if len(found) != 1:
            raise RuntimeError(f'Expected one visible test-process window, found {len(found)}')
        self.window = found[0]
        self.thread = self.user.GetWindowThreadProcessId(self.window, None)
        self.original_layout = self.user.GetKeyboardLayout(self.thread)

    def select_pinyin(self):
        count = self.user.GetKeyboardLayoutList(0, None)
        layouts = (wt.HANDLE * count)()
        self.user.GetKeyboardLayoutList(count, layouts)
        chinese = [layout for layout in layouts if layout & 0xffff == 0x0804]
        if not chinese:
            raise RuntimeError('Simplified Chinese IME is not installed')
        self.user.SetForegroundWindow(self.window)
        self.user.SendMessageW(self.window, 0x0050, 0, chinese[0])
        time.sleep(0.5)
        if self.user.GetKeyboardLayout(self.thread) & 0xffff != 0x0804:
            raise RuntimeError('Test window did not switch to Chinese IME')
        context = self.imm.ImmGetContext(self.window)
        if context:
            try:
                self.imm.ImmSetOpenStatus(context, True)
                conversion, sentence = wt.DWORD(), wt.DWORD()
                if self.imm.ImmGetConversionStatus(context, ct.byref(conversion), ct.byref(sentence)):
                    self.imm.ImmSetConversionStatus(context, conversion.value | 1, sentence.value)
            finally:
                self.imm.ImmReleaseContext(self.window, context)

    def key(self, vk):
        if self.user.GetForegroundWindow() != self.window:
            raise RuntimeError('Test host lost foreground; refusing to type into another app')
        for flags in (0, 2):
            event = Input(type=1, data=InputData(keyboard=KeyboardInput(vk=vk, flags=flags)))
            if self.user.SendInput(1, ct.byref(event), ct.sizeof(Input)) != 1:
                raise ct.WinError(ct.get_last_error())
            time.sleep(0.06)

    def capture(self, ffmpeg, path):
        rect = wt.RECT()
        if not self.user.GetWindowRect(self.window, ct.byref(rect)):
            raise ct.WinError(ct.get_last_error())
        subprocess.run([str(ffmpeg), '-hide_banner', '-loglevel', 'error', '-y',
                        '-f', 'gdigrab', '-framerate', '1', '-offset_x', str(rect.left),
                        '-offset_y', str(rect.top), '-video_size',
                        f'{rect.right - rect.left}x{rect.bottom - rect.top}', '-i', 'desktop',
                        '-frames:v', '1', '-update', '1', str(path)], check=True, timeout=15)

    def restore(self):
        if self.user.IsWindow(self.window):
            self.user.SendMessageW(self.window, 0x0050, 0, self.original_layout)


def wait_file(path, process, seconds=30):
    deadline = time.monotonic() + seconds
    while not path.exists():
        if process.poll() is not None or time.monotonic() > deadline:
            raise RuntimeError(f'Test host did not produce {path.name}; inspect host.log')
        time.sleep(0.1)


def main():
    # gdigrab uses physical desktop pixels; prevent virtualized window bounds
    # from cropping an unrelated region on a scaled Windows desktop.
    user = ct.WinDLL('user32', use_last_error=True)
    user.SetProcessDpiAwarenessContext.argtypes = [wt.HANDLE]
    if not user.SetProcessDpiAwarenessContext(wt.HANDLE(-4)):
        raise ct.WinError(ct.get_last_error())
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--resources', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--ffmpeg', type=Path, default=Path(
        'C:/source/vcpkg/installed/x64-windows-static-release/tools/ffmpeg/ffmpeg.exe'))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = args.output.resolve() / uuid.uuid4().hex
    executable = args.executable.resolve()
    if not output.is_relative_to(root / 'out') or not executable.is_relative_to(root / 'out'):
        raise ValueError('Test executable and evidence must be in project output')
    output.mkdir(parents=True)
    print(f'System IME evidence: {output}', flush=True)
    keyboard = None
    with (output / 'host.log').open('w', encoding='utf-8') as log:
        process = subprocess.Popen([str(executable), str(args.resources.resolve()),
                                    str(root / 'third_party/assets/noto-cjk/NotoSansCJKsc-Regular.otf'),
                                    str(root / 'third_party/notices/noto-cjk/LICENSE.txt'), str(output)],
                                   stdout=log, stderr=subprocess.STDOUT)
        try:
            wait_file(output / 'ready.json', process)
            keyboard = Keyboard(process.pid)
            keyboard.select_pinyin()
            for letter in 'NIHAO':
                keyboard.key(ord(letter))
            time.sleep(0.8)
            keyboard.capture(args.ffmpeg, output / 'candidate.png')
            keyboard.key(0x20)
            time.sleep(0.5)
            keyboard.capture(args.ffmpeg, output / 'committed-draft.png')
            (output / 'draft.flag').write_text('', encoding='utf-8')
            wait_file(output / 'draft.json', process, 5)
            keyboard.key(0x0d)
            time.sleep(0.3)
            keyboard.restore()
            keyboard = None
            (output / 'finish.flag').write_text('', encoding='utf-8')
            if process.wait(timeout=45) != 0:
                raise RuntimeError('System IME Studio validation failed; inspect host.log')
            result = json.loads((output / 'result.json').read_text(encoding='utf-8'))
            if result.get('text') != '你好' or not result.get('save_publish_reopen'):
                raise RuntimeError('Unexpected committed IME output')
            print('Actual OS Pinyin, draft isolation, save/publication/reopen passed; review screenshots')
        finally:
            if keyboard:
                keyboard.restore()
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)


if __name__ == '__main__':
    main()
