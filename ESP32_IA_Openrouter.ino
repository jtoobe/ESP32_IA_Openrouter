/*
    ==========================================================
    ESP32 + OPENROUTER
    ==========================================================

    Ejemplo educativo para alumnos.

    El programa:

    1) Se conecta a una red WiFi.
    2) Envía una pregunta a OpenRouter.
    3) Obtiene una respuesta de IA.
    4) Muestra la respuesta por el Monitor Serie.

    ==========================================================
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

//----------------------------------------------------------
// CONFIGURACION WIFI
//----------------------------------------------------------

const char* ssid = "tu wifi";
const char* password = "tu password";

//----------------------------------------------------------
// CONFIGURACION OPENROUTER
//----------------------------------------------------------

const char* OPENROUTER_API_KEY = "tu key de openrouter";

const char* OPENROUTER_URL =
"https://openrouter.ai/api/v1/chat/completions";

//----------------------------------------------------------
// MODELOS: dejar asi para que openrouter elija el modelo o fijarlos explicitamente:

// const char* modelosIA[] =
// {
//     "google/gemini-2.5-flash",
//     "meta-llama/llama-3.3-70b-instruct",
//     "mistralai/mistral-small-3.1"
// };

//----------------------------------------------------------

const int CANTIDAD_MODELOS_IA = 1;

const char* modelosIA[CANTIDAD_MODELOS_IA] =
{
    "openrouter/free"
};

int indiceModeloIA = 0;

//----------------------------------------------------------
// HARDWARE
//----------------------------------------------------------

const int LED_WIFI = 2;

//----------------------------------------------------------
// VARIABLES
//----------------------------------------------------------

bool consultaRealizada = false;

//----------------------------------------------------------
// CONEXION WIFI
//----------------------------------------------------------

bool conectarWiFi()
{
    Serial.println();
    Serial.println("Conectando a WiFi...");

    pinMode(LED_WIFI, OUTPUT);
    digitalWrite(LED_WIFI, LOW);

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    WiFi.begin(ssid, password);

    uint32_t inicio = millis();

    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);

        if (millis() - inicio > 30000)
        {
            Serial.println();
            Serial.println("Tiempo de espera agotado");
            return false;
        }
    }

    Serial.println();
    Serial.println("WiFi conectado");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    digitalWrite(LED_WIFI, HIGH);

    return true;
}

//----------------------------------------------------------
// LLAMADA HTTP
//----------------------------------------------------------

String llamarIAConModelo(
    const String &prompt,
    const String &modelo)
{
    WiFiClientSecure client;

    // Para simplificar el ejemplo
    client.setInsecure();  // tratar de no usar este modo por su nula seguridad

    HTTPClient http;

    http.setConnectTimeout(10000);
    http.setTimeout(45000);

    if (!http.begin(client, OPENROUTER_URL))
    {
        Serial.println("No se pudo iniciar HTTPS");
        return "";
    }

    //------------------------------------------------------
    // CABECERAS
    //------------------------------------------------------

    http.addHeader(
        "Content-Type",
        "application/json");

    http.addHeader(
        "Authorization",
        "Bearer " + String(OPENROUTER_API_KEY));

    http.addHeader(
        "HTTP-Referer",
        "https://github.com/jtoobe");

    http.addHeader(
        "X-Title",
        "ESP32 IA Demo");

    //------------------------------------------------------
    // CREACION DEL JSON
    //------------------------------------------------------

    DynamicJsonDocument requestDoc(2048);

    requestDoc["model"] = modelo;

    JsonArray messages =
        requestDoc.createNestedArray("messages");

    JsonObject msg =
        messages.createNestedObject();

    msg["role"] = "user";
    msg["content"] = prompt;

    String requestBody;

    serializeJson(requestDoc, requestBody);

    Serial.println();
    Serial.println("JSON ENVIADO:");
    Serial.println(requestBody);

    //------------------------------------------------------
    // POST
    //------------------------------------------------------

    int httpCode = http.POST(requestBody);

    Serial.print("HTTP CODE: ");
    Serial.println(httpCode);

    String respuesta = "";

    //------------------------------------------------------
    // RESPUESTA
    //------------------------------------------------------

    String payload = http.getString();

    Serial.println();
    Serial.println("RESPUESTA CRUDA:");
    Serial.println("--------------------------------");
    Serial.println(payload);
    Serial.println("--------------------------------");

    if (httpCode == 200)
    {
        DynamicJsonDocument responseDoc(32768);  // aumentar a 65536 si es insuficiente el contexto, pero se corre el riesgo de saturar al ESP32

        DeserializationError error =
            deserializeJson(responseDoc, payload);

        if (error)
        {
            Serial.print("Error JSON: ");
            Serial.println(error.c_str());

            http.end();
            return "";
        }

        if (responseDoc.containsKey("error"))
        {
            Serial.println("OpenRouter devolvio un error");

            String mensaje =
                responseDoc["error"]["message"] | "";

            Serial.println(mensaje);

            http.end();
            return "";
        }

        respuesta =
            responseDoc["choices"][0]
                       ["message"]
                       ["content"] |
            "";

        respuesta.trim();
    }
    else
    {
        Serial.println("Error HTTP");
    }

    http.end();

    return respuesta;
}

//----------------------------------------------------------
// FUNCION PRINCIPAL DE IA
//----------------------------------------------------------

String llamarIA(const String &prompt)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi desconectado");
        return "";
    }

    String modelo =
        modelosIA[indiceModeloIA];

    indiceModeloIA++;

    if (indiceModeloIA >= CANTIDAD_MODELOS_IA)
        indiceModeloIA = 0;

    Serial.println();
    Serial.println("--------------------------------");
    Serial.println("Modelo elegido: " + modelo);
    Serial.println("--------------------------------");

    return llamarIAConModelo(
        prompt,
        modelo);
}

//----------------------------------------------------------
// SETUP
//----------------------------------------------------------

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("================================");
    Serial.println("ESP32 + OPENROUTER");
    Serial.println("================================");

    conectarWiFi();
}

//----------------------------------------------------------
// LOOP
//----------------------------------------------------------

void loop()
{
    if (!consultaRealizada)
    {
        String pregunta =
            "Explica la funcion de Cristobal Colon "
            "en America en menos de 15 palabras.";   // importante limitar el tanaño de la respuesta 

        String respuesta =
            llamarIA(pregunta);

        if (respuesta.length() == 0)
        {
            respuesta =
                "No fue posible obtener respuesta.";
        }

        Serial.println();
        Serial.println("================================");
        Serial.println("RESPUESTA IA");
        Serial.println("================================");
        Serial.println(respuesta);
        Serial.println("================================");

        consultaRealizada = true;
    }

    //------------------------------------------------------
    // Reconexion automatica WiFi
    //------------------------------------------------------

    if (WiFi.status() != WL_CONNECTED)
    {
        digitalWrite(LED_WIFI, LOW);

        Serial.println("WiFi perdido");

        WiFi.reconnect();

        delay(5000);
    }

    delay(100);
}
