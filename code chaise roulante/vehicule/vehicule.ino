/***********************************************************************
 * Projet : Véhicule intelligent ESP32 piloté par IR, Web et Voix
 *
 * Développé par : Exaucé KIMBEMBE
 * Pseudo        : @OpenProgramming
 * Date          : 24 Mai 2026
 * Version       : 2.1
 *
 * Description :
 * - Mode IR par défaut
 * - Mode Web via application stockée dans LittleFS
 * - Mode Voix via navigateur
 * - Sécurité obstacle avant/arrière
 * - Klaxon, feux avant, feux rouges et clignotants
 ***********************************************************************/

#define DECODE_NEC

#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <IRremote.hpp>

// ==================== WIFI ====================
const char* WIFI_SSID = "Vehicule_ESP32";
const char* WIFI_PASSWORD = "12345678";

WebServer server(80);

// ==================== MODES ====================
enum ModeCommande {
  MODE_IR,
  MODE_WEB,
  MODE_VOICE
};

ModeCommande modeCommande = MODE_IR;

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

// ==================== MOUVEMENT ====================
enum Mouvement {
  MVT_STOP,
  MVT_AVANT,
  MVT_ARRIERE,
  MVT_GAUCHE,
  MVT_DROITE
};

Mouvement mouvementActuel = MVT_STOP;

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

const unsigned long timeoutMoteurIR = 300;
const unsigned long timeoutKlaxon = 300;
const unsigned long timeoutVirage = 300;

const unsigned long intervalBlink = 150;
const unsigned long intervalControleObstacle = 200;

// ==================== SETUP ====================
void setup() {
  pinMode(IR_PIN, INPUT_PULLUP);

  pinMode(TRIG_AVANT, OUTPUT);
  pinMode(ECHO_AVANT, INPUT);

  pinMode(TRIG_ARRIERE, OUTPUT);
  pinMode(ECHO_ARRIERE, INPUT);

  digitalWrite(TRIG_AVANT, LOW);
  digitalWrite(TRIG_ARRIERE, LOW);

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

  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);

  setupServeurWeb();
}

// ==================== LOOP ====================
void loop() {
  server.handleClient();

  if (modeCommande == MODE_IR) {
    lireTelecommande();
  }

  gererSecuriteObstacleContinue();
  gererClignotants();
  gererAlerteObstacle();

  if (modeCommande == MODE_IR) {
    if (millis() - dernierSignalMoteur > timeoutMoteurIR) {
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

// Initialise LittleFS et sert les fichiers HTML/CSS/JS stockés dans /data.
void setupServeurWeb() {
  LittleFS.begin(true);

  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  server.on("/mode", changerMode);
  server.on("/cmd", recevoirCommandeWeb);
  server.on("/etat", envoyerEtat);

  server.begin();
}

// Change le mode actif : IR, Web ou Voix.
void changerMode() {
  if (!server.hasArg("value")) {
    server.send(400, "text/plain", "mode manquant");
    return;
  }

  String value = server.arg("value");

  stopMoteurs();
  arreterAlerte();
  mouvementActuel = MVT_STOP;

  if (value == "ir") {
    modeCommande = MODE_IR;
  } else if (value == "web") {
    modeCommande = MODE_WEB;
  } else if (value == "voice") {
    modeCommande = MODE_VOICE;
  }

  server.send(200, "text/plain", "ok");
}

// Reçoit les commandes venant des boutons web ou de la voix.
void recevoirCommandeWeb() {
  if (!server.hasArg("action")) {
    server.send(400, "text/plain", "action manquante");
    return;
  }

  String action = server.arg("action");

  if (modeCommande == MODE_WEB || modeCommande == MODE_VOICE) {
    traiterActionTexte(action);
  }

  server.send(200, "text/plain", "ok");
}

// Envoie l'état actuel du véhicule à l'application web.
void envoyerEtat() {
  String mode = "ir";

  if (modeCommande == MODE_WEB) {
    mode = "web";
  } else if (modeCommande == MODE_VOICE) {
    mode = "voice";
  }

  String json = "{";
  json += "\"mode\":\"" + mode + "\",";
  json += "\"ledAvant\":" + String(ledAvantEtat ? "true" : "false") + ",";
  json += "\"feuxRouges\":" + String(feuxRougesEtat ? "true" : "false") + ",";
  json += "\"alerteAvant\":" + String(alerteAvant ? "true" : "false") + ",";
  json += "\"alerteArriere\":" + String(alerteArriere ? "true" : "false") + ",";
  json += "\"alarmeMuette\":" + String(alarmeMuette ? "true" : "false");
  json += "}";

  server.send(200, "application/json", json);
}

// Convertit une commande texte en action véhicule.
void traiterActionTexte(String action) {
  if (action == "avant") {
    commandeAvancerContinu();
  } else if (action == "arriere") {
    commandeReculerContinu();
  } else if (action == "gauche") {
    commandeGaucheContinu();
  } else if (action == "droite") {
    commandeDroiteContinu();
  } else if (action == "stop") {
    commandeStop();
  } else if (action == "klaxon") {
    commandeKlaxon();
  } else if (action == "feuxavant") {
    commandeFeuxAvant();
  } else if (action == "feuxrouges") {
    commandeFeuxRouges();
  } else if (action == "mute") {
    couperAlarme();
  }
}

// ==================== TELECOMMANDE IR ====================

// Lit les signaux IR NEC et gère les touches maintenues.
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

// Traite les commandes de la télécommande IR.
void traiterCommandeIR(uint16_t cmd) {
  if (cmd == CMD_AVANT) {
    commandeAvancerIR();
  } else if (cmd == CMD_ARRIERE) {
    commandeReculerIR();
  } else if (cmd == CMD_GAUCHE) {
    commandeGaucheIR();
  } else if (cmd == CMD_DROITE) {
    commandeDroiteIR();
  } else if (cmd == CMD_KLAXON) {
    commandeKlaxon();
  } else if (cmd == CMD_CH) {
    commandeFeuxAvant();
  } else if (cmd == CMD_PAUSE) {
    commandeFeuxRouges();
  }
}

// ==================== COMMANDES VEHICULE ====================

// Avance en mode IR : la touche doit rester maintenue.
void commandeAvancerIR() {
  if (dangerAvant()) {
    stopMoteurs();
    mouvementActuel = MVT_STOP;
    activerAlerteAvant();
    return;
  }

  arreterAlerte();
  avancer();
  mouvementActuel = MVT_AVANT;
  dernierSignalMoteur = millis();
}

// Recule en mode IR : la touche doit rester maintenue.
void commandeReculerIR() {
  if (dangerArriere()) {
    stopMoteurs();
    mouvementActuel = MVT_STOP;
    activerAlerteArriere();
    return;
  }

  arreterAlerte();
  reculer();
  mouvementActuel = MVT_ARRIERE;
  dernierSignalMoteur = millis();
}

// Tourne à gauche en mode IR.
void commandeGaucheIR() {
  arreterAlerte();

  tournerGauche();
  mouvementActuel = MVT_GAUCHE;

  activerClignotant(true);

  dernierSignalVirage = millis();
  dernierSignalMoteur = millis();
}

// Tourne à droite en mode IR.
void commandeDroiteIR() {
  arreterAlerte();

  tournerDroite();
  mouvementActuel = MVT_DROITE;

  activerClignotant(false);

  dernierSignalVirage = millis();
  dernierSignalMoteur = millis();
}

// Avance en mode Web ou Voix jusqu'à stop ou obstacle.
void commandeAvancerContinu() {
  if (dangerAvant()) {
    stopMoteurs();
    mouvementActuel = MVT_STOP;
    activerAlerteAvant();
    return;
  }

  arreterAlerte();
  avancer();
  mouvementActuel = MVT_AVANT;
}

// Recule en mode Web ou Voix jusqu'à stop ou obstacle.
void commandeReculerContinu() {
  if (dangerArriere()) {
    stopMoteurs();
    mouvementActuel = MVT_STOP;
    activerAlerteArriere();
    return;
  }

  arreterAlerte();
  reculer();
  mouvementActuel = MVT_ARRIERE;
}

// Tourne à gauche en mode Web ou Voix.
void commandeGaucheContinu() {
  arreterAlerte();

  tournerGauche();
  mouvementActuel = MVT_GAUCHE;

  activerClignotant(true);
  dernierSignalVirage = millis();
}

// Tourne à droite en mode Web ou Voix.
void commandeDroiteContinu() {
  arreterAlerte();

  tournerDroite();
  mouvementActuel = MVT_DROITE;

  activerClignotant(false);
  dernierSignalVirage = millis();
}

// Stoppe le véhicule.
void commandeStop() {
  stopMoteurs();
  mouvementActuel = MVT_STOP;

  clignotantActif = false;
  eteindreJaunes();
  appliquerFeuxRouges();
}

// Active le klaxon normal ou coupe l'alarme si une alerte est active.
void commandeKlaxon() {
  if (alerteAvant || alerteArriere) {
    couperAlarme();
    return;
  }

  digitalWrite(KLAXON_PIN, HIGH);
  dernierSignalKlaxon = millis();
}

// Allume ou éteint la LED avant.
void commandeFeuxAvant() {
  ledAvantEtat = !ledAvantEtat;
  digitalWrite(LED_AVANT, ledAvantEtat);
}

// Allume ou éteint les feux rouges arrière.
void commandeFeuxRouges() {
  feuxRougesEtat = !feuxRougesEtat;

  if (!clignotantActif && !alerteAvant && !alerteArriere) {
    appliquerFeuxRouges();
  }
}

// Coupe seulement le bip d'alarme.
void couperAlarme() {
  if (alerteAvant || alerteArriere) {
    alarmeMuette = true;
    digitalWrite(KLAXON_PIN, LOW);
  }
}

// ==================== SECURITE OBSTACLE ====================

// Surveille les obstacles pendant un mouvement continu.
void gererSecuriteObstacleContinue() {
  if (millis() - dernierControleObstacle < intervalControleObstacle) {
    return;
  }

  dernierControleObstacle = millis();

  if (mouvementActuel == MVT_AVANT && dangerAvant()) {
    stopMoteurs();
    mouvementActuel = MVT_STOP;
    activerAlerteAvant();
  }

  if (mouvementActuel == MVT_ARRIERE && dangerArriere()) {
    stopMoteurs();
    mouvementActuel = MVT_STOP;
    activerAlerteArriere();
  }
}

// ==================== ULTRASON ====================

// Mesure une distance avec un capteur ultrason.
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

// Vérifie si un obstacle est dangereux devant.
bool dangerAvant() {
  float distance = mesurerDistance(TRIG_AVANT, ECHO_AVANT);
  return distance > 0 && distance <= DISTANCE_DANGER;
}

// Vérifie si un obstacle est dangereux derrière.
bool dangerArriere() {
  float distance = mesurerDistance(TRIG_ARRIERE, ECHO_ARRIERE);
  return distance > 0 && distance <= DISTANCE_DANGER;
}

// ==================== ALERTE OBSTACLE ====================

// Active l'alerte obstacle avant.
void activerAlerteAvant() {
  alerteAvant = true;
  alerteArriere = false;
  alarmeMuette = false;
  clignotantActif = false;
}

// Active l'alerte obstacle arrière.
void activerAlerteArriere() {
  alerteArriere = true;
  alerteAvant = false;
  alarmeMuette = false;
  clignotantActif = false;
}

// Gère le bip et les LEDs pendant une alerte.
void gererAlerteObstacle() {
  if (!alerteAvant && !alerteArriere) {
    return;
  }

  if (alerteAvant && !dangerAvant()) {
    arreterAlerte();
    return;
  }

  if (alerteArriere && !dangerArriere()) {
    arreterAlerte();
    return;
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

// Arrête l'alerte et restaure les LEDs.
void arreterAlerte() {
  alerteAvant = false;
  alerteArriere = false;
  alarmeMuette = false;

  digitalWrite(KLAXON_PIN, LOW);

  eteindreJaunes();
  appliquerFeuxRouges();
}

// ==================== MOTEURS ====================

// Fait avancer le véhicule.
void avancer() {
  digitalWrite(MG_IN1, HIGH);
  digitalWrite(MG_IN2, LOW);

  digitalWrite(MD_IN1, HIGH);
  digitalWrite(MD_IN2, LOW);
}

// Fait reculer le véhicule.
void reculer() {
  digitalWrite(MG_IN1, LOW);
  digitalWrite(MG_IN2, HIGH);

  digitalWrite(MD_IN1, LOW);
  digitalWrite(MD_IN2, HIGH);
}

// Fait tourner le véhicule à gauche.
void tournerGauche() {
  digitalWrite(MG_IN1, LOW);
  digitalWrite(MG_IN2, HIGH);

  digitalWrite(MD_IN1, HIGH);
  digitalWrite(MD_IN2, LOW);
}

// Fait tourner le véhicule à droite.
void tournerDroite() {
  digitalWrite(MG_IN1, HIGH);
  digitalWrite(MG_IN2, LOW);

  digitalWrite(MD_IN1, LOW);
  digitalWrite(MD_IN2, HIGH);
}

// Arrête les deux moteurs.
void stopMoteurs() {
  digitalWrite(MG_IN1, LOW);
  digitalWrite(MG_IN2, LOW);

  digitalWrite(MD_IN1, LOW);
  digitalWrite(MD_IN2, LOW);
}

// ==================== CLIGNOTANTS ====================

// Active le clignotant gauche ou droit.
void activerClignotant(bool gauche) {
  clignotantActif = true;
  clignotantGauche = gauche;
}

// Gère le clignotement pendant les virages.
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

// Éteint uniquement les LEDs jaunes.
void eteindreJaunes() {
  digitalWrite(LAMPE_JAUNE_GAUCHE, LOW);
  digitalWrite(LAMPE_JAUNE_DROITE, LOW);
}

// Applique l'état mémorisé des feux rouges.
void appliquerFeuxRouges() {
  digitalWrite(LAMPE_ROUGE_GAUCHE, feuxRougesEtat);
  digitalWrite(LAMPE_ROUGE_DROITE, feuxRougesEtat);
}