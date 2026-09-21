if CHARDICT
helperdatadir = $(datadir)/uim/helperdata
dist_helperdata_DATA = bushu.t
endif

if HAVE_DBUS
dbussessionservicedir = $(DBUS_SESSION_SERVICES_DIR)
dbussessionservice_DATA = dbus-1/services/org.uim.HelperBus.service

if HAVE_SYSTEMD_USER_UNITDIR
systemduserunitdir = $(SYSTEMD_USER_UNIT_DIR)
systemduserunit_DATA = systemd/uim-helper-bus.service
endif
endif

EXTRA_DIST = dbus-1/services/org.uim.HelperBus.service.in \
	     systemd/uim-helper-bus.service.in \
	     dbus-1/interfaces/org.uim.Factory1.xml \
	     dbus-1/interfaces/org.uim.InputContext1.xml
CLEANFILES = dbus-1/services/org.uim.HelperBus.service \
	     systemd/uim-helper-bus.service
