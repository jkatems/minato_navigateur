from django.urls import path
from django.contrib.auth.views import LogoutView
from monitor import views
urlpatterns = [
    path("", views.dashboard, name="dashboard"),
    path("connexion/", views.AdminLoginView.as_view(), name="login"),
    path("deconnexion/", LogoutView.as_view(), name="logout"),
    path("appareils/", views.devices, name="devices"),
    path("appareils/<uuid:pk>/revoquer/", views.revoke, name="revoke"),
    path("appareils/<uuid:pk>/effacer/", views.erase, name="erase"),
    path("evenements/<int:pk>/", views.event_detail, name="event"),
    path("api/v1/events/", views.ingest, name="ingest"),
]
