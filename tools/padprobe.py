"""Controller probe, no game needed: what the mod's HID reader (src/gamepad_input.cpp) would see.

Lists every present HID interface of a Sony pad (DualShock 4 / DualSense, USB or Bluetooth), opens the gamepad
collection the way the mod does (shared read), and prints the first reports of each id for a few seconds.
Also asks XInput 1.4 which slots answer (Steam Input / DS4Windows virtual pads show up there).

    uv run tools/padprobe.py [seconds]
"""
import ctypes
import ctypes.wintypes as wt
import sys
import time

setupapi = ctypes.WinDLL("setupapi", use_last_error=True)
hid = ctypes.WinDLL("hid", use_last_error=True)
k32 = ctypes.WinDLL("kernel32", use_last_error=True)


class GUID(ctypes.Structure):
    _fields_ = [("d1", wt.DWORD), ("d2", wt.WORD), ("d3", wt.WORD), ("d4", ctypes.c_ubyte * 8)]


class SP_DEVICE_INTERFACE_DATA(ctypes.Structure):
    _fields_ = [("cbSize", wt.DWORD), ("guid", GUID), ("flags", wt.DWORD), ("reserved", ctypes.c_void_p)]


class HIDD_ATTRIBUTES(ctypes.Structure):
    _fields_ = [("Size", wt.ULONG), ("VendorID", wt.USHORT), ("ProductID", wt.USHORT), ("VersionNumber", wt.USHORT)]


class HIDP_CAPS(ctypes.Structure):
    _fields_ = [("Usage", wt.USHORT), ("UsagePage", wt.USHORT), ("InputReportByteLength", wt.USHORT),
                ("OutputReportByteLength", wt.USHORT), ("FeatureReportByteLength", wt.USHORT),
                ("Reserved", wt.USHORT * 17), ("rest", wt.USHORT * 10)]


INVALID = ctypes.c_void_p(-1).value
k32.CreateFileW.restype = ctypes.c_void_p
k32.CreateFileW.argtypes = [wt.LPCWSTR, wt.DWORD, wt.DWORD, ctypes.c_void_p, wt.DWORD, wt.DWORD, ctypes.c_void_p]
k32.ReadFile.argtypes = [ctypes.c_void_p, ctypes.c_void_p, wt.DWORD, ctypes.POINTER(wt.DWORD), ctypes.c_void_p]
k32.CloseHandle.argtypes = [ctypes.c_void_p]
setupapi.SetupDiGetClassDevsW.restype = ctypes.c_void_p
setupapi.SetupDiGetClassDevsW.argtypes = [ctypes.POINTER(GUID), ctypes.c_void_p, ctypes.c_void_p, wt.DWORD]
setupapi.SetupDiEnumDeviceInterfaces.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(GUID), wt.DWORD,
                                                 ctypes.POINTER(SP_DEVICE_INTERFACE_DATA)]
setupapi.SetupDiGetDeviceInterfaceDetailW.argtypes = [ctypes.c_void_p, ctypes.POINTER(SP_DEVICE_INTERFACE_DATA),
                                                      ctypes.c_void_p, wt.DWORD, ctypes.POINTER(wt.DWORD), ctypes.c_void_p]
for f in ("HidD_GetAttributes", "HidD_GetPreparsedData", "HidD_FreePreparsedData"):
    getattr(hid, f).argtypes = [ctypes.c_void_p, ctypes.c_void_p] if f != "HidD_FreePreparsedData" else [ctypes.c_void_p]
hid.HidP_GetCaps.argtypes = [ctypes.c_void_p, ctypes.POINTER(HIDP_CAPS)]
hid.HidP_GetCaps.restype = ctypes.c_long

SONY = {0x05C4: "DualShock 4 v1", 0x09CC: "DualShock 4 v2", 0x0BA0: "DS4 USB adapter",
        0x0CE6: "DualSense", 0x0DF2: "DualSense Edge"}


def hid_paths():
    g = GUID()
    hid.HidD_GetHidGuid(ctypes.byref(g))
    s = setupapi.SetupDiGetClassDevsW(ctypes.byref(g), None, None, 0x12)  # PRESENT | DEVICEINTERFACE
    i = 0
    while True:
        d = SP_DEVICE_INTERFACE_DATA(cbSize=ctypes.sizeof(SP_DEVICE_INTERFACE_DATA))
        if not setupapi.SetupDiEnumDeviceInterfaces(s, None, ctypes.byref(g), i, ctypes.byref(d)):
            break
        i += 1
        size = wt.DWORD()
        setupapi.SetupDiGetDeviceInterfaceDetailW(s, ctypes.byref(d), None, 0, ctypes.byref(size), None)
        buf = ctypes.create_string_buffer(size.value)
        ctypes.cast(buf, ctypes.POINTER(wt.DWORD))[0] = 8  # cbSize of SP_DEVICE_INTERFACE_DETAIL_DATA_W on x64
        if setupapi.SetupDiGetDeviceInterfaceDetailW(s, ctypes.byref(d), buf, size, None, None):
            yield ctypes.wstring_at(ctypes.addressof(buf) + 4)
    setupapi.SetupDiDestroyDeviceInfoList(ctypes.c_void_p(s))


def main():
    secs = float(sys.argv[1]) if len(sys.argv) > 1 else 3.0
    paths = list(hid_paths())
    print(f"{len(paths)} HID interfaces")
    pads = []
    for p in paths:
        q = k32.CreateFileW(p, 0, 3, None, 3, 0, None)
        sony_path = "054c" in p.lower()
        if q in (None, INVALID):
            if sony_path:
                print(f"  cannot query (error {ctypes.get_last_error()}): {p}")
            continue
        a = HIDD_ATTRIBUTES(Size=ctypes.sizeof(HIDD_ATTRIBUTES))
        ok = hid.HidD_GetAttributes(ctypes.c_void_p(q), ctypes.byref(a))
        if ok and a.VendorID == 0x054C:
            pp = ctypes.c_void_p()
            caps = HIDP_CAPS()
            if hid.HidD_GetPreparsedData(ctypes.c_void_p(q), ctypes.byref(pp)):
                hid.HidP_GetCaps(pp, ctypes.byref(caps))
                hid.HidD_FreePreparsedData(pp)
            name = SONY.get(a.ProductID, f"unknown pid {a.ProductID:#06x}")
            print(f"  Sony {name}: usage {caps.UsagePage:#x}/{caps.Usage:#x} input len {caps.InputReportByteLength}: {p}")
            if caps.UsagePage == 1 and caps.Usage == 5 and a.ProductID in SONY:
                pads.append((p, name, caps.InputReportByteLength))
        elif sony_path:
            print(f"  Sony-looking path, attributes ok={ok} vid {a.VendorID:#06x} pid {a.ProductID:#06x}: {p}")
        k32.CloseHandle(ctypes.c_void_p(q))
    for p, name, n in pads:
        h = k32.CreateFileW(p, 0x80000000, 3, None, 3, 0, None)  # GENERIC_READ, shared, synchronous
        if h in (None, INVALID):
            print(f"{name}: cannot open for reading (error {ctypes.get_last_error()}): another program holds it exclusively?")
            continue
        print(f"{name}: reading for {secs:.0f} s (move a stick or press a button)")
        buf = ctypes.create_string_buffer(n)
        got = wt.DWORD()
        seen = {}
        end = time.time() + secs
        while time.time() < end:
            if not k32.ReadFile(ctypes.c_void_p(h), buf, n, ctypes.byref(got), None):
                print(f"  read failed (error {ctypes.get_last_error()})")
                break
            b = buf.raw[:got.value]
            seen.setdefault(b[0], [0, b])[0] += 1
            seen[b[0]][1] = b
        for rid, (count, last) in sorted(seen.items()):
            print(f"  report {rid:#04x} x{count}, {len(last)} bytes, last: {last[:16].hex(' ')}")
        k32.CloseHandle(ctypes.c_void_p(h))
    try:
        x = ctypes.WinDLL("xinput1_4")
        st = ctypes.create_string_buffer(16)
        print("XInput 1.4 slots:", " ".join("connected" if x.XInputGetState(i, st) == 0 else "-" for i in range(4)))
    except OSError:
        print("XInput 1.4 not available")


if __name__ == "__main__":
    main()
