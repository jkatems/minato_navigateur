import hashlib
import json
import uuid
from datetime import timedelta
from io import StringIO
from django.contrib.auth import get_user_model
from django.core.management import call_command
from django.test import TestCase, Client, override_settings
from django.urls import reverse
from django.utils import timezone
from .models import Device, Event

@override_settings(SECURE_SSL_REDIRECT=False, ALLOWED_HOSTS=['testserver', 'localhost'], DEBUG=False)
class ReportTests(TestCase):
    @classmethod
    def setUpTestData(cls):
        cls.admin = get_user_model().objects.create_user('admin', password='test-long-password', is_staff=True)
        cls.reader = get_user_model().objects.create_user('reader', password='test-long-password')
        cls.device, cls.token = Device.issue('Poste de test', cls.admin)

    def event(self, **changes):
        value = {'id': str(uuid.uuid4()), 'kind': 'search', 'occurred_at': timezone.now().isoformat(),
                 'profile': 'Travail', 'url': 'https://example.org/?q=minato', 'title': '', 'query': 'minato',
                 'interfaces': [{'name': 'Test Ethernet', 'type': 'Ethernet', 'state': 'active',
                                 'ipv4': '192.0.2.2', 'ipv6': '2001:db8::1', 'mac': '02:00:00:00:00:01'}],
                 'consent_version': 1}
        return value | changes

    def send(self, events, token=None, **kwargs):
        return self.client.post(reverse('ingest'), data=json.dumps({'events': events}),
            content_type='application/json', HTTP_AUTHORIZATION='Bearer ' + (token or self.token), secure=True, **kwargs)

    def test_authenticated_ingestion_and_idempotent_retry(self):
        item = self.event()
        for _ in range(2):
            response = self.send([item], REMOTE_ADDR='203.0.113.1', HTTP_X_FORWARDED_FOR='192.0.2.99')
            self.assertEqual(response.status_code, 200)
            self.assertEqual(response.json()['accepted'], [item['id']])
        self.assertEqual(Event.objects.count(), 1)
        event = Event.objects.get()
        self.assertEqual(event.peer_ip, '203.0.113.1')
        self.assertEqual(event.interfaces[0]['mac'], '02:00:00:00:00:01')
        self.device.refresh_from_db()
        self.assertIsNotNone(self.device.last_seen)
        self.assertEqual(self.device.token_hash, hashlib.sha256(self.token.encode()).hexdigest())
        self.assertNotEqual(self.device.token_hash, self.token)

    def test_bad_token_and_revocation(self):
        self.assertEqual(self.send([self.event()], token='x'*43).status_code, 401)
        Device.objects.filter(pk=self.device.pk).update(active=False)
        self.assertEqual(self.send([self.event()]).status_code, 401)
        self.assertFalse(Event.objects.exists())

    def test_session_does_not_authorize_api(self):
        self.client.force_login(self.admin)
        response = self.client.post(reverse('ingest'), data='{}', content_type='application/json', secure=True)
        self.assertEqual(response.status_code, 401)

    def test_http_disallowed_except_local_development(self):
        response = self.client.post(reverse('ingest'), data='{}', content_type='application/json', HTTP_AUTHORIZATION='Bearer '+self.token)
        self.assertEqual(response.status_code, 403)
        with self.settings(DEBUG=True):
            response = self.client.post(reverse('ingest'), data=json.dumps({'events':[self.event()]}), content_type='application/json', HTTP_AUTHORIZATION='Bearer '+self.token, HTTP_HOST='localhost')
            self.assertEqual(response.status_code, 200)

    def test_invalid_batch_atomic(self):
        for invalid in [self.event(consent_version=0), self.event(url='javascript:alert(1)'), self.event(url='https://u:secret@example.org'),
                        self.event(occurred_at='yesterday'), self.event(occurred_at='2026-01-01T10:00:00'), self.event(query='x'*2049),
                        self.event(interfaces=[{}]*65), self.event(cookies='forbidden'), self.event(kind='password')]:
            with self.subTest(invalid=invalid):
                self.assertEqual(self.send([self.event(), invalid]).status_code, 400)
        self.assertFalse(Event.objects.exists())

    def test_invalid_json_and_size(self):
        for body in ['{', '[]', '{"events": []}', json.dumps({'events': [self.event()]*21})]:
            response = self.client.post(reverse('ingest'), data=body, content_type='application/json', HTTP_AUTHORIZATION='Bearer '+self.token, secure=True)
            self.assertEqual(response.status_code, 400)
        response = self.client.post(reverse('ingest'), data='x'*270000, content_type='application/json', HTTP_AUTHORIZATION='Bearer '+self.token, secure=True)
        self.assertEqual(response.status_code, 413)

    def test_sensitive_url_parameters_removed(self):
        self.send([self.event(url='https://example.org/?q=hello&token=secret&code=oauth#access_token=hidden')])
        self.assertEqual(Event.objects.get().url, 'https://example.org/?q=hello')

    def test_staff_access_only_and_output_escaped(self):
        self.send([self.event(title='<script>alert(1)</script>')])
        self.assertEqual(self.client.get('/').status_code, 302)
        self.client.force_login(self.reader)
        self.assertEqual(self.client.get('/').status_code, 403)
        self.assertEqual(self.client.get(reverse('event', args=[Event.objects.get().pk])).status_code, 403)
        self.client.force_login(self.admin)
        response = self.client.get('/')
        self.assertContains(response, 'Poste de test')
        self.assertIn('no-store', response.headers['Cache-Control'])
        detail = self.client.get(reverse('event', args=[Event.objects.get().pk]))
        self.assertContains(detail, '&lt;script&gt;')
        self.assertNotContains(detail, '<script>')
        self.assertContains(detail, '02:00:00:00:00:01')
        self.assertNotContains(detail, self.token)

    def test_search_filters_and_invalid_device(self):
        self.send([self.event(query='needle')])
        self.client.force_login(self.admin)
        self.assertContains(self.client.get('/?q=needle&kind=search'), 'needle')
        self.assertEqual(len(self.client.get('/?q=absent').context['page']), 0)
        self.assertEqual(len(self.client.get('/?device=invalid-uuid').context['page']), 0)

    def test_create_revoke_erase_requires_csrf(self):
        self.send([self.event()])
        client = Client(enforce_csrf_checks=True)
        client.force_login(self.admin)
        for url in [reverse('devices'), reverse('revoke', args=[self.device.pk]), reverse('erase', args=[self.device.pk])]:
            self.assertEqual(client.post(url).status_code, 403)
        self.client.force_login(self.admin)
        self.assertEqual(self.client.get(reverse('revoke', args=[self.device.pk])).status_code, 405)
        self.assertEqual(self.client.post(reverse('erase', args=[self.device.pk])).status_code, 400)
        self.assertEqual(self.client.post(reverse('erase', args=[self.device.pk]), {'confirm':'erase'}).status_code, 302)
        self.assertFalse(Event.objects.exists())
        self.assertEqual(self.client.post(reverse('revoke', args=[self.device.pk])).status_code, 302)
        self.device.refresh_from_db()
        self.assertFalse(self.device.active)

    def test_token_shown_only_on_creation(self):
        self.client.force_login(self.admin)
        response = self.client.post(reverse('devices'), {'name':'New device'})
        token = response.context['token']
        self.assertContains(response, token)
        self.assertNotContains(self.client.get(reverse('devices')), token)
        self.assertEqual(Device.objects.count(), 2)

    def test_login_nonstaff_refused_and_throttled(self):
        response = self.client.post(reverse('login'), {'username':'reader', 'password':'test-long-password'})
        self.assertEqual(response.status_code, 200)
        self.assertNotIn('_auth_user_id', self.client.session)
        for _ in range(9):
            self.client.post(reverse('login'), {'username':'absent', 'password':'wrong'})
        self.assertEqual(self.client.post(reverse('login'), {'username':'admin', 'password':'wrong'}).status_code, 429)

    def test_browser_login_with_csrf(self):
        client = Client(enforce_csrf_checks=True)
        response = client.get(reverse('login'), secure=True)
        self.assertContains(response, 'content="same-origin"')
        token = client.cookies['csrftoken'].value
        response = client.post(reverse('login'), {'username':'admin', 'password':'test-long-password', 'csrfmiddlewaretoken':token}, secure=True, HTTP_ORIGIN='https://testserver')
        self.assertEqual(response.status_code, 302)
        self.assertIn('_auth_user_id', client.session)

    def test_purge_retention(self):
        self.send([self.event()])
        Event.objects.update(received_at=timezone.now()-timedelta(days=31))
        call_command('purge_reports', stdout=StringIO())
        self.assertFalse(Event.objects.exists())

    def test_rate_limit(self):
        for _ in range(12):
            self.assertEqual(self.send([self.event() for _ in range(20)]).status_code, 200)
        self.assertEqual(self.send([self.event()]).status_code, 429)

@override_settings(EXAM_MODE=True, DEBUG=True, SECURE_SSL_REDIRECT=False, ALLOWED_HOSTS=['testserver', 'localhost'])
class ExamTests(TestCase):
    def event(self):
        return {'id':str(uuid.uuid4()), 'kind':'visit', 'occurred_at':timezone.now().isoformat(),
                'profile':'Examen', 'url':'https://example.org/', 'title':'Site examen', 'query':'',
                'interfaces':[{'name':'Ethernet test', 'ipv4':'192.0.2.10', 'mac':'02:00:00:00:00:01'}],
                'consent_version':1}

    def send(self, **extra):
        return self.client.post(reverse('ingest'), data=json.dumps({'events':[self.event()]}),
            content_type='application/json', HTTP_X_MINATO_EXAM='1', **extra)

    def test_no_login_or_token_needed_locally(self):
        self.assertEqual(self.send().status_code, 200)
        self.assertEqual(Event.objects.count(), 1)
        response = self.client.get('/')
        self.assertContains(response, 'Site examen')
        self.assertContains(response, '02:00:00:00:00:01')
        self.assertContains(response, '192.0.2.10')
        self.assertContains(response, '127.0.0.1')
        self.assertNotContains(response, 'Gérer les appareils')
        event = Event.objects.get()
        self.assertEqual(self.client.get(reverse('event', args=[event.pk])).status_code, 200)
        self.assertFalse(event.device.created_by.has_usable_password())
        self.assertFalse(event.device.created_by.is_active)

    def test_remote_clients_and_forged_forwarding_refused(self):
        self.assertEqual(self.send(REMOTE_ADDR='192.0.2.50', HTTP_X_REAL_IP='127.0.0.1').status_code, 403)
        self.assertEqual(self.client.get('/', REMOTE_ADDR='192.0.2.50').status_code, 403)
        self.assertFalse(Event.objects.exists())

    def test_remote_web_pages_cannot_post_or_preflight(self):
        self.assertEqual(self.send(HTTP_ORIGIN='https://malicious.example').status_code, 403)
        self.assertEqual(self.send(HTTP_ORIGIN='null').status_code, 403)
        response = self.client.post(reverse('ingest'), data=json.dumps({'events':[self.event()]}), content_type='application/json')
        self.assertEqual(response.status_code, 403)
        self.assertEqual(self.client.options(reverse('ingest'), HTTP_ORIGIN='https://malicious.example').status_code, 405)
        self.assertFalse(Event.objects.exists())

    def test_global_local_only_middleware(self):
        from .exam import ExamLocalOnlyMiddleware
        from django.http import HttpResponse
        from django.test import RequestFactory
        middleware = ExamLocalOnlyMiddleware(lambda request: HttpResponse('local'))
        self.assertEqual(middleware(RequestFactory().get('/', REMOTE_ADDR='192.0.2.1')).status_code, 403)
        self.assertEqual(middleware(RequestFactory().get('/', REMOTE_ADDR='::1')).status_code, 200)


@override_settings(EPHEMERAL_DEMO=True, EXAM_MODE=False, DEBUG=False, SECURE_SSL_REDIRECT=False,
                   ALLOWED_HOSTS=['testserver'], DEMO_INSTANCE='012345abcdef')
class RemoteDemoTests(TestCase):
    def setUp(self):
        self.deadline = self.settings(DEMO_UNTIL=timezone.now() + timedelta(minutes=50))
        self.deadline.enable()
        self.addCleanup(self.deadline.disable)

    def event(self):
        return {'id':str(uuid.uuid4()), 'kind':'visit', 'occurred_at':timezone.now().isoformat(),
                'profile':'Demonstration', 'url':'https://example.org/demo', 'title':'Demo', 'query':'',
                'interfaces':[{'name':'demo-interface','mac':'02:00:00:00:00:01'}], 'consent_version':1}

    def send(self, **extra):
        return self.client.post(reverse('ingest'), data=json.dumps({'events':[self.event()]}),
            content_type='application/json', HTTP_X_MINATO_DEMO='1', secure=True, **extra)

    def test_https_collection_and_anonymous_report(self):
        response = self.send(REMOTE_ADDR='203.0.113.2')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json()['instance'], '012345abcdef')
        response = self.client.get('/', secure=True)
        self.assertContains(response, 'https://example.org/demo')
        self.assertContains(response, '02:00:00:00:00:01')
        self.assertContains(response, '203.0.113.2')
        self.assertContains(response, '012345abcdef')
        self.assertEqual(self.client.get(reverse('event', args=[Event.objects.get().pk]), secure=True).status_code, 200)

    def test_expiration_rejects_reads_and_writes(self):
        with self.settings(DEMO_UNTIL=timezone.now()-timedelta(seconds=1)):
            self.assertEqual(self.send().status_code, 410)
            self.assertEqual(self.client.get('/', secure=True).status_code, 410)
        self.assertFalse(Event.objects.exists())

    def test_requires_https_native_client_and_no_origin(self):
        self.assertEqual(self.client.get('/').status_code, 403)
        self.assertEqual(self.send(HTTP_ORIGIN='https://example.org').status_code, 403)
        response = self.client.post(reverse('ingest'), data='{}', content_type='application/json', secure=True)
        self.assertEqual(response.status_code, 403)

    def test_middleware_no_cache_and_expiry_purge(self):
        from unittest.mock import patch
        from django.http import HttpResponse
        from django.test import RequestFactory
        from .demo import EphemeralDemoMiddleware
        self.send()
        middleware = EphemeralDemoMiddleware(lambda request: HttpResponse('demo'))
        with patch('monitor.demo.ensure_database') as ensure:
            response = middleware(RequestFactory().get('/', secure=True))
            ensure.assert_called_once()
            self.assertIn('no-store', response.headers['Cache-Control'])
            self.assertEqual(response.headers['X-Minato-Instance'], '012345abcdef')
        with self.settings(DEMO_UNTIL=timezone.now()-timedelta(seconds=1)), patch('monitor.demo._ready', True):
            response = middleware(RequestFactory().get('/', secure=True))
            self.assertEqual(response.status_code, 410)
            self.assertFalse(Event.objects.exists())
