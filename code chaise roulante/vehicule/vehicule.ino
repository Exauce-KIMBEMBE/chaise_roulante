/***********************************************************************
 * Projet : Véhicule intelligent ESP32
 *
 * Modes :
 * - IR par défaut
 * - Interface Web via WiFi + SPIFFS
 * - Commande vocale via BluetoothSerial
 *
 * Bluetooth :
 * - Nom : RoverLink_BT
 * - Commandes attendues :
 *   avant, arriere, gauche, droite, stop,
 *   klaxon, feuxavant, feuxrouges, mute
 *
 ***********************************************************************/

#define DECODE_NEC

#include <IRremote.hpp>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <BluetoothSerial.h>

BluetoothSerial SerialBT;
WebServer server(80);

// ==================== WIFI ====================

const char* WIFI_SSID = "RoverLink";
const char* WIFI_PASSWORD = "12345678";

// ==================== MODES ====================

enum ModeCommande {
    MODE_IR,
    MODE_WEB,
    MODE_VOICE
};

ModeCommande modeActuel = MODE_IR;

// ==================== IR ====================

#define IR_PIN 25

// ==================== ULTRASON ====================

#define TRIG_AVANT 22
#define ECHO_AVANT 21

#define TRIG_ARRIERE 18
#define ECHO_ARRIERE 19

#define DISTANCE_DANGER 15

// ==================== MOTEURS ====================

#define MG_IN1 12
#define MG_IN2 14

#define MD_IN1 27
#define MD_IN2 26

// ==================== ACCESSOIRES ====================

#define KLAXON_PIN 1      // TX
#define LED_AVANT 23

#define LAMPE_JAUNE_GAUCHE 13
#define LAMPE_JAUNE_DROITE 3   // RX

#define LAMPE_ROUGE_GAUCHE 33
#define LAMPE_ROUGE_DROITE 32

// ==================== COMMANDES IR ====================

#define CMD_AVANT   0x18
#define CMD_ARRIERE 0x52
#define CMD_GAUCHE  0x08
#define CMD_DROITE  0x5A
#define CMD_KLAXON  0x1C
#define CMD_CH      0x46
#define CMD_PAUSE   0x43

// ==================== ETATS ====================

bool ledAvantEtat = false;
bool feuxRougesEtat = false;

bool clignotantActif = false;
bool clignotantGauche = false;

bool alerteAvant = false;
bool alerteArriere = false;
bool alarmeMuette = false;

bool etatBlink = false;

uint16_t derniereCommande = 0;

// ==================== TEMPS ====================

unsigned long dernierSignalMoteur = 0;
unsigned long dernierSignalKlaxon = 0;
unsigned long dernierSignalVirage = 0;
unsigned long dernierBlink = 0;
unsigned long dernierControleObstacle = 0;

const unsigned long timeoutMoteur = 300;
const unsigned long timeoutKlaxon = 300;
const unsigned long timeoutVirage = 300;

const unsigned long intervalBlink = 150;
const unsigned long intervalControleObstacle = 200;

// ==================== PROTOTYPES ====================

void setupServeurWeb();
void lireTelecommande();
void lireBluetooth();

void traiterCommandeIR(uint16_t cmd);
void traiterCommandeTexte(String action);

float mesurerDistance(int trigPin, int echoPin);
bool dangerAvant();
bool dangerArriere();

void gererAlerteObstacle();
void arreterAlerte();

void avancer();
void reculer();
void tournerGauche();
void tournerDroite();
void stopMoteurs();

void activerClignotant(bool gauche);
void gererClignotants();

void eteindreJaunes();
void appliquerFeuxRouges();

// ==================== SETUP ====================

void setup() {
    pinMode(IR_PIN, INPUT_PULLUP);

    pinMode(TRIG_AVANT, OUTPUT);
    pinMode(ECHO_AVANT, INPUT);

    pinMode(TRIG_ARRIERE, OUTPUT);
    pinMode(ECHO_ARRIERE, INPUT);

    pinMode(MG_IN1, OUTPUT);
    pinMode(MG_IN2, OUTPUT);
    pinMode(MD_IN1, OUTPUT);
    pinMode(MD_IN2, OUTPUT);

    pinMode(KLAXON_PIN, OUTPUT);
    pinMode(LED_AVANT, OUTPUT);

    pinMode(LAMPE_JAUNE_GAUCHE, OUTPUT);
    pinMode(LAMPE_JAUNE_DROITE, OUTPUT);
    pinMode(LAMPE_ROUGE_GAUCHE, OUTPUT);
    pinMode(LAMPE_ROUGE_DROITE, OUTPUT);

    stopMoteurs();

    digitalWrite(KLAXON_PIN, LOW);
    digitalWrite(LED_AVANT, LOW);

    eteindreJaunes();
    appliquerFeuxRouges();

    IrReceiver.begin(IR_PIN, DISABLE_LED_FEEDBACK);

    SPIFFS.begin(true);

    WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);

    SerialBT.begin("RoverLink_BT");

    setupServeurWeb();
}

// ==================== LOOP ====================

void loop() {
    server.handleClient();

    if (modeActuel == MODE_IR) {
        lireTelecommande();
    }

    if (modeActuel == MODE_VOICE) {
        lireBluetooth();
    }

    gererClignotants();
    gererAlerteObstacle();

    if (modeActuel != MODE_VOICE) {
        if (millis() - dernierSignalMoteur > timeoutMoteur) {
            stopMoteurs();
        }
    }

    if (!alerteAvant && !alerteArriere) {
        if (millis() - dernierSignalKlaxon > timeoutKlaxon) {
            digitalWrite(KLAXON_PIN, LOW);
        }
    }

    if (clignotantActif && millis() - dernierSignalVirage > timeoutVirage) {
        clignotantActif = false;

        eteindreJaunes();
        appliquerFeuxRouges();
    }
}

// ==================== SERVEUR WEB ====================

void setupServeurWeb() {

    // ==================== INDEX ====================

    server.on("/", HTTP_GET, []() {

        File file = SPIFFS.open(
            "/index.html",
            "r"
        );

        server.streamFile(
            file,
            "text/html"
        );

        file.close();

    });


    // ==================== CSS ====================

    server.on("/style.css", HTTP_GET, []() {

        File file = SPIFFS.open(
            "/style.css",
            "r"
        );

        server.streamFile(
            file,
            "text/css"
        );

        file.close();

    });


    // ==================== JS ====================

    server.on("/app.js", HTTP_GET, []() {

        File file = SPIFFS.open(
            "/app.js",
            "r"
        );

        server.streamFile(
            file,
            "application/javascript"
        );

        file.close();

    });


    // ==================== MODE ====================

    server.on("/mode", HTTP_GET, []() {

        String value =
            server.arg("value");

        stopMoteurs();

        arreterAlerte();


        if (value == "ir") {

            modeActuel = MODE_IR;

        }

        else if (value == "web") {

            modeActuel = MODE_WEB;

        }

        else if (value == "voice") {

            modeActuel = MODE_VOICE;

        }

        server.send(
            200,
            "text/plain",
            "OK"
        );

    });


    // ==================== COMMANDES ====================

    server.on("/cmd", HTTP_GET, []() {

        String action =
            server.arg("action");

        // Le WEB ne fonctionne
        // qu'en mode WEB

        if (
            modeActuel == MODE_WEB
        ) {

            traiterCommandeTexte(
                action
            );

        }

        server.send(
            200,
            "text/plain",
            "OK"
        );

    });


    // ==================== ETAT ====================

    server.on("/etat", HTTP_GET, []() {

        String mode = "ir";

        if (modeActuel == MODE_WEB) {

            mode = "web";

        }

        else if (modeActuel == MODE_VOICE) {

            mode = "voice";

        }


        bool clignoteGauche =

            clignotantActif &&
            clignotantGauche &&
            etatBlink;


        bool clignoteDroite =

            clignotantActif &&
            !clignotantGauche &&
            etatBlink;


        if (alerteAvant || alerteArriere) {

            clignoteGauche = etatBlink;
            clignoteDroite = etatBlink;

        }


        String json = "{";

        json += "\"mode\":\"" + mode + "\",";

        json += "\"ledAvant\":";
        json += ledAvantEtat ? "true" : "false";
        json += ",";

        json += "\"feuxRouges\":";
        json += feuxRougesEtat ? "true" : "false";
        json += ",";

        json += "\"clignotantGauche\":";
        json += clignoteGauche ? "true" : "false";
        json += ",";

        json += "\"clignotantDroite\":";
        json += clignoteDroite ? "true" : "false";
        json += ",";

        json += "\"alerteAvant\":";
        json += alerteAvant ? "true" : "false";
        json += ",";

        json += "\"alerteArriere\":";
        json += alerteArriere ? "true" : "false";
        json += ",";

        json += "\"alarmeMuette\":";
        json += alarmeMuette ? "true" : "false";

        json += "}";


        server.send(
            200,
            "application/json",
            json
        );

    });


    server.begin();

}

// ==================== TELECOMMANDE IR ====================

void lireTelecommande() {
    if (IrReceiver.decode()) {
        uint16_t cmd = IrReceiver.decodedIRData.command;
        uint8_t flags = IrReceiver.decodedIRData.flags;

        if (cmd != 0) {
            derniereCommande = cmd;
            traiterCommandeIR(cmd);
        } else if (flags & IRDATA_FLAGS_IS_REPEAT) {
            if (derniereCommande != 0) {
                traiterCommandeIR(derniereCommande);
            }
        }

        IrReceiver.resume();
    }
}

void traiterCommandeIR(uint16_t cmd) {
    if (cmd == CMD_AVANT) {
        traiterCommandeTexte("avant");
    } else if (cmd == CMD_ARRIERE) {
        traiterCommandeTexte("arriere");
    } else if (cmd == CMD_GAUCHE) {
        traiterCommandeTexte("gauche");
    } else if (cmd == CMD_DROITE) {
        traiterCommandeTexte("droite");
    } else if (cmd == CMD_KLAXON) {
        traiterCommandeTexte("klaxon");
    } else if (cmd == CMD_CH) {
        traiterCommandeTexte("feuxavant");
    } else if (cmd == CMD_PAUSE) {
        traiterCommandeTexte("feuxrouges");
    }
}

// ==================== BLUETOOTH ====================

void lireBluetooth() {
    if (!SerialBT.available()) {
        return;
    }

    String commande = SerialBT.readStringUntil('\n');

    commande.trim();
    commande.toLowerCase();

    traiterCommandeTexte(commande);
}

// ==================== COMMANDES ====================

void traiterCommandeTexte(String action) {
    action.trim();
    action.toLowerCase();

    if (action == "avant" || action == "avance") {
        if (dangerAvant()) {
            stopMoteurs();

            alerteAvant = true;
            alerteArriere = false;
            alarmeMuette = false;

            return;
        }

        arreterAlerte();
        avancer();

        dernierSignalMoteur = millis();
    }

    else if (action == "arriere" || action == "arrière" || action == "recule") {
        if (dangerArriere()) {
            stopMoteurs();

            alerteArriere = true;
            alerteAvant = false;
            alarmeMuette = false;

            return;
        }

        arreterAlerte();
        reculer();

        dernierSignalMoteur = millis();
    }

    else if (action == "gauche") {
        arreterAlerte();

        tournerGauche();
        activerClignotant(true);

        dernierSignalVirage = millis();
        dernierSignalMoteur = millis();
    }

    else if (action == "droite") {
        arreterAlerte();

        tournerDroite();
        activerClignotant(false);

        dernierSignalVirage = millis();
        dernierSignalMoteur = millis();
    }

    else if (action == "stop") {
        stopMoteurs();

        clignotantActif = false;

        eteindreJaunes();
        appliquerFeuxRouges();
    }

    else if (action == "klaxon") {
        if (alerteAvant || alerteArriere) {
            alarmeMuette = true;
            digitalWrite(KLAXON_PIN, LOW);
        } else {
            digitalWrite(KLAXON_PIN, HIGH);
            dernierSignalKlaxon = millis();
        }
    }

    else if (action == "feuxavant" || action == "feux_avant") {
        ledAvantEtat = !ledAvantEtat;
        digitalWrite(LED_AVANT, ledAvantEtat);
    }

    else if (action == "feuxrouges" || action == "feux_rouges") {
        feuxRougesEtat = !feuxRougesEtat;

        if (!clignotantActif && !alerteAvant && !alerteArriere) {
            appliquerFeuxRouges();
        }
    }

    else if (action == "mute" || action == "silence") {
        if (alerteAvant || alerteArriere) {
            alarmeMuette = true;
            digitalWrite(KLAXON_PIN, LOW);
        }
    }
}

// ==================== ULTRASON ====================

float mesurerDistance(int trigPin, int echoPin) {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(5);

    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);

    digitalWrite(trigPin, LOW);

    long duree = pulseIn(echoPin, HIGH, 25000);

    if (duree == 0) {
        return -1;
    }

    float distance = duree * 0.0343 / 2.0;

    if (distance < 2 || distance > 200) {
        return -1;
    }

    return distance;
}

bool dangerAvant() {
    float distance = mesurerDistance(TRIG_AVANT, ECHO_AVANT);

    return distance > 0 && distance <= DISTANCE_DANGER;
}

bool dangerArriere() {
    float distance = mesurerDistance(TRIG_ARRIERE, ECHO_ARRIERE);

    return distance > 0 && distance <= DISTANCE_DANGER;
}

// ==================== ALERTE OBSTACLE ====================

void gererAlerteObstacle() {
    if (!alerteAvant && !alerteArriere) {
        return;
    }

    if (millis() - dernierControleObstacle >= intervalControleObstacle) {
        dernierControleObstacle = millis();

        if (alerteAvant && !dangerAvant()) {
            arreterAlerte();
            return;
        }

        if (alerteArriere && !dangerArriere()) {
            arreterAlerte();
            return;
        }
    }

    if (millis() - dernierBlink >= intervalBlink) {
        dernierBlink = millis();
        etatBlink = !etatBlink;
    }

    if (alarmeMuette) {
        digitalWrite(KLAXON_PIN, LOW);
    } else {
        digitalWrite(KLAXON_PIN, etatBlink);
    }

    if (alerteAvant) {
        digitalWrite(LAMPE_JAUNE_GAUCHE, etatBlink);
        digitalWrite(LAMPE_JAUNE_DROITE, etatBlink);

        digitalWrite(LAMPE_ROUGE_GAUCHE, feuxRougesEtat);
        digitalWrite(LAMPE_ROUGE_DROITE, feuxRougesEtat);
    }

    if (alerteArriere) {
        eteindreJaunes();

        digitalWrite(LAMPE_ROUGE_GAUCHE, etatBlink);
        digitalWrite(LAMPE_ROUGE_DROITE, etatBlink);
    }
}

void arreterAlerte() {
    alerteAvant = false;
    alerteArriere = false;
    alarmeMuette = false;

    digitalWrite(KLAXON_PIN, LOW);

    eteindreJaunes();
    appliquerFeuxRouges();
}

// ==================== MOTEURS ====================

void avancer() {
    digitalWrite(MG_IN1, HIGH);
    digitalWrite(MG_IN2, LOW);

    digitalWrite(MD_IN1, HIGH);
    digitalWrite(MD_IN2, LOW);
}

void reculer() {
    digitalWrite(MG_IN1, LOW);
    digitalWrite(MG_IN2, HIGH);

    digitalWrite(MD_IN1, LOW);
    digitalWrite(MD_IN2, HIGH);
}

void tournerGauche() {
    digitalWrite(MG_IN1, LOW);
    digitalWrite(MG_IN2, HIGH);

    digitalWrite(MD_IN1, HIGH);
    digitalWrite(MD_IN2, LOW);
}

void tournerDroite() {
    digitalWrite(MG_IN1, HIGH);
    digitalWrite(MG_IN2, LOW);

    digitalWrite(MD_IN1, LOW);
    digitalWrite(MD_IN2, HIGH);
}

void stopMoteurs() {
    digitalWrite(MG_IN1, LOW);
    digitalWrite(MG_IN2, LOW);

    digitalWrite(MD_IN1, LOW);
    digitalWrite(MD_IN2, LOW);
}

// ==================== CLIGNOTANTS ====================

void activerClignotant(bool gauche) {
    clignotantActif = true;
    clignotantGauche = gauche;
}

void gererClignotants() {
    if (!clignotantActif || alerteAvant || alerteArriere) {
        return;
    }

    if (millis() - dernierBlink >= intervalBlink) {
        dernierBlink = millis();
        etatBlink = !etatBlink;
    }

    if (clignotantGauche) {
        digitalWrite(LAMPE_JAUNE_GAUCHE, etatBlink);
        digitalWrite(LAMPE_ROUGE_GAUCHE, etatBlink);

        digitalWrite(LAMPE_JAUNE_DROITE, LOW);
        digitalWrite(LAMPE_ROUGE_DROITE, feuxRougesEtat);
    } else {
        digitalWrite(LAMPE_JAUNE_DROITE, etatBlink);
        digitalWrite(LAMPE_ROUGE_DROITE, etatBlink);

        digitalWrite(LAMPE_JAUNE_GAUCHE, LOW);
        digitalWrite(LAMPE_ROUGE_GAUCHE, feuxRougesEtat);
    }
}

// ==================== FEUX ====================

void eteindreJaunes() {
    digitalWrite(LAMPE_JAUNE_GAUCHE, LOW);
    digitalWrite(LAMPE_JAUNE_DROITE, LOW);
}

void appliquerFeuxRouges() {
    digitalWrite(LAMPE_ROUGE_GAUCHE, feuxRougesEtat);
    digitalWrite(LAMPE_ROUGE_DROITE, feuxRougesEtat);
}
