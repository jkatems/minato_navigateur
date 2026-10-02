"""Qt exam client -> unauthenticated local Django -> report. Disposable data only."""
import os
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from urllib.request import urlopen

ROOT = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='minato-exam-test-') as temp:
    os.environ.update(MINATO_EXAM_MODE='1', MINATO_DEBUG='1', MINATO_DATABASE=str(Path(temp)/'test.sqlite3'),
                      MINATO_TEST_EXAM='1', DJANGO_SETTINGS_MODULE='config.settings')
    import django
    django.setup()
    from django.core.management import call_command
    from monitor.models import Event
    call_command('migrate', verbosity=0)
    with socket.socket() as probe:
        try:
            probe.bind(('127.0.0.1', 8000))
        except OSError:
            raise SystemExit('Port 8000 occupé : arrêter votre serveur de test avant ce test isolé.')
    process = subprocess.Popen([sys.executable, str(ROOT/'manage.py'), 'runserver', '127.0.0.1:8000', '--noreload'],
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        for _ in range(100):
            try:
                with urlopen('http://127.0.0.1:8000/', timeout=1) as response:
                    if response.status == 200: break
            except OSError:
                if process.poll() is not None: raise RuntimeError('Serveur arrêté')
                time.sleep(.1)
        else:
            raise RuntimeError('Délai de démarrage dépassé')
        executable = ROOT.parent/'build'/('sync_tests.exe' if os.name == 'nt' else 'sync_tests')
        if not executable.exists(): executable = ROOT.parent/'build-windows'/'Release'/'sync_tests.exe'
        subprocess.run([str(executable), 'examIntegration'], check=True, timeout=30)
        event = Event.objects.get()
        assert event.url == 'https://example.org/examen' and event.peer_ip == '127.0.0.1'
        with urlopen('http://127.0.0.1:8000/') as response:
            content = response.read().decode()
            assert response.status == 200 and event.url in content and '127.0.0.1' in content
            for interface in event.interfaces:
                if interface['mac']: assert interface['mac'] in content
        print('PASS: Qt examen sans configuration -> API locale sans jeton -> historique/IP/MAC sans connexion')
    finally:
        process.terminate()
        process.wait(timeout=10)
