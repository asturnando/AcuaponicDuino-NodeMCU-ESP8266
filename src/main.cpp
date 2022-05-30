/*
  AcuaponicDuino - Automatización y control de cultivos acuapónicos con Arduino y Node-RED sobre protocolo MQTT
  Codigo perteneciente al TFG del Grado de Ingenieria Informatica de la Universitat Oberta de Catalunya
  Autor: Fernando Suarez Rodriguez
  Fecha: 2022/06/06
  Version: 1.0
  Descripcion: Programa que controlara el funcionamiento de la placa que dota de comunicaciones al sistema un ESP8266 NodeMCU.
  Al contrario que la placa principal se ha optado por superloop para su programacion ya que no necesita realizar mas tareas que las de enviar y recibir datos a
  Node-RED mediante MQTT. Enviara todos los datos que recibe de la placa principal por el puerto serial y escribira en el mismo  puerto los datos que recibe de MQTT.
*/

#include <ESP8266WiFi.h> //Librería proporciona las rutinas específicas WiFi de ESP8266
#include <DNSServer.h> //Librería proporciona las rutinas de DNS
#include <ESP8266WebServer.h > //Librería proporciona las rutinas de servidor web de ESP8266
#include <WiFiManager.h> //A dministrador de conexión WiFi Espressif ESPx con portal de configuración web alternativo
#include <Ticker.h> //Librería proporciona las rutinas de temporización
#include <Arduino.h> //Librería proporciona las rutinas de Arduino
#include <PubSubClient.h> //Librería proporciona las rutinas de MQTT

// Instancia a la clase Ticker para el temporizador
Ticker ticker;
// Pin LED azul
byte pinLed = D4;
// Declaramos el cliente MQTT asi como la direccion ip del broker
WiFiClient espClient;
PubSubClient client;
IPAddress server(192, 168, 1, 102);

// Funciones para la comunicacion MQTT
void callback(char *topic, byte *payload, unsigned int length);
void reconnect();
// Parpadeo del led mientras no esta conectado a la wifi y esta en modo AP
void blink();

void setup()
{
  // Inicializamos el puerto serial
  Serial.begin(115200);

  // Modo del pin
  pinMode(pinLed, OUTPUT);

  // Empezamos el temporizador que hará parpadear el LED
  ticker.attach(0.2, blink);

  // Creamos una instancia de la clase WiFiManager
  WiFiManager wifiManager;
  // Desactivamos la salida por el puerto serial para que no influya con la placa de control
  wifiManager.setDebugOutput(false);
  // Descomentar en caso de necesitar un reset
  // wifiManager.setDebugOutput(false);

  // Cremos AP y portal cautivo y comprobamos si se establece la conexión
  if (!wifiManager.autoConnect("AcuaponicDuino"))
  {
    //Serial.println("Fallo en la conexión (timeout)");
    ESP.reset();
    delay(1000);
  }
  // Eliminamos el temporizador
  ticker.detach();
  // Apagamos el LED
  digitalWrite(pinLed, HIGH);
  // Configuramos la conexion a MQTT
  client.setClient(espClient);
  client.setServer(server, 1883);
  client.setCallback(callback);
}

void loop()
{
  // Si no esta conectado a MQTT se intentara conectar
  if (!client.connected())
  {
    reconnect();
  }
  // Si esta conectado se comprueba si hay mensajes MQTT
  client.loop();
}

void blink()
{
  // Cambiar de estado el LED
  byte estado = digitalRead(pinLed);
  digitalWrite(pinLed, !estado);
}

void callback(char *topic, byte *payload, unsigned int length)
{
  // Recibe los datos de MQTT y los formatea para escribirlos por el puerto serial posteriormente
  // activa y desactiva el LED para indicar que se ha recibido un dato
  String content = "";
  for (unsigned int i = 0; i < length; i++)
  {
    content.concat((char)payload[i]);
    digitalWrite(LED_BUILTIN, LOW);
  }
  Serial.print("R [");
  Serial.print(topic);
  Serial.print("] <");
  Serial.print(content);
  Serial.println(">");
  delay(200);
  content = "";
  digitalWrite(LED_BUILTIN, HIGH);
}

void reconnect()
{
  // Mensajes debug desactivados para que no influyan en la placa de control
  bool correcto = false;
  // Bucle principal si no esta conectado intentara seguir conectandose
  while (!client.connected())
  {
    // Serial.println("Intentando reestablecer la conexion MQTT...");
    // Si se consigue una conexion se suscribe a los topics de los que se quiere recibir datos
    if (client.connect("ESP8266Client"))
    {
      // Serial.println("Conectado al servidor MQTT");
      // Serial.print("Estado, rc=");
      // Serial.println(client.state());
      //  Nos suscribimos a todos los topics
      correcto = client.subscribe("AcuaponicDuino/Commands");
      if (correcto == true)
      {
        Serial.println("Se ha realizado la suscripcion a COMMANDS correctamente");
      }
      correcto = client.subscribe("AcuaponicDuino/Config/Agua");
      if (correcto == true)
      {
        Serial.println("Se ha realizado la suscripcion a CONFIG/AGUA correctamente");
      }
      correcto = client.subscribe("AcuaponicDuino/Config/Ambiente");
      if (correcto == true)
      {
        Serial.println("Se ha realizado la suscripcion a CONFIG/AMBIENTE correctamente");
      }
      correcto = client.subscribe("AcuaponicDuino/Config/Flujo");
      if (correcto == true)
      {
        Serial.println("Se ha realizado la suscripcion a CONFIG/FLUJO correctamente");
      }
      correcto = client.subscribe("AcuaponicDuino/Config/Temperatura");
      if (correcto == true)
      {
        Serial.println("Se ha realizado la suscripcion a CONFIG/TEMPERATURA correctamente");
      }
    }
    else
    {
      // Si no hemos podido conectarnos lo intentamos en cincos segundos
      //Serial.print("Fallo de conexion, rc=");
      //Serial.print(client.state());
      //Serial.println(" Reconctando en 5 segundos");
      delay(5000);
    }
  }
}

void serialEvent()
{
  // Recibe los datos del puerto serial y los envia por MQTT
  String mensaje = "";
  String topic, command;
  char payload[200];
  char length;
  // Si hay datos en el puerto serial
  if (Serial.available() > 0)
  {
    // Se reciben hasta recibir un salto de linea
    mensaje = Serial.readStringUntil('\n');
    if (mensaje != "")
    {
      // Si es un mensaje de envio de datos que proviene de la placa de control se formatea para enviar por MQTT
      if (mensaje.startsWith("S"))
      {
        // Se separa el topic y el comando
        topic = mensaje.substring(mensaje.indexOf("[") + 1, mensaje.indexOf("]"));
        command = mensaje.substring(mensaje.indexOf("<") + 1, mensaje.indexOf(">"));
        // En funcion del topic se envia el comando correspondiente
        if (topic == "AcuaponicDuino/Ambiente/Temperatura")
        {
          length = command.length() + 1;
          command.toCharArray(payload, length);
          client.publish(topic.c_str(), payload);
          mensaje = "";
          topic = "";
          command = "";
        }
        if (topic == "AcuaponicDuino/Ambiente/Humedad")
        {
          length = command.length() + 1;
          command.toCharArray(payload, length);
          client.publish(topic.c_str(), payload);
          mensaje = "";
          topic = "";
          command = "";
        }
        if (topic == "AcuaponicDuino/Ambiente/Luz")
        {
          length = command.length() + 1;
          command.toCharArray(payload, length);
          client.publish(topic.c_str(), payload);
          mensaje = "";
          topic = "";
          command = "";
        }
        if (topic == "AcuaponicDuino/Flujo/Entrada")
        {
          length = command.length() + 1;
          command.toCharArray(payload, length);
          client.publish(topic.c_str(), payload);
          mensaje = "";
          topic = "";
          command = "";
        }
        if (topic == "AcuaponicDuino/Flujo/Salida")
        {
          length = command.length() + 1;
          command.toCharArray(payload, length);
          client.publish(topic.c_str(), payload);
          mensaje = "";
          topic = "";
          command = "";
        }
        if (topic == "AcuaponicDuino/Agua/TDS")
        {
          length = command.length() + 1;
          command.toCharArray(payload, length);
          client.publish(topic.c_str(), payload);
          mensaje = "";
          topic = "";
          command = "";
        }
        if (topic == "AcuaponicDuino/Agua/pH")
        {
          length = command.length() + 1;
          command.toCharArray(payload, length);
          client.publish(topic.c_str(), payload);
          mensaje = "";
          topic = "";
          command = "";
        }
        if (topic == "AcuaponicDuino/Agua/Temperatura")
        {
          length = command.length() + 1;
          command.toCharArray(payload, length);
          client.publish(topic.c_str(), payload);
          mensaje = "";
          topic = "";
          command = "";
        }
        if (topic == "AcuaponicDuino/Start/Flujo")
        {
          length = command.length() + 1;
          command.toCharArray(payload, length);
          client.publish(topic.c_str(), payload);
          mensaje = "";
          topic = "";
          command = "";
        }
        if (topic == "AcuaponicDuino/Start/Agua")
        {
          length = command.length() + 1;
          command.toCharArray(payload, length);
          client.publish(topic.c_str(), payload);
          mensaje = "";
          topic = "";
          command = "";
        }
        if (topic == "AcuaponicDuino/Start/Ambiental")
        {
          length = command.length() + 1;
          command.toCharArray(payload, length);
          client.publish(topic.c_str(), payload);
          mensaje = "";
          topic = "";
          command = "";
        }
        if (topic == "AcuaponicDuino/Start/TempAgua")
        {
          length = command.length() + 1;
          command.toCharArray(payload, length);
          client.publish(topic.c_str(), payload);
          mensaje = "";
          topic = "";
          command = "";
        }
        if (topic == "AcuaponicDuino/Config/Stop")
        {
          length = command.length() + 1;
          command.toCharArray(payload, length);
          client.publish(topic.c_str(), payload);
          mensaje = "";
          topic = "";
          command = "";
        }
      }
    }
  }
}