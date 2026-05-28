import json
import time
import sys

try:
    from websocket import WebSocketApp
except Exception as exc:
    raise SystemExit(
        "websocket-client gerekli. Kurulum: pip install websocket-client\n"
        "Eger 'websocket' paketi yuku ise kaldir: pip uninstall websocket"
    ) from exc

WS_URL = "ws://kumpas.local:81/"
ENTER_DELAY_SEC = 0.1
MIN_PRESS_INTERVAL_SEC = 0.2
DECIMALS = 2
DECIMAL_COMMA = True

try:
    import pyautogui
except Exception as exc:
    raise SystemExit(
        "pyautogui gerekli. Kurulum: pip install pyautogui\n"
        "Not: aktif pencereye yazar; hedef uygulama odakta olmali"
    ) from exc

last_btn_pressed = None
last_press_ts = 0.0


def on_message(ws, message):
    global last_btn_pressed, last_press_ts
    try:
        data = json.loads(message)
    except json.JSONDecodeError:
        print("invalid json:", message)
        return

    mm = data.get("mm")
    inch = data.get("inch")
    is_inch = data.get("isInch")
    is_neg = data.get("isNegative")
    age_ms = data.get("ageMs")
    raw_key = data.get("rawKey")
    btn_pressed = data.get("buttonPressed")

    print(
        f"mm={mm:.2f} inch={inch:.4f} unit={'inch' if is_inch else 'mm'} "
        f"sign={'-' if is_neg else '+'} age={age_ms} raw=0x{raw_key:06X}"
    )

    if btn_pressed is None or mm is None:
        return

    now = time.monotonic()
    if last_btn_pressed is None:
        last_btn_pressed = btn_pressed
        return

    if (not last_btn_pressed) and btn_pressed:
        if (now - last_press_ts) < MIN_PRESS_INTERVAL_SEC:
            last_btn_pressed = btn_pressed
            return

        value = f"{mm:.{DECIMALS}f}"
        if DECIMAL_COMMA:
            value = value.replace(".", ",")

        pyautogui.typewrite(value, interval=0.0)
        time.sleep(ENTER_DELAY_SEC)
        pyautogui.press("enter")
        last_press_ts = now

    last_btn_pressed = btn_pressed


def on_error(ws, error):
    print("ws error:", error)


def on_close(ws, close_status_code, close_msg):
    print("ws closed", close_status_code, close_msg)


def on_open(ws):
    print("ws connected")


def main():
    while True:
        ws = WebSocketApp(
            WS_URL,
            on_open=on_open,
            on_message=on_message,
            on_error=on_error,
            on_close=on_close,
        )
        ws.run_forever(ping_interval=20, ping_timeout=5)
        time.sleep(2)


if __name__ == "__main__":
    main()
