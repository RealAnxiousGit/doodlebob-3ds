# DoodleBob 3DS Converter: pick your copy of the game, get the 3DS files (SD card folder, .3dsx and .cia).
import os
import queue
import subprocess
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

import convert

TITLE = 'DoodleBob 3DS Converter'
INTRO = ("Turns your copy of DoodleBob and the Magic Pencil DX: Doodler's Cut into a 3DS version: "
         "a folder for your SD card, plus DoodleBob.cia and DoodleBob.3dsx.\n\n"
         "Choose the game's .exe as you downloaded it (or data.win from the game folder).")


class App:
    def __init__(self, root):
        self.root = root
        self.events = queue.Queue()
        self.game = tk.StringVar()
        self.out = tk.StringVar()
        self.busy = False
        self.result = None
        root.title(f'{TITLE} {".".join(map(str, convert.VERSION))}')
        root.resizable(False, False)
        pad = {'padx': 14}

        ttk.Label(root, text=TITLE, font=('Segoe UI', 14, 'bold')).pack(anchor='w', pady=(14, 4), **pad)
        ttk.Label(root, text=INTRO, wraplength=520, justify='left').pack(anchor='w', **pad)

        box = ttk.Frame(root)
        box.pack(fill='x', pady=(12, 0), **pad)
        ttk.Label(box, text='Game').grid(row=0, column=0, sticky='w')
        ttk.Entry(box, textvariable=self.game, width=58, state='readonly').grid(row=0, column=1, padx=6)
        self.pick_btn = ttk.Button(box, text='Choose...', command=self.pick_game)
        self.pick_btn.grid(row=0, column=2)
        ttk.Label(box, text='Save to').grid(row=1, column=0, sticky='w', pady=(6, 0))
        ttk.Entry(box, textvariable=self.out, width=58, state='readonly').grid(row=1, column=1, padx=6, pady=(6, 0))
        self.out_btn = ttk.Button(box, text='Change...', command=self.pick_out)
        self.out_btn.grid(row=1, column=2, pady=(6, 0))

        self.bar = ttk.Progressbar(root, length=520, maximum=1000)
        self.bar.pack(pady=(14, 4), **pad)
        self.status = ttk.Label(root, text='Choose the game to start.', wraplength=520, justify='left')
        self.status.pack(anchor='w', **pad)

        buttons = ttk.Frame(root)
        buttons.pack(fill='x', pady=14, **pad)
        self.open_btn = ttk.Button(buttons, text='Open folder', command=self.open_folder, state='disabled')
        self.open_btn.pack(side='right')
        self.go_btn = ttk.Button(buttons, text='Convert', command=self.start, state='disabled')
        self.go_btn.pack(side='right', padx=(0, 8))
        root.after(100, self.poll)

    def pick_game(self):
        path = filedialog.askopenfilename(
            title="Choose DoodleBob's .exe (or data.win)",
            filetypes=[('Game (.exe or data.win)', '*.exe *.win'), ('All files', '*.*')])
        if not path:
            return
        self.game.set(os.path.normpath(path))
        if not self.out.get():
            self.out.set(os.path.join(os.path.dirname(os.path.normpath(path)), 'DoodleBob 3DS'))
        self.go_btn.config(state='normal')
        self.status.config(text='Ready. Press Convert.')

    def pick_out(self):
        path = filedialog.askdirectory(title='Where should the 3DS files go?')
        if path:
            self.out.set(os.path.join(os.path.normpath(path), 'DoodleBob 3DS'))

    def start(self):
        if self.busy:
            return
        self.busy = True
        self.result = None
        for b in (self.go_btn, self.pick_btn, self.out_btn, self.open_btn):
            b.config(state='disabled')
        self.bar['value'] = 0
        threading.Thread(target=self.work, args=(self.game.get(), self.out.get()), daemon=True).start()

    def work(self, game, out):
        try:
            result = convert.convert(game, out, log=lambda s: self.events.put(('log', s)),
                                     progress=lambda f: self.events.put(('progress', f)))
            self.events.put(('done', result))
        except convert.ConvertError as e:
            self.events.put(('error', str(e)))
        except Exception as e:  # anything unexpected: show it rather than vanish
            self.events.put(('error', f'Something went wrong: {e!r}'))

    def poll(self):
        try:
            while True:
                kind, value = self.events.get_nowait()
                if kind == 'log':
                    self.status.config(text=value.splitlines()[-1] if value else '')
                elif kind == 'progress':
                    self.bar['value'] = int(value * 1000)
                elif kind == 'done':
                    self.finish(value)
                elif kind == 'error':
                    self.finish(None)
                    self.status.config(text=value)
                    messagebox.showerror(TITLE, value)
        except queue.Empty:
            pass
        self.root.after(100, self.poll)

    def finish(self, result):
        self.busy = False
        self.result = result
        for b in (self.go_btn, self.pick_btn, self.out_btn):
            b.config(state='normal')
        if result:
            self.open_btn.config(state='normal')
            self.status.config(text='Done! Copy "SD card\\3ds" to your SD card and install DoodleBob.cia '
                                    '(see "How to install.txt").')

    def open_folder(self):
        if os.path.isdir(self.out.get()):
            subprocess.Popen(['explorer', self.out.get()])


def main():
    import sys
    if len(sys.argv) == 4 and sys.argv[1] == '--convert':
        # Unattended run (for testing the packed .exe): writes its messages to <output>\convert.log.
        os.makedirs(sys.argv[3], exist_ok=True)
        with open(os.path.join(sys.argv[3], 'convert.log'), 'w', encoding='utf-8') as log:
            try:
                convert.convert(sys.argv[2], sys.argv[3], log=lambda s: (log.write(s + '\n'), log.flush()))
            except convert.ConvertError as e:
                log.write(f'Error: {e}\n')
                sys.exit(1)
        return
    try:  # sharp text on scaled displays
        import ctypes
        ctypes.windll.shcore.SetProcessDpiAwareness(1)
    except Exception:
        pass
    root = tk.Tk()
    try:
        ttk.Style().theme_use('vista')
    except tk.TclError:
        pass
    App(root)
    root.mainloop()


if __name__ == '__main__':
    main()
