# SMTP test fixtures

The certificate and private key in this directory belong only to the local fake SMTP server used by `smtp_tests`. They are public test data, not credentials for an actual service. The certificate covers localhost and 127.0.0.1. Only the test process adds it temporarily to its Qt trust configuration; the application and operating system trust stores are not changed. These files are not installed with Minato.
