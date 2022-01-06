import json
import threading
import time

from SimpleWebSocketServer import SimpleWebSocketServer, WebSocket


CONTROLS = ["forward", "backward", "left", "right"]

def stream_data() -> None:
    """
    Streams physiological data to the client
    TODO: Get the real data from the sensors
    """
    if "gui" not in types:
        return -1
    while True:
        if stream_physiological:
            types["gui"].sendMessage(
                json.dumps({
                    "physiological":[60, 2000, 3000]
                    })) # Fake data
        time.sleep(2)


def evaluate() -> None:
    """
    Determines the action to be taken based on the current
    state of the player
    TODO: Implement a real agent using the EBM's data
    """
    while True:
        if mode == "automatic":
            actions = [1, 0, 0, 0]  # Replace with an agent
            choices = [1 if action > 0 else 0 for action in actions]
            message = {"movement": {}}
            for i, choice in enumerate(choices):
                message["movement"][CONTROLS[i]] = choice
            types["gui"].sendMessage(json.dumps(message))
            print(message)
        time.sleep(2)


class Messenger(WebSocket):
    """Intrepret messages from the clients and forward them"""
    def __init__(self, *args, **kwargs):
        WebSocket.__init__(self, *args, **kwargs)

    def handleMessage(self):
        if self.data is None:
            self.data = ""

        if self.data[0] != "{":
            try:
                audio_id = int(self.data)
                if "gui" in types:
                    types["gui"].sendMessage(json.dumps({"audio_id": audio_id}))
            except Exception as e:
                pass

        try:
            data = json.loads(self.data)
            if type(data) == type(dict()):
                if "type" in data:
                    types[data["type"]] = self
                    if data["type"] == "gui":
                        self.sendMessage(json.dumps({"mode": "manual"}))
                        threading.Thread(target=stream_data).start()
                if "mode" in data:
                    global mode
                    mode = data["mode"]
                    if data["mode"] == "manual":
                        self.sendMessage(json.dumps({"mode": "manual"}))
                    elif data["mode"] == "automatic":
                        self.sendMessage(json.dumps({"mode": "automatic"}))
                if "position" in data:
                    global position
                    position = [
                        data["position"][0] / 2048,
                        data["position"][1] / 100,
                        data["position"][2] / 2048,
                    ]
                    for client in wsclients:
                        if client != self:
                            client.sendMessage(
                                f"{int(-962 + 3178*data['position'][0]/2048)},{int(-3076 + 4596*(2048 - data['position'][2])/2048)}"
                            )

        except Exception as e:
            print(e)
            try:
                data = json.loads(self.data)
            except Exception as e:
                print("Error:", e)

    def handleConnected(self):
        wsclients.append(self)
        print(self.address, "connected")

    def handleClose(self):
        wsclients.remove(self)
        print(self.address, "closed")


if __name__ == "__main__":
    mode = "manual"
    stream_physiological = False

    wsclients = []
    types = {}

    position = [0, 0, 0]

    threading.Thread(target=evaluate).start()

    server = SimpleWebSocketServer("", 9002, Messenger)
    server.serveforever()
