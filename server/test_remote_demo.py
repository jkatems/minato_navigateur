"""Real Qt HTTPS client -> ephemeral Django SQLite -> anonymous report, on loopback only."""
import os
import socket
import ssl
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timedelta, timezone
from pathlib import Path
from urllib.request import urlopen

ROOT = Path(__file__).resolve().parent
FIXTURES = ROOT.parent/'tests'/'fixtures'
if len(sys.argv) > 1 and sys.argv[1] == 'serve':
    from wsgiref.simple_server import make_server, WSGIRequestHandler
    from config.wsgi import application
    class Handler(WSGIRequestHandler):
        def get_environ(self):
            env = super().get_environ()
            env['HTTPS'] = 'on'
            return env
        def log_message(self, *args):
            pass
    server = make_server('127.0.0.1', int(sys.argv[2]), application, handler_class=Handler)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(FIXTURES/'smtp-test-cert.pem', FIXTURES/'smtp-test-key.pem')
    server.socket = context.wrap_socket(server.socket, server_side=True)
    server.serve_forever()
else:
    with tempfile.TemporaryDirectory(prefix='minato-remote-demo-') as temp:
        env = dict(os.environ, MINATO_EPHEMERAL_DEMO='1', MINATO_EXAM_MODE='0', MINATO_DEBUG='0',
                   MINATO_SECRET_KEY='isolated-local-test-key-not-for-deployment',
                   MINATO_DEMO_UNTIL=(datetime.now(timezone.utc)+timedelta(minutes=55)).isoformat(),
                   MINATO_ALLOWED_HOSTS='localhost,127.0.0.1', TMPDIR=temp)
        with socket.socket() as probe:
            probe.bind(('127.0.0.1', 0))
            port = probe.getsockname()[1]
        base = f'https://localhost:{port}'
        env['MINATO_TEST_REMOTE_DEMO'] = base
        context = ssl.create_default_context(cafile=str(FIXTURES/'smtp-test-cert.pem'))
        process = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), 'serve', str(port)], env=env,
                                   stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        try:
            for _ in range(100):
                try:
                    with urlopen(base, context=context, timeout=2) as response:
                        if response.status == 200: break
                except OSError:
                    if process.poll() is not None:
                        raise RuntimeError(process.stderr.read().decode())
                    time.sleep(.1)
            else: raise RuntimeError('TLS server startup timeout')
            executable = ROOT.parent/'build'/'sync_tests'
            if os.name == 'nt': executable = ROOT.parent/'build-windows'/'Release'/'sync_tests.exe'
            subprocess.run([str(executable), 'remoteDemoIntegration'], env=env, check=True, timeout=30)
            with urlopen(base, context=context, timeout=10) as response:
                assert 'https://example.org/remote-demo' in response.read().decode()
                assert 'no-store' in response.headers['Cache-Control']
                assert response.headers['X-Minato-Instance']
            assert list(Path(temp).glob('minato-demo-*.sqlite3'))
            print('PASS: real Qt HTTPS -> ephemeral SQLite -> anonymous report; temporary files removed on exit')
        finally:
            process.terminate()
            process.wait(timeout=10)
