"""Client of linuxloader's agent mode (--agent, --agent-show): the game driven frame by frame over stdin/stdout.

The protocol is described at the top of src/loader/agent/agent.c; it is the one of sm2-emu's and Supermodel's agent
modes. Example:

    from llagent import LLAgent
    with LLAgent("build-mingw/linuxloader.exe", "games/outrun2spdx/disk0/Jennifer/Jennifer") as game:
        game.step(600)
        game.set("Coin", 1); game.step(5); game.release("Coin")
        game.shot("outrun.bmp")
        print(game.tel())
"""
import json
import os
import subprocess


class AgentError(RuntimeError):
    pass


class LLAgent:
    def __init__(self, exe, elf, show=False, config=None, cwd=None, log=None, extra_args=()):
        """Start the loader in agent mode. cwd: the folder it runs in (the exe's by default), where controls.ini and
        linuxloader.ini are looked for. log: a file for the game's output (stderr), os.devnull by default."""
        exe = os.path.abspath(exe)
        args = [exe, "--agent-show" if show else "--agent"]
        if config:
            args += ["-c", os.path.abspath(config)]
        args += list(extra_args) + [os.path.abspath(elf)]
        self._log = open(log or os.devnull, "wb")
        self.process = subprocess.Popen(args, cwd=cwd or os.path.dirname(exe), stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, stderr=self._log, bufsize=0)
        line = self._line()
        if not line.startswith("ready "):
            raise AgentError("expected ready, got %r" % line)
        _, self.game, width, height = line.split()
        self.width, self.height = int(width), int(height)

    # --- plumbing

    def _line(self):
        line = self.process.stdout.readline()
        if not line:
            raise AgentError("the loader closed the connection (exit code %s)" % self.process.poll())
        return line.decode("utf-8", "replace").rstrip("\r\n")

    def _read(self, count):
        data = bytearray()
        while len(data) < count:
            chunk = self.process.stdout.read(count - len(data))
            if not chunk:
                raise AgentError("the loader closed the connection")
            data += chunk
        return bytes(data)

    def send(self, command):
        self.process.stdin.write((command + "\n").encode())
        self.process.stdin.flush()

    def command(self, command):
        """One command, its one-line answer; an "err" answer raises AgentError."""
        self.send(command)
        line = self._line()
        if line.startswith("err"):
            raise AgentError("%s: %s" % (command, line))
        return line

    def _counted(self, command, word):
        line = self.command(command)
        head, count = line.split()[:2]
        if head != word:
            raise AgentError("%s: %r" % (command, line))
        return [self._line() for _ in range(int(count))]

    # --- the commands

    def step(self, frames=1, steer=None, accel=0.0, brake=0.0):
        """Run frames; with steer (-1 left .. 1 right, as sm2-emu's positional step), hold the wheel and pedals.
        Returns (frame, the drive board bytes written meanwhile)."""
        if steer is None:
            line = self.command("step %d" % frames)
        else:
            line = self.command("step %d %f %f %f" % (frames, steer, accel, brake))
        words = line.split()
        return int(words[1]), bytes(int(b, 16) for b in words[2:])

    def set(self, name, value):
        self.command("set %s %s" % (name, value))

    def setraw(self, name, raw):
        self.command("setraw %s %d" % (name, raw))

    def release(self, name="all"):
        self.command("release %s" % name)

    def press(self, name, frames=5):
        """Press a switch for some frames, then release it."""
        self.set(name, 1)
        self.step(frames)
        self.release(name)
        self.step(frames)

    def get(self, name):
        return int(self.command("get %s" % name).split()[1])

    def inputs(self):
        return [tuple(line.split()) for line in self._counted("inputs", "inputs")]

    def ffb(self):
        """[(frame, effect, force)] since the last call."""
        out = []
        for line in self._counted("ffb", "ffb"):
            frame, _, effect, force = line.split()
            out.append((int(frame), effect, float(force)))
        return out

    def servo(self, on=None):
        """servo(True/False) records the drive board frames or stops; servo() reads [(frame, bytes)]."""
        if on is not None:
            self.command("servo %s" % ("on" if on else "off"))
            return None
        out = []
        for line in self._counted("servo", "servo"):
            words = line.split()
            out.append((int(words[0]), bytes(int(b, 16) for b in words[1:])))
        return out

    def frame(self):
        return int(self.command("frame").split()[1])

    def obs(self, width, height):
        """The picture scaled to width x height, RGB bytes, top row first."""
        line = self.command("obs %d %d" % (width, height))
        _, w, h = line.split()
        return self._read(int(w) * int(h) * 3)

    def shot(self, path):
        """The game's picture, full size."""
        self.command("shot %s" % os.path.abspath(path))

    def screen(self, path):
        """What the window shows: the picture placed by the aspect ratio and the display mode."""
        self.command("screen %s" % os.path.abspath(path))

    def mem(self, address, length):
        line = self.command("mem %x %x" % (address, length))
        count = int(line.split()[1])
        return self._read(count) if count else b""

    def tel(self, keys=None):
        return json.loads(self.command("tel %s" % ",".join(keys) if keys else "tel"))

    def perf(self, frames=600):
        return self.command("perf %d" % frames)

    def quit(self):
        try:
            self.command("quit")
        except AgentError:
            pass
        try:
            self.process.wait(10)
        except subprocess.TimeoutExpired:
            self.process.kill()
        self._log.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        if self.process.poll() is None:
            self.quit()
        else:
            self._log.close()
