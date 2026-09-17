# Python-side prototype module. The final module ABI will be C++.
#
# A timer handle is a plain dict owned by the module. The interpreter is the
# time source: it injects handle["_now"] with its virtual frame clock before
# every object method call, and it fires handle["_fire_callback"] (a Resiris
# function name) once after the call that set it.

NAME = "RSBase"
FUNCTIONS = (
    "await, set_timer, on_timeout, reset_timer, stop_timer, free_timer, process"
)
VARIABLES = ""


def _INIT_():
    pass


def _await_timer():
    return {
        "running": False,
        "start": 0.0,
        "duration": 0.0,
        "callback": None,
        "fired": False,
        "freed": False,
    }


def set_timer(handle, seconds):
    handle["duration"] = float(seconds)
    handle["start"] = float(handle.get("_now", 0.0))
    handle["running"] = True
    handle["fired"] = False
    handle["freed"] = False
    return handle


def on_timeout(handle, function_name):
    handle["callback"] = function_name
    return handle


def reset_timer(handle):
    handle["start"] = float(handle.get("_now", 0.0))
    handle["running"] = True
    handle["fired"] = False
    handle["freed"] = False
    return handle


def stop_timer(handle):
    handle["running"] = False
    return handle


def free_timer(handle):
    handle["running"] = False
    handle["freed"] = True
    return handle


def process(handle):
    now = float(handle.get("_now", 0.0))

    if not handle.get("running"):
        return 0.0

    remaining = handle["start"] + handle["duration"] - now

    if remaining > 0.0:
        return remaining

    if not handle.get("fired"):
        handle["fired"] = True
        if handle.get("callback"):
            handle["_fire_callback"] = handle["callback"]

    return 0.0


globals()["await"] = _await_timer