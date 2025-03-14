#!/usr/bin/python3

import dbus
import dbus.service
import dbus.mainloop.glib
from gi.repository import GLib
import sys

BLUEZ_SERVICE_NAME = 'org.bluez'
GATT_MANAGER_IFACE = 'org.bluez.GattManager1'
DBUS_OM_IFACE = 'org.freedesktop.DBus.ObjectManager'
GATT_SERVICE_IFACE = 'org.bluez.GattService1'

class Service(dbus.service.Object):
    def __init__(self, bus, path, uuid):
        self.path = path
        self.uuid = uuid
        dbus.service.Object.__init__(self, bus, path)
    
    def get_properties(self):
        return {
            GATT_SERVICE_IFACE: {
                'UUID': self.uuid,
                'Primary': True
            }
        }

class Application(dbus.service.Object):
    def __init__(self, bus, path):
        self.path = path
        self.services = []
        dbus.service.Object.__init__(self, bus, path)
        
        # 최소한 하나의 서비스 추가
        self.add_service(Service(bus, path + '/service0', '180F'))  # Battery Service
    
    def add_service(self, service):
        self.services.append(service)
    
    def get_path(self):
        return dbus.ObjectPath(self.path)
    
    @dbus.service.method(DBUS_OM_IFACE, out_signature='a{oa{sa{sv}}}')
    def GetManagedObjects(self):
        response = {}
        print('GetManagedObjects called, returning services:', len(self.services))
        
        for service in self.services:
            response[service.path] = service.get_properties()
        
        print('Response:', response)
        return response

if __name__ == '__main__':
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SystemBus()
    
    # Get adapter interface
    adapter = dbus.Interface(
        bus.get_object(BLUEZ_SERVICE_NAME, '/org/bluez/hci0'),
        'org.bluez.Adapter1'
    )
    
    # Power on adapter
    props = dbus.Interface(
        bus.get_object(BLUEZ_SERVICE_NAME, '/org/bluez/hci0'),
        'org.freedesktop.DBus.Properties'
    )
    
    try:
        props.Set('org.bluez.Adapter1', 'Powered', dbus.Boolean(1))
        print('Bluetooth adapter powered on')
    except Exception as e:
        print(f'Error setting adapter power: {e}')
    
    # Create and register application
    app = Application(bus, '/com/example/gatt')
    
    # Get GattManager1 interface
    print('Getting GattManager interface...')
    manager = dbus.Interface(
        bus.get_object(BLUEZ_SERVICE_NAME, '/org/bluez/hci0'),
        GATT_MANAGER_IFACE
    )
    
    print('Registering application...')
    try:
        manager.RegisterApplication(app.get_path(), {})
        print('Application successfully registered!')
    except dbus.exceptions.DBusException as e:
        print(f'Failed to register: {e}')
    
    mainloop = GLib.MainLoop()
    print('Running main loop...')
    try:
        mainloop.run()
    except KeyboardInterrupt:
        print('Exiting...')