#!/usr/bin/python3
import dbus
import dbus.service
import dbus.mainloop.glib
from gi.repository import GLib
import sys

class Advertisement(dbus.service.Object):
    def __init__(self, bus, path):
        dbus.service.Object.__init__(self, bus, path)

    @dbus.service.method("org.freedesktop.DBus.Properties",
                         in_signature="s", out_signature="a{sv}")
    def GetAll(self, interface):
        print(f"GetAll called for interface: {interface}")
        if interface != "org.bluez.LEAdvertisement1":
            raise dbus.exceptions.DBusException(
                "org.bluez.InvalidArgsException",
                f"Interface {interface} not supported")
        
        # 가능한 단순한 속성 반환
        properties = {
            "Type": dbus.String("peripheral"),
            "LocalName": dbus.String("JetsonBLE"),
            "IncludeTxPower": dbus.Boolean(True)
        }
        return properties

    @dbus.service.method("org.bluez.LEAdvertisement1", in_signature="", out_signature="")
    def Release(self):
        print("Release called")

def main():
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SystemBus()
    
    # 어댑터 가져오기
    adapter_obj = bus.get_object("org.bluez", "/org/bluez/hci0")
    adapter = dbus.Interface(adapter_obj, "org.bluez.LEAdvertisingManager1")
    
    # 어댑터가 켜져 있는지 확인
    props = dbus.Interface(adapter_obj, "org.freedesktop.DBus.Properties")
    if not props.Get("org.bluez.Adapter1", "Powered"):
        props.Set("org.bluez.Adapter1", "Powered", dbus.Boolean(True))
    
    # 광고 객체 생성
    adv_path = "/org/test/advertisement"
    advertisement = Advertisement(bus, adv_path)
    
    print(f"Registering advertisement at path: {adv_path}")
    
    # 빈 옵션으로 광고 등록
    try:
        adapter.RegisterAdvertisement(adv_path, {})
        print("Advertisement registered successfully!")
        
        # 메인 루프 시작
        mainloop = GLib.MainLoop()
        print("Running... Press Ctrl+C to stop")
        mainloop.run()
    except dbus.exceptions.DBusException as e:
        print(f"Failed to register advertisement: {e}")
    except KeyboardInterrupt:
        print("\nUnregistering advertisement...")
        try:
            adapter.UnregisterAdvertisement(adv_path)
            print("Advertisement unregistered")
        except Exception as e:
            print(f"Error unregistering: {e}")

if __name__ == "__main__":
    main()