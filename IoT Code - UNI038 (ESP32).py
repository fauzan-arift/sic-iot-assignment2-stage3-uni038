import network
import machine
import ujson
import uasyncio as asyncio
from umqtt.simple import MQTTClient

# 🔹 Konfigurasi WiFi
SSID = "POCO M3"
PASSWORD = "Palem2f8771"

station = network.WLAN(network.STA_IF)
station.active(True)
station.connect(SSID, PASSWORD)

print("⏳ Menghubungkan ke WiFi...")
while not station.isconnected():
    asyncio.sleep(1)
print("✅ Terhubung ke WiFi! IP:", station.ifconfig()[0])

# 🔹 MQTT Konfigurasi (Ubidots)
MQTT_BROKER = "industrial.api.ubidots.com"
MQTT_PORT = 1883
MQTT_TOPIC = "/v1.6/devices/esp32/lamp"
MQTT_TOKEN = "BBUS-4p3JS6lOPE6pouh8uHFWg3CjWkBm7B"

# 🔹 Konfigurasi Lampu & Servo
lampu = machine.Pin(2, machine.Pin.OUT)  # Lampu di Pin D2
servo = machine.PWM(machine.Pin(4), freq=50)  # Servo di Pin D4
status = 0  # Status awal (lampu mati, servo 0°)

# 🔹 Fungsi menggerakkan servo
def set_servo_angle(angle):
    duty = int((angle / 180) * 102 + 26)  # Konversi ke nilai duty PWM (26 - 128)
    servo.duty(duty)

# 🔹 Callback MQTT
def on_message(topic, msg):
    global status
    print(f"📩 Pesan dari Ubidots: {msg.decode()}")
    
    try:
        data = ujson.loads(msg.decode())
        new_status = int(data.get("value", 0))

        if new_status != status:
            status = new_status
            if status == 1:
                lampu.value(1)
                set_servo_angle(60)
                print("💡 Lampu Nyala & Servo ke 180°")
            else:
                lampu.value(0)
                set_servo_angle(0)
                print("🌑 Lampu Mati & Servo ke 0°")
    
    except Exception as e:
        print("❌ Error parsing JSON:", e)

# 🔹 MQTT Task
async def mqtt_task():
    global client
    try:
        client = MQTTClient("ESP32", MQTT_BROKER, user=MQTT_TOKEN, password="")
        client.set_callback(on_message)
        client.connect()
        client.subscribe(MQTT_TOPIC)
        print("✅ Terhubung ke Ubidots MQTT!")

        while True:
            client.check_msg()  # Cek pesan MQTT tanpa blocking
            await asyncio.sleep(0.1)
    
    except Exception as e:
        print("⚠️ Error MQTT:", e)
        await asyncio.sleep(5)  # Coba reconnect

# 🔹 Fungsi utama
async def main():
    asyncio.create_task(mqtt_task())  
    while True:
        if not station.isconnected():
            print("🔄 Reconnecting to WiFi...")
            station.connect(SSID, PASSWORD)
            while not station.isconnected():
                await asyncio.sleep(1)
            print("✅ WiFi Reconnected!")
        
        await asyncio.sleep(5)  # Cek koneksi setiap 5 detik

# 🔹 Jalankan program
asyncio.run(main())
