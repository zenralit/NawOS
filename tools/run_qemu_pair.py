import argparse
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
IMAGE = ROOT / "os-image.bin"
GUESTS = (
    ("10.0.0.1", "52:54:00:4e:41:01"),
    ("10.0.0.2", "52:54:00:4e:41:02"),
)
QMP_TIMEOUT = 10
BOOT_TIMEOUT = 90


class QMP:
    def __init__(self, port, process):
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            exit_code = process.poll()
            if exit_code is not None:
                raise RuntimeError(
                    f"QEMU exited with status {exit_code} before opening QMP on port {port}"
                )
            try:
                self.sock = socket.create_connection(("127.0.0.1", port), timeout=1)
                break
            except OSError:
                time.sleep(0.1)
        else:
            raise RuntimeError(f"Could not connect to QEMU QMP on port {port}")

        self.sock.settimeout(QMP_TIMEOUT)
        self.stream = self.sock.makefile("rwb")
        greeting = json.loads(self.stream.readline())
        if "QMP" not in greeting:
            raise RuntimeError("QEMU returned an invalid QMP greeting")
        self.sequence = 0
        self.execute("qmp_capabilities")

    def execute(self, command, arguments=None):
        self.sequence += 1
        sequence = self.sequence
        request = {"execute": command, "id": sequence}
        if arguments:
            request["arguments"] = arguments
        self.stream.write((json.dumps(request) + "\r\n").encode())
        self.stream.flush()

        while True:
            response = json.loads(self.stream.readline())
            if response.get("id") != sequence:
                continue
            if "error" in response:
                raise RuntimeError(f"QMP {command} failed: {response['error']}")
            return response.get("return")

    def hmp(self, command):
        return self.execute(
            "human-monitor-command", {"command-line": command}
        )

    def close(self):
        self.stream.close()
        self.sock.close()


def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


def find_qemu():
    requested = os.environ.get("QEMU", "qemu-system-i386")
    executable = shutil.which(requested)
    if executable:
        return executable

    # Make may pass an MSYS path while this script runs under native Windows Python.
    executable = shutil.which(Path(requested).name)
    if executable:
        return executable
    raise RuntimeError(
        f"QEMU executable not found: {requested}. Install QEMU or set QEMU to its path."
    )


def read_screen(qmp, screen_path):
    path = str(screen_path.resolve()).replace("\\", "/")
    quoted_path = path.replace('"', '\\"')
    qmp.hmp(f'pmemsave 0xb8000 4000 "{quoted_path}"')
    raw = screen_path.read_bytes()
    return "\n".join(
        "".join(
            chr(value) if 32 <= value < 127 else " "
            for value in raw[row * 160 : (row + 1) * 160 : 2]
        ).rstrip()
        for row in range(25)
    )


def wait_for_guest(qmp, screen_path, index, ip):
    deadline = time.monotonic() + BOOT_TIMEOUT
    output = ""
    while time.monotonic() < deadline:
        output = read_screen(qmp, screen_path)
        if (
            "RTL8139 MAC:" in output
            and "IRQ 11" in output
            and f"Static IP set to {ip}" in output
        ):
            print(f"Guest {index + 1} ready at {ip}", flush=True)
            return
        time.sleep(0.25)
    status = qmp.execute("query-status")
    registers = qmp.hmp("info registers")
    raise RuntimeError(
        f"Guest {index + 1} did not finish booting "
        f"(QEMU status: {status}):\n{output}\n{registers}"
    )


def launch_guest(qemu, image, network, mac, qmp_port, headless):
    command = [
        qemu,
        "-machine",
        "pc",
        "-m",
        "32M",
        "-snapshot",
        "-no-reboot",
        "-no-shutdown",
        "-drive",
        f"file={image},format=raw,if=ide,index=0",
        "-qmp",
        f"tcp:127.0.0.1:{qmp_port},server=on,wait=off",
        "-netdev",
        network,
        "-device",
        f"rtl8139,netdev=net0,mac={mac}",
    ]
    if headless:
        command.extend(("-display", "none"))
    return subprocess.Popen(command, cwd=ROOT)


def stop_guests(processes, monitors):
    for monitor in monitors:
        try:
            monitor.execute("quit")
        except (OSError, RuntimeError, ValueError):
            pass
        try:
            monitor.close()
        except OSError:
            pass

    for process in processes:
        if process.poll() is None:
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()


def main():
    parser = argparse.ArgumentParser(
        description="Start two NawOS QEMU guests on a shared broadcast network."
    )
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="boot both guests headlessly and verify automatic IP setup",
    )
    args = parser.parse_args()

    if not IMAGE.is_file():
        raise RuntimeError(f"OS image not found: {IMAGE}. Run make os-image.bin first.")

    qemu = find_qemu()
    network_port = free_port()
    qmp_ports = (free_port(), free_port())
    processes = []
    monitors = []

    try:
        with tempfile.TemporaryDirectory(prefix="nawos-qemu-") as directory:
            screen_paths = (
                Path(directory) / "guest1-vga.bin",
                Path(directory) / "guest2-vga.bin",
            )
            for index, (ip, mac) in enumerate(GUESTS):
                if index == 0:
                    backend = f"socket,id=net0,listen=127.0.0.1:{network_port}"
                else:
                    backend = f"socket,id=net0,connect=127.0.0.1:{network_port}"

                processes.append(
                    launch_guest(
                        qemu,
                        IMAGE,
                        backend,
                        mac,
                        qmp_ports[index],
                        args.self_test,
                    )
                )
                monitors.append(QMP(qmp_ports[index], processes[index]))
                wait_for_guest(monitors[index], screen_paths[index], index, ip)

            if args.self_test:
                print("Self-test passed: both guests are ready on the shared link.")
                return

            print(
                "Both guests are connected and ready. Type `netmsg <text>` in "
                "either QEMU window; Ctrl+C here closes both guests.",
                flush=True,
            )
            while True:
                for index, process in enumerate(processes):
                    exit_code = process.poll()
                    if exit_code is not None:
                        raise RuntimeError(
                            f"QEMU guest {index + 1} exited with status {exit_code}"
                        )
                time.sleep(0.5)
    finally:
        stop_guests(processes, monitors)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nStopping both NawOS guests.", flush=True)
    except Exception as error:
        print(f"QEMU pair startup failed: {error}", file=sys.stderr, flush=True)
        sys.exit(1)
