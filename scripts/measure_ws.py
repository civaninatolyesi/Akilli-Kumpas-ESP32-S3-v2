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


def on_message(ws, message):
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

    print(
        f"mm={mm:.2f} inch={inch:.4f} unit={'inch' if is_inch else 'mm'} "
        f"sign={'-' if is_neg else '+'} age={age_ms} raw=0x{raw_key:06X}"
    )


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
