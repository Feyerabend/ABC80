import network, time, sys, gc
from machine import WDT, Pin
import machine

print('boot..')
time.sleep(1)   # flash window - mpremote can connect right after replug

# -- Reset cause
_cause = machine.reset_cause()
_NAMES = {1: 'power-on', 2: 'hard-reset', 3: 'WATCHDOG', 5: 'soft-reset'}
print('reset cause:', _NAMES.get(_cause, 'unknown ({})'.format(_cause)))

# -- LED (Pico W/2W: connected via CYW43)
try:
    led = Pin('LED', Pin.OUT)
    led.on()
except Exception:
    led = None

# -- WiFi AP
# Watchdog is NOT started yet - AP init timing is unpredictable.
ap = network.WLAN(network.AP_IF)
ap.active(True)
ap.config(ssid='PicoFS', password='pico1234')

deadline = time.ticks_add(time.ticks_ms(), 10000)
while not ap.active():
    if time.ticks_diff(deadline, time.ticks_ms()) <= 0:
        print('AP failed to start')
        raise SystemExit
    time.sleep(0.1)

ip = ap.ifconfig()[0]
if ip == '0.0.0.0':
    print('AP gave no IP')
    raise SystemExit

print('AP up  ssid=PicoFS  ip={}  pw=pico1234'.format(ip))

# -- Watchdog - started only after AP is confirmed up
# Fed every ~1 s in the server accept loop.
wdt = WDT(timeout=8000)
wdt.feed()

# -- SD card (optional)
fs = None
try:
    from file_server import mount_sd
    fs = mount_sd()
    print('SD mounted OK')
except Exception as e:
    sys.print_exception(e)
    print('SD unavailable - file ops will return 503')

wdt.feed()

# -- HTTP server
from server import HTTPServer

if led:
    led.off()   # boot complete - LED now blinks on each request

print('free RAM: {} B'.format(gc.mem_free()))
print('open http://{}  in browser'.format(ip))

srv = HTTPServer(fs, wdt=wdt, led=led)

while True:
    try:
        srv.serve(80)
    except KeyboardInterrupt:
        print('stopped')
        if led:
            led.off()
        break
    except Exception as e:
        sys.print_exception(e)
        print('server crashed - restarting in 2 s')
        wdt.feed()
        time.sleep(2)
        wdt.feed()
