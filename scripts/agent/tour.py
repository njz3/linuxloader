"""A tour of the games in agent mode: pictures along the boot and the attract mode, a coin and Start, the telemetry
and the force feedback, for each game. Usage:

    python tour.py EXE OUT_DIR [GAME...]

GAME is a key of GAMES below (all of them by default); the games are looked for in ../../games. Each game gets a
folder OUT_DIR/GAME with its pictures (BMP, frame number in the name), tel.json, ffb.txt and the game's output.
"""
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from llagent import LLAgent, AgentError  # noqa: E402

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
GAMES = {
    # name: (program, frames of the pictures before the coin, frames after Start)
    "outrun": ("games/outrun2spdx/disk0/Jennifer/Jennifer", [600, 1800, 3000, 4200], [600, 1800]),
    "srtv": ("games/segaracetv/drive.elf", [600, 1800, 3600, 4800], [600, 1800]),
    "hummer": ("games/hummer/disk0/hummer_Master.elf", [1200, 3600, 6000, 7200], [600, 1800]),
    "id4": ("games/initiald4/id4.elf", [600, 1800, 3000, 4200], [600, 1800]),
    "rtuned": ("games/rtuned/dsr", [600, 1800, 3000, 4200], [600, 1800]),
}


def tour(exe, out, name):
    program, before, after = GAMES[name]
    folder = os.path.join(out, name)
    os.makedirs(folder, exist_ok=True)
    report = {"game": name}
    started = time.time()
    with LLAgent(exe, os.path.join(REPO, program), log=os.path.join(folder, "game_output.txt")) as game:
        report["ready"] = [game.game, game.width, game.height, round(time.time() - started, 1)]
        report["inputs"] = [i[0] for i in game.inputs()]
        game.servo(True)
        ffb = []
        frame = game.frame()
        for target in before:
            frame, _ = game.step(target - frame)
            game.shot(os.path.join(folder, "%s_%05d.bmp" % (name, frame)))
            ffb += game.ffb()
        report["tel_attract"] = game.tel()
        report["servo_frames_attract"] = len(game.servo())
        # A coin, then Start
        game.press("Coin", 6)
        game.press("Start", 6)
        report["coins_after_coin"] = game.get("Coin")
        start = game.frame()
        for target in after:
            frame, _ = game.step(start + target - game.frame())
            game.shot(os.path.join(folder, "%s_%05d_start.bmp" % (name, frame)))
            ffb += game.ffb()
        report["tel_after_start"] = game.tel()
        report["perf"] = game.perf(300)
        report["frames"] = game.frame()
        report["seconds"] = round(time.time() - started, 1)
        report["ffb_lines"] = len(ffb)
        with open(os.path.join(folder, "ffb.txt"), "w") as f:
            for line in ffb:
                f.write("%d %s %.4f\n" % line)
    with open(os.path.join(folder, "tel.json"), "w") as f:
        json.dump(report, f, indent=1)
    return report


def main():
    exe, out = sys.argv[1], sys.argv[2]
    names = sys.argv[3:] or list(GAMES)
    for name in names:
        try:
            report = tour(exe, out, name)
            print("%s: ready %s, %d frames in %.0f s, ffb lines %d, coins %s, %s" % (
                name, report["ready"], report["frames"], report["seconds"], report["ffb_lines"],
                report["coins_after_coin"], report["perf"]), flush=True)
        except AgentError as error:
            print("%s: %s" % (name, error), flush=True)


if __name__ == "__main__":
    main()
