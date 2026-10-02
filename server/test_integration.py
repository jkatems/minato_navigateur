"""Real Qt client -> local Django -> SQLite -> authenticated report.
Run with server/.venv/bin/python server/test_integration.py after building sync_tests.
Uses a disposable database and loopback only; no real browsing history.
"""
import os
import secrets
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from urllib.request import urlopen

ROOT = Path(__file__).resolve().parent

def main():
    with tempfile.TemporaryDirectory(prefix='minato-api-test-') as temp:
        os.environ.update(MINATO_DEBUG='1', MINATO_DATABASE=str(Path(temp)/'test.sqlite3'), DJANGO_SETTINGS_MODULE='config.settings')
        sys.path.insert(0, str(ROOT))
        import django
        django.setup()
        from django.core.management import call_command
        from django.contrib.auth import get_user_model
        from django.test import Client
        from monitor.models import Device, Event
        call_command('migrate', verbosity=0)
        user = get_user_model().objects.create_user('test-admin', password=secrets.token_urlsafe(24), is_staff=True)
        _, token = Device.issue('Qt integration', user)
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0))
            port = sock.getsockname()[1]
        base = f'http://127.0.0.1:{port}'
        environment = dict(os.environ, MINATO_TEST_SERVER=base, MINATO_TEST_TOKEN=token)
        process = subprocess.Popen([sys.executable, str(ROOT/'manage.py'), 'runserver', f'127.0.0.1:{port}', '--noreload'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=environment)
        try:
            for _ in range(100):
                try:
                    with urlopen(base+'/connexion/', timeout=1) as response:
                        if response.status == 200: break
                except OSError:
                    if process.poll() is not None: raise RuntimeError('Django failed to start')
                    time.sleep(.1)
            else:
                raise RuntimeError('Django startup timeout')
            executable = ROOT.parent/'build'/('sync_tests.exe' if os.name == 'nt' else 'sync_tests')
            if not executable.exists():
                executable = ROOT.parent/'build-windows'/'Release'/'sync_tests.exe'
            subprocess.run([str(executable), 'djangoIntegration'], env=environment, check=True, timeout=30)
            event = Event.objects.get()
            assert event.kind == 'search' and event.query == 'integration minato'
            assert event.peer_ip == '127.0.0.1' and event.interfaces is not None
            browser = Client(HTTP_HOST='localhost')
            browser.force_login(user)
            response = browser.get('/')
            assert response.status_code == 200 and b'integration minato' in response.content
            response = browser.get(f'/evenements/{event.pk}/')
            assert response.status_code == 200 and b'127.0.0.1' in response.content
            print('PASS: Qt -> Django API -> SQLite -> authenticated dashboard and network details')
        finally:
            process.terminate()
            process.wait(timeout=10)

if __name__ == '__main__':
    main()
