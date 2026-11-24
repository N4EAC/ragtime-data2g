"""Connect the Cocoa app to a real external HF host over loopback, without RF.

Run with --app and --host paths. The host uses a temporary, silent TCI bench;
all app/host settings and TCP ports are private to this test.
"""
import argparse
from pathlib import Path
import socket
import subprocess
import tempfile
import time


def stop(process):
    if process is not None and process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


def ports():
    for _ in range(50):
        sockets = []
        try:
            command = socket.socket()
            sockets.append(command)
            command.bind(('127.0.0.1', 0))
            number = command.getsockname()[1]
            if number == 65535:
                continue
            data = socket.socket()
            sockets.append(data)
            data.bind(('127.0.0.1', number + 1))
            kiss = socket.socket()
            sockets.append(kiss)
            kiss.bind(('127.0.0.1', 0))
            return number, kiss.getsockname()[1]
        except OSError:
            pass
        finally:
            for sock in sockets:
                sock.close()
    raise RuntimeError('No free loopback ports')


def check(app_path, host_path):
    command, kiss = ports()
    app = host = None
    with tempfile.TemporaryDirectory(prefix='ragtime-real-host-') as folder, socket.socket() as tci:
        root = Path(folder)
        tci.bind(('127.0.0.1', 0))
        tci.listen()
        config = root / 'station.conf'
        config.write_text('[Station]\nCallsign=W1AW\nLocator=FN31\nAnswerRollCalls=0\n'
                          f'[Connection]\nHost=127.0.0.1\nKissPort={kiss}\nCommandPort={command}\n'
                          '[Modem]\nEnabled=0\n'
                          f'[Files]\nReceivedFolder={root / "received"}\n')
        host_config = root / 'host.conf'
        host_config.touch()
        with (root / 'host.log').open('w+') as host_log, (root / 'app.log').open('w+') as app_log:
            try:
                host = subprocess.Popen([str(host_path), '--config', str(host_config), '--mycall', 'W1AW',
                                         '--listen', '127.0.0.1', '--kiss-port', str(kiss),
                                         '--command-port', str(command), '--tci', '--tci-host',
                                         f'127.0.0.1:{tci.getsockname()[1]}', '--no-rig', '--no-record'],
                                        stdout=host_log, stderr=subprocess.STDOUT)
                deadline = time.monotonic() + 15
                while True:
                    if host.poll() is not None:
                        raise RuntimeError('External host exited')
                    try:
                        with socket.create_connection(('127.0.0.1', command), timeout=.2):
                            break
                    except OSError:
                        if time.monotonic() >= deadline:
                            raise RuntimeError('External host did not open its ports')
                        time.sleep(.1)
                app = subprocess.Popen([str(app_path), '--config', str(config)],
                                       stdout=app_log, stderr=subprocess.STDOUT)
                deadline = time.monotonic() + 25
                while time.monotonic() < deadline:
                    if app.poll() is not None:
                        raise RuntimeError('App exited before initialization')
                    flight = '\n'.join(p.read_text() for p in root.glob('station.conf.flight/*.log'))
                    if all(reply in flight for reply in ('cmd< REGISTERED W1AW', 'cmd< BCAST PORT', 'cmd< MODE ')):
                        break
                    time.sleep(.1)
                else:
                    raise RuntimeError('App did not initialize the external Data2G broadcast group')
                # Verify that the independent session-data socket is connected as well.
                connections = subprocess.check_output(['lsof', '-nP', '-a', '-p', str(app.pid),
                                                       '-iTCP', '-sTCP:ESTABLISHED'], text=True)
                for port in (kiss, command, command + 1):
                    if f'->{"127.0.0.1"}:{port} ' not in connections:
                        raise RuntimeError(f'App has no connection to modem port {port}')
                app.terminate()
                app.wait(timeout=10)
                if app.returncode != 0 or host.poll() is not None:
                    raise RuntimeError('App shutdown failed or stopped the external host')
                print('External HF host: callsign, broadcast group, mode, KISS/command/session sockets and independent shutdown passed')
            except Exception:
                host_log.seek(0)
                app_log.seek(0)
                print('HOST:', host_log.read(), '\nAPP:', app_log.read())
                raise
            finally:
                stop(app)
                stop(host)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', required=True, type=Path)
    parser.add_argument('--host', required=True, type=Path)
    args = parser.parse_args()
    check(args.app.resolve(), args.host.resolve())
